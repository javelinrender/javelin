// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "ray.h"
#include "types.h"

class Scene;

class Path {
public:
    static constexpr std::size_t MaxLength = 10;
    static constexpr float TerminationProbability = 0.35826f;
    static constexpr float ExplicitPathProbability = 1.0f;

    struct Vertex {
        enum class BounceType {
            None = 0,
            Diffuse,
            Reflective,
            Refractive
        };

        BounceType bounceType;
        Vec3 position;
        Vec3 normal;
        Vec3 geometricNormal;
        Vec2 textureCoord;
        std::optional<std::size_t> materialIdx;
        /// Only used for explicit vertices.
        std::optional<std::size_t> lightIdx;
        /// `normal` and `geometricNormal` are flipped to face the incoming ray;
        /// this is set if that flip happened (i.e. the ray hit the back side).
        bool backFace = false;

        /// `position` nudged to the side of the surface that `direction`
        /// points towards, so that rays leaving the surface don't self-hit.
        Vec3 offsetPosition(const Vec3& direction) const;
    };

    using Slice = std::span<const Vertex>;

    Path() : _pathLength(0) {}
    explicit Path(const Vertex &vertex) : _path{vertex}, _pathLength{1} {}
    /// Creates a random path in the scene originating from `ray`.
    static Path createRandomEyePath(const Scene& scene, Ray ray);
    static Path createRandomLightPath(const Scene& scene);
    std::optional<Ray> addBounce(
        const Scene& scene,
        const Ray& inRay,
        std::optional<float> terminationProbability = std::nullopt);
        
    void appendPath(Slice other);

    std::size_t length() const { return _pathLength; }
    Slice getSlice(std::size_t first, std::size_t last) const;
    Slice toSlice() const { return getSlice(0, _pathLength); }
    const Vertex& vertex(std::size_t idx) const { return _path[idx]; }
    Vertex& last() {return _path[_pathLength - 1]; }
    const Vertex& last() const {return _path[_pathLength - 1]; }

private:
    std::array<Vertex, MaxLength> _path;
    std::size_t _pathLength;
};

bool hasVisibility(
    const Scene& scene,
    const Path::Vertex& v1, const Path::Vertex& v2);

struct EvaluationResult {
    Vec3 radiance{1.0f};     // The true radiance along some path
    Vec3 russianRouletteRadiance{1.0f}; // The radiance scaled by inverse russian roulette
};

EvaluationResult evaluateImplicit(
    const Scene& scene,
    const Path::Vertex& v1, const Path::Vertex& v2, const Path::Vertex& v3);

Vec3 evaluateExplicitLight(
    const Scene& scene,
    const Path::Vertex& x1, const Path::Vertex& x2,
    const Path::Vertex& lightVertex);

Vec3 evaluateExplicit(
    const Scene& scene,
    const Path::Vertex& x1, const Path::Vertex& x2,
    const Path::Vertex& y1, const Path::Vertex& y2);

EvaluationResult evaluate(const Scene& scene, Path::Slice slice);