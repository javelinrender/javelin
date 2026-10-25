// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cassert>
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

struct DisneySheen {
    Vec4 baseColorFactor = Vec4(1.0f);
    std::optional<std::size_t> baseColorTextureIdx;

    float sheenTintFactor = 0.0f;
    std::optional<std::size_t> sheenTintTextureIdx;
};

struct DisneyClearcoat {
    Vec4 glossFactor = Vec4(1.0f);
    std::optional<std::size_t> glossTextureIdx;
};

/// Rough dielectric (Disney 2015 glass lobe). `ior` is the index of refraction
/// of the inside of the surface relative to the outside; the surface normals
/// are assumed to point outwards.
struct DisneyGlass {
    Vec4 baseColorFactor = Vec4(1.0f);
    std::optional<std::size_t> baseColorTextureIdx;

    float roughnessFactor = 1.0f;
    std::optional<std::size_t> roughnessTextureIdx;

    float anisotropyFactor = 0.0f;
    std::optional<std::size_t> anisotropyTextureIdx;

    float ior = 1.5f;
};

/// Disney principled BSDF
struct Disney {
    Vec4 baseColorFactor = Vec4(1.0f);
    std::optional<std::size_t> baseColorTextureIdx;

    float roughnessFactor = 1.0f;
    std::optional<std::size_t> roughnessTextureIdx;

    float anisotropyFactor = 0.0f;
    std::optional<std::size_t> anisotropyTextureIdx;

    float metallicFactor = 0.0f;
    std::optional<std::size_t> metallicTextureIdx;

    float specularTransmissionFactor = 0.0f;
    std::optional<std::size_t> specularTransmissionTextureIdx;

    float specularFactor = 1.0f;
    std::optional<std::size_t> specularTextureIdx;

    float specularTintFactor = 0.0f;
    std::optional<std::size_t> specularTintTextureIdx;

    float sheenFactor = 0.0f;
    std::optional<std::size_t> sheenTextureIdx;

    float sheenTintFactor = 0.0f;
    std::optional<std::size_t> sheenTintTextureIdx;

    float ior = 1.5f;
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

/// Samples a visible microfacet normal, then chooses between reflection and
/// refraction proportionally to the dielectric Fresnel term.
struct GGXVNDFGlass {
    float roughnessFactor = 1.0f;
    std::optional<std::size_t> roughnessTextureIdx;

    float anisotropyFactor = 0.0f;
    std::optional<std::size_t> anisotropyTextureIdx;

    float ior = 1.5f;
};

struct Disney {
    float roughnessFactor = 1.0f;
    std::optional<std::size_t> roughnessTextureIdx;

    float anisotropyFactor = 0.0f;
    std::optional<std::size_t> anisotropyTextureIdx;

    float metallicFactor = 0.0f;
    std::optional<std::size_t> metallicTextureIdx;

    float specularTransmissionFactor = 0.0f;
    std::optional<std::size_t> specularTransmissionTextureIdx;

    float ior = 1.5f;
};

};

using BSDF = std::variant<
    bsdf::LambertianDiffuse,
    bsdf::DisneyDiffuse,
    bsdf::DisneyMetal,
    bsdf::DisneyGlass,
    bsdf::DisneySheen,
    bsdf::Disney>;

using PDF = std::variant<
    pdf::Uniform,
    pdf::CosineWeighted,
    pdf::GGXVNDF,
    pdf::GGXVNDFGlass,
    pdf::Disney>;

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
Vec3 evaluateBsdf<bsdf::DisneySheen>(
        const Scene& scene,
        const bsdf::DisneySheen& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 samplePdf<pdf::GGXVNDFGlass>(
        const Scene& scene,
        const pdf::GGXVNDFGlass& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex);

template<>
float evaluatePdf<pdf::GGXVNDFGlass>(
        const Scene& scene,
        const pdf::GGXVNDFGlass& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 evaluateBsdf<bsdf::DisneyGlass>(
        const Scene& scene,
        const bsdf::DisneyGlass& bsdf,
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

template<>
Vec3 evaluateBsdf<bsdf::Disney>(
        const Scene& scene,
        const bsdf::Disney& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 samplePdf<pdf::Disney>(
        const Scene& scene,
        const pdf::Disney& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex);

template<>
float evaluatePdf<pdf::Disney>(
        const Scene& scene,
        const pdf::Disney& pdf,
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

/// @returns The zero vector if sampling failed and the path should end.
inline Vec3 samplePdf(
        const Scene& scene,
        const PDF& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex) {
    return std::visit(
        [&](const auto& value) -> Vec3 {
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

template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::DisneyGlass& bsdf,
        const pdf::GGXVNDFGlass& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex);

template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::Disney& bsdf,
        const pdf::Disney& pdf,
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
            return evaluateBsdfWeight(
                scene, bsdfValue, pdfValue, inDir, outDir, vertex);
        },
        bsdf, pdf);
}


} // namespace javelin::materials