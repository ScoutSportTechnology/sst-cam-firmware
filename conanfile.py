from conan import ConanFile
from conan.tools.cmake import cmake_layout


class SstCamFirmwareConan(ConanFile):
    name = "sst-cam-firmware"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"

    requires = (
        "fmt/11.0.2",
        "spdlog/1.14.1",
        "nlohmann_json/3.11.3",
    )

    # IMPORTANT: cmake-conan wants config packages
    generators = ("CMakeConfigDeps",)

    def layout(self):
        cmake_layout(self)
