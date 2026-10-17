// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <algorithm>

#include <variant>

#include "path.h"
#include "types.h"

class Scene;

namespace javelin::materials {
    
namespace bsdf {

struct LambertianDiffuse {
    Vec4 baseColorFactor = Vec4(1.0f);
    std::optional<std::size_t> baseColorTextureIdx;
};

struct DisneyDiffuse {
    Vec4 baseColorFactor = Vec4(1.0f);
    std::optional<std::size_t> baseColorTextureIdx;

    float roughnessFactor = 1.0f;
    std::optional<std::size_t> roughnessTextureIdx;
};

struct DisneyMetal {
    Vec4 baseColorFactor = Vec4(1.0f);
    std::optional<std::size_t> baseColorTextureIdx;

    float roughnessFactor = 1.0f;
    std::optional<std::size_t> roughnessTextureIdx;

    float anisotropyFactor = 1.0f;
    std::optional<std::size_t> anisotropyTextureIdx;
};

} // namespace bsdf

namespace pdf {

struct Uniform {};
struct Delta {};
struct CosineWeighted {};

};

using BSDF = std::variant<
    bsdf::LambertianDiffuse,
    bsdf::DisneyDiffuse,
    bsdf::DisneyMetal>;

using PDF = std::variant<
    pdf::Uniform,
    pdf::Delta,
    pdf::CosineWeighted>;

template<typename T>
Vec3 evaluateBsdf(
    const Scene& scene,
    const T& bsdf,
    const Vec3& inDir,
    const Vec3& outDir,
    const Path::Vertex& vertex);

template<typename T>
float evaluatePdf(
    const Scene& scene,
    const T& pdf,
    const Vec3& inDir,
    const Vec3& outDir,
    const Path::Vertex& vertex);

template<typename T>
Vec3 samplePdf(
    const Scene& scene,
    const T& pdf,
    const Vec3& inDir,
    const Path::Vertex& vertex);

template<>
Vec3 evaluateBsdf<bsdf::LambertianDiffuse>(
        const Scene& scene,
        const bsdf::LambertianDiffuse& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
float evaluatePdf<pdf::Uniform>(
        const Scene& scene,
        const pdf::Uniform& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 samplePdf<pdf::Uniform>(
        const Scene& scene,
        const pdf::Uniform& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex);

template<>
Vec3 samplePdf<pdf::CosineWeighted>(
        const Scene& scene,
        const pdf::CosineWeighted& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex);

template<>
float evaluatePdf<pdf::CosineWeighted>(
        const Scene& scene,
        const pdf::CosineWeighted& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 evaluateBsdf<bsdf::DisneyDiffuse>(
        const Scene& scene,
        const bsdf::DisneyDiffuse& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 evaluateBsdf<bsdf::DisneyMetal>(
        const Scene& scene,
        const bsdf::DisneyMetal& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

inline Vec3 evaluateBsdf(
        const Scene& scene,
        const BSDF& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    return std::visit(
        [&](const auto& value) {
            return evaluateBsdf(scene, value, inDir, outDir, vertex);
        },
        bsdf);
}

inline Vec3 samplePdf(
        const Scene& scene,
        const PDF& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex) {
    return std::visit(
        [&](const auto& value) {
            return samplePdf(scene, value, inDir, vertex);
        },
        pdf);
}

template<typename BSDF, typename PDF>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const BSDF& bsdf,
        const PDF& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    return std::max(0.0f, dot(vertex.normal, outDir)) * (
        evaluateBsdf(scene, bsdf, inDir, outDir, vertex) /
        evaluatePdf(scene, pdf, inDir, outDir, vertex));
}

template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::LambertianDiffuse& bsdf,
        const pdf::CosineWeighted& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);


} // namespace javelin::materials