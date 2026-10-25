// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <algorithm>
#include <variant>
#include <limits>

#include "image.h"
#include "material.h"
#include "math.h"
#include "mesh.h"
#include "types.h"

struct Camera {
    Camera(
        int width, int height, float fov, float filmSize,
        Vec3 position, Vec3 forward, Vec3 up)
            : width(width),
              height(height),
              aspectRatio(static_cast<float>(width) / static_cast<float>(height)),
              fov(fov), filmSize(filmSize),
              distanceToFilm(filmSize / (2.0f * std::tan(fov * DegToRad * 0.5f))),
              position(position),
              forward(normalize(forward)),
              up(normalize(up)),
              right(normalize(cross(forward, up))) {}

    void setResolution(int width, int height) {
        this->width = width;
        this->height = height;
        aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    }

    void move(Vec3 delta);
    void rotate(float yaw, float pitch);

    int width;
    int height;
    float aspectRatio;
    const float fov;
    const float filmSize;
    const float distanceToFilm;
    Vec3 position;
    Vec3 forward;
    Vec3 up;
    Vec3 right;
};

struct PointLight {
    Vec3 position, wattage;
};

struct MeshLight {
    std::size_t meshIdx;
    std::size_t primitiveIdx;
};

using Light = std::variant<PointLight, MeshLight>;

struct Texture {
    std::size_t imageIdx;
};

class Scene {
public:
    explicit Scene(Camera camera) : camera(camera) {}

    Camera camera;
    std::vector<Mesh> meshes;
    std::vector<Texture> textures;
    std::vector<Image> images;
    std::vector<Light> lights;

private:
    static inline const MaterialData DefaultMaterialData{};
    std::vector<MaterialData> materials;

public:
    struct HitInfo {
        float distance;
        Vec3 position;
        Vec3 normal;
        Vec3 geometricNormal;
        Vec2 textureCoord;
        std::optional<std::size_t> materialIdx;
    };

    std::optional<HitInfo> intersect(
        const Ray& ray,
        float minDistance = 0.0f,
        float maxDistance = std::numeric_limits<float>::max()) const;

    bool loadGltf(const std::filesystem::path& filePath);

    Ray eyeRay(Vec2 pixel) const;

    Material getMaterial(std::optional<std::size_t> materialIdx) const {
        if (!materialIdx)
            return Material(*this, DefaultMaterialData);
        assert(*materialIdx < materials.size());
        return Material(*this, materials[*materialIdx]);
    }

    Material getMaterial(std::size_t meshIdx, std::size_t primitiveIdx) const {
        assert(meshIdx < meshes.size());
        const Mesh& mesh = meshes[meshIdx];
        assert(primitiveIdx < mesh.primitives.size());
        return getMaterial(mesh.primitives[primitiveIdx].materialIdx);
    }

    Vec3 sampleTexture(std::size_t textureIdx, const Vec2& textureCoord) const {
        assert(0 <= textureIdx && textureIdx < textures.size());
        const Texture& texture = textures[textureIdx];
        const Image& image = images[texture.imageIdx];
        if (image.empty())
            return Vec3(1.0f);

        // Repeating
        std::size_t u = int(textureCoord.x * image.width()) % image.width();
        std::size_t v = int(textureCoord.y * image.height()) % image.height();
        if (u < 0) u += image.width();
        if (v < 0) v += image.height();

        return image.rgb(u, v);
    }
};