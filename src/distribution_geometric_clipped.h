// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include "random.h"
#include <cmath>

class ClippedGeometricDistribution {
public:
    using result_type = int;

    ClippedGeometricDistribution(float base)
        : _base(base), _invLogBase(1.0f / std::log2f(_base)) {}

    void setParameters(result_type n) {
        _normalization = 1.0f - std::powf(_base, n + 1);
        _invNormalization = 1.0f / _normalization;
    } 

    template<typename Generator>
    result_type operator()(Generator& g) const {
        float u = javelin::random::random(g);
        u *= _normalization;
        return std::max(0, static_cast<result_type>(
            std::ceil(std::log2f(1.0f - u) * _invLogBase) - 1.0f));
    }

    float pdf(result_type i) const {
        return (1.0f - _base) * std::powf(_base, i) * _invNormalization;
    }

private:
    float _base;
    float _invLogBase;
    float _normalization;
    float _invNormalization;
};

class TwoSidedClippedGeometricDistribution {
public:
    using result_type = int;

    TwoSidedClippedGeometricDistribution(float base)
        : _base(base), _invLogBase(1.0f / std::log2f(_base)) {}

    void setParameters(result_type left, result_type center, result_type right) {
        _offset = std::powf(_base, center - left + 1);
        _normalization = 2.0f - _offset - std::powf(_base, right - center + 1);
        _invNormalization = 1.0f / _normalization;
        _left = left;
        _center = center;
    } 

    template<typename Generator>
    result_type operator()(Generator& g) const {
        float u = javelin::random::random(g);
        u *= _normalization;
        u += _offset;
        if (u < 1.0f)
            return std::max(_left, static_cast<result_type>(
                _center - std::ceil(std::log2f(u) * _invLogBase) + 1.0f));
        else
            return std::max(_left, static_cast<result_type>(
                _center + std::ceil(std::log2f(2.0f - u) * _invLogBase) - 1.0f));
    }

    float pdf(result_type i) const {
        float result = (1.0f - _base) * std::powf(_base, std::abs(i - _center)) * _invNormalization;
        if (i == 0)
            result *= 2.0f;
        return result;
    }

private:
    float _base;
    float _invLogBase;
    float _normalization;
    float _invNormalization;
    float _offset;
    result_type _left;
    result_type _center;
};
