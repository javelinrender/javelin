// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace javelin {

struct RenderSettings;

struct ParameterVisibility {
    using Predicate = std::function<bool(const RenderSettings&)>;

    ParameterVisibility(bool visible = true) : condition(visible) {}

    // This template ctor takes priority over the bool ctor above.
    template<typename Callable>
    ParameterVisibility(Callable predicate)
        : condition(std::in_place_type<Predicate>, std::move(predicate)) {}

    bool operator()(const RenderSettings& settings) const {
        if (const bool* visible = std::get_if<bool>(&condition))
            return *visible;
        return std::get<Predicate>(condition)(settings);
    }

private:
    std::variant<bool, Predicate> condition;
};

template<typename Enum>
struct EnumParameter {
    std::string_view name;
    std::string_view label;
    Enum value;

    struct Choice {
        Enum value;
        std::string_view key;
        std::string_view label;
    };
    std::vector<Choice> choices;
    ParameterVisibility visible = true;
};

template<typename Integer>
struct IntegerParameter {
    std::string_view name;
    std::string_view label;
    Integer value;
    std::optional<Integer> min;
    std::optional<Integer> max;
    Integer step = 1;
    ParameterVisibility visible = true;
};

struct BoolParameter {
    std::string_view name;
    std::string_view label;
    bool value;
    ParameterVisibility visible = true;
};

struct RenderSettings {
    enum class Integrator {
        Unidirectional,
        Metropolis,
    };
    EnumParameter<Integrator> integrator{
        .name = "integrator",
        .label = "Integrator",
        .value = Integrator::Unidirectional,
        .choices = {
            {Integrator::Unidirectional, "unidirectional", "Unidirectional"},
            {Integrator::Metropolis, "metropolis", "Metropolis"},
        },
    };
    IntegerParameter<int> width{
        .name = "width",
        .label = "Width",
        .value = 512,
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };
    IntegerParameter<int> height{
        .name = "height",
        .label = "Height",
        .value = 512,
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };
    IntegerParameter<int> samplesPerPixel{
        .name = "samples-per-pixel",
        .label = "Samples per Pixel",
        .value = 256,
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };

    IntegerParameter<int> threadPoolSize{
        .name = "thread-pool-size",
        .label = "Thread Pool Size",
        .value = std::max<int>(1, std::thread::hardware_concurrency()),
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };

    static bool isMetropolis(const RenderSettings& settings) {
        return settings.integrator.value == Integrator::Metropolis;
    }

    IntegerParameter<int> metropolisMarkovChains{
        .name = "metropolis-markov-chains",
        .label = "Markov Chains",
        .value = std::max<int>(1, std::thread::hardware_concurrency()),
        .min = 1,
        .max = std::nullopt,
        .step = 1,
        .visible = isMetropolis,
    };
    BoolParameter newPathMutation{
        .name = "new-path-mutation",
        .label = "New Path Mutations",
        .value = true,
        .visible = isMetropolis,
    };
    BoolParameter lensPerturbation{
        .name = "lens-perturbation",
        .label = "Lens Perturbations",
        .value = true,
        .visible = isMetropolis,
    };
    BoolParameter multiChainPerturbation{
        .name = "multi-chain-perturbation",
        .label = "Multi-Chain Perturbations",
        .value = true,
        .visible = isMetropolis,
    };
    BoolParameter bidirectionalMutation{
        .name = "bidirectional-mutation",
        .label = "Bidirectional Mutations",
        .value = true,
        .visible = isMetropolis,
    };

    auto parameters() {
        return std::tie(
            integrator,
            width,
            height,
            samplesPerPixel,
            threadPoolSize,
            metropolisMarkovChains,
            newPathMutation,
            lensPerturbation,
            multiChainPerturbation,
            bidirectionalMutation);
    }
    auto parameters() const {
        return std::apply(
            [](auto&... parameter) { 
                return std::tie(std::as_const(parameter)...);
            },
            const_cast<RenderSettings&>(*this).parameters());
    }

    template<typename Function>
    void forEachParameter(Function&& function) {
        std::apply(
            [&](auto&... parameter) { (function(parameter), ...); },
            parameters());
    }

    template<typename Function>
    void forEachParameter(Function&& function) const {
        std::apply(
            [&](const auto&... parameter) { (function(parameter), ...); },
            parameters());
    }
};

/// Applies a value from the CLI, QML, or another text-based interface.
/// @returns an error message if the name or value is invalid.
std::optional<std::string> setParameterValue(
    RenderSettings& options,
    std::string_view name,
    std::string_view value);

/// Validates all parameter values against their constraints.
std::vector<std::string> validateRenderSettings(const RenderSettings& options);

/// Formats parameter help from the same registry used by other frontends.
std::string renderSettingsHelp();

} // namespace javelin
