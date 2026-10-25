// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "metropolis_integrator.h"

#include <memory>
#include <print>
#include <utility>
#include <vector>

#include "distribution_geometric_clipped.h"
#include "path.h"
#include "profiling.h"
#include "random.h"

namespace {

std::pair<int, int> clampPixel(const Vec2& pixel, const Image& image) {
    const int x = std::clamp<int>(pixel.x, 0, image.width() - 1);
    const int y = std::clamp<int>(pixel.y, 0, image.height() - 1);
    return {x, y};
}

std::tuple<Vec2, Ray> randomEyeRay(const Scene& scene) {
    const Vec2 pixel(
        PCG32::rand() * scene.camera.width,
        PCG32::rand() * scene.camera.height);
    return {pixel, scene.eyeRay(pixel)};
}

Vec2 pixelOffset(float r1, float r2) {
    float phi = PCG32::rand() * 2 * PI;
    float r = r2 * std::exp(-std::log(r2/r1) * PCG32::rand());
    return {r * std::cos(phi), r * std::sin(phi)};
}

Vec3 offsetBounceDirection(float theta1, float theta2, const Vec3& dir) {
    // Make a UVN coordinate system from N
    Vec3 U, V;
    if (std::abs(dir.x) < 0.5f) U = cross(dir, Vec3(1.0f,0.0f,0.0f));
    else U = cross(dir, Vec3(0.0f,1.0f,0.0f));
    U = normalize(U);
    V = cross(U, dir);
    // Determine offsets using the approximation θ ≈ sinθ
    float phi = PCG32::rand() * 2.0f * PI;
    float r = theta2 * std::exp( -std::log(theta2/theta1) * PCG32::rand());
    // Calculate the new direction
    return normalize(dir + r * std::cos(phi) * U + r * std::sin(phi) * V);
}


// a and b are the vertices of the explicit connection
float invGeometryTerm(const Path::Vertex& a, const Path::Vertex& b) {
    Vec3 aTob = b.position - (a.position + Epsilon * a.geometricNormal); 
    float d2 = length2(aTob);
    aTob /= std::sqrt(d2);
    const float cos1 = std::max(0.0f, dot(a.normal, aTob));
    const float cos2 = std::max(0.0f, dot(b.normal, -aTob));
    return d2 / (cos1 * cos2);
}

} // namespace

namespace javelin::integrators {

namespace {

class MetropolisResult final : public Integrator::Result {
public:
    MetropolisResult(
            std::vector<Image> accumulationBuffers,
            int width,
            int height,
            float scaleFactor,
            int samplesPerPixel)
        : _accumulationBuffers(std::move(accumulationBuffers)),
          _width(width),
          _height(height),
          _scaleFactor(scaleFactor),
          _samplesPerPixel(samplesPerPixel) {}

    Image evaluate() const override {
        Image image(_width, _height, 4);
        for (int y = 0; y < _height; ++y) {
            for (int x = 0; x < _width; ++x) {
                Vec3 radiance(0.0f);
                for (const Image& accumulation : _accumulationBuffers)
                    radiance += accumulation.rgb(x, y) * _scaleFactor;
                image.rgb(x, y) = radiance;
                image.a(x, y) = 1.0f;
            }
        }
        return image;
    }

