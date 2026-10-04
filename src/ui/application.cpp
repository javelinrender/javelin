// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "application.h"

#include <utility>

#include <QQmlContext>
#include <QQuickStyle>

namespace javelin::ui {

Application::Application(int& argc, char* argv[], RenderSettings settings)
    : QGuiApplication(argc, argv),
      _application(std::move(settings)),
      _settingsModel(_application.settings()),
      _viewport(_application.renderer()) {
    QQuickStyle::setStyle("Fusion");
    connect(&_settingsModel, &RenderSettingsModel::settingsChanged,
        this, &Application::handleSettingsChanged);
    connect(&_viewport, &Viewport::renderingFinished,
        this, &Application::renderStateChanged);
}

Application::~Application() {
    _viewport.stopListening();
    _application.renderer().stop();
}

int Application::run() {
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

void Application::startRender() {
    _viewport.stopListening();
    _application.startRender();
    emit renderStateChanged();
    _viewport.startListening();
}

void Application::pauseRender() {
    if (!running() || paused())
        return;
    _application.renderer().pause();
    emit renderStateChanged();
}

void Application::resumeRender() {
    if (!paused())
        return;
    _viewport.stopListening();
    _application.renderer().resume();
    _viewport.startListening(false);
    emit renderStateChanged();
}

void Application::handleSettingsChanged() {
    _application.handleSettingsChanged();
    emit renderStateChanged();
}

} // namespace javelin::ui
