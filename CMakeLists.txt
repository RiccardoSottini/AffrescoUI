cmake_minimum_required(VERSION 3.25)
project(AffrescoUI VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
	enable_language(OBJC OBJCXX) # the render module's native menus and colour scheme (Cocoa); OBJC first, for GLFW's .m files
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimised unless asked otherwise (-DCMAKE_BUILD_TYPE=Debug): the runtime evaluates the whole screen on every update
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug, Release, RelWithDebInfo or MinSizeRel" FORCE)
endif()

option(AFFRESCOUI_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
option(AFFRESCOUI_BUILD_TESTS "Build the rendering check and the unit tests (tests/)" ${PROJECT_IS_TOP_LEVEL})
option(AFFRESCOUI_BUILD_FUZZERS "Build the libFuzzer targets (tests/fuzz); needs clang, instruments the whole build" OFF)
option(AFFRESCOUI_BUILD_BENCHMARKS "Build the benchmarks (tests/perf)" OFF)
option(AFFRESCOUI_BUILD_TOOLS "Build the affrescoui command-line tool (tools/affrescoui)" ${PROJECT_IS_TOP_LEVEL})
option(AFFRESCOUI_BUILD_CAPI "Build the C API (modules/capi) and its test (tests/capi)" ${PROJECT_IS_TOP_LEVEL})
option(AFFRESCOUI_BUILD_CAPI_SHARED "Also build the C API as one shared library, libaffresco (bindings/python loads it)" OFF)
if(AFFRESCOUI_BUILD_CAPI_SHARED)
	set(AFFRESCOUI_BUILD_CAPI ON CACHE BOOL "" FORCE)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON) # before the dependencies: the static libraries go into the shared one
endif()

if(AFFRESCOUI_BUILD_FUZZERS)
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
		message(FATAL_ERROR "AFFRESCOUI_BUILD_FUZZERS needs clang with libFuzzer, which Apple clang doesn't have: "
			"see tests/fuzz/README.md")
	endif()
	# Everything is built with coverage for libFuzzer and the sanitizers, so the libraries the targets call are
	# explored and checked too. -O1 -g as libFuzzer recommends; a Debug build also keeps the assertions.
	add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
		-O1 -g)
	add_link_options(-fsanitize=address,undefined)
endif()

# Only static libraries can be installed (cmake/AffrescoUIInstall.cmake says why), so a shared build leaves it off
if(BUILD_SHARED_LIBS)
	set(AFFRESCOUI_INSTALL_DEFAULT OFF)
else()
	set(AFFRESCOUI_INSTALL_DEFAULT ${PROJECT_IS_TOP_LEVEL})
endif()
option(AFFRESCOUI_INSTALL "Generate the install rules and the AffrescoUI CMake package" ${AFFRESCOUI_INSTALL_DEFAULT})

include(FetchContent)

# GLFW: windows, OpenGL context, input. The versions pinned here are also the ones the installed package asks for.
set(AFFRESCOUI_GLFW_VERSION 3.4)
include(cmake/AffrescoUIWeb.cmake) # the browser build: GLFW's Emscripten port and the browser's network instead
if(NOT EMSCRIPTEN)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL ${AFFRESCOUI_INSTALL} CACHE BOOL "" FORCE) # installed static libraries link it
# The release archive, pinned by its hash like the other dependencies (a git tag can be moved)
FetchContent_Declare(glfw
	URL https://github.com/glfw/glfw/releases/download/${AFFRESCOUI_GLFW_VERSION}/glfw-${AFFRESCOUI_GLFW_VERSION}.zip
	URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9 SYSTEM)
FetchContent_MakeAvailable(glfw)
endif()
# Warnings as errors are for our code: GLFW's own warnings (e.g. sprintf, deprecated in recent macOS SDKs) must not
# fail the build
set_target_properties(glfw PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
# The OpenGL backend: gl41 (the default) or gles3, through ANGLE, Mesa's EGL or WebGL2
include(cmake/AffrescoUIAngle.cmake)

# nlohmann/json: the documents (format, vcs and channel modules; the rendering engine for GPU surfaces' parameters)
set(AFFRESCOUI_JSON_VERSION 3.11.3)
set(JSON_Install ${AFFRESCOUI_INSTALL} CACHE BOOL "" FORCE) # core/Json.hpp includes it
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v${AFFRESCOUI_JSON_VERSION}/json.tar.xz
	URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d SYSTEM)
