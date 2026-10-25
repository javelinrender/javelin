// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>
#include <filesystem>
#include <span>

#include "types.h"

template<typename T>
struct ChannelCount;
template<> struct ChannelCount<float> { static constexpr int value = 1; };
template<> struct ChannelCount<Vec2>  { static constexpr int value = 2; };
template<> struct ChannelCount<Vec3>  { static constexpr int value = 3; };
template<> struct ChannelCount<Vec4>  { static constexpr int value = 4; };

template<typename T>
int ChannelCount_v = ChannelCount<T>::value;

class Image {
public:
    Image() = default;
    Image(std::size_t w, std::size_t h, int channels)
        : _width(w), _height(h), _channels(channels), _pixels(w * h * channels) {}

    [[nodiscard]] std::size_t width() const { return _width; }
    [[nodiscard]] std::size_t height() const { return _height; }
    [[nodiscard]] int channels() const { return _channels; }
    [[nodiscard]] float* pixels() { return _pixels.data(); }
    [[nodiscard]] const float* pixels() const { return _pixels.data(); }
    [[nodiscard]] bool empty() const { return _pixels.empty(); }

    void resize(std::size_t w, std::size_t h) {
        _width = w;
        _height = h;
        _pixels.resize(w * h * _channels);
    }

    void clear(float value = 0.0f) {
        std::fill(_pixels.begin(), _pixels.end(), value);
    }
    void clear(Vec2 value) {
        for (std::size_t y = 0; y < _height; ++y)
            for (std::size_t x = 0; x < _width; ++x)
                rg(x, y) = value;
    }
    void clear(Vec3 value) {
        for (std::size_t y = 0; y < _height; ++y)
            for (std::size_t x = 0; x < _width; ++x)
                rgb(x, y) = value;
    }
    void clear(Vec4 value) {
        for (std::size_t y = 0; y < _height; ++y)
            for (std::size_t x = 0; x < _width; ++x)
                rgba(x, y) = value;
    }

    [[nodiscard]] bool valid(std::size_t x, std::size_t y) const {
        return x >= 0 && x < _width && y >= 0 && y < _height;
    }

#define IMAGE_CHANNEL_ACCESSOR(NAME, TYPE, OFFSET) \
    [[nodiscard]] TYPE& NAME(std::size_t x, std::size_t y) { \
        assert(valid(x, y)); \
        return *reinterpret_cast<TYPE*>( \
            &_pixels[(x + y * _width) * _channels + OFFSET]); \
    } \
    [[nodiscard]] const TYPE& NAME(std::size_t x, std::size_t y) const { \
        assert(valid(x, y)); \
        return *reinterpret_cast<const TYPE*>( \
            &_pixels[(x + y * _width) * _channels + OFFSET]); \
    }

    IMAGE_CHANNEL_ACCESSOR(r, float, 0)
    IMAGE_CHANNEL_ACCESSOR(g, float, 1)
    IMAGE_CHANNEL_ACCESSOR(b, float, 2)
    IMAGE_CHANNEL_ACCESSOR(a, float, 3)
    IMAGE_CHANNEL_ACCESSOR(rg, Vec2, 0)
    IMAGE_CHANNEL_ACCESSOR(rgb, Vec3, 0)
    IMAGE_CHANNEL_ACCESSOR(rgba, Vec4, 0)

#undef IMAGE_CHANNEL_ACCESSOR

    static float toneMapping(float r) {
        return std::clamp(r, 0.0f, 1.0f);
    }
    static float gammaCorrection(float r, float gamma = 1.0f) {
        return std::pow(r, 1.0f / gamma);
    }
    template<typename T>
    static T toneMapping(T r) {
        for (int i = 0; i < ChannelCount_v<T>; ++i)
            r[i] = toneMapping(r[i]);
        return r;
    }
    template<typename T>
    static T gammaCorrection(T r, float gamma = 1.0f) {
        for (int i = 0; i < ChannelCount_v<T>; ++i)
            r[i] = gammaCorrection(r[i], gamma);
        return r;
    }

    template<typename T>
    static T applyCorrection(T r) {
        return gammaCorrection(toneMapping(r), 2.2f);
    }

    void applyCorrection();
    void flipVertically();

    void load(const std::filesystem::path& fileName);
    void load(const std::span<const std::byte> bytes);
    void save(const std::filesystem::path& fileName) const;

private:
    std::vector<float> _pixels;
    std::size_t _width = 0, _height = 0;
    int _channels = 0;
};
