// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <string>
#include <optional>

#include "math.h"
#include "path.h"
#include "types.h"
#include "random.h"
#include "texture.h"


struct MaterialData {
    std::string name;

    Texture<Vec3> baseColor = Vec3(1.0f);

    Texture<float> metallic = 1.0f;
    Texture<float> roughness = 1.0f;

    Texture<Vec3> emissive = Vec3(0.0f);
    float emissiveStrength = 1.0f;

    Texture<float> transmission = 0.0f;

    float ior = 1.5f;

    Path::Vertex::BounceType getType() const {
        if (transmission.factor > 0.5f && metallic.factor < 0.5f)
            return Path::Vertex::BounceType::Refractive;
        if (metallic.factor > 0.5f && roughness.factor < 0.5f)
            return Path::Vertex::BounceType::Reflective;
        return Path::Vertex::BounceType::Diffuse;
    }    
};

class Material {
public:
    Material(const Scene& scene, const MaterialData& data)
        : _scene(scene), _data(data) {}
    
    // This is an unfortunately named method given that it does not compute the
    // bsdf (it does not even have the right signature for that). All it does
    // is return the (base color * texture color) / PI.
    //
    // TODO(Max): This is currently only being used for explicit light
    // connections. This can be removed when that logic is removed / rewritten.
    Vec3 bsdf(const Path::Vertex& vertex) const;

    // This gives the base color * texture color (except refractive materials)
    // are always white. 
    //
    // TODO(Max): Think about the name.
    Vec3 expectedContribution(const Path::Vertex& vertex, const Vec3& inDir) const;

    // Gets the color that this material emits
    Vec3 emission(const Path::Vertex& vertex) const;
    Path::Vertex::BounceType getType() const { return _data.getType(); }

    // inRay is meant to point away from the surface normal
    std::pair<Ray, Path::Vertex::BounceType> sampleDirection(
        Vec3 inDir, const Path::Vertex& vertex) const;
private:
    const Scene& _scene;
    const MaterialData& _data;
};