    int samplesPerPixel() const override { return _samplesPerPixel; }

private:
    std::vector<Image> _accumulationBuffers;
    int _width;
    int _height;
    float _scaleFactor;
    int _samplesPerPixel;
};

} // namespace

using namespace metropolis;

std::optional<MutationInfo> PathSpaceMutator::bidirectionalMutation(
        const Scene& scene) {
    thread_local ClippedGeometricDistribution clippedGeoDist(0.5f);
    thread_local TwoSidedClippedGeometricDistribution twoSidedClippedGeoDist(0.5f);
    int currentLength = _currentPath->length();
    clippedGeoDist.setParameters(currentLength - 1);
    int deletedLength = clippedGeoDist(PCG32::RandomGenerator);

    std::uniform_int_distribution sDist(0, currentLength - deletedLength - 1);

    // vertices s to t are to be deleted (non-inclusive)
    std::size_t s = sDist(PCG32::RandomGenerator);
    std::size_t t = s + deletedLength + 1;

    // If we are not deleting the entire suffix, and the first vertex of the
    // suffix is not diffuse, we can't make the explicit connection; reject.
    if (t < currentLength &&
            _currentPath->vertex(t).bounceType != Path::Vertex::BounceType::Diffuse) 
        return std::nullopt;

    int maxAddedLength = Path::MaxLength - currentLength + deletedLength;
    // TODO(alex): We should use this logic once we fix the clipped geo dist.
    // int minAddedLength = (s == 0 || deletedLength == 0 ? 1 : 0);
    int minAddedLength = 0;
    twoSidedClippedGeoDist.setParameters(minAddedLength, deletedLength, maxAddedLength);
    int addedLength = twoSidedClippedGeoDist(PCG32::RandomGenerator);

    MutationInfo info{.isNewPath = false};
    _potentialPath = Path(_currentPath->vertex(0));

    float Txy = 1.0f;
    float Tyx = 1.0f;

    _potentialPath.appendPath(_currentPath->getSlice(1, s + 1));
    std::optional<Ray> ray;

    if (s == 0) {
        // If the first vertex we are deleting in the path is at index 1, it is
        // the point of contact of the eye ray, so when we delete that, we need
        // to create a new eye ray.
        auto [pixel, newRay] = randomEyeRay(scene);
        ray = newRay;
        info.proposal.pixel = pixel;
    } else {
        // Otherwise we bounce in a new direction according to the material
        // at vertex s.
        info.proposal.pixel = _currentState->pixel;
        Path::Vertex& current = _potentialPath.last();
        const Vec3 inDir = current.position - _potentialPath.vertex(s-1).position;
        const Material& material = scene.getMaterial(current.materialIdx);
        const Vec3 newDirection = javelin::materials::samplePdf(
            scene, material.surfacePdf(), -inDir, current);
        if (length2(newDirection) == 0.0f)
            return std::nullopt;
        ray = Ray(current.offsetPosition(newDirection), newDirection);
        current.bounceType = material.getType();
    }

    // Add our new vertices
    for (int i = 0;i < addedLength; ++i) {
        ray = _potentialPath.addBounce(scene, *ray);
        if (!ray)
            return std::nullopt;
    }

    // If we are not deleting the entire suffix we have to connect back to the original path
    if (t < currentLength) {
        if (_potentialPath.last().bounceType != Path::Vertex::BounceType::Diffuse)
            return std::nullopt;
        if (!hasVisibility(scene, _potentialPath.last(), _currentPath->vertex(t)))
            return std::nullopt;
        if (_potentialPath.length() > 1) {
            Tyx *= PI * invGeometryTerm(
                _potentialPath.last(), _currentPath->vertex(t));
        }
        if (t > 1) {
            Txy *= PI * invGeometryTerm(
                _currentPath->vertex(t-1), _currentPath->vertex(t));
        }
        _potentialPath.appendPath(_currentPath->getSlice(t, currentLength));
    }

    // pd is the probability of deleting the path that we did
    // pa is the probability of adding the path that we did

    // divide by (currentLength - deletedLength) since it was a uniform distribution.
    float pd = clippedGeoDist.pdf(deletedLength) / (currentLength - deletedLength);
    float pa = twoSidedClippedGeoDist.pdf(addedLength);
    Tyx *= pd * pa;

    int newLength = currentLength + addedLength - deletedLength;
    clippedGeoDist.setParameters(newLength - 1);

    maxAddedLength = Path::MaxLength - newLength + addedLength;
    minAddedLength = 0;
    twoSidedClippedGeoDist.setParameters(minAddedLength, addedLength, maxAddedLength);

    pd = clippedGeoDist.pdf(addedLength) / (newLength - addedLength);
    pa = twoSidedClippedGeoDist.pdf(deletedLength);
    Txy *= pd * pa;

    info.proposal.evaluation = evaluate(scene, _potentialPath.toSlice());
    const float currentLuminance = luminance(_currentState->evaluation.radiance);
    const float proposalLuminance = luminance(info.proposal.evaluation.radiance);
    info.acceptance = std::min(1.0f, (proposalLuminance * Txy) / (currentLuminance * Tyx));
    return info;
}

std::optional<MutationInfo> PathSpaceMutator::eyePathPerturbation(
        const Scene& scene, bool multiChain) {
    const int width = _renderer.getWidth();
    const int height = _renderer.getHeight();
    const Vec2 newPixel = _currentState->pixel + pixelOffset(0.1f, 0.1f * width);
    if (newPixel.x > width || newPixel.x < 0 ||
        newPixel.y > height || newPixel.y < 0) return std::nullopt;
    
    std::optional<Ray> nextRay = scene.eyeRay(newPixel);

    _potentialPath = Path(Path::Vertex{
                .bounceType = Path::Vertex::BounceType::None,
                .position = nextRay->o});

    MutationInfo info{
        .proposal = {.pixel = newPixel},
        .isNewPath = false};

    float Txy = 1.0f;
    float Tyx = 1.0f;
    
    for (int i = 1;i < _currentPath->length(); ++i) {
        const Path::Vertex& currentVertex = _currentPath->vertex(i);
        nextRay = _potentialPath.addBounce(scene, *nextRay);

        if(!nextRay)
            return std::nullopt;

        if (_potentialPath.last().bounceType != currentVertex.bounceType) {
            return std::nullopt;
        }

        if (currentVertex.bounceType == Path::Vertex::BounceType::Diffuse) {
            // In this case we can't reconnect to the original path.
            if(i == _currentPath->length()-1)
                break;

            const Path::Vertex& nextVertex = _currentPath->vertex(i+1);

            if (nextVertex.bounceType != Path::Vertex::BounceType::Diffuse) {
                if (!multiChain)
                    return std::nullopt;
                // Multi-chain bounce
                Vec3 originalDirection =
                    normalize(nextVertex.position - currentVertex.position);
                nextRay->d = offsetBounceDirection(0.0001f, 0.1f, originalDirection);
                Txy *= std::max(0.0f, dot(originalDirection, currentVertex.normal));
                Tyx *= std::max(0.0f, dot(nextRay->d, currentVertex.normal));
                continue;
            }

            if (!hasVisibility(scene, _potentialPath.last(), nextVertex))
                return std::nullopt;

            Txy *= invGeometryTerm(currentVertex, nextVertex);
            Tyx *= invGeometryTerm(_potentialPath.last(), nextVertex);

            _potentialPath.appendPath(
                _currentPath->getSlice(i+1, _currentPath->length()));
            break;
        }
    }

    info.proposal.evaluation = evaluate(scene, _potentialPath.toSlice());
    const float currentLuminance = luminance(_currentState->evaluation.radiance);
    const float proposalLuminance = luminance(info.proposal.evaluation.radiance);

    info.acceptance = std::min(1.0f, (proposalLuminance * Txy) / (currentLuminance * Tyx));
    return info;
}

std::optional<MutationInfo> PathSpaceMutator::computeNewPathMutation(
        const Scene& scene) {
    MutationInfo info{.isNewPath = true};
    Ray newRay;
    std::tie(info.proposal.pixel, newRay) = randomEyeRay(scene);
    _potentialPath = Path::createRandomEyePath(scene, newRay);
    if (_potentialPath.length() <= 1) {
        // set acceptance to 0 rather than rejecting since we need to preserve
        // the isNewPath = true
        info.acceptance = 0.0f;
        info.proposal.evaluation = EvaluationResult{Vec3(0.0f), Vec3(0.0f)};
        return info;
    }
    
    info.proposal.evaluation = evaluate(scene, _potentialPath.toSlice());
    const float currentLuminance = luminance(
        _currentState->evaluation.russianRouletteRadiance);
    const float proposalLuminance = luminance(
        info.proposal.evaluation.russianRouletteRadiance);

    info.acceptance = std::min(1.0f, proposalLuminance / currentLuminance);
    return info;
}

std::optional<MutationInfo> PathSpaceMutator::computeRandomMutation(
        const Scene& scene) {
    const auto mutationType =
        static_cast<MutationType>(_mutationDistribution(PCG32::RandomGenerator));
    switch (mutationType) {
    case MutationType::NewPath:         return computeNewPathMutation(scene);
    case MutationType::Lens:            return eyePathPerturbation(scene, false);
    case MutationType::MultiChain:      return eyePathPerturbation(scene, true);
    case MutationType::Bidirectional:   return bidirectionalMutation(scene);
    }
    return std::nullopt;
}

void PathSpaceMutator::initialize(const Scene& scene, std::stop_token stopToken) {
    // Set up a valid initial state, loop until we find one.
    while (!stopToken.stop_requested() && !_currentPath) {
        // Create a random path and evaluate it.
        const auto [pixel, ray] = randomEyeRay(scene);
        const Path path = Path::createRandomEyePath(scene, ray);
        EvaluationResult evaluation = evaluate(scene, path.toSlice());
        const float lum = luminance(evaluation.radiance);
        // For a state to be valid, we need non-zero luminance.
        if (lum > Epsilon)
        {
            _currentPath = path;
            _currentState = State{pixel, evaluation};
            break;
        }
    }
}

void PathSpaceMutator::acceptMutation(State&& newState) {
    Mutator::acceptMutation(std::move(newState));
    _currentPath = std::move(_potentialPath);
}

Mutator::Mutator(const MetropolisIntegrator& renderer) : _renderer(renderer) {}

PathSpaceMutator::PathSpaceMutator(const MetropolisIntegrator& renderer)
        : Mutator(renderer),
        _mutationDistribution{
            1.0  * _renderer.getSettings().metropolisMutations.newPath.value,
            1.0  * _renderer.getSettings().metropolisMutations.lens.value,
            1.0  * _renderer.getSettings().metropolisMutations.multiChain.value,
            1.0  * _renderer.getSettings().metropolisMutations.bidirectional.value} {}

MLTProcess::MLTProcess(const MetropolisIntegrator& renderer, int width, int height) 
        : _renderer(renderer),
        _mutator(std::make_unique<PathSpaceMutator>(renderer)),
        _accumulationBuffer(width, height, 3) {}

void MLTProcess::accumulate(
        const Scene& scene, const int numMutations, std::stop_token stopToken) {
    JAVELIN_ZONE_SCOPED;
    _mutator->initialize(scene, stopToken);

    for (std::size_t i = 0; i < numMutations; ++i) {
        if (stopToken.stop_requested())
            break;

        const State& currentState = _mutator->getCurrentState();
        Vec3 currentColor = currentState.evaluation.radiance;
        currentColor /= luminance(currentColor);

        const auto [x, y] = clampPixel(currentState.pixel, _accumulationBuffer);
        std::optional<MutationInfo> info = _mutator->computeRandomMutation(scene);

        if (!info) {
            _accumulationBuffer.rgb(x, y) += currentColor;
            continue;
        }

        if (info->isNewPath) {
            ++_numNewPathMutations;
            _accumulatedLuminance +=
                luminance(info->proposal.evaluation.russianRouletteRadiance);
        }

        Vec3 newColor = info->proposal.evaluation.radiance;
        float newLum = luminance(newColor);
        if (newLum < Epsilon) {
            _accumulationBuffer.rgb(x, y) += currentColor;
            continue;
        }
        newColor /= newLum;

        const auto [newX, newY] = clampPixel(info->proposal.pixel, _accumulationBuffer);

        _accumulationBuffer.rgb(x, y) += currentColor * (1.0f - info->acceptance);
        _accumulationBuffer.rgb(newX, newY) += newColor * info->acceptance;

        if (PCG32::rand() < info->acceptance) {
            _mutator->acceptMutation(std::move(info->proposal));
        }
    }

    const std::size_t numPixels =
        _accumulationBuffer.width() * _accumulationBuffer.height();
    if (!stopToken.stop_requested())
        _averageSamplesPerPixel += static_cast<float>(numMutations) / numPixels;
}

void MLTProcess::reset() {
    _accumulationBuffer.clear();
    _accumulatedLuminance = 0.0f;
    _numNewPathMutations = 0;
    _averageSamplesPerPixel = 0;
}

MetropolisIntegrator::MetropolisIntegrator(const RenderSettings& settings)
        : _settings{settings} {
    if (_settings.metropolisMutations.newPath.value)
        std::println("New path mutations enabled");
    if (_settings.metropolisMutations.lens.value)
        std::println("Lens perturbations enabled");
    if (_settings.metropolisMutations.multiChain.value) 
        std::println("Multi-chain perturbations enabled");
    if (_settings.metropolisMutations.bidirectional.value)  
        std::println("Bidirectional mutations enabled");
    for (int i = 0; i < _settings.metropolisMarkovChains.value; ++i) {
        _processes.emplace_back(*this, _settings.width.value, _settings.height.value);
    }
}

std::unique_ptr<Integrator::Result> MetropolisIntegrator::accumulate(
        const Scene& scene, ThreadPool* pool, int sampleCount,
        std::stop_token stopToken) {
    JAVELIN_ZONE_SCOPED;
    const int numMutationsPerProcess =
        sampleCount * _settings.width.value * _settings.height.value / _processes.size();
    ThreadPool::TaskGroup group;
    for (MLTProcess& process : _processes) {
        auto work = [&, process = &process] {
            process->accumulate(scene, numMutationsPerProcess, stopToken);
        };
        if (pool)
            group.assignWork(*pool, std::move(work));
        else
            work();
    }
    group.wait();
    if (stopToken.stop_requested())
        return nullptr;
    _averageSamplesPerPixel += sampleCount;
    std::vector<Image> accumulationBuffers;
    {
        JAVELIN_ZONE_SCOPED_N("Copy accumulation buffers");
        accumulationBuffers.reserve(_processes.size());
        for (const MLTProcess& process : _processes)
            accumulationBuffers.push_back(process.accumulationBuffer());
    }
    const int width = _settings.width.value;
    const int height = _settings.height.value;
    const float scaleFactor = computeScaleFactor();
    return std::make_unique<MetropolisResult>(
        std::move(accumulationBuffers), width, height, scaleFactor,
        _averageSamplesPerPixel);
}

void MetropolisIntegrator::reset() {
    for (MLTProcess& process : _processes)
        process.reset();
    _averageSamplesPerPixel = 0;
}

float MetropolisIntegrator::computeScaleFactor() const {
    float totalAccumulatedLuminance = 0.0f;
    int totalNumNewPathMutations = 0;
    for (const MLTProcess& process : _processes) {
        totalAccumulatedLuminance += process.accumulatedLuminance();
        totalNumNewPathMutations += process.numNewPathMutations();
    }
    return (totalAccumulatedLuminance / totalNumNewPathMutations) / _averageSamplesPerPixel;
}

} // namespace javelin::integrators
