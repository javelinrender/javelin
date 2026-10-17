// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "scene.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <print>
#include <numeric>

#include "profiling.h"
#include "fastgltf/core.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/quaternion.hpp"

#include "types.h"

constexpr float PBRLumensToWatts = 1.0f / 683;

void Camera::move(const Vec3 delta) {
    position = position + delta;
}

void Camera::rotate(float yaw, float pitch) {
    // Rotate right by yaw, then rotate up by pitch
    forward = normalize(forward * std::cos(yaw) + right * std::sin(yaw));
    forward = normalize(forward * std::cos(pitch) + up * std::sin(pitch));
    // Recalculate right and up
    right = normalize(cross(forward, Vec3(0.0f, 1.0f, 0.0f)));
    up = normalize(cross(right, forward));
}

std::optional<Scene::HitInfo> Scene::intersect(
        const Ray& ray,
        float minDistance,
        float maxDistance) const {
    struct Hit {
        std::reference_wrapper<const Mesh> mesh;
        std::reference_wrapper<const Mesh::Primitive> primitive;
        BVH::HitInfo hitInfo;
    };
    std::optional<Hit> closestHit;
    for (const Mesh& mesh : meshes) {
        for (const Mesh::Primitive& primitive : mesh.primitives) {
            std::optional<BVH::HitInfo> hitInfo = primitive.bvh.intersect(
                ray, minDistance, maxDistance);
            if (hitInfo && (!closestHit || hitInfo->distance < closestHit->hitInfo.distance))
                closestHit = Hit{mesh, primitive, std::move(*hitInfo)};
        }
    }
    if (!closestHit)
        return std::nullopt;

    const Mesh::Triangle& triangle =
        closestHit->mesh.get().triangles[closestHit->hitInfo.triangleIdx];
    const Vec3 edge1 = triangle.positions[1] - triangle.positions[0];
    const Vec3 edge2 = triangle.positions[2] - triangle.positions[0];
    const Vec3 weights = closestHit->hitInfo.barycentricCoords;

    return HitInfo{
        .distance = closestHit->hitInfo.distance,
        .position = closestHit->hitInfo.position,
        .normal = normalize(
            weights[0] * triangle.normals[0] +
            weights[1] * triangle.normals[1] +
            weights[2] * triangle.normals[2]),
        .geometricNormal = normalize(cross(edge1, edge2)),
        .textureCoord =
            weights[0] * triangle.textureCoords[0] +
            weights[1] * triangle.textureCoords[1] +
            weights[2] * triangle.textureCoords[2],
        .materialIdx = closestHit->primitive.get().materialIdx};
}

