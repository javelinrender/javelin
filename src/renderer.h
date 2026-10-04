// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

#include "image.h"
#include "integrators/integrator.h"
#include "render_settings.h"
#include "scene.h"
#include "thread_pool.h"

namespace javelin {

struct RenderUpdate {
    int samplesPerPixel = 0;
    Image image;
};

class Renderer {
public:
    Renderer() = default;

    ~Renderer() { stop(); }

    /// Starts progressive rendering.
    /// A null `pool` runs work on the render thread.
    /// @pre
    ///     `start`, `stop`, and `resume` calls must be serialized.
    /// @pre
    ///     `scene` and any supplied `pool` must remain alive until `stop`
    ///     returns, including while paused at the sample target. `scene` must
    ///     not be modified during this time.
    void start(
        const Scene& scene,
        const RenderSettings& settings,
        ThreadPool* pool = nullptr,
        int samplesPerBatch = 1);
    void stop();

    /// Pauses between completed batches, preserving accumulated samples.
    void pause();
    /// Continues accumulated work.
    void resume();
    void setSampleLimit(int sampleLimit);

    bool isRunning() const;
    bool isPaused() const;

    /// Transfers the next available update to a single consumer.
    std::unique_ptr<RenderUpdate> waitForUpdate(
        int sampleCount, std::stop_token stopToken);

private:
    void startIntegration(
        std::unique_ptr<integrators::Integrator::Result> result,
        ThreadPool* pool,
        std::stop_token stopToken);
    void waitForIntegration();

    void run(
        const Scene& scene,
        ThreadPool* pool,
        int samplesPerBatch,
        std::stop_token stopToken);

    std::unique_ptr<integrators::Integrator> _integrator;

    mutable std::mutex _mutex;
    /// Wakes update listeners when a new update is available or rendering
    /// finishes.
    std::condition_variable_any _updateCV;
    std::unique_ptr<RenderUpdate> _update;
    RenderSettings _settings;
    int _completedSamples = 0;

    bool _integrating = false;
    /// Wakes the render thread when its pending snapshot integration finishes.
    std::condition_variable _integrationCV;
    bool _finished = true;
    /// Wakes the paused render thread when rendering resumes or stop is
    /// requested.
    std::condition_variable_any _pausedCV;
    bool _paused = false;

    std::jthread _renderThread;
};

} // namespace javelin
