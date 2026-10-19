#!/usr/bin/env python3
"""Package check: installs AffrescoUI to a temporary prefix and builds an application against it with find_package.

Usage:
    python3 tests/package/check.py [--build DIR] [--config CONFIG] [--keep]

DIR is a configured and built AffrescoUI (default: build), with AFFRESCOUI_INSTALL on, its default for a top-level
static build. The script runs `cmake --install` into a temporary prefix, checks what landed there, then configures and
builds this directory's consumer project with CMAKE_PREFIX_PATH set to the prefix and runs its two programs (neither
needs a display). It also checks that the dependencies came from the prefix, and that find_package(AffrescoUI 0.2)
refuses the 0.1 package. --keep leaves the prefix and the consumer's build for a look. Only the Python standard library
and CMake are needed.
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

# What an install must contain, as globs under the prefix (lib may be lib64 on some Linux distributions)
EXPECTED = [
    "include/affresco/affresco.hpp",
    "include/affresco/Screen.hpp",
    "include/affresco/core/Log.hpp",
    "include/affresco/render/Window.hpp",
    "include/affresco/format/Runtime.hpp",
    "include/affresco/vcs/Repository.hpp",
    "include/affresco/channel/Session.hpp",
    "include/nlohmann/json.hpp",
    "include/GLFW/glfw3.h",
    "lib*/cmake/AffrescoUI/AffrescoUIConfig.cmake",
    "lib*/cmake/AffrescoUI/AffrescoUIConfigVersion.cmake",
    "lib*/cmake/AffrescoUI/AffrescoUITargets.cmake",
    "share/doc/AffrescoUI/NOTICE",
    "share/doc/AffrescoUI/LICENSE",
]
LIBRARIES = ["affrescoui", "affrescoui_render", "affrescoui_format", "affrescoui_vcs", "affrescoui_channel"]


def run(command, expect_success=True):
    """Runs a command, printing it; returns its output. Exits when it fails although it should succeed."""
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if expect_success and result.returncode != 0:
        print(result.stdout)
        sys.exit(f"package check: failed: {command[0]} exited with {result.returncode}")
    return result


def check_installed(prefix):
    """Exits if a file the package needs is missing from the prefix."""
    missing = [pattern for pattern in EXPECTED if not glob.glob(os.path.join(prefix, pattern))]
    for library in LIBRARIES:  # libaffrescoui.a, or affrescoui.lib with MSVC
        if not glob.glob(os.path.join(prefix, "lib*", f"*{library}.*")):
            missing.append(f"lib/{library}")
    if missing:
        sys.exit("package check: not installed: " + ", ".join(missing))
    print(f"installed: {len(EXPECTED)} files and {len(LIBRARIES)} libraries checked")


def cached(build, name):
    """A variable from a CMake build directory's cache, or None."""
    with open(os.path.join(build, "CMakeCache.txt")) as file:
        for line in file:
            if line.startswith(name + ":"):
                return line.split("=", 1)[1].strip()
    return None


def configure(build, config, version, locate):
    """Configures the consumer, asking find_package for a version; locate is how CMake finds the package."""
    command = ["cmake", "-S", HERE, "-B", build, *locate, f"-DCMAKE_BUILD_TYPE={config}",
               f"-DAFFRESCOUI_VERSION={version}", "-DCMAKE_COMPILE_WARNING_AS_ERROR=ON"]
    if shutil.which("ninja"):
        command += ["-G", "Ninja"]
    return command


def check_found_in_prefix(build, prefix):
    """Exits unless AffrescoUI and both of its dependencies were found in the prefix, not elsewhere."""
    for package in ["AffrescoUI", "nlohmann_json", "glfw3"]:
        found = cached(build, f"{package}_DIR") or ""
        if not os.path.realpath(found).startswith(os.path.realpath(prefix) + os.sep):
            sys.exit(f"package check: {package} was found outside the prefix: {found}")


def check_dependencies_follow_package(prefix, work, config):
    """Exits unless the dependencies come from AffrescoUI's prefix even when the system has copies of them too.

    The package is located by AffrescoUI_DIR alone, and a decoy system prefix holds copies of the dependencies'
    package files: find_package searches system prefixes before PATHS, so only HINTS keep the tested copies first.
    """
    decoy = os.path.join(work, "decoy")
    for package in ["nlohmann_json", "glfw3"]:
        source = glob.glob(os.path.join(prefix, "*", "cmake", package))[0]
        shutil.copytree(source, os.path.join(decoy, "lib", "cmake", package))
    build = os.path.join(work, "consumer-decoy")
    package_dir = glob.glob(os.path.join(prefix, "lib*", "cmake", "AffrescoUI"))[0]
    run(configure(build, config, "0.1", [f"-DAffrescoUI_DIR={package_dir}", f"-DCMAKE_SYSTEM_PREFIX_PATH={decoy}"]))
    check_found_in_prefix(build, prefix)
    print("the dependencies came from the prefix, not from the system's copies")


def program(build, config, name):
    """The path of a built consumer program, single- or multi-configuration generator."""
    for folder in [build, os.path.join(build, config)]:  # multi-configuration generators add a folder per configuration
        for path in [os.path.join(folder, name), os.path.join(folder, name + ".exe")]:
            if os.path.exists(path):
                return path
    sys.exit(f"package check: {name} wasn't built")


def main():
    """Installs, builds the consumer, runs it, and checks the version rule."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default=os.path.join(ROOT, "build"), help="the built AffrescoUI (default: build)")
    parser.add_argument("--config", help="the configuration to install and build (default: the build's "
                        "CMAKE_BUILD_TYPE, or Release with a multi-configuration generator)")
    parser.add_argument("--keep", action="store_true", help="keep the prefix and the consumer's build")
    options = parser.parse_args()
    # A single-configuration build installs only its own configuration's imported targets, so match it
    options.config = options.config or cached(options.build, "CMAKE_BUILD_TYPE") or "Release"

    work = tempfile.mkdtemp(prefix="affrescoui-package-")
    prefix, build = os.path.join(work, "prefix"), os.path.join(work, "consumer")
    try:
        run(["cmake", "--install", options.build, "--prefix", prefix, "--config", options.config])
        check_installed(prefix)

        locate = [f"-DCMAKE_PREFIX_PATH={prefix}"]
        run(configure(build, options.config, "0.1", locate))
        check_found_in_prefix(build, prefix)
        check_dependencies_follow_package(prefix, work, options.config)
        run(["cmake", "--build", build, "--config", options.config])
        for name in ["screen_consumer", "vcs_consumer"]:
            print(run([program(build, options.config, name)]).stdout, end="")

        # SameMinorVersion while the version is 0.x: a request for 0.2 must not accept 0.1
        refused = run(configure(os.path.join(work, "consumer-0.2"), options.config, "0.2", locate), False)
        if refused.returncode == 0:
            sys.exit("package check: find_package(AffrescoUI 0.2) accepted the 0.1 package")
        if 'compatible with requested version "0.2"' not in " ".join(refused.stdout.split()):
            print(refused.stdout)
            sys.exit("package check: the 0.2 consumer failed for another reason than the version rule")
        print("find_package(AffrescoUI 0.2) refused the 0.1 package, as it should")
        print("package check: passed")
    finally:
        if options.keep:
            print(f"kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
