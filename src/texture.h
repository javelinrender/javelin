// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cstddef>
#include <optional>
#include <type_traits>

#include "types.h"

class Scene;

/// A material parameter: a constant factor that is optionally modulated by a
/// texture.
/// `T` is either `float` or `Vec3`. A `float` parameter samples a
/// single-channel texture and a `Vec3` parameter samples an RGB texture.
template<typename T>
struct Texture {
    static_assert(
        std::is_same_v<T, float> || std::is_same_v<T, Vec3>,
        "Texture only supports float and Vec3 parameters");

    T factor{};
    /// Index into `Scene::images`.
    std::optional<std::size_t> imageIdx;

    constexpr Texture() = default;
    constexpr Texture(
        const T& factor,
        std::optional<std::size_t> imageIdx = std::nullopt)
            : factor(factor), imageIdx(imageIdx) {}

    T sample(const Scene& scene, const Vec2& textureCoord) const;
};
