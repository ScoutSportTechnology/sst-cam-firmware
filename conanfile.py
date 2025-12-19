from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout


class SSTCamFirmwareConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("nlohmann_json/3.12.0")
        self.requires("spdlog/1.16.0")
        self.requires("gtest/1.17.0")


    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
