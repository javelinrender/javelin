// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include "types.h"

constexpr float PI = 3.14159265358979f;
constexpr float DegToRad = PI / 180.0f;
constexpr float RadToDeg = 180.0f / PI;
constexpr float Epsilon = 5e-5f;

inline float luminance(const Vec3& color) {
    return dot(color, Vec3(0.2126f, 0.7152f, 0.0722f));
}
