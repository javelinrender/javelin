// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "application.h"

#include <utility>

namespace javelin {

Application::Application(RenderSettings settings)
    : _settings(std::move(settings)),
      _pool(_settings.threadPoolSize.value),
      _scene(Camera(
          _settings.width.value, _settings.height.value, 45.0f, 0.032f,
          Vec3(0.0f, 0.0f, 1.5f),
          Vec3(0.0f, 0.0f, -1.0f),
          Vec3(0.0f, 1.0f, 0.0f))) {}


bool Application::initialize() {
    if (!_initialized)
        _initialized = _scene.loadGltf("./media/cornellbox_plane_light.glb");
    return _initialized;
}

void Application::startRender() {
    if (!_initialized)
        return;
    _renderer.stop();
    _pool.setThreadCount(_settings.threadPoolSize.value);
    _scene.camera.setResolution(_settings.width.value, _settings.height.value);
    _renderer.start(_scene, _settings, &_pool, 32);
}

void Application::handleSettingsChanged() {
    _renderer.setSampleLimit(_settings.samplesPerPixel.value);
}

} // namespace javelin
