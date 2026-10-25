// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "viewport.h"

#include <mutex>
#include <utility>

#include <QImage>
#include <QMetaObject>

#include "system.h"

namespace javelin::ui {

class Viewport::ImageProvider : public QQuickImageProvider {
public:
    explicit ImageProvider(State& state)
        : QQuickImageProvider(QQuickImageProvider::Image), _state(state) {}

    QImage requestImage(const QString&, QSize* size, const QSize&) override {
        QImage image;
        {
            std::lock_guard lock(_state.mutex);
            image = _state.latestImage;
        }
        if (size)
            *size = image.size();
        return image;
    }

private:
    State& _state;
};

Viewport::Viewport(
        Renderer& renderer, QObject* parent)
    : QObject(parent), _renderer(renderer) {}

Viewport::~Viewport() {
    stopListening();
}

void Viewport::stopListening() {
    ++_version;
    if (_listener.joinable()) {
        _listener.request_stop();
        _listener.join();
    }
    std::lock_guard lock(_state.mutex);
    _state.uiUpToDate = true;
}

QUrl Viewport::imageSource() const {
    if (_samplesPerPixel == 0)
        return {};
    return QUrl(QStringLiteral("image://viewport/%1/%2")
            .arg(_version).arg(_samplesPerPixel));
}

QQuickImageProvider* Viewport::createImageProvider() {
    return new ImageProvider(_state);
}

void Viewport::startListening(bool reset) {
    stopListening();
    {
        std::lock_guard lock(_state.mutex);
        if (reset) {
            _state.latestImage = {};
            _state.samplesPerPixel = 0;
        }
        _samplesPerPixel = _state.samplesPerPixel;
    }
    emit imageChanged();

    const auto version = _version;
    _listener = std::jthread([this, version, sampleCount = _samplesPerPixel]
            (std::stop_token token) mutable {
        javelin::system::setThreadName("Viewport");
        while (!token.stop_requested()) {
            std::unique_ptr<RenderUpdate> update =
                _renderer.waitForUpdate(sampleCount, token);
            if (!update)
                break;
            sampleCount = update->samplesPerPixel;
            update->image.applyCorrection();
            update->image.flipVertically();

            // Wrap render update image data in QImage, with callback to clean
            // up the unique ptr when the QImage is destroyed.
            javelin::RenderUpdate* owner = update.release();
            QImage image(
                reinterpret_cast<const uchar*>(owner->image.pixels()),
                static_cast<int>(owner->image.width()),
                static_cast<int>(owner->image.height()),
                static_cast<qsizetype>(owner->image.width() * 4 * sizeof(float)),
                QImage::Format_RGBA32FPx4,
                [](void* data) { delete static_cast<RenderUpdate*>(data); },
                owner);

            bool notify;
            {
                std::lock_guard lock(_state.mutex);
                _state.latestImage = std::move(image);
                _state.samplesPerPixel = sampleCount;
                notify = _state.uiUpToDate;
                _state.uiUpToDate = false;
            }
            if (!notify)
                continue;

            // Coalesce multiple updates
            QMetaObject::invokeMethod(this, [this, version] {
                if (version != _version)
                    return;
                {
                    std::lock_guard lock(_state.mutex);
                    _samplesPerPixel = _state.samplesPerPixel;
                    _state.uiUpToDate = true;
                }
                emit imageChanged();
            }, Qt::QueuedConnection);
        }
        if (!token.stop_requested()) {
            QMetaObject::invokeMethod(this, [this, version] {
                if (version == _version)
                    emit renderingFinished();
            }, Qt::QueuedConnection);
        }
    });
}

} // namespace javelin::ui
