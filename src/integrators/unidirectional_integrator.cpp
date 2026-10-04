// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "unidirectional_integrator.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "path.h"
#include "profiling.h"
#include "random.h"
#include "scene.h"
#include "thread_pool.h"

namespace javelin::integrators {

namespace {

class UnidirectionalResult final : public Integrator::Result {
public:
    UnidirectionalResult(Image accumulation, int samplesPerPixel)
        : _accumulation(std::move(accumulation)),
          _reciprocalSamples(1.0f / samplesPerPixel),
          _samplesPerPixel(samplesPerPixel) {}

    Image evaluate() const override {
        Image image(_accumulation.width(), _accumulation.height(), 4);
        for (std::size_t j = 0; j < _accumulation.height(); ++j) {
            for (std::size_t i = 0; i < _accumulation.width(); ++i) {
                image.rgb(i, j) = _accumulation.rgb(i, j) * _reciprocalSamples;
                image.a(i, j) = 1.0f;
            }
        }
        return image;
    }

    int samplesPerPixel() const override { return _samplesPerPixel; }

private:
    Image _accumulation;
    float _reciprocalSamples;
    int _samplesPerPixel;
};

Vec3 sample(const Scene& scene, std::size_t x, std::size_t y) {
    const Ray ray = scene.eyeRay(Vec2(x + PCG32::rand(), y + PCG32::rand()));
    const auto eyePath = Path::createRandomEyePath(scene, ray);

    Vec3 throughput(1.0f);
    Vec3 radiance(0.0f);
    for (std::size_t i = 1; i < eyePath.length(); ++i) {
        const Path::Vertex& prevVertex = eyePath.vertex(i - 1);
        const Path::Vertex& vertex = eyePath.vertex(i);

        const Material& material = scene.getMaterial(vertex.materialIdx);
        radiance += throughput * material.emission(vertex);

        if (i < eyePath.length() - 1) {
            const Path::Vertex& nextVertex = eyePath.vertex(i + 1);
            EvaluationResult implicitEvaluation =
                evaluateImplicit(scene, prevVertex, vertex, nextVertex);
            throughput *= implicitEvaluation.russianRouletteRadiance;
        }
    }

    return radiance;
}

} // namespace

std::unique_ptr<Integrator::Result> UnidirectionalIntegrator::accumulate(
        const Scene& scene,
        ThreadPool* pool,
        int sampleCount,
        std::stop_token stopToken) {
    ThreadPool::TaskGroup group;
    constexpr std::size_t blockWidth = 32;
    for (std::size_t y = 0; y < _accumulation.height(); y += blockWidth) {
        if (stopToken.stop_requested())
            break;
        for (std::size_t x = 0; x < _accumulation.width(); x += blockWidth) {
            auto work = [&, x, y] {
                const std::size_t endX = std::min(_accumulation.width(),  x + blockWidth);
                const std::size_t endY = std::min(_accumulation.height(), y + blockWidth);
                for (std::size_t j = y; j < endY; ++j) {
                    for (std::size_t i = x; i < endX; ++i) {
                        Vec3 radiance(0.0f);
                        for (int k = 0; k < sampleCount; ++k) {
                            if (stopToken.stop_requested())
                                return;
                            radiance += sample(scene, i, j);
                        }
                        _accumulation.rgb(i, j) += radiance;
                    }
                }
            };
            if (pool)
                group.assignWork(*pool, std::move(work));
            else
                work();
        }
    }
    group.wait();
    if (stopToken.stop_requested())
        return nullptr;
    _samplesPerPixel += sampleCount;
    Image accumulation;
    {
        JAVELIN_ZONE_SCOPED_N("Copy accumulation buffer");
        accumulation = _accumulation;
    }
    return std::make_unique<UnidirectionalResult>(
        std::move(accumulation), _samplesPerPixel);
}

} // namespace javelin::integrators
