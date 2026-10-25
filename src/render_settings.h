// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <algorithm>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

/// Macro to make the given list of parameters visible to the GUI/CLI.
#define JAVELIN_REGISTER_PARAMETERS(...) \
    auto parameters() { \
        return std::tie(__VA_ARGS__); \
    } \
    auto parameters() const { \
        return std::tie(__VA_ARGS__); \
    }

/// Macro for metadata present in all parameter types.
/// @note
///     We use this macro instead of struct inheritance because inheritance
///     prevents using designated initializers.
#define JAVELIN_PARAMETER_METADATA \
    std::string_view name; \
    std::string_view label; \
    std::vector<::javelin::CliOption> cli; \
    std::string_view help;

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

using CliOption = std::variant<char, std::string_view>;

template<typename Enum>
struct EnumParameter {
    JAVELIN_PARAMETER_METADATA
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
    JAVELIN_PARAMETER_METADATA
    Integer value;
    std::optional<Integer> min;
    std::optional<Integer> max;
    Integer step = 1;
    ParameterVisibility visible = true;
};

struct FloatParameter {
    JAVELIN_PARAMETER_METADATA
    float value;
    std::optional<float> min;
    std::optional<float> max;
    float step = 0.01f;
    ParameterVisibility visible = true;
};

struct BoolParameter {
    JAVELIN_PARAMETER_METADATA
    bool value;
    ParameterVisibility visible = true;
};

struct FilePathParameter {
    JAVELIN_PARAMETER_METADATA
    std::string value;
    std::vector<std::string_view> extensions;
    ParameterVisibility visible = true;
};

template<typename Fields>
struct ParameterGroup : Fields {
    std::string_view name;
    std::string_view label;
    ParameterVisibility visible = true;
};

template<typename Parameter, typename Function>
void forEachLeafParameter(
        Parameter& parameter,
        Function&& function) {
    function(parameter);
}

template<typename Fields, typename Function>
void forEachLeafParameter(ParameterGroup<Fields>& group, Function&& function) {
    std::apply(
        [&](auto&... parameter) {
            (forEachLeafParameter(parameter, function), ...);
        },
        group.parameters());
}

template<typename Fields, typename Function>
void forEachLeafParameter(
        const ParameterGroup<Fields>& group,
        Function&& function) {
    std::apply(
        [&](const auto&... parameter) {
            (forEachLeafParameter(parameter, function), ...);
        },
        group.parameters());
}

struct MutationParameters {
    BoolParameter newPath{
        .name = "new_path",
        .label = "New Path Mutations",
        .value = true,
    };
    BoolParameter lens{
        .name = "lens",
        .label = "Lens Perturbations",
        .value = true,
    };
    BoolParameter multiChain{
        .name = "multi_chain",
        .label = "Multi-Chain Perturbations",
        .value = true,
    };
    BoolParameter bidirectional{
        .name = "bidirectional",
        .label = "Bidirectional Mutations",
        .value = true,
    };

    JAVELIN_REGISTER_PARAMETERS(
        newPath,
        lens,
        multiChain,
        bidirectional)
};

/// Material of the outer part of the shaderball in the shaderball scene.
struct ShaderballMaterialParameters {
    enum class Bsdf {
        LambertianDiffuse,
        DisneyDiffuse,
        DisneyMetal,
        DisneyGlass,
    };
    enum class Pdf {
        Uniform,
        CosineWeighted,
        GGXVNDF,
        GGXVNDFGlass,
    };

    static bool hasAnisotropy(const RenderSettings& settings);
    static bool isDisneyGlass(const RenderSettings& settings);

    EnumParameter<Bsdf> bsdf{
        .name = "shaderball_bsdf",
        .label = "BSDF",
        .value = Bsdf::DisneyDiffuse,
        .choices = {
            {Bsdf::LambertianDiffuse, "lambertian_diffuse", "Lambertian Diffuse"},
            {Bsdf::DisneyDiffuse, "disney_diffuse", "Disney Diffuse"},
            {Bsdf::DisneyMetal, "disney_metal", "Disney Metal"},
            {Bsdf::DisneyGlass, "disney_glass", "Disney Glass"},
        },
    };
    EnumParameter<Pdf> pdf{
        .name = "shaderball_pdf",
        .label = "PDF",
        .value = Pdf::CosineWeighted,
        .choices = {
            {Pdf::Uniform, "uniform", "Uniform"},
            {Pdf::CosineWeighted, "cosine_weighted", "Cosine Weighted"},
            {Pdf::GGXVNDF, "ggx_vndf", "GGX VNDF"},
            {Pdf::GGXVNDFGlass, "ggx_vndf_glass", "GGX VNDF (Glass)"},
        },
    };
    FloatParameter baseColorR{
        .name = "shaderball_base_color_r",
        .label = "Base Color (Red)",
        .value = 0.17585601f,
        .min = 0.0f,
        .max = 1.0f,
    };
    FloatParameter baseColorG{
        .name = "shaderball_base_color_g",
        .label = "Base Color (Green)",
        .value = 0.40394878f,
        .min = 0.0f,
        .max = 1.0f,
    };
    FloatParameter baseColorB{
        .name = "shaderball_base_color_b",
        .label = "Base Color (Blue)",
        .value = 0.79992914f,
        .min = 0.0f,
        .max = 1.0f,
    };
    FloatParameter roughness{
        .name = "shaderball_roughness",
        .label = "Roughness",
        .value = 0.5f,
        .min = 0.0f,
        .max = 1.0f,
    };
    FloatParameter anisotropy{
        .name = "shaderball_anisotropy",
        .label = "Anisotropy",
        .value = 0.0f,
        .min = 0.0f,
        .max = 1.0f,
        .visible = hasAnisotropy,
    };
    FloatParameter ior{
        .name = "shaderball_ior",
        .label = "Index of Refraction",
        .value = 1.5f,
        .min = 1.01f,
        .max = 2.5f,
        .visible = isDisneyGlass,
    };

