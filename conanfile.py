from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class JavelinConan(ConanFile):
    name = "javelin"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "enable_profiling": [True, False],
    }
    default_options = {
        "enable_profiling": False,
    }

    def requirements(self):
        self.requires("fastgltf/0.9.0")
        self.requires("glm/1.0.3")
        self.requires("tracy/0.13.1")

    def layout(self):
        cmake_layout(self, src_folder=".", build_folder="build")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["JAVELIN_ENABLE_PROFILING"] = self.options.enable_profiling
        tc.generate()
