// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <QApplication>
#include <QQmlApplicationEngine>

#include "application.h"
#include "render_settings_model.h"
#include "viewport.h"

namespace javelin::ui {

class QtApplication : public QApplication {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY renderStateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY renderStateChanged)

public:
    QtApplication(int& argc, char* argv[], RenderSettings settings);
    ~QtApplication() override;

    int run();
    bool running() const { return _application.running(); }
    bool paused() const { return _application.paused(); }

    Q_INVOKABLE void startRender();
    Q_INVOKABLE void pauseRender();
    Q_INVOKABLE void resumeRender();

signals:
    void renderStateChanged();

private:
    void handleSettingsChanged();

    Application _application;
    RenderSettingsModel _settingsModel;
    Viewport _viewport;
    QQmlApplicationEngine _engine;
};

} // namespace javelin::ui
