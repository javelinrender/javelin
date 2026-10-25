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

Vec3 tangentFor(const Vec3 normal) {
    return std::abs(normal.x) > std::abs(normal.z) ?
        normalize(cross(Vec3(0, 1, 0), normal)) :
        normalize(cross(Vec3(1, 0, 0), normal));
}

Vec3 toWorld(const Vec3 local, const Vec3 normal) {
    Vec3 tangent = tangentFor(normal);
    Vec3 bitangent = cross(normal, tangent);
    return local.x * tangent + local.y * bitangent + local.z * normal;
}

Vec3 toLocal(const Vec3 world, const Vec3 normal) {
    Vec3 tangent = tangentFor(normal);
    Vec3 bitangent = cross(normal, tangent);
    return Vec3(dot(world, tangent), dot(world, bitangent), dot(world, normal));
}

template<typename T>
T schlick(const T& f0, const T& f90, float cosTheta) {
    const float m = 1.0f - std::abs(cosTheta);
    const float m5 = m * m * m * m * m;
    return f0 + (f90 - f0) * m5;
}

// Roughness and anisotropy mapped to (alpha_x, alpha_y).
Vec2 ggxAlpha(
        const Scene& scene,
        float roughnessFactor,
        std::optional<std::size_t> roughnessTextureIdx,
        float anisotropyFactor,
        std::optional<std::size_t> anisotropyTextureIdx,
        const Vec2& textureCoord) {
    constexpr float AlphaMin = 0.0001f;

    float roughness = roughnessFactor;
    if (roughnessTextureIdx)
        roughness *= scene.sampleTexture(*roughnessTextureIdx, textureCoord).y;

    float anisotropy = anisotropyFactor;
    if (anisotropyTextureIdx)
        anisotropy *= scene.sampleTexture(*anisotropyTextureIdx, textureCoord).z;

    const float aspect = std::sqrt(1.0f - 0.9f * anisotropy);
    return Vec2(
        std::max(AlphaMin, roughness * roughness / aspect),
        std::max(AlphaMin, roughness * roughness * aspect));
}

// Anisotropic GGX normal distribution; `hl` is in the local frame.
float ggxD(const Vec3& hl, const Vec2& alpha) {
    const float t = (hl.x * hl.x) / (alpha.x * alpha.x) +
        (hl.y * hl.y) / (alpha.y * alpha.y) + hl.z * hl.z;
    return 1.0f / (PI * alpha.x * alpha.y * t * t);
}

// Smith masking function; `wl` is in the local frame.
float smithG1(const Vec3& wl, const Vec2& alpha) {
    const float lambda = 0.5f * (std::sqrt(1.0f +
        ((wl.x * alpha.x) * (wl.x * alpha.x) +
         (wl.y * alpha.y) * (wl.y * alpha.y)) / (wl.z * wl.z)) - 1.0f);
    return 1.0f / (1.0f + lambda);
}

// Visible-normal sampling of anisotropic GGX; `ve` is the local view direction.
Vec3 sampleGGXVNDF(const Vec3& ve, const Vec2& alpha, float u1, float u2) {
    // Transform the view direction to the hemisphere configuration.
    const Vec3 vh = normalize(Vec3(alpha.x * ve.x, alpha.y * ve.y, ve.z));
    // Orthonormal basis, handling the degenerate case.
    const float lensq = vh.x * vh.x + vh.y * vh.y;
    const Vec3 t1 = lensq > 0.0f ?
        Vec3(-vh.y, vh.x, 0.0f) / std::sqrt(lensq) : Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 t2 = cross(vh, t1);
    // Parameterization of the projected area.
    const float r = std::sqrt(u1);
    const float phi = 2.0f * PI * u2;
    const float p1 = r * std::cos(phi);
    float p2 = r * std::sin(phi);
    const float s = 0.5f * (1.0f + vh.z);
    p2 = (1.0f - s) * std::sqrt(1.0f - p1 * p1) + s * p2;
    // Reproject onto the hemisphere.
    const Vec3 nh = p1 * t1 + p2 * t2 +
        std::sqrt(std::max(0.0f, 1.0f - p1 * p1 - p2 * p2)) * vh;
    // Transform the normal back to the ellipsoid configuration.
    return normalize(Vec3(
        alpha.x * nh.x, alpha.y * nh.y, std::max(0.0f, nh.z)));
}

