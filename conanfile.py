from conan import ConanFile
from conan.tools.cmake import cmake_layout

class SstCamFirmwareConan(ConanFile):
    name = "sst-cam-firmware"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"
    generators = ("CMakeConfigDeps",)

    options = {"with_tests": [True, False]}
    default_options = {"with_tests": False}

    def requirements(self):
        self.requires("spdlog/1.14.1")
        self.requires("nlohmann_json/3.11.3")

        if self.options.with_tests:
            self.requires("gtest/1.15.0")

    def layout(self):
        cmake_layout(self)