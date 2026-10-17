// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "material.h"

#include <cmath>

#include "math.h"
#include "random.h"
#include "scene.h"

namespace javelin::materials {

namespace detail {

Vec3 toWorld(const Vec3 local, const Vec3 normal) {
    Vec3 tangent = std::abs(normal.x) > std::abs(normal.z) ?
        normalize(cross(Vec3(0, 1, 0), normal)) :
        normalize(cross(Vec3(1, 0, 0), normal));
    Vec3 bitangent = cross(normal, tangent);
    return local.x * tangent + local.y * bitangent + local.z * normal;
}

} // namespace detail

template<>
Vec3 evaluateBsdf<bsdf::LambertianDiffuse>(
        const Scene& scene,
        const bsdf::LambertianDiffuse& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    Vec3 result = (1.0f / PI) * bsdf.baseColorFactor.rgb();
    if (bsdf.baseColorTextureIdx)
        result *= scene.sampleTexture(
            *bsdf.baseColorTextureIdx, vertex.textureCoord);
    return result;
}

template<>
float evaluatePdf<pdf::Uniform>(
        const Scene& scene,
        const pdf::Uniform& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    return (dot(vertex.normal, outDir) >= 0.0f) ? 1.0f / (2.0f * PI) : 0.0f;
}

template<>
Vec3 samplePdf<pdf::Uniform>(
        const Scene& scene,
        const pdf::Uniform& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex) {
    const float z = PCG32::rand();
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float phi = 2.0f * PI * PCG32::rand();
    const float x = r * std::cos(phi);
    const float y = r * std::sin(phi);
    return detail::toWorld(Vec3(x, y, z), vertex.normal);
}

template<>
Vec3 samplePdf<pdf::CosineWeighted>(
        const Scene& scene,
        const pdf::CosineWeighted& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex) {
    const float r = std::sqrt(PCG32::rand());
    const float phi = 2.0f * PI * PCG32::rand();
    const float x = r * std::cos(phi);
    const float y = r * std::sin(phi);
    const float z = std::sqrt(std::max(0.0f, 1.0f - r * r));
    return detail::toWorld(Vec3(x, y, z), vertex.normal);
}

template<>
float evaluatePdf<pdf::CosineWeighted>(
        const Scene& scene,
        const pdf::CosineWeighted& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    return std::max(0.0f, dot(vertex.normal, outDir)) / PI;
}


template<>
Vec3 evaluateBsdf<bsdf::DisneyDiffuse>(
        const Scene& scene,
        const bsdf::DisneyDiffuse& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    
}

template<>
Vec3 evaluateBsdf<bsdf::DisneyMetal>(
        const Scene& scene,
        const bsdf::DisneyMetal& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    // ...
    
}

template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::LambertianDiffuse& bsdf,
        const pdf::CosineWeighted& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    Vec3 result = bsdf.baseColorFactor.rgb();
    if (bsdf.baseColorTextureIdx)
        result *= scene.sampleTexture(
            *bsdf.baseColorTextureIdx, vertex.textureCoord);
    return result;
}

} // namespace javelin::materials