bool Scene::loadGltf(const std::filesystem::path& filePath) {
    JAVELIN_ZONE_SCOPED;
    JAVELIN_ZONE_TEXTF("filePath=%s", filePath.string().c_str());

    std::println(std::cout, "Loading GLTF: {}", filePath.string());

    fastgltf::Expected<fastgltf::GltfDataBuffer> dataBuffer =
        fastgltf::GltfDataBuffer::FromPath(filePath);
    if (!dataBuffer) {
        std::println(
            std::cerr, "Failed to load GLTF: error={}",
            fastgltf::to_underlying(dataBuffer.error()));
        return false;
    }

    constexpr auto extensionsToLoad =
        fastgltf::Extensions::KHR_materials_transmission |
        fastgltf::Extensions::KHR_materials_emissive_strength |
        fastgltf::Extensions::KHR_materials_ior |
        fastgltf::Extensions::KHR_lights_punctual;
    fastgltf::Parser parser(extensionsToLoad);
    
    constexpr auto options = fastgltf::Options::LoadExternalBuffers;
    fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltfBinary(
        dataBuffer.get(), filePath.parent_path(), options);
    if (!asset) {
        std::println(
            std::cerr, "Failed to load GLTF: error={}",
            fastgltf::to_underlying(asset.error()));
        return false;
    }

    // In case we are loading a new scene we need to clear the existing one.
    meshes.clear();
    textures.clear();
    images.clear();
    lights.clear();
    materials.clear();

    // Load images
    // Adapted from example:
    // https://github.com/spnda/fastgltf/blob/main/examples/gl_viewer/gl_viewer.cpp
    for (const fastgltf::Image& image : asset->images) {
        Image& newImage = images.emplace_back();
        std::visit(Visitor{
            [](const auto& arg) {},
            [&](const fastgltf::sources::URI& filePath) {
                // We don't support offsets with stbi.
                assert(filePath.fileByteOffset == 0);
                // We're only capable of loading local files.
                assert(filePath.uri.isLocalPath());
                newImage.load(filePath.uri.path());
            },
            [&](const fastgltf::sources::Array& vector) {
                newImage.load(vector.bytes);
            },
            [&](const fastgltf::sources::BufferView& view) {
                const fastgltf::BufferView& bufferView =
                    asset->bufferViews[view.bufferViewIndex];
                const fastgltf::Buffer& buffer =
                    asset->buffers[bufferView.bufferIndex];
                std::visit(Visitor{
                    // We only care about VectorWithMime here, because we
                    // specify LoadExternalBuffers, meaning
                    // all buffers are already loaded into a vector.
                    [](auto& arg) {},
                    [&](const fastgltf::sources::Array& vector) {
                        newImage.load(std::span(
                            vector.bytes.data() + bufferView.byteOffset,
                            bufferView.byteLength));
                    }
                }, buffer.data);
            },
        }, image.data);
    }
    // Load textures
    for (const fastgltf::Texture& texture : asset->textures) {
        assert(texture.imageIndex.has_value());
        textures.emplace_back(*texture.imageIndex);
    }
    // Load materials
    for (const fastgltf::Material& material : asset->materials) {
        MaterialData& newMaterial = materials.emplace_back(
            std::string(material.name));

        // Base color
        for (int i = 0; i < 4; ++i)
            newMaterial.baseColorFactor[i] = material.pbrData.baseColorFactor[i];
        if (material.pbrData.baseColorTexture)
            newMaterial.baseColorTextureIdx =
                material.pbrData.baseColorTexture->textureIndex;

        // Metallic and roughness
        newMaterial.metallicFactor = material.pbrData.metallicFactor;
        newMaterial.roughnessFactor = material.pbrData.roughnessFactor;
        if (material.pbrData.metallicRoughnessTexture)
            newMaterial.metallicRoughnessTextureIdx =
                material.pbrData.metallicRoughnessTexture->textureIndex;

        // Emmisive
        for (int i = 0; i < 3; ++i)
            newMaterial.emissiveFactor[i] = material.emissiveFactor[i];
        newMaterial.emissiveStrength = material.emissiveStrength;
        if (material.emissiveTexture)
            newMaterial.emissiveTextureIdx =
                material.emissiveTexture->textureIndex;

        // Transmission
        if (material.transmission) {
            newMaterial.transmissionFactor =
                material.transmission->transmissionFactor;
            if (material.transmission->transmissionTexture)
                newMaterial.transmissionTextureIdx =
                    material.transmission->transmissionTexture->textureIndex;
        }

        // Index of refraction
        newMaterial.ior = material.ior;

        std::println(std::cout, "Loaded material name={}", material.name);
    }
    // Load lights (just point lights for now)
    for (const fastgltf::Light& light : asset->lights) {
        if (light.type == fastgltf::LightType::Point) {
            // Convert luminous intensity in candelas to power in watts.
            const float wattage = light.intensity * 4 * PI * PBRLumensToWatts;
            PointLight pointLight;
            for (int i = 0; i < 3; ++i)
                pointLight.wattage[i] = light.color[i] * wattage;
            lights.emplace_back(pointLight);
        }
    }

    // TODO(alex): We should handle nodes more properly... for now just extract
    // light positions and mesh transforms (and only from TRS transform type...)
    std::vector<glm::mat4> meshTransforms(asset->meshes.size(), glm::mat4(1.0f));
    for (const fastgltf::Node& node : asset->nodes) {
        std::visit(Visitor{
            [](const fastgltf::math::fmat4x4& transform) {},
            [&](const fastgltf::TRS& trs) {
                glm::mat4 trsMat = glm::translate(glm::mat4(1.0f), glm::make_vec3(trs.translation.data()));
                trsMat = glm::scale(trsMat, glm::make_vec3(trs.scale.data()));
                trsMat *= glm::mat4_cast(glm::make_quat(trs.rotation.data()));
                if (node.lightIndex) {
                    std::visit(Visitor{
                            [](auto& arg) {},
                            [&](PointLight& light) {
                                for (int i = 0; i < 3; ++i)
                                    light.position[i] = trs.translation[i];
                            },
                        },
                        lights[*node.lightIndex]);
                } else if (node.meshIndex) {
                    meshTransforms[*node.meshIndex] = trsMat;
                } else if (node.cameraIndex == 0) {
                    camera.position = trsMat * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
                    camera.forward  = normalize(Vec3(trsMat * Vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                    camera.up       = normalize(Vec3(trsMat * Vec4(0.0f, 1.0f, 0.0f, 0.0f)));
                    camera.right    = normalize(Vec3(trsMat * Vec4(1.0f, 0.0f, 0.0f, 0.0f)));
                }
            }},
            node.transform);
    }


    // Load meshes
    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 textureCoordinate;
    };
    for (std::size_t meshIdx = 0; meshIdx < asset->meshes.size(); ++meshIdx) {
        const fastgltf::Mesh& mesh = asset->meshes[meshIdx];
        Mesh newMesh{.name = std::string{mesh.name}};
        for (const fastgltf::Primitive& primitive : mesh.primitives) {
            std::size_t primitiveStartIdx = newMesh.triangles.size();
            std::size_t primitiveTriangleCount = 0;

            std::vector<std::uint32_t> indices;
            std::vector<Vertex> vertices;

            const auto positionsIt = primitive.findAttribute("POSITION");
            const auto normalsIt = primitive.findAttribute("NORMAL");
            const auto textureCoordsIt = primitive.findAttribute("TEXCOORD_0");

            // Load indices
            {
                const fastgltf::Accessor& accessor =
                    asset->accessors[*primitive.indicesAccessor];
                indices.reserve(accessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(
                    asset.get(), accessor,
                    [&](std::uint32_t idx) { indices.push_back(idx); });
            }
            // Load positions
            {
                const fastgltf::Accessor& accessor =
                    asset->accessors[positionsIt->accessorIndex];
                vertices.resize(vertices.size() + accessor.count);
                fastgltf::iterateAccessorWithIndex<Vec3>(
                    asset.get(), accessor,
                    [&](Vec3 position, std::size_t idx) {
                        vertices[idx] = Vertex{
                            .position = meshTransforms[meshIdx] * Vec4(position, 1.0f),
                            .normal = Vec3(1.0f, 0.0f, 0.0f),
                            .textureCoordinate = Vec2(0.0f)};
                    });
            }
            // Load normals
            if (normalsIt != primitive.attributes.end()) {
                const fastgltf::Accessor& accessor =
                    asset->accessors[normalsIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<Vec3>(
                    asset.get(), accessor,
                    [&](Vec3 normal, std::size_t idx) {
                        vertices[idx].normal =
                            meshTransforms[meshIdx] * Vec4(normal, 0.0f);
                    });
            }
            // Load texture coordinates
            if (textureCoordsIt != primitive.attributes.end()) {
                const fastgltf::Accessor& accessor =
                    asset->accessors[textureCoordsIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<Vec2>(
                    asset.get(), accessor,
                    [&](Vec2 textureCoordinate, std::size_t idx) {
                        vertices[idx].textureCoordinate = textureCoordinate;
                    });
            }

            for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
                Mesh::Triangle triangle;
                for (int j = 0; j < 3; ++j) {
                    const Vertex& v = vertices[indices[i + j]];
                    triangle.positions[j] = v.position;
                    triangle.normals[j] = v.normal;
                    triangle.textureCoords[j] = v.textureCoordinate;
                }
                newMesh.triangles.push_back(triangle);
                const float triangleArea = triangle.computeArea();
                newMesh.triangleAreas.push_back(triangleArea);
                ++primitiveTriangleCount;
            }

            // If this primitive has an emmisive material, we need to also
            // add it as a light source.
            const MaterialData& primitiveMaterial =
                primitive.materialIndex
                    ? materials[*primitive.materialIndex]
                    : DefaultMaterialData;

            if (primitiveMaterial.emissiveStrength > 0.0f &&
                    length2(primitiveMaterial.emissiveFactor) > 0.0f) {
                const auto& meshLight = std::get<MeshLight>(
                    lights.emplace_back(MeshLight{
                        .meshIdx = meshes.size(),
                        .primitiveIdx = newMesh.primitives.size()}));
                std::println(
                    std::cout, "Added mesh name={} primitiveIdx={} as a light",
                    newMesh.name, meshLight.primitiveIdx);
            }

            newMesh.addPrimitive(
                primitiveStartIdx, primitiveTriangleCount,
                primitive.materialIndex);
        }

        for (Mesh::Primitive& primitive : newMesh.primitives) {
            const auto firstArea = newMesh.triangleAreas.begin() + primitive.startIdx;
            const auto lastArea = firstArea + primitive.count;

            primitive.totalArea = std::accumulate(firstArea, lastArea, 0.0f);
            newMesh.primitiveTriangleDistibutions.emplace_back(firstArea, lastArea);
        }

        meshes.emplace_back(std::move(newMesh));
        std::println(std::cout, "Loaded mesh name={}", mesh.name);
    }

    return true;
}

Ray Scene::eyeRay(Vec2 pixel) const {
    // compute the camera coordinate system 
    const Vec3 wDir = -camera.forward;
    const Vec3 uDir = camera.right;
    const Vec3 vDir = camera.up;

    // compute the pixel location in the world coordinate system using the camera coordinate system
    // trace a ray through the center of each pixel
    const float imPlaneUPos = pixel.x / float(camera.width) - 0.5f;
    const float imPlaneVPos = pixel.y / float(camera.height) - 0.5f;

    const Vec3 pixelPos =
        camera.position +
        float(camera.aspectRatio * camera.filmSize * imPlaneUPos) * uDir +
        float(camera.filmSize * imPlaneVPos) * vDir - camera.distanceToFilm * wDir;

    return Ray(camera.position, normalize(pixelPos - camera.position));
}
