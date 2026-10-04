// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "renderer.h"

#include <algorithm>
#include <utility>

#include "integrators/metropolis_integrator.h"
#include "integrators/unidirectional_integrator.h"
#include "profiling.h"
#include "system.h"

namespace javelin {

void Renderer::start(const Scene& scene, const RenderSettings& settings,
        ThreadPool* pool, int samplesPerBatch) {
    stop();
    switch (settings.integrator.value) {
    using namespace integrators;
    case RenderSettings::Integrator::Unidirectional:
        _integrator = std::make_unique<UnidirectionalIntegrator>(settings);
        break;
    case RenderSettings::Integrator::Metropolis:
        _integrator = std::make_unique<MetropolisIntegrator>(settings);
        break;
    }
    {
        std::lock_guard lock(_mutex);
        _update.reset();
        _settings = settings;
        _completedSamples = 0;

        _integrating = false;
        _finished = false;
        _paused = false;
    }
    _renderThread = std::jthread(
        [this, &scene, pool, samplesPerBatch](std::stop_token token) {
            run(scene, pool, samplesPerBatch, token);
        });
}

void Renderer::stop() {
    if (_renderThread.joinable()) {
        _renderThread.request_stop();
        _renderThread.join();
    }
    std::lock_guard lock(_mutex);
    _finished = true;
    _paused = false;
}

void Renderer::pause() {
    std::lock_guard lock(_mutex);
    if (!_finished)
        _paused = true;
}

void Renderer::resume() {
    if (!_renderThread.joinable())
        return;
    {
        std::lock_guard lock(_mutex);
        _paused = _completedSamples >= _settings.samplesPerPixel.value;
        if (!_paused)
            _finished = false;
    }
    _pausedCV.notify_all();
    _updateCV.notify_all();
}

void Renderer::setSampleLimit(int sampleLimit) {
    std::lock_guard lock(_mutex);
    _settings.samplesPerPixel.value = sampleLimit;
}

bool Renderer::isRunning() const {
    std::lock_guard lock(_mutex);
    return !_finished;
}

bool Renderer::isPaused() const {
    std::lock_guard lock(_mutex);
    return _paused;
}

std::unique_ptr<RenderUpdate> Renderer::waitForUpdate(
        int sampleCount, std::stop_token stopToken) {
    std::unique_lock lock(_mutex);
    const auto hasUpdate =
        [&] { return _update && _update->samplesPerPixel > sampleCount; };
    const bool ready = _updateCV.wait(
        lock, stopToken, [&] { return _finished || hasUpdate(); });
    if (ready && hasUpdate())
        return std::move(_update);
    return nullptr;
}

void Renderer::startIntegration(
        std::unique_ptr<integrators::Integrator::Result> result,
        ThreadPool* pool,
        std::stop_token stopToken) {
    {
        std::lock_guard lock(_mutex);
        _integrating = true;
    }
    auto work = [this, result = std::move(result), stopToken] {
        JAVELIN_ZONE_SCOPED_N("Eval integrator result")
        auto update = std::make_unique<RenderUpdate>();
        update->samplesPerPixel = result->samplesPerPixel();
        update->image = result->evaluate();
        std::lock_guard lock(_mutex);
        if (!stopToken.stop_requested()) {

            _update = std::move(update);
            _updateCV.notify_all();
        }
        _integrating = false;
        _integrationCV.notify_all();
    };
    if (pool)
        pool->assignWork(std::move(work));
    else
        work();
}

void Renderer::waitForIntegration() {
    std::unique_lock lock(_mutex);
    _integrationCV.wait(lock, [&] { return !_integrating; });
}

void Renderer::run(const Scene& scene, ThreadPool* pool,
        int samplesPerBatch, std::stop_token stopToken) {
    javelin::system::setThreadName("Renderer");
    using Result = integrators::Integrator::Result;
    while (!stopToken.stop_requested()) {
        int batchSamples;
        {
            std::unique_lock lock(_mutex);
            if (_completedSamples >= _settings.samplesPerPixel.value) {
                // On completion, ensure the final integral is evaluated.
                _integrationCV.wait(lock, [&] { return !_integrating; });
                _finished = true;
                _paused = true;
                _updateCV.notify_all();
            }
            const bool ready = _pausedCV.wait(
                lock, stopToken, [&] { return !_paused; });
            if (!ready || stopToken.stop_requested())
                break;
            if (_completedSamples >= _settings.samplesPerPixel.value)
                continue;
            batchSamples = std::min(
                samplesPerBatch,
                _settings.samplesPerPixel.value - _completedSamples);
        }

        std::unique_ptr<Result> result =
            _integrator->accumulate(scene, pool, batchSamples, stopToken);
        if (!result || stopToken.stop_requested())
            break;
        {
            std::lock_guard lock(_mutex);
            _completedSamples = result->samplesPerPixel();
        }

        waitForIntegration();
        startIntegration(std::move(result), pool, stopToken);
    }

    // Restart and shutdown must not leave integration work using the old pool.
    waitForIntegration();
    {
        std::lock_guard lock(_mutex);
        _finished = true;
    }
    _updateCV.notify_all();
}

} // namespace javelin
