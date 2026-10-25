// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <algorithm>
#include <cassert>
#include <type_traits>
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

    float anisotropyFactor = 0.0f;
    std::optional<std::size_t> anisotropyTextureIdx;
};

} // namespace bsdf

namespace pdf {

struct Uniform {};
struct Delta {};
struct CosineWeighted {};

struct GGXVNDF {
    float roughnessFactor = 1.0f;
    std::optional<std::size_t> roughnessTextureIdx;

    float anisotropyFactor = 0.0f;
    std::optional<std::size_t> anisotropyTextureIdx;
};

};

using BSDF = std::variant<
    bsdf::LambertianDiffuse,
    bsdf::DisneyDiffuse,
    bsdf::DisneyMetal>;

using PDF = std::variant<
    pdf::Uniform,
    pdf::Delta,
    pdf::CosineWeighted,
    pdf::GGXVNDF>;

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
Vec3 samplePdf<pdf::GGXVNDF>(
        const Scene& scene,
        const pdf::GGXVNDF& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex);

template<>
float evaluatePdf<pdf::GGXVNDF>(
        const Scene& scene,
        const pdf::GGXVNDF& pdf,
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
        [&](const auto& value) -> Vec3 {
            // TODO: pdf::Delta has no sampling or evaluation yet.
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, pdf::Delta>) {
                assert(false && "Delta pdf is not supported");
                return Vec3(0.0f);
            } else {
                return samplePdf(scene, value, inDir, vertex);
            }
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
    // Directions the pdf can't produce (e.g. below the surface) carry no energy.
    const float pdfValue = evaluatePdf(scene, pdf, inDir, outDir, vertex);
    if (!(pdfValue > 0.0f))
        return Vec3(0.0f);
    return evaluateBsdf(scene, bsdf, inDir, outDir, vertex) *
        std::abs(dot(vertex.normal, outDir)) / pdfValue;
}

template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::LambertianDiffuse& bsdf,
        const pdf::CosineWeighted& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::DisneyMetal& bsdf,
        const pdf::GGXVNDF& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

inline Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const BSDF& bsdf,
        const PDF& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    return std::visit(
        [&](const auto& bsdfValue, const auto& pdfValue) -> Vec3 {
            if constexpr (std::is_same_v<
                    std::decay_t<decltype(pdfValue)>, pdf::Delta>) {
                assert(false && "Delta pdf is not supported");
                return Vec3(0.0f);
            } else {
                return evaluateBsdfWeight(
                    scene, bsdfValue, pdfValue, inDir, outDir, vertex);
            }
        },
        bsdf, pdf);
}


} // namespace javelin::materials