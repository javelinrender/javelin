// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once


#include "render_settings.h"
#include "renderer.h"
#include "scene.h"
#include "thread_pool.h"

namespace javelin {

class Application {
public:
    explicit Application(RenderSettings settings);

    bool initialize();
    RenderSettings& settings() { return _settings; }
    Renderer& renderer() { return _renderer; }

    bool running() const { return _renderer.isRunning(); }
    bool paused() const { return _renderer.isPaused(); }

    void startRender();

    void handleSettingsChanged();

private:
    RenderSettings _settings;
    ThreadPool _pool;
    Scene _scene;
    Renderer _renderer;

    bool _initialized = false;

};

} // namespace javelin