FetchContent_MakeAvailable(json)

# FreeType and HarfBuzz: the text of the rendering engine
include(cmake/AffrescoUIText.cmake)

# libcurl with the platform's TLS: the network module (RFC 0003, decision D3)
if(NOT EMSCRIPTEN)
include(cmake/AffrescoUINetwork.cmake)
endif()

# Monocypher: the Ed25519 signatures of features and catalogues (RFC 0004, decision D12)
include(cmake/AffrescoUISignatures.cmake)
# libvterm: the terminal emulator of Forge's terminal panel, which the examples, the tests and the benchmarks build
if(AFFRESCOUI_BUILD_EXAMPLES OR AFFRESCOUI_BUILD_TESTS OR AFFRESCOUI_BUILD_BENCHMARKS)
	include(cmake/AffrescoUITerminal.cmake)
endif()

# The Lucide icon font the templates and the rendering check use (decision D11), vendored and pinned by its hash: lucide.ttf
# of lucide-static 1.48.0 (examples/assets/fonts/README.md)
file(SHA256 ${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts/Lucide.ttf AFFRESCOUI_LUCIDE_SHA256)
if(NOT AFFRESCOUI_LUCIDE_SHA256 STREQUAL "eaf30a2a0d2ec0cfbf7ab69ed6dc6e39b7e5e2f0255c17856dcb0b73a15584bb")
	message(FATAL_ERROR "examples/assets/fonts/Lucide.ttf isn't lucide-static 1.48.0's lucide.ttf (its hash differs)")
endif()

# Strict warnings for our C++ code only (not for glad.c or dependencies)
if(MSVC)
	# /utf-8 isn't a warning, but goes wherever these do: the sources are UTF-8, and MSVC reads the ANSI code page
	set(AFFRESCOUI_WARNINGS /W4 /permissive- /utf-8)
else()
	set(AFFRESCOUI_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast
		-Wnon-virtual-dtor -Woverloaded-virtual)
endif()

# PCRE2 and affresco::TextPattern: the regular expressions of the editor's find and the search service
include(cmake/AffrescoUIRegex.cmake)

# AccessKit: the accessibility tree of each window, for screen readers (decision D8)
include(cmake/AffrescoUIAccessibility.cmake)
# tree-sitter and its grammars: the syntax service and the editor's highlighting, folds and brackets
include(cmake/AffrescoUISyntax.cmake)

# The library is a set of modules, each its own target. They depend on each other only as shown here; anything
# crossing a module boundary goes through the interfaces in core (ARCHITECTURE.md). Each module lists its sources in
# modules/<name>/CMakeLists.txt, so a new file touches only its own module's list.
#
#   core     interfaces + Signal: Executor, Channel, VersionControl, DocumentRules   (header-only)
#   render   rendering engine: windows, panels, text, OpenGL                         core
#   format   JSON screen format: rules, expressions, building panels, actions        core, render
#   vcs      version control: commits, branches, permission rules, review, change sets  core
#   channel  communication: process/stdio channels, the live protocol                core
#   net      network: HTTP(S), SSE and WebSocket over libcurl                        core
#   services services: folders, programs and JSON-RPC servers a screen calls         core
#   affrescoui  affresco::Screen, which puts them together                         all of the above
#   automation  scenarios: scripted checks a screen ships with, played headless     affrescoui
#
# affrescoui_module(<name> <sources...>) makes the target affrescoui_<name> with the alias affrescoui::<name>, its
# public headers in modules/<name>/include, its private ones in modules/<name>/src, and the strict warnings.
function(affrescoui_module name)
	add_library(affrescoui_${name} ${ARGN})
	add_library(affrescoui::${name} ALIAS affrescoui_${name})
	target_include_directories(affrescoui_${name} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules/${name}/include>
		PRIVATE ${PROJECT_SOURCE_DIR}/modules/${name}/src)
	target_compile_features(affrescoui_${name} PUBLIC cxx_std_23)
	set_target_properties(affrescoui_${name} PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${AFFRESCOUI_WARNINGS}>)
endfunction()

foreach(module core render format vcs channel net services screen automation)
	add_subdirectory(modules/${module})
endforeach()
if(AFFRESCOUI_BUILD_CAPI)
	add_subdirectory(modules/capi) # the C API, on top of the others
endif()

# cmake --install: the libraries, their headers and the AffrescoUI package for find_package
if(AFFRESCOUI_INSTALL)
	include(cmake/AffrescoUIInstall.cmake)
endif()

# Forge's natives, which the benchmark builds too (without the examples)
set(AFFRESCOUI_FORGE_SOURCES examples/forge/Forge.cpp examples/forge/features/palette/FileIndex.cpp
	examples/forge/features/themes/ThemeImport.cpp examples/forge/features/themes/IconTheme.cpp
	examples/forge/features/scm/Scm.cpp examples/forge/features/scm/GitStatus.cpp
	examples/forge/features/scm/GitArguments.cpp examples/forge/features/scm/LineDiff.cpp
	examples/forge/features/accounts/Accounts.cpp examples/forge/features/editor/Buffers.cpp
	examples/forge/features/editor/Editor.cpp examples/forge/features/search/Search.cpp
	examples/forge/features/explorer/Explorer.cpp examples/forge/features/terminal/Terminal.cpp
	examples/forge/features/terminal/TerminalModel.cpp examples/forge/features/debug/Debug.cpp)
# The lsp feature (examples/forge/features/lsp)
list(APPEND AFFRESCOUI_FORGE_SOURCES examples/forge/features/lsp/Lsp.cpp examples/forge/features/lsp/LspProtocol.cpp)
if(AFFRESCOUI_BUILD_EXAMPLES)
	# affrescoui_host renders a screen and accepts peers; affrescoui_studio is a client for it (itself an AffrescoUI screen)
	foreach(example host studio)
		add_executable(affrescoui_${example} examples/${example}.cpp)
		if(example STREQUAL "host")
			target_sources(affrescoui_host PRIVATE examples/CodeEditor.cpp) # the Forge template's "code" type
			# A proposal's evidence plays its scenarios; automation brings the library with it (linking both twice
			# makes the linker warn about a duplicate library)
			target_link_libraries(affrescoui_host PRIVATE affrescoui::automation)
		else()
			target_link_libraries(affrescoui_${example} PRIVATE affrescoui)
		endif()
		target_compile_options(affrescoui_${example} PRIVATE ${AFFRESCOUI_WARNINGS})
		target_compile_definitions(affrescoui_${example} PRIVATE AFFRESCOUI_EXAMPLES_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
	endforeach()
	# Forge, a demo IDE: its workbench is examples/forge/forge.screen.json, with a few natives (examples/forge/README.md)
	add_executable(affrescoui_forge examples/forge/main.cpp ${AFFRESCOUI_FORGE_SOURCES} examples/CodeEditor.cpp)
	target_include_directories(affrescoui_forge PRIVATE examples examples/forge)
	target_link_libraries(affrescoui_forge PRIVATE affrescoui affrescoui_vterm)
	target_compile_options(affrescoui_forge PRIVATE ${AFFRESCOUI_WARNINGS})
	target_compile_definitions(affrescoui_forge PRIVATE AFFRESCOUI_EXAMPLES_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
	# GPU surfaces: native OpenGL drawing in "surface" nodes, a shader, a cube and a blur (examples/surfaces)
	add_executable(affrescoui_surfaces examples/surfaces/main.cpp examples/surfaces/Surfaces.cpp)
	target_include_directories(affrescoui_surfaces SYSTEM PRIVATE external/glad/include)
	target_link_libraries(affrescoui_surfaces PRIVATE affrescoui)
	target_compile_options(affrescoui_surfaces PRIVATE ${AFFRESCOUI_WARNINGS})
	target_compile_definitions(affrescoui_surfaces PRIVATE
		AFFRESCOUI_SURFACES_SCREEN="${CMAKE_CURRENT_SOURCE_DIR}/examples/surfaces/surfaces.screen.json")
	# The document server, headless: documents and their history on disk, shared with people (examples/server/README.md)
	set(AFFRESCOUI_SERVER_SOURCES examples/server/DocumentServer.cpp examples/server/TokenFile.cpp
	    examples/server/UserInfoVerifier.cpp)
	add_executable(affrescoui_server examples/server/main.cpp ${AFFRESCOUI_SERVER_SOURCES})
	target_link_libraries(affrescoui_server PRIVATE affrescoui::format affrescoui::vcs affrescoui::channel affrescoui::net)
	target_compile_options(affrescoui_server PRIVATE ${AFFRESCOUI_WARNINGS})
	if(AFFRESCOUI_BUILD_CAPI)
		# A host's own GLFW and OpenGL app, with a screen over its scene or on a turning quad (examples/embed/README.md)
		add_executable(affrescoui_embed examples/embed/main.cpp)
		target_link_libraries(affrescoui_embed PRIVATE affrescoui::capi) # GLFW and glad come with it
		target_include_directories(affrescoui_embed SYSTEM PRIVATE external/glad/include
			$<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
		target_compile_options(affrescoui_embed PRIVATE ${AFFRESCOUI_WARNINGS})
		target_compile_definitions(affrescoui_embed PRIVATE AFFRESCOUI_EMBED_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples/embed")
	endif()
endif()

if(AFFRESCOUI_BUILD_TESTS)
	enable_testing() # here, so ctest finds the tests the subdirectories add
	add_subdirectory(tests/render) # the rendering check
	add_subdirectory(tests/unit)   # the unit tests and the window tests
	if(AFFRESCOUI_BUILD_CAPI)
		add_subdirectory(tests/capi) # the C API's test, compiled as C
	endif()
	# The MCP server's queries (outline, find_nodes, the guide by topic), which need no host
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_FOUND)
		add_test(NAME mcp COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/mcp/test_affrescoui_mcp.py)
		# The game launcher example, end to end: host, MCP server, a proposal, an approval, a picture
		add_test(NAME mcp_game_launcher COMMAND Python3::Interpreter
			${CMAKE_CURRENT_SOURCE_DIR}/examples/game-launcher/e2e_test.py ${CMAKE_BINARY_DIR})
	endif()
endif()

if(AFFRESCOUI_BUILD_TOOLS)
	add_subdirectory(tools/affrescoui) # the affrescoui command-line tool
endif()

# The fuzz targets' sources can also be compiled alone, without libFuzzer, so that clang-tidy finds them in
# compile_commands.json (tools/quality/run.sh)
option(AFFRESCOUI_COMPILE_FUZZ_SOURCES "Compile tests/fuzz without libFuzzer, for tools that read the build" OFF)
if(AFFRESCOUI_BUILD_FUZZERS OR AFFRESCOUI_COMPILE_FUZZ_SOURCES)
	add_subdirectory(tests/fuzz)
endif()

if(AFFRESCOUI_BUILD_BENCHMARKS)
	# Speed, allocations and memory of the workloads in tests/perf (its README); CI counts instructions with callgrind,
	# using valgrind's header when it is installed
	add_executable(affrescoui_bench tests/perf/bench.cpp tests/perf/allocations.cpp tests/perf/screens.cpp
		tests/perf/engine.cpp tests/perf/history.cpp tests/perf/views.cpp tests/perf/scaling_screens.cpp
		tests/perf/scaling_engine.cpp tests/perf/watch.cpp tests/perf/editor.cpp examples/CodeEditor.cpp tests/perf/buffers.cpp
		tests/perf/search.cpp tests/perf/forge.cpp tests/perf/canvas.cpp tests/perf/graph.cpp tests/perf/budgets.cpp ${AFFRESCOUI_FORGE_SOURCES}
		tests/perf/syntax.cpp)
	# Expression.hpp and render's Headless.hpp (a frame drawn without presenting it) stay private
	target_include_directories(affrescoui_bench PRIVATE examples examples/forge modules/format/src modules/render/src)
	target_include_directories(affrescoui_bench SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
	target_link_libraries(affrescoui_bench PRIVATE affrescoui affrescoui_vterm)
	target_compile_definitions(affrescoui_bench PRIVATE
		AFFRESCOUI_EXAMPLES_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets"
		AFFRESCOUI_PERF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/perf"
		AFFRESCOUI_FORGE_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/tests/render/forge/folder")
	set_target_properties(affrescoui_bench PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_bench PRIVATE ${AFFRESCOUI_WARNINGS})
endif()
