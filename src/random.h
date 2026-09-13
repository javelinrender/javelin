// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2025. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <cstdint>
#include <limits>

namespace javelin::random {

/// @returns `float` value in range [0, 1).
template<typename Generator>
float random(Generator& g) {
    // We could use `generate_canonical`, but that results in a call to `log`.
    // Profiling, this method is faster:
    // Before, with `generate_canonical`: Samples per pixel: 1023, Time: 20.967s
    //                             After: Samples per pixel: 1023, Time: 12.055s
    std::uint32_t x = static_cast<std::uint32_t>(g()) >> 8; // 24 bits
    constexpr float inv2p24 = 0x1.0p-24f; // 2^(-24)
    return static_cast<float>(x) * inv2p24;
}

} // namespace javelin::random

// fast random number generator based pcg32_fast
namespace PCG32 {

class Generator {
public:
    using result_type = std::uint32_t;
    Generator() = default;
    Generator(std::uint64_t seed) : mcgState(seed | 1u) {} // ensure odd
    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
    result_type operator()();

private:
    static constexpr std::uint64_t Multiplier = 6364136223846793005u;
    std::uint64_t mcgState = 0xcafef00dd15ea5e5u; // must be odd
};

extern thread_local Generator RandomGenerator; 

std::uint32_t pcg32_fast();
float rand();

} // namespace PCG32
