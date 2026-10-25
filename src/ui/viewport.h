// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cstdint>
#include <mutex>
#include <thread>

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QUrl>

#include "renderer.h"

namespace javelin::ui {

class Viewport : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY imageChanged)
    Q_PROPERTY(int samplesPerPixel READ samplesPerPixel NOTIFY imageChanged)

public:
    explicit Viewport(Renderer& renderer, QObject* parent = nullptr);
    ~Viewport() override;

    QUrl imageSource() const;
    int samplesPerPixel() const { return _samplesPerPixel; }

    /// The QML engine takes ownership of the returned provider and must be
    /// destroyed before this viewport.
    QQuickImageProvider* createImageProvider();
    void startListening(bool reset = true);
    void stopListening();

signals:
    void imageChanged();
    void renderingFinished();

private:
    struct State {
        std::mutex mutex;
        QImage latestImage;
        int samplesPerPixel = 0;
        bool uiUpToDate = true;
    };
    class ImageProvider;

    Renderer& _renderer;
    State _state;

    int _samplesPerPixel = 0;
    /// Incremented on each restart.
    std::uint64_t _version = 0;
    std::jthread _listener;
};

} // namespace javelin::ui