// Unpolarized dielectric Fresnel reflectance. `eta` is the index of refraction
// of the transmitting medium relative to the medium of the incident direction.
float dielectricFresnel(float cosIn, float eta) {
    const float sinOut2 = (1.0f - cosIn * cosIn) / (eta * eta);
    if (sinOut2 >= 1.0f)
        return 1.0f;
    const float cosOut = std::sqrt(1.0f - sinOut2);
    const float rs = (cosIn - eta * cosOut) / (cosIn + eta * cosOut);
    const float rp = (eta * cosIn - cosOut) / (eta * cosIn + cosOut);
    return 0.5f * (rs * rs + rp * rp);
}

// Everything the glass bsdf and pdf need to know about a pair of directions.
// The local frame is oriented so that the incoming direction is above the
// surface (`inLocal.z > 0`); the outgoing direction is below it when
// transmitted.
struct GlassConfiguration {
    // False if the directions can't be connected by a microfacet.
    bool valid = false;
    bool reflection = false;
    // Index of refraction of the medium opposite to the incoming direction,
    // relative to the medium of the incoming direction.
    float eta = 1.0f;
    Vec2 alpha;
    Vec3 inLocal;
    Vec3 outLocal;
    // Microfacet normal, in the same frame as the directions, with `h.z > 0`.
    Vec3 h;
    float fresnel = 0.0f;
};

struct GlassFrame {
    Vec3 normal;
    float eta;
};

// Orients the shading normal towards `inDir` and picks the relative index of
// refraction depending on whether `inDir` is outside or inside the surface.
GlassFrame glassFrame(const Vec3& inDir, const Path::Vertex& vertex, float ior) {
    const Vec3 outwardGeometric =
        vertex.backFace ? -vertex.geometricNormal : vertex.geometricNormal;
    const Vec3 outwardNormal = vertex.backFace ? -vertex.normal : vertex.normal;
    const bool outside = dot(inDir, outwardGeometric) > 0.0f;
    return {outside ? outwardNormal : -outwardNormal, outside ? ior : 1.0f / ior};
}

GlassConfiguration glassConfiguration(
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex,
        float ior,
        const Vec2& alpha) {
    GlassConfiguration config;
    const GlassFrame frame = glassFrame(inDir, vertex, ior);
    config.eta = frame.eta;
    config.alpha = alpha;
    config.inLocal = toLocal(inDir, frame.normal);
    config.outLocal = toLocal(outDir, frame.normal);
    const Vec3& in = config.inLocal;
    const Vec3& out = config.outLocal;
    if (in.z <= 0.0f || out.z == 0.0f)
        return config;

    config.reflection = out.z > 0.0f;
    Vec3 h = config.reflection ? in + out : in + config.eta * out;
    if (dot(h, h) < 1e-12f)
        return config;
    h = normalize(h);
    if (h.z < 0.0f)
        h = -h;

    // The microfacet must see the incoming direction from the front, and the
    // outgoing direction from the front (reflection) or back (transmission).
    const float hDotIn = dot(h, in);
    const float hDotOut = dot(h, out);
    if (hDotIn <= 0.0f ||
            (config.reflection ? hDotOut <= 0.0f : hDotOut >= 0.0f))
        return config;

    config.h = h;
    config.fresnel = dielectricFresnel(hDotIn, config.eta);
    config.valid = true;
    return config;
}

