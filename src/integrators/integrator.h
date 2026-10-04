// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <memory>
#include <stop_token>

#include "image.h"

class Scene;
class ThreadPool;

namespace javelin::integrators {

/// Abstract base clase for a rendering integrator.
class Integrator {
public:
    /// Completed accumulation snapshot that can be evaluated.
    class Result {
    public:
        virtual ~Result() = default;
        virtual Image evaluate() const = 0;
        virtual int samplesPerPixel() const = 0;
    };
    virtual ~Integrator() = default;

    /// Accumulates a batch of samples and waits for all sampling jobs to
    /// finish. A null `pool` runs sampling directly on the calling thread.
    /// @pre
    ///     Calls must be serialized. After an interrupted batch, reset or
    ///     recreate the integrator before sampling or integrating again.
    /// @returns
    ///     Unevaluated integration result which needs to be evaluated.
    ///     `nullptr` if the batch was interrupted.
    /// @note
    ///     The idea is that the integration result can be evaluated in parallel
    ///     with further accumulation.
    virtual std::unique_ptr<Result> accumulate(
        const Scene& scene,
        ThreadPool* pool,
        int sampleCount,
        std::stop_token stopToken) = 0;
};

} // namespace javelin::integrators
