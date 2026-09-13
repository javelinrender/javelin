// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "random.h"

#include <random>

namespace PCG32 {

Generator::result_type Generator::operator()() {
    uint64_t x = mcgState;
    const unsigned count = (unsigned)(x >> 61);
    mcgState = x * Multiplier;
    x ^= x >> 22;
    return static_cast<result_type>(x >> (22 + count));
}

thread_local Generator RandomGenerator([]{
        std::random_device rd;
        return static_cast<std::uint64_t>(rd());
    }());

std::uint32_t pcg32_fast() {
    return RandomGenerator();
}

float rand() {
    return javelin::random::random(RandomGenerator);
}

} // namespace PCG32