// Solid angle density with which `samplePdf<pdf::GGXVNDFGlass>` produces the
// outgoing direction of a valid configuration.
float glassPdf(const GlassConfiguration& config) {
    const Vec3& in = config.inLocal;
    const Vec3& out = config.outLocal;
    const float hDotIn = dot(config.h, in);
    // D_vis(h) = G1(in) max(0, in . h) D(h) / (in . n).
    const float visible = smithG1(in, config.alpha) * hDotIn *
        ggxD(config.h, config.alpha) / in.z;
    if (config.reflection)
        return visible * config.fresnel / (4.0f * hDotIn);

    const float hDotOut = dot(config.h, out);
    const float denominator = hDotIn + config.eta * hDotOut;
    const float jacobian = config.eta * config.eta * std::abs(hDotOut) /
        (denominator * denominator);
    return visible * (1.0f - config.fresnel) * jacobian;
}

Vec3 glassBaseColor(
        const Scene& scene,
        const bsdf::DisneyGlass& bsdf,
        const Path::Vertex& vertex) {
    Vec3 baseColor = bsdf.baseColorFactor.xyz();
    if (bsdf.baseColorTextureIdx)
        baseColor *= scene.sampleTexture(
            *bsdf.baseColorTextureIdx, vertex.textureCoord);
    return baseColor;
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
    Vec3 baseColor = bsdf.baseColorFactor.xyz();
    if (bsdf.baseColorTextureIdx)
        baseColor *= scene.sampleTexture(
            *bsdf.baseColorTextureIdx, vertex.textureCoord);

    float roughness = bsdf.roughnessFactor;
    if (bsdf.roughnessTextureIdx)
        roughness *= scene.sampleTexture(
            *bsdf.roughnessTextureIdx, vertex.textureCoord).y;

    const Vec3 h = normalize(inDir + outDir);
    const float hDotOut = dot(h, outDir);
    const float fd90 = 0.5f + 2.0f * roughness * hDotOut * hDotOut;

    auto fd = [&](const Vec3& w) {
        return detail::schlick(1.0f, fd90, dot(vertex.normal, w));
    };

    return (1.0f / PI) * baseColor * fd(inDir) * fd(outDir);
}

template<>
Vec3 evaluateBsdf<bsdf::DisneyMetal>(
        const Scene& scene,
        const bsdf::DisneyMetal& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    Vec3 baseColor = bsdf.baseColorFactor.xyz();
    if (bsdf.baseColorTextureIdx)
        baseColor *= scene.sampleTexture(
            *bsdf.baseColorTextureIdx, vertex.textureCoord);

    const Vec2 alpha = detail::ggxAlpha(
        scene,
        bsdf.roughnessFactor, bsdf.roughnessTextureIdx,
        bsdf.anisotropyFactor, bsdf.anisotropyTextureIdx,
        vertex.textureCoord);

    const Vec3 h = normalize(inDir + outDir);
    const Vec3 hl = detail::toLocal(h, vertex.normal);
    const Vec3 inLocal = detail::toLocal(inDir, vertex.normal);
    const Vec3 outLocal = detail::toLocal(outDir, vertex.normal);

    const Vec3 fm = detail::schlick(baseColor, Vec3(1.0f), dot(h, outDir));

    const float dm = detail::ggxD(hl, alpha);
    const float gm = detail::smithG1(inLocal, alpha) *
        detail::smithG1(outLocal, alpha);

    return fm * (dm * gm / (4.0f * std::abs(inLocal.z) * std::abs(outLocal.z)));
}

template<>
Vec3 samplePdf<pdf::GGXVNDF>(
        const Scene& scene,
        const pdf::GGXVNDF& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex) {
    const Vec2 alpha = detail::ggxAlpha(
        scene,
        pdf.roughnessFactor, pdf.roughnessTextureIdx,
        pdf.anisotropyFactor, pdf.anisotropyTextureIdx,
        vertex.textureCoord);

    const Vec3 inLocal = detail::toLocal(inDir, vertex.normal);
    const float u1 = PCG32::rand();
    const float u2 = PCG32::rand();
    const Vec3 h = detail::sampleGGXVNDF(inLocal, alpha, u1, u2);
    const Vec3 outLocal = 2.0f * dot(inLocal, h) * h - inLocal;
    return detail::toWorld(outLocal, vertex.normal);
}

