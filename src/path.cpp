// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "path.h"

#include <random>
#include <print>

#include "material.h"
#include "scene.h"

#include "materials/material.h"

namespace {

std::size_t chooseRandomLight(const Scene& scene) {
    return PCG32::pcg32_fast() % scene.lights.size();
}

/// Weighted by triangle area.
std::size_t chooseRandomTriangle(
        const Scene& scene, const std::size_t meshIdx,
        const std::size_t primitiveIdx) {
    const Mesh& mesh = scene.meshes[meshIdx];
    return mesh.primitiveTriangleDistibutions[primitiveIdx](PCG32::RandomGenerator);
}

Path::Vertex chooseRandomVertexOnTriangle(const Mesh::Triangle& triangle) {
    const float sqrtU1 = std::sqrt(PCG32::rand());
    const float u2 = PCG32::rand();

    const float alpha = 1 - sqrtU1;
    const float beta = (1 - u2) * sqrtU1;
    const float gamma = u2 * sqrtU1;

    return Path::Vertex{
        .bounceType = Path::Vertex::BounceType::None,
        .position =
            triangle.positions[0] * alpha +
            triangle.positions[1] * beta +
            triangle.positions[2] * gamma,
        .normal = normalize(
            triangle.normals[0] * alpha +
            triangle.normals[1] * beta +
            triangle.normals[2] * gamma),
        .geometricNormal = normalize(cross(
            triangle.positions[1] - triangle.positions[0],
            triangle.positions[2] - triangle.positions[0])),
        .textureCoord =
            triangle.textureCoords[0] * alpha +
            triangle.textureCoords[1] * beta +
            triangle.textureCoords[2] * gamma};
}

Path::Vertex chooseRandomVertexOnLight(const Scene& scene, const std::size_t lightIdx) {
    return std::visit(Visitor{
        [&](const PointLight& light) {
            return Path::Vertex{
                .position = light.position,
                .lightIdx = lightIdx};
        },
        [&](const MeshLight& light) {
            const Mesh::Primitive& primitive = scene.meshes[light.meshIdx].primitives[light.primitiveIdx];
            const std::size_t triangleIdx = chooseRandomTriangle(scene, light.meshIdx, light.primitiveIdx);
            const Mesh::Triangle& triangle = scene.meshes[light.meshIdx].triangles[triangleIdx];
            Path::Vertex vertex = chooseRandomVertexOnTriangle(triangle);
            vertex.materialIdx = primitive.materialIdx;
            vertex.lightIdx = lightIdx;
            return vertex;
        }}, 
        scene.lights[lightIdx]);
}

} // namespace


Vec3 Path::Vertex::offsetPosition(const Vec3& direction) const {
    const float side = dot(direction, geometricNormal) < 0.0f ? -1.0f : 1.0f;
    return position + side * Epsilon * geometricNormal;
}

Path Path::createRandomLightPath(const Scene& scene) {
    Path path;
    if (scene.lights.empty())
        return path;
    path._path[path._pathLength] =
        chooseRandomVertexOnLight(scene, chooseRandomLight(scene));
    ++path._pathLength;
    return path;
}

std::optional<Ray> Path::addBounce(
        const Scene& scene,
        const Ray& inRay,
        std::optional<float> terminationProbability) {
    std::optional<Scene::HitInfo> hit = scene.intersect(inRay);

    if (!hit)
        return std::nullopt;

    const Material& material = scene.getMaterial(hit->materialIdx);
    const bool backFace = dot(inRay.d, hit->geometricNormal) > 0.0f;
    if (backFace) {
        hit->normal *= -1;
        hit->geometricNormal *= -1;
    }

    _path[_pathLength] = Vertex{
        Path::Vertex::BounceType::None,
        hit->position,
        hit->normal,
        hit->geometricNormal, hit->textureCoord, hit->materialIdx};
    _path[_pathLength].backFace = backFace;
    ++_pathLength;

    if (terminationProbability && PCG32::rand() < *terminationProbability)
        return std::nullopt;

    const Vec3 newDirection = javelin::materials::samplePdf(
        scene, material.surfacePdf(), -inRay.d, last());
    // A zero direction means the pdf failed to produce a valid sample.
    if (length2(newDirection) == 0.0f)
        return std::nullopt;

    last().bounceType = javelin::materials::bounceType(material.surfaceBsdf());
    return Ray(last().offsetPosition(newDirection), newDirection);
}

void Path::appendPath(Slice other) {
    std::copy(other.begin(), other.end(), _path.begin() + _pathLength);
    _pathLength += other.size();
}

Path::Slice Path::getSlice(std::size_t first, std::size_t last) const {
    return Path::Slice(_path.begin() + first, _path.begin() + last);
}

Path Path::createRandomEyePath(const Scene& scene, Ray ray) {
    Path p;
    p._path[0] = Vertex{
        .bounceType = Path::Vertex::BounceType::None,
        .position = ray.o
    };
    
    p._pathLength = 1;
    while (p._pathLength < MaxLength) {
        std::optional<Ray> nextRay = p.addBounce(scene, ray, TerminationProbability);
        if(!nextRay) 
            return p;
        ray = *nextRay;
    }

    return p;
}

