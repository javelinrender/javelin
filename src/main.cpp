// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include <iostream>
#include <print>
#include <string_view>

#include "render_settings.h"
#include "ui/qt_application.h"

namespace {

bool isHelpArgument(const std::string_view argument) {
    return argument == "--help" || argument == "-h";
}

} // namespace

int main(int argc, char* argv[]) {
    javelin::RenderSettings options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (isHelpArgument(argument)) {
            std::print(std::cout, "Usage: javelin [render settings]\n\n{}",
                javelin::renderSettingsHelp(options));
            return 0;
        }
        const std::optional<std::string_view> name =
            javelin::findCliParameter(options, argument);
        if (!name) {
            std::println(std::cerr, "Unknown command-line option: {}", argument);
            return 1;
        }
        if (++index == argc) {
            std::println(std::cerr, "Missing value for {}", argument);
            return 1;
        }

        if (const auto error = javelin::setParameterValue(
                options, *name, argv[index])) {
            std::println(std::cerr, "{}", *error);
            return 1;
        }
    }

    javelin::ui::QtApplication application(argc, argv, options);
    return application.run();
}