template<>
float evaluatePdf<pdf::GGXVNDF>(
        const Scene& scene,
        const pdf::GGXVNDF& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    const Vec3 inLocal = detail::toLocal(inDir, vertex.normal);
    const Vec3 outLocal = detail::toLocal(outDir, vertex.normal);
    if (inLocal.z <= 0.0f || outLocal.z <= 0.0f)
        return 0.0f;

    const Vec2 alpha = detail::ggxAlpha(
        scene,
        pdf.roughnessFactor, pdf.roughnessTextureIdx,
        pdf.anisotropyFactor, pdf.anisotropyTextureIdx,
        vertex.textureCoord);

    const Vec3 hl = normalize(inLocal + outLocal);
    // D_vis(h) / (4 (in . h)) with D_vis = G1(in) max(0, in . h) D(h) / (in . n).
    return detail::smithG1(inLocal, alpha) * detail::ggxD(hl, alpha) /
        (4.0f * inLocal.z);
}

template<>
Vec3 evaluateBsdf<bsdf::DisneyGlass>(
        const Scene& scene,
        const bsdf::DisneyGlass& bsdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    const Vec2 alpha = detail::ggxAlpha(
        scene,
        bsdf.roughnessFactor, bsdf.roughnessTextureIdx,
        bsdf.anisotropyFactor, bsdf.anisotropyTextureIdx,
        vertex.textureCoord);
    const detail::GlassConfiguration config =
        detail::glassConfiguration(inDir, outDir, vertex, bsdf.ior, alpha);
    if (!config.valid)
        return Vec3(0.0f);

    const Vec3 baseColor = detail::glassBaseColor(scene, bsdf, vertex);
    const float d = detail::ggxD(config.h, alpha);
    const float g = detail::smithG1(config.inLocal, alpha) *
        detail::smithG1(config.outLocal, alpha);
    const float cosines = std::abs(config.inLocal.z * config.outLocal.z);

    if (config.reflection)
        return baseColor * (config.fresnel * d * g / (4.0f * cosines));

    // Radiance is compressed/expanded when crossing the interface; this
    // cancels the eta^2 of the generalized half-vector Jacobian.
    const float hDotIn = dot(config.h, config.inLocal);
    const float hDotOut = dot(config.h, config.outLocal);
    const float denominator = hDotIn + config.eta * hDotOut;
    return glm::sqrt(baseColor) *
        ((1.0f - config.fresnel) * d * g * hDotIn * std::abs(hDotOut) /
         (cosines * denominator * denominator));
}

template<>
Vec3 samplePdf<pdf::GGXVNDFGlass>(
        const Scene& scene,
        const pdf::GGXVNDFGlass& pdf,
        const Vec3& inDir,
        const Path::Vertex& vertex) {
    const Vec2 alpha = detail::ggxAlpha(
        scene,
        pdf.roughnessFactor, pdf.roughnessTextureIdx,
        pdf.anisotropyFactor, pdf.anisotropyTextureIdx,
        vertex.textureCoord);

    const detail::GlassFrame frame = detail::glassFrame(inDir, vertex, pdf.ior);
    const Vec3 inLocal = detail::toLocal(inDir, frame.normal);
    // Consume the random numbers up front so the stream doesn't depend on the
    // outcome.
    const float u1 = PCG32::rand();
    const float u2 = PCG32::rand();
    const float u3 = PCG32::rand();
    if (inLocal.z <= 0.0f)
        return Vec3(0.0f);

    const Vec3 h = detail::sampleGGXVNDF(inLocal, alpha, u1, u2);
    const float hDotIn = dot(h, inLocal);
    const float fresnel = detail::dielectricFresnel(hDotIn, frame.eta);

    Vec3 outLocal;
    if (u3 < fresnel) {
        outLocal = 2.0f * hDotIn * h - inLocal;
        if (outLocal.z <= 0.0f)
            return Vec3(0.0f);
    } else {
        const float sinOut2 = (1.0f - hDotIn * hDotIn) / (frame.eta * frame.eta);
        const float cosOut = std::sqrt(std::max(0.0f, 1.0f - sinOut2));
        outLocal = -inLocal / frame.eta + (hDotIn / frame.eta - cosOut) * h;
        if (outLocal.z >= 0.0f)
            return Vec3(0.0f);
    }
    return detail::toWorld(outLocal, frame.normal);
}