    JAVELIN_REGISTER_PARAMETERS(
        bsdf,
        pdf,
        baseColorR,
        baseColorG,
        baseColorB,
        roughness,
        anisotropy,
        ior)
};

struct RenderSettings {
    FilePathParameter sceneFile{
        .name = "scene_file",
        .label = "Scene File",
        .cli = {'s', "scene"},
        .value = "./media/shaderball.glb",
        .extensions = {".glb"},
    };

    enum class Integrator {
        Unidirectional,
        Metropolis,
    };
    EnumParameter<Integrator> integrator{
        .name = "integrator",
        .label = "Integrator",
        .cli = {"integrator"},
        .value = Integrator::Unidirectional,
        .choices = {
            {Integrator::Unidirectional, "unidirectional", "Unidirectional"},
            {Integrator::Metropolis, "metropolis", "Metropolis"},
        },
    };
    IntegerParameter<int> width{
        .name = "width",
        .label = "Width",
        .cli = {'W', "width"},
        .value = 512,
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };
    IntegerParameter<int> height{
        .name = "height",
        .label = "Height",
        .cli = {'H', "height"},
        .value = 512,
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };
    IntegerParameter<int> samplesPerPixel{
        .name = "samples_per_pixel",
        .label = "Samples per Pixel",
        .cli = {'p', "samples"},
        .value = 256,
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };

    IntegerParameter<int> threadPoolSize{
        .name = "thread_pool_size",
        .label = "Thread Pool Size",
        .cli = {'j'},
        .value = std::max<int>(1, std::thread::hardware_concurrency()),
        .min = 1,
        .max = std::nullopt,
        .step = 1,
    };

    static bool isMetropolis(const RenderSettings& settings) {
        return settings.integrator.value == Integrator::Metropolis;
    }

    static bool isShaderballScene(const RenderSettings& settings) {
        return std::filesystem::path(settings.sceneFile.value).stem() ==
            "shaderball";
    }

    IntegerParameter<int> metropolisMarkovChains{
        .name = "metropolis_markov_chains",
        .label = "Markov Chains",
        .value = std::max<int>(1, std::thread::hardware_concurrency()),
        .min = 1,
        .max = std::nullopt,
        .step = 1,
        .visible = isMetropolis,
    };
    ParameterGroup<MutationParameters> metropolisMutations{
        .name = "metropolis_mutations",
        .label = "Enabled Mutations",
        .visible = isMetropolis,
    };
    ParameterGroup<ShaderballMaterialParameters> shaderballMaterial{
        .name = "shaderball_material",
        .label = "Shaderball Material",
        .visible = isShaderballScene,
    };

    JAVELIN_REGISTER_PARAMETERS(
        sceneFile,
        integrator,
        width,
        height,
        samplesPerPixel,
        threadPoolSize,
        metropolisMarkovChains,
        metropolisMutations,
        shaderballMaterial)

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

    template<typename Function>
    void forEachLeafParameter(Function&& function) {
        forEachParameter([&](auto& parameter) {
            javelin::forEachLeafParameter(parameter, function);
        });
    }

    template<typename Function>
    void forEachLeafParameter(Function&& function) const {
        forEachParameter([&](const auto& parameter) {
            javelin::forEachLeafParameter(parameter, function);
        });
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

/// Resolves an explicit CLI alias, including its leading '-' or '--', to a name.
/// Parameters with no CLI aliases are not exposed on the command line.
std::optional<std::string_view> findCliParameter(
    const RenderSettings& options, std::string_view argument);

/// Formats CLI aliases and help text for exposed parameters only.
std::string renderSettingsHelp(const RenderSettings& options = {});

} // namespace javelin