bool hasVisibility(const Scene& scene, const Path::Vertex& v1, const Path::Vertex& v2) {
    Vec3 origin = v1.position + v1.geometricNormal * Epsilon;
    Vec3 dir = v2.position - origin;
    float dist = length(dir);
    dir /= dist;
    if (dot(dir, v1.normal) < Epsilon ||
            (length2(v2.normal) > Epsilon && dot(-dir, v2.normal) < Epsilon))
        return false;
    return !scene.intersect({origin, dir}, 0.0f, dist - 2 * Epsilon).has_value();
}

EvaluationResult evaluateImplicit(
    const Scene& scene,
    const Path::Vertex& v1,
    const Path::Vertex& v2,
    const Path::Vertex& v3) {

    EvaluationResult result;

    const Material& material = scene.getMaterial(v2.materialIdx);

    constexpr float ContinuationProbability =
        1.0f - Path::TerminationProbability;

    result.radiance = javelin::materials::evaluateBsdfWeight(
        scene, material.surfaceBsdf(), material.surfacePdf(),
        normalize(v1.position - v2.position),
        normalize(v3.position - v2.position),
        v2);
    result.russianRouletteRadiance = result.radiance / ContinuationProbability;

    return result;
}

Vec3 evaluateExplicitLight(
        const Scene& scene,
        const Path::Vertex& x1,
        const Path::Vertex& x2,
        const Path::Vertex& lightVertex) {

    Vec3 result(1.0f);
    float lightDist = length(lightVertex.position - x2.position);

    const Vec3 inDir = normalize(x1.position - x2.position);
    const Vec3 outDir = normalize(lightVertex.position - x2.position);

    if(!hasVisibility(scene, x2, lightVertex))
        return Vec3(0.0f);

    const Material& material = scene.getMaterial(x2.materialIdx);
    
    result *= javelin::materials::evaluateBsdf(
        scene, material.surfaceBsdf(), inDir, outDir, x2);
    result /= lightDist * lightDist;
    result *= std::max(0.0f, dot(x2.normal, outDir));

    std::visit(Visitor{
        [&](const PointLight& light) {
            result *= 1.0f / (4 * PI);
            result *= light.wattage;
        },
        [&](const MeshLight& light) {
            const Mesh::Primitive& primitive =
                scene.meshes[light.meshIdx].primitives[light.primitiveIdx];
            const Material& material = scene.getMaterial(lightVertex.materialIdx);
            result *= std::max(0.0f, dot(lightVertex.normal, -outDir));
            result *= primitive.totalArea;
            result *= material.emission(lightVertex);
        }
    }, scene.lights[*lightVertex.lightIdx]);

    result *= scene.lights.size();

    return result;
}

Vec3 evaluateExplicit(
        const Scene& scene,
        const Path::Vertex& x1, const Path::Vertex& x2,
        const Path::Vertex& y1, const Path::Vertex& y2) {
    
    Vec3 result(1.0f);
    
    float invDist = 1.0f / length(y2.position - x2.position);

    const Vec3 x2toy2 = normalize(y2.position - x2.position);
    const Vec3 x1tox2 = normalize(x2.position - x1.position);
    const Vec3 y1toy2 = normalize(y2.position - y1.position);

    const Material& material1 = scene.getMaterial(x2.materialIdx);
    const Material& material2 = scene.getMaterial(y2.materialIdx);

    result *= javelin::materials::evaluateBsdf(
        scene, material1.surfaceBsdf(), -x1tox2, x2toy2, x2);
    result *= javelin::materials::evaluateBsdf(
        scene, material2.surfaceBsdf(), -y1toy2, -x2toy2, y2);
    result *= invDist * invDist;

    result *= std::max(0.0f, dot(x2.normal, x2toy2));
    result *= std::max(0.0f, dot(y2.normal, -x2toy2));

    return result;
}

EvaluationResult evaluate(const Scene& scene, Path::Slice path) {
    Vec3 throughput(1.0f);
    Vec3 russianRouletteThroughput(1.0f);
    EvaluationResult result{
        .radiance = Vec3(0.0f),
        .russianRouletteRadiance = Vec3(0.0f)};

    for (std::size_t i = 1;i < path.size()-1; ++i) {
        const Material& material = scene.getMaterial(path[i].materialIdx);
        const Vec3 emission = material.emission(path[i]);
        result.radiance += throughput * emission;
        result.russianRouletteRadiance += russianRouletteThroughput * emission;

        EvaluationResult implicitEvaluation =
            evaluateImplicit(scene, path[i-1], path[i], path[i+1]);
        throughput *= implicitEvaluation.radiance;
        russianRouletteThroughput *= implicitEvaluation.russianRouletteRadiance;        
    }

    const Material& material = scene.getMaterial(path.back().materialIdx);
    const Vec3 emission = material.emission(path.back());
    result.radiance += throughput * emission;
    result.russianRouletteRadiance += russianRouletteThroughput * emission;

    return result;
}
