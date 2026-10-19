# Package check

Proves that an application can use an installed AffrescoUI with `find_package`, not only with `add_subdirectory`.
`check.py` installs a built AffrescoUI to a temporary prefix, then configures and builds the consumer project here
against it, with `CMAKE_PREFIX_PATH` pointing at the prefix, and runs its two programs. Neither needs a display.

```sh
cmake -S . -B build -G Ninja && cmake --build build   # AFFRESCOUI_INSTALL is on in a top-level static build
python3 tests/package/check.py                        # --build DIR, --config CONFIG (default: the build's), --keep
```

It checks:
- **The install:** the headers, the five libraries, the package files and the NOTICE are in the prefix.
- **Where the packages come from:** `AffrescoUI`, `nlohmann_json` and `glfw3` are all found in the prefix.
- **The two consumers:**
  - `screen_consumer` links `affrescoui::affrescoui` (GLFW too), loads a screen and applies a revision;
  - `vcs_consumer` links `affrescoui::vcs` alone and commits and reverts a document of its own format.
- **The version rule:** `find_package(AffrescoUI 0.2)` refuses the 0.1 package (SameMinorVersion while the version is
  0.x).

## Using the installed package

```cmake
find_package(AffrescoUI 0.1 CONFIG REQUIRED)                 # CMAKE_PREFIX_PATH names the install prefix
target_link_libraries(app PRIVATE affrescoui::affrescoui)     # or a module: affrescoui::core, ::render, ::format,
                                                              # ::vcs, ::channel
```

The install rules are in `cmake/AffrescoUIInstall.cmake`. They install the exact nlohmann/json and GLFW that the build
pinned into the same prefix, and only static libraries are supported. The file explains both choices.