template<>
float evaluatePdf<pdf::GGXVNDFGlass>(
        const Scene& scene,
        const pdf::GGXVNDFGlass& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    const Vec2 alpha = detail::ggxAlpha(
        scene,
        pdf.roughnessFactor, pdf.roughnessTextureIdx,
        pdf.anisotropyFactor, pdf.anisotropyTextureIdx,
        vertex.textureCoord);
    const detail::GlassConfiguration config =
        detail::glassConfiguration(inDir, outDir, vertex, pdf.ior, alpha);
    return config.valid ? detail::glassPdf(config) : 0.0f;
}

// Assumes the pdf uses the same roughness/anisotropy/ior as the bsdf.
template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::DisneyGlass& bsdf,
        const pdf::GGXVNDFGlass& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    const Vec2 alpha = detail::ggxAlpha(
        scene,
        bsdf.roughnessFactor, bsdf.roughnessTextureIdx,
        bsdf.anisotropyFactor, bsdf.anisotropyTextureIdx,
        vertex.textureCoord);
    const detail::GlassConfiguration config =
        detail::glassConfiguration(inDir, outDir, vertex, bsdf.ior, alpha);
    if (!config.valid)
        return Vec3(0.0f);

    const Vec3 baseColor = detail::glassBaseColor(scene, bsdf, vertex);
    const float g1Out = detail::smithG1(config.outLocal, alpha);
    // f * |n . out| / pdf reduces to the albedo times G1(out) since D, G1(in)
    // and the Fresnel terms cancel. Refraction also scales the radiance by
    // 1 / eta^2.
    if (config.reflection)
        return baseColor * g1Out;
    return glm::sqrt(baseColor) * (g1Out / (config.eta * config.eta));
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

// Assumes the pdf uses the same roughness/anisotropy as the bsdf.
template<>
Vec3 evaluateBsdfWeight(
        const Scene& scene,
        const bsdf::DisneyMetal& bsdf,
        const pdf::GGXVNDF& pdf,
        const Vec3& inDir,
        const Vec3& outDir,
        const Path::Vertex& vertex) {
    const Vec3 inLocal = detail::toLocal(inDir, vertex.normal);
    const Vec3 outLocal = detail::toLocal(outDir, vertex.normal);
    if (inLocal.z <= 0.0f || outLocal.z <= 0.0f)
        return Vec3(0.0f);

    Vec3 baseColor = bsdf.baseColorFactor.xyz();
    if (bsdf.baseColorTextureIdx)
        baseColor *= scene.sampleTexture(
            *bsdf.baseColorTextureIdx, vertex.textureCoord);

    const Vec2 alpha = detail::ggxAlpha(
        scene,
        bsdf.roughnessFactor, bsdf.roughnessTextureIdx,
        bsdf.anisotropyFactor, bsdf.anisotropyTextureIdx,
        vertex.textureCoord);

    const Vec3 h = normalize(inDir + outDir);
    const Vec3 fm = detail::schlick(baseColor, Vec3(1.0f), dot(h, outDir));

    // f * |n . out| / pdf reduces to F * G1(out) since D and G1(in) cancel.
    return fm * detail::smithG1(outLocal, alpha);
}

} // namespace javelin::materials
