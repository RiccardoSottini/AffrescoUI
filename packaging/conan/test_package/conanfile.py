"""Conan's test of the AffrescoUI package: a consumer that finds it with find_package(AffrescoUI) and runs."""

import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class AffrescoUITestConan(ConanFile):
    """Builds CMakeLists.txt against the package and runs it."""

    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        """The package under test."""
        self.requires(self.tested_reference_str)

    def layout(self):
        """The usual CMake layout."""
        cmake_layout(self)

    def build(self):
        """Configures and builds the consumer."""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        """Runs the consumer, unless cross-building."""
        if can_run(self):
            self.run(os.path.join(self.cpp.build.bindir, "test_package"), env="conanrun")
