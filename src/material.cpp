// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "material.h"

#include <optional>

#include "scene.h"
#include "ray.h"
#include "texture_impl.h"
#include "types.h"

namespace {

Vec3 toWorld(const Vec3 local, const Vec3 normal) {
    Vec3 tangent = std::abs(normal.x) > std::abs(normal.z) ?
        normalize(cross(Vec3(0, 1, 0), normal)) :
        normalize(cross(Vec3(1, 0, 0), normal));
    Vec3 bitangent = cross(normal, tangent);
    return local.x * tangent + local.y * bitangent + local.z * normal;
}

std::pair<Ray, Path::Vertex::BounceType> sampleReflectedRay(
        const Vec3 inDir,
        const Vec3 position,
        const Vec3 shadingNormal,
        const Vec3 geometricNormal) {
    Vec3 reflectedDirection =
            -glm::normalize(inDir - 2 * dot(inDir, shadingNormal) * shadingNormal);
    if (dot(reflectedDirection, geometricNormal) < 0) {
        reflectedDirection = -normalize(inDir - 
            2 * dot(inDir, geometricNormal) * geometricNormal);
    }
    return {
        {position + Epsilon * geometricNormal, reflectedDirection},
        Path::Vertex::BounceType::Reflective};
}

float computeFresnel(float cosIn, float cosOut, float eta1, float eta2) {
    float ps = (eta1 * cosIn - eta2 * cosOut) / (eta1 * cosIn + eta2 * cosOut);
    float pt = (eta1 * cosOut - eta2 * cosIn) / (eta1 * cosOut + eta2 * cosIn);

    return 0.5 * (ps * ps + pt * pt);
}

std::pair<Ray, Path::Vertex::BounceType> sampleRefractedRay(
        const Vec3& inDir,
        const Vec3& position,
        const Vec3& shadingNormal,
        const Vec3& geometricNormal,
        const float ior) {
    Vec3 trueDir = -inDir;
    bool isEntering = dot(trueDir, shadingNormal) < 0;

    const float eta1 = isEntering ? 1.0f : ior;
    const float eta2 = isEntering ? ior : 1.0f;
    const float refractionRatio = eta1 / eta2;

    Vec3 normal = isEntering ? shadingNormal : -shadingNormal;

    const float cosIn = -dot(normal, trueDir);

    const float discriminant = 1.0f - refractionRatio * refractionRatio * (1.0f - cosIn * cosIn);
    if (discriminant < 0.0f) {
        // total internal reflection
        return sampleReflectedRay(inDir, position, shadingNormal, geometricNormal);
    }

    const float cosOut = std::sqrt(discriminant);

    Vec3 refractedDirection = normalize(
        refractionRatio * trueDir + (refractionRatio * cosIn - cosOut) * normal);

    const float fresnel = computeFresnel(cosIn, cosOut, eta1, eta2);

    if (PCG32::rand() < fresnel) {
        return sampleReflectedRay(inDir, position, shadingNormal, geometricNormal);
    }
    const Vec3 bias = geometricNormal * Epsilon * (isEntering ? -1.0f : 1.0f);
    return {
        {position + bias, refractedDirection},
        Path::Vertex::BounceType::Refractive};
}

std::pair<Ray, Path::Vertex::BounceType> sampleDiffusedRay(
        const Vec3& position,
        const Vec3& shadingNormal,
        const Vec3& geometricNormal) {
    // Sample from unit disk (cosine-weighted hemisphere in tangent space)
    float r = std::sqrt(PCG32::rand());
    float theta = 2.0f * PI * PCG32::rand();

    float x = r * std::cos(theta);
    float y = r * std::sin(theta);
    float z = std::sqrt(std::max(0.0f, 1.0f - x*x - y*y));

    return {
        {position + Epsilon * geometricNormal, toWorld(Vec3(x, y, z), shadingNormal)},
        Path::Vertex::BounceType::Diffuse};
}

} // namespace

Vec3 Material::bsdf(const Path::Vertex& vertex) const {
    return _data.baseColor.sample(_scene, vertex.textureCoord) / PI;
}

Vec3 Material::expectedContribution(const Path::Vertex& vertex, const Vec3& inDir) const {
    // Refractive materials are always white for now.
    if (_data.getType() == Path::Vertex::BounceType::Refractive)
        return Vec3(1.0f);
    return _data.baseColor.sample(_scene, vertex.textureCoord);
}

Vec3 Material::emission(const Path::Vertex& vertex) const {
    return _data.emissive.sample(_scene, vertex.textureCoord) *
        _data.emissiveStrength;
}

std::pair<Ray, Path::Vertex::BounceType> Material::sampleDirection(
        const Vec3 inDir,
        const Path::Vertex& vertex) const {
    Path::Vertex::BounceType type = _data.getType();
    if (type == Path::Vertex::BounceType::Refractive) {
        return sampleRefractedRay(
            inDir, vertex.position, vertex.normal, vertex.geometricNormal, _data.ior);
    } else if (type == Path::Vertex::BounceType::Reflective) {
        return sampleReflectedRay(
            inDir, vertex.position, vertex.normal, vertex.geometricNormal);
    } 
    return sampleDiffusedRay(vertex.position, vertex.normal, vertex.geometricNormal);
}
