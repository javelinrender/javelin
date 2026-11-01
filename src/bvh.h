// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <array>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "aabb.h"
#include "aabb4.h"
#include "ray.h"
#include "types.h"

class Mesh;

class BVH {
public:
    struct Node {
        AABB4 childBounds;

        /// Mesh triangle index if `isLeaf` returns true. Index of the first
        /// child node otherwise. The `i`th child is at index `idx + i`.
        std::uint32_t idx;

        /// Zero if this is an internal node. 
        std::uint32_t numTriangles;

        bool isLeaf() const {
            return numTriangles != 0;
        }
    };

    // Stored in edge representation so this doesn't
    // have to be recomputed in doesRayIntersectTriangle
    struct Triangle {
        Vec3 a;
        Vec3 ba;
        Vec3 ca;
        std::size_t idx;

        std::array<Vec3, 3> positions() const;
        Vec3 center() const;
    };

    std::vector<Triangle> triangles;
    std::vector<Node> nodes;
    AABB rootBounds;

    BVH(const Mesh& mesh, std::size_t startIdx, std::size_t count);

    struct HitInfo {
        std::size_t triangleIdx;
        float distance;
        Vec3 barycentricCoords;
        Vec3 unscaledGeoNormal;
    };

    std::optional<HitInfo> intersect(
        const Ray& ray,
        float minDistance,
        float maxDistance) const;

private:
    static constexpr int NumSplits = 5;
    static constexpr int MaxNumTrianglesInLeaf = 4;
    void split(
        std::optional<std::uint32_t> parentNodeIdx, int childIdx,
        float nodeCost, std::span<Vec3> triangleCenters);
};
