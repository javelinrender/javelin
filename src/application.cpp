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
          _settings.width.value,
          _settings.height.value,
          23.7f, 0.020955f,
          Vec3(0.065315, 0.195, 0.179485),
          Vec3(-0.262003f, -0.642788f, -0.719846f),
          Vec3(-0.219846f, 0.766044f, -0.604023f))) {}

          // _settings.width.value, _settings.height.value, 45.0f, 0.032f,
          // Vec3(0.0f, 0.0f, 1.5f),
          // Vec3(0.0f, 0.0f, -1.0f),
          // Vec3(0.0f, 1.0f, 0.0f))) {}
          
bool Application::initialize() {
    if (_initialized && _loadedScenePath == _settings.sceneFile.value)
        return true;
    if (!_scene.loadGltf(_settings.sceneFile.value))
        return false;
    _loadedScenePath = _settings.sceneFile.value;
    _initialized = true;
    return true;
}

void Application::startRender() {
    _renderer.stop();
    if (!initialize())
        return;
    _pool.setThreadCount(_settings.threadPoolSize.value);
    _scene.camera.setResolution(_settings.width.value, _settings.height.value);
    _renderer.start(_scene, _settings, &_pool, 32);
}

void Application::handleSettingsChanged() {
    _renderer.setSampleLimit(_settings.samplesPerPixel.value);
}

} // namespace javelin
