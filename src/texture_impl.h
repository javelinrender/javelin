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
    if (!imageIdx)
        return factor;
    const Image& image = scene.images[*imageIdx];
    if (image.empty())
        return factor;

    std::size_t u = int(textureCoord.x * image.width()) % image.width();
    std::size_t v = int(textureCoord.y * image.height()) % image.height();
    if (u < 0) u += image.width();
    if (v < 0) v += image.height();

    if constexpr (std::is_same_v<T, float>) {
        return factor * image.r(u, v);
    } else {
        return factor * image.rgb(u, v);
    }
}
