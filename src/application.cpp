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
    applyShaderballMaterial();
    _renderer.start(_scene, _settings, &_pool, 32);
}

void Application::applyShaderballMaterial() {
    using Parameters = ShaderballMaterialParameters;
    namespace materials = javelin::materials;
    const Parameters& shaderball = _settings.shaderballMaterial;
    const Vec4 baseColor(
        shaderball.baseColorR.value,
        shaderball.baseColorG.value,
        shaderball.baseColorB.value,
        1.0f);
    const float roughness = shaderball.roughness.value;
    const bool isMetal = shaderball.bsdf.value == Parameters::Bsdf::DisneyMetal;
    const float anisotropy = isMetal ? shaderball.anisotropy.value : 0.0f;

    materials::BSDF bsdf;
    switch (shaderball.bsdf.value) {
    case Parameters::Bsdf::LambertianDiffuse:
        bsdf = materials::bsdf::LambertianDiffuse{.baseColorFactor = baseColor};
        break;
    case Parameters::Bsdf::DisneyDiffuse:
        bsdf = materials::bsdf::DisneyDiffuse{
            .baseColorFactor = baseColor,
            .roughnessFactor = roughness};
        break;
    case Parameters::Bsdf::DisneyMetal:
        bsdf = materials::bsdf::DisneyMetal{
            .baseColorFactor = baseColor,
            .roughnessFactor = roughness,
            .anisotropyFactor = anisotropy};
        break;
    }

    materials::PDF pdf;
    switch (shaderball.pdf.value) {
    case Parameters::Pdf::Uniform:
        pdf = materials::pdf::Uniform{};
        break;
    case Parameters::Pdf::CosineWeighted:
        pdf = materials::pdf::CosineWeighted{};
        break;
    case Parameters::Pdf::GGXVNDF:
        pdf = materials::pdf::GGXVNDF{
            .roughnessFactor = roughness,
            .anisotropyFactor = anisotropy};
        break;
    }

    _scene.setShaderballMaterial(bsdf, pdf);
}

void Application::handleSettingsChanged() {
    _renderer.setSampleLimit(_settings.samplesPerPixel.value);
}

} // namespace javelin
