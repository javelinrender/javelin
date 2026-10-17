// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "qt_application.h"

#include <utility>

#include <QQmlContext>
#include <QQuickStyle>

namespace javelin::ui {

QtApplication::QtApplication(int& argc, char* argv[], RenderSettings settings)
    : QApplication(argc, argv),
      _application(std::move(settings)),
      _settingsModel(_application.settings()),
      _viewport(_application.renderer()) {
    connect(&_settingsModel, &RenderSettingsModel::settingsChanged,
        this, &QtApplication::handleSettingsChanged);
    connect(&_viewport, &Viewport::renderingFinished,
        this, &QtApplication::renderStateChanged);
}

QtApplication::~QtApplication() {
    _viewport.stopListening();
    _application.renderer().stop();
}

int QtApplication::run() {
    if (!_application.initialize())
        return 1;

    _engine.addImageProvider("viewport", _viewport.createImageProvider());
    _engine.rootContext()->setContextProperty("app", this);
    _engine.rootContext()->setContextProperty("renderSettings", &_settingsModel);
    _engine.rootContext()->setContextProperty("viewport", &_viewport);
    _engine.loadFromModule("javelin_ui", "Main");
    if (_engine.rootObjects().isEmpty())
        return 1;
    _engine.loadFromModule("javelin_ui", "ViewportPanel");
    if (_engine.rootObjects().size() != 2)
        return 1;

    startRender();
    return exec();
}

void QtApplication::startRender() {
    _viewport.stopListening();
    _application.startRender();
    emit renderStateChanged();
    _viewport.startListening();
}

void QtApplication::pauseRender() {
    if (!running() || paused())
        return;
    _application.renderer().pause();
    emit renderStateChanged();
}

void QtApplication::resumeRender() {
    if (!paused())
        return;
    _viewport.stopListening();
    _application.renderer().resume();
    _viewport.startListening(false);
    emit renderStateChanged();
}

void QtApplication::handleSettingsChanged() {
    _application.handleSettingsChanged();
    emit renderStateChanged();
}

} // namespace javelin::ui
