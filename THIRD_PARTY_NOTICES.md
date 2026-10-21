# Third-party notices

AffrescoUI is licensed under the Apache License 2.0 (see `LICENSE` and `NOTICE`). It includes or downloads the
following components, which keep their own licenses:

| Component | Where | License |
|---|---|---|
| glad 0.1.36 (generated OpenGL loader) | `external/glad/` (vendored) | Public domain; `khrplatform.h`: MIT-style Khronos license (in the file) |
| Roboto font | `examples/assets/fonts/` (vendored) | Apache License 2.0 (`examples/assets/fonts/LICENSE-Roboto.txt`) |
| JetBrains Mono font | `examples/assets/fonts/` (vendored) | SIL Open Font License 1.1 (`examples/assets/fonts/LICENSE-JetBrainsMono.txt`) |
| Lucide 1.48.0 icon font (`lucide-static`) | `examples/assets/fonts/Lucide.ttf` (vendored, SHA-256 checked by CMake) | ISC License; the icons derived from Feather: MIT License (`examples/assets/fonts/LICENSE-Lucide.txt`) |
| GLFW 3.4 | downloaded by CMake | zlib/libpng license |
| stb_image 2.30 (PNG and JPEG decoder) | downloaded by CMake at a pinned commit, SHA-256 checked | Public domain (Unlicense) or MIT, at your choice (end of `stb_image.h`) |
| nlohmann/json 3.11.3 | downloaded by CMake | MIT |
| FreeType 2.14.3 (glyph rasteriser) | downloaded by CMake | The FreeType License (FTL), chosen of FTL or GPLv2 (`docs/FTL.TXT` in its archive) |
| HarfBuzz 12.3.2 (text shaping) | downloaded by CMake | "Old MIT" (`COPYING` in its archive; `src/ms-use/COPYING`: MIT) |
| nativefiledialog-extended 1.4.1 (file dialogs) | downloaded by CMake, SHA-256 checked | zlib license (`LICENSE` in its archive) |
| curl 8.22.0 (libcurl, the `net` module's transport) | downloaded by CMake, SHA-256 checked, and built statically on Linux and Windows; the system's libcurl on macOS | The curl license, MIT-style (`COPYING` in its archive). Its TLS is the platform's: OpenSSL 3 on Linux (Apache License 2.0, a system package), Schannel on Windows and the system's on macOS (parts of the operating system) |
| Noto Sans Arabic, Devanagari, SC, JP and KR, subsets (rendering check only) | `tests/render/fonts/` (vendored) | SIL Open Font License 1.1 (`tests/render/fonts/LICENSE-NotoSans*.txt`) |
| Twemoji Mozilla, a subset (rendering check only) | `tests/render/fonts/` (vendored) | Art: CC BY 4.0, © Twitter, Inc. and other contributors; font build: Apache License 2.0 (`tests/render/fonts/LICENSE-TwemojiMozilla.md`) |

Portions of this software are copyright © 2026 The FreeType Project (https://freetype.org). All rights reserved.

Versions published before this change (under the name sGLGUI) were licensed under the GNU LGPL 3.0; those copies keep that license.
