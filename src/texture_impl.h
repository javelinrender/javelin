// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cassert>
#include <type_traits>

#include "scene.h"
#include "texture.h"

template<typename T>
T Texture<T>::sample(const Scene& scene, const Vec2& textureCoord) const {
    if (!textureIdx)
        return factor;
    assert(*textureIdx < scene.textures.size());
    const Image& image = scene.images[scene.textures[*textureIdx].imageIdx];
    if (image.empty())
        return factor;

    std::size_t u = int(textureCoord.x * image.width()) % image.width();
    std::size_t v = int(textureCoord.y * image.height()) % image.height();
    if (u < 0) u += image.width();
    if (v < 0) v += image.height();

    if constexpr (std::is_same_v<T, float>) {
        assert(image.channels() == 1);
        return factor * image.r(u, v);
    } else {
        assert(image.channels() >= 3);
        return factor * image.rgb(u, v);
    }
}
