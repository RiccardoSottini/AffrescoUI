"""Conan 2 recipe for AffrescoUI, built from the release's source tarball (conandata.yml, which
tools/release/packaging.py fills in). Try it on the filled copy, from its conan folder:

    conan create . --version <version> --build=missing

The package is what `cmake --install` makes: the static libraries, their headers and AffrescoUI's own CMake package,
which consumers find with find_package(AffrescoUI CONFIG), with the same targets (affrescoui::affrescoui,
affrescoui::vcs, ...). The exact nlohmann/json, GLFW and FreeType it was built with are installed beside it, downloaded
by its CMake and checked against their SHA-256 (cmake/dependencies.json).
"""

import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir


class AffrescoUIConan(ConanFile):
    """The AffrescoUI static libraries and their CMake package."""

    name = "affrescoui"
    description = "GUI library whose UIs are JSON screen documents that people and AI agents change live"
    license = "Apache-2.0"
    url = "https://github.com/RiccardoSottini/AffrescoUI"
    homepage = "https://github.com/RiccardoSottini/AffrescoUI"
    topics = ("gui", "opengl", "json", "version-control")
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    def config_options(self):
        """Windows has no fPIC."""
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        """The usual CMake layout, with the sources in src/."""
        cmake_layout(self, src_folder="src")

    def validate(self):
        """The library is C++23."""
        check_min_cppstd(self, 23)

    def source(self):
        """Downloads the release's source tarball and checks its SHA-256."""
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        """Builds the libraries and their install rules only: no examples, tests or tools."""
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["AFFRESCOUI_INSTALL"] = True
        toolchain.cache_variables["AFFRESCOUI_BUILD_EXAMPLES"] = False
        toolchain.cache_variables["AFFRESCOUI_BUILD_TESTS"] = False
        toolchain.cache_variables["AFFRESCOUI_BUILD_TOOLS"] = False
        toolchain.cache_variables["BUILD_SHARED_LIBS"] = False
        toolchain.generate()

    def build(self):
        """Configures and builds with CMake."""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        """Installs what `cmake --install` installs, and the licences."""
        cmake = CMake(self)
        cmake.install()
        for name in ("LICENSE", "NOTICE", "THIRD_PARTY_NOTICES.md"):
            copy(self, name, self.source_folder, os.path.join(self.package_folder, "licenses"))
        rmdir(self, os.path.join(self.package_folder, "share", "doc"))

    def package_info(self):
        """Consumers use the package's own AffrescoUIConfig.cmake, not one Conan generates, so the targets are the
        ones find_package(AffrescoUI) defines; it finds its dependencies in the same folder."""
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [os.path.join("lib", "cmake", "AffrescoUI")]
