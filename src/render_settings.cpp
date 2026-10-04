// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "render_settings.h"

#include <charconv>
#include <format>
#include <utility>

namespace javelin {

namespace {

template<typename Enum>
std::optional<std::string> setValue(
        EnumParameter<Enum>& parameter, std::string_view value) {
    using Choice = EnumParameter<Enum>::Choice;
    // Try find a match.
    for (const Choice& choice : parameter.choices) {
        if (choice.key == value) {
            parameter.value = choice.value;
            return std::nullopt;
        }
    }
    // No match found, list the options.
    std::string choices;
    for (const Choice& choice : parameter.choices) {
        if (!choices.empty())
            choices += ", ";
        choices += choice.key;
    }
    return std::format("{} must be one of: {}.", parameter.label, choices);
}

template<typename Integer>
std::optional<std::string> setValue(
        IntegerParameter<Integer>& parameter, std::string_view value) {
    Integer parsedValue;
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), parsedValue);
    if (error != std::errc{} || end != value.data() + value.size())
        return std::format("{} must be an integer.", parameter.label);

    parameter.value = parsedValue;
    return std::nullopt;
}

std::optional<std::string> setValue(
        BoolParameter& parameter, std::string_view value) {
    if (value == "true")
        parameter.value = true;
    else if (value == "false")
        parameter.value = false;
    else
        return std::format("{} must be true or false.", parameter.label);
    return std::nullopt;
}

template<typename Enum>
std::optional<std::string> validate(const EnumParameter<Enum>& parameter) {
    using Choice = EnumParameter<Enum>::Choice;
    for (const Choice& choice : parameter.choices) {
        if (choice.value == parameter.value)
            return std::nullopt;
    }
    return std::format("{} has an unsupported value.", parameter.label);
}

template<typename Integer>
std::optional<std::string> validate(const IntegerParameter<Integer>& parameter) {
    if (parameter.min && parameter.value < *parameter.min)
        return std::format(
            "{} must be at least {}.", parameter.label, *parameter.min);
    if (parameter.max && parameter.value > *parameter.max)
        return std::format(
            "{} must be at most {}.", parameter.label, *parameter.max);
    return std::nullopt;
}

std::optional<std::string> validate(const BoolParameter&) {
    return std::nullopt;
}

template<typename Enum>
void appendHelpDetails(std::string& help, const EnumParameter<Enum>& parameter) {
    help += " (choices: ";
    for (std::size_t i = 0; i < parameter.choices.size(); ++i) {
        if (i != 0)
            help += ", ";
        help += parameter.choices[i].key;
    }
    help += ')';
}

template<typename Integer>
void appendHelpDetails(std::string& help, const IntegerParameter<Integer>& parameter) {
    if (parameter.min)
        help += std::format(" (minimum: {})", *parameter.min);
    if (parameter.max)
        help += std::format(" (maximum: {})", *parameter.max);
}

void appendHelpDetails(std::string& help, const BoolParameter&) {
    help += " (choices: true, false)";
}

} // namespace

std::optional<std::string> setParameterValue(
        RenderSettings& options,
        const std::string_view name,
        const std::string_view value) {
    RenderSettings candidate = options;
    std::optional<std::string> result;
    bool found = false;

    candidate.forEachParameter([&](auto& parameter) {
        if (parameter.name == name) {
            found = true;
            result = setValue(parameter, value);
        }
    });

    if (!found)
        return std::format("Unknown render setting: {}.", name);
    if (result)
        return result;

    const std::vector<std::string> errors = validateRenderSettings(candidate);
    if (!errors.empty())
        return errors.front();

    options = std::move(candidate);
    return std::nullopt;
}

std::vector<std::string> validateRenderSettings(const RenderSettings& options) {
    std::vector<std::string> errors;
    options.forEachParameter([&](const auto& parameter) {
        if (auto error = validate(parameter))
            errors.push_back(std::move(*error));
    });
    return errors;
}

std::string renderSettingsHelp() {
    std::string help = "Render settings:\n";
    RenderSettings{}.forEachParameter([&](const auto& parameter) {
        help += std::format(
            "  --{} <value>\n      {}", parameter.name, parameter.label);
        appendHelpDetails(help, parameter);
        help += '\n';
    });
    return help;
}

} // namespace javelin
