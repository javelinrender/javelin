// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include <iostream>
#include <string_view>

#include "render_settings.h"
#include "ui/application.h"

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
            std::cout << "Usage: javelin [render settings]\n\n" << javelin::renderSettingsHelp();
            return 0;
        }
        if (!argument.starts_with("--")) {
            std::println(std::cerr, "Unexpected argument: {}", argument);
            return 1;
        }
        if (++index == argc) {
            std::println(std::cerr, "Missing value for {}", argument);
            return 1;
        }

        if (const auto error = javelin::setParameterValue(
                options, argument.substr(2), argv[index])) {
            std::println(std::cerr, "{}", *error);
            return 1;
        }
    }

    javelin::ui::Application application(argc, argv, options);
    return application.run();
}
