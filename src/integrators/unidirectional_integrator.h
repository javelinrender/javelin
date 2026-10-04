// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include "integrator.h"
#include "render_settings.h"

namespace javelin::integrators {

class UnidirectionalIntegrator final : public Integrator {
public:
    explicit UnidirectionalIntegrator(const RenderSettings& settings)
        : _accumulation(settings.width.value, settings.height.value, 3) {}

    std::unique_ptr<Result> accumulate(
        const Scene& scene,
        ThreadPool* pool,
        int sampleCount,
        std::stop_token stopToken) override;

private:
    Image _accumulation;
    int _samplesPerPixel = 0;
};

} // namespace javelin::integrators
