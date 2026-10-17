// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <memory>
#include <random>
#include <vector>

#include "image.h"
#include "scene.h"
#include "thread_pool.h"
#include "integrator.h"
#include "render_settings.h"
#include "path.h"

namespace javelin::integrators {

class MetropolisIntegrator;

namespace metropolis {

struct State {
    Vec2 pixel;
    EvaluationResult evaluation;
};

struct MutationInfo {
    State proposal;
    float acceptance;
    bool isNewPath;
};

} // namespace metropolis

class Mutator {
public:
    explicit Mutator(const MetropolisIntegrator& renderer);
    virtual ~Mutator() = default;
    virtual std::optional<metropolis::MutationInfo>
    computeRandomMutation(const Scene& scene) = 0;
    virtual void initialize(const Scene& scene, std::stop_token stopToken) = 0;
    virtual void acceptMutation(metropolis::State&& newState) {
        _currentState = std::move(newState);
    };
    const metropolis::State& getCurrentState() { return *_currentState; };
protected:
    const MetropolisIntegrator& _renderer;
    std::optional<metropolis::State> _currentState;
};

class PathSpaceMutator : public Mutator {
public:
    enum class MutationType : int {
        NewPath = 0,
        Lens = 1,
        MultiChain = 2,
        Bidirectional = 3
    };
    explicit PathSpaceMutator(const MetropolisIntegrator& renderer);
    virtual std::optional<metropolis::MutationInfo>
    computeRandomMutation(const Scene& scene) override;

    virtual void initialize(const Scene& scene, std::stop_token stopToken) override;

    virtual void acceptMutation(metropolis::State&& newState) override;
private:
    // Bidirectional mutations involve taking the current light path,
    // deleting a subpath and replacing it with a newly generated subpath.
    // Note that our implementation differs slightly from that of Veach and
    // Guibas since we currently only generate paths from the eye rather than
    // bidirectionally. Still, the spirit of this mutation should remain the
    // same.
    std::optional<metropolis::MutationInfo>
    bidirectionalMutation(const Scene& scene);

    // Eye path perturbations involve slightly adjusting the outgoing direction
    // of the eye ray, propagating through the same number of specular bounces
    // as the original path, and then connecting back to the original path.
    // Multichain perturbations are the same with the exception that diffuse
    // bounces are also slightly perturbed reconnecting
    std::optional<metropolis::MutationInfo>
    eyePathPerturbation(const Scene& scene, bool multiChain);

    // New path mutations generate a new path independent of the current path
    // based on Russian Roulette.
    std::optional<metropolis::MutationInfo>
    computeNewPathMutation(const Scene& scene);

    std::optional<Path> _currentPath;
    Path _potentialPath;
    std::discrete_distribution<> _mutationDistribution;
};

class MLTProcess {
public:
    MLTProcess(const MetropolisIntegrator& renderer, int width, int height);

    MLTProcess(const MLTProcess&) = delete;
    MLTProcess& operator=(const MLTProcess&) = delete;
    MLTProcess(MLTProcess&&) = default;
    MLTProcess& operator=(MLTProcess&&) = delete;

    void accumulate(const Scene& scene, int numMutations, std::stop_token stopToken);
    const Image& accumulationBuffer() const { return _accumulationBuffer; }
    float accumulatedLuminance() const { return _accumulatedLuminance; }
    int numNewPathMutations() const { return _numNewPathMutations; }
    float averageSamplesPerPixel() const { return _averageSamplesPerPixel; }
    void reset();

private:
    const MetropolisIntegrator& _renderer;
    std::unique_ptr<Mutator> _mutator;
    Image _accumulationBuffer;
    float _accumulatedLuminance = 0.0f;
    int _numNewPathMutations = 0;
    float _averageSamplesPerPixel = 0.0f;
};

class MetropolisIntegrator final : public Integrator {
public:
    explicit MetropolisIntegrator(const RenderSettings& settings);

    MetropolisIntegrator(const MetropolisIntegrator&) = delete;
    MetropolisIntegrator& operator=(const MetropolisIntegrator&) = delete;
    MetropolisIntegrator(MetropolisIntegrator&&) = delete;
    MetropolisIntegrator& operator=(MetropolisIntegrator&&) = delete;

    std::unique_ptr<Result> accumulate(
        const Scene& scene,
        ThreadPool* pool,
        int sampleCount,
        std::stop_token stopToken) override;


    void reset();

    const RenderSettings& getSettings() const { return _settings; }

    int getWidth() const { return _settings.width.value; }
    int getHeight() const { return _settings.height.value; }

private:
    /// Compute the scaling factor needed to make the histogram approximate the image.
    float computeScaleFactor() const;

    RenderSettings _settings;
    std::vector<MLTProcess> _processes;
    int _averageSamplesPerPixel = 0;
};

} // namespace javelin::integrators
