// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cassert>
#include <variant>

#include "path.h"
#include "texture.h"
#include "types.h"

class Scene;

namespace javelin::materials {

namespace bsdf {

struct LambertianDiffuse {
    ColorTexture baseColor = Vec3(1.0f);
};

struct DisneyDiffuse {
    ColorTexture baseColor = Vec3(1.0f);

    ScalarTexture roughness = 1.0f;
};

struct DisneyMetal {
    ColorTexture baseColor = Vec3(1.0f);

    ScalarTexture roughness = 1.0f;

    ScalarTexture anisotropy = 0.0f;
};

struct DisneySheen {
    ColorTexture baseColor = Vec3(1.0f);

    ScalarTexture sheenTint = 0.0f;
};

struct DisneyClearcoat {
    ScalarTexture gloss = 1.0f;
};

/// Rough dielectric (Disney 2015 glass lobe). `ior` is the index of refraction
/// of the inside of the surface relative to the outside; the surface normals
/// are assumed to point outwards.
struct DisneyGlass {
    ColorTexture baseColor = Vec3(1.0f);

    ScalarTexture roughness = 1.0f;

    ScalarTexture anisotropy = 0.0f;

    float ior = 1.5f;
};

/// Disney principled BSDF
struct Disney {
    ColorTexture baseColor = Vec3(1.0f);

    ScalarTexture roughness = 1.0f;

    ScalarTexture anisotropy = 0.0f;

    ScalarTexture metallic = 0.0f;

    ScalarTexture specularTransmission = 0.0f;

    ScalarTexture specular = 1.0f;

    ScalarTexture specularTint = 0.0f;

    ScalarTexture sheen = 0.0f;

    ScalarTexture sheenTint = 0.0f;

    float ior = 1.5f;
};

} // namespace bsdf

namespace pdf {

struct Uniform {};
struct Delta {};
struct CosineWeighted {};

struct GGXVNDF {
    ScalarTexture roughness = 1.0f;

    ScalarTexture anisotropy = 0.0f;
};

/// Samples a visible microfacet normal, then chooses between reflection and
/// refraction proportionally to the dielectric Fresnel term.
struct GGXVNDFGlass {
    ScalarTexture roughness = 1.0f;

    ScalarTexture anisotropy = 0.0f;

    float ior = 1.5f;
};

struct Disney {
    ScalarTexture roughness = 1.0f;

    ScalarTexture anisotropy = 0.0f;

    ScalarTexture metallic = 0.0f;

    ScalarTexture specularTransmission = 0.0f;

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
