cmake_minimum_required(VERSION 3.25)
project(AffrescoUI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimised unless asked otherwise (-DCMAKE_BUILD_TYPE=Debug): the runtime evaluates the whole screen on every update
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug, Release, RelWithDebInfo or MinSizeRel" FORCE)
endif()

option(AFFRESCOUI_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
option(AFFRESCOUI_BUILD_TESTS "Build the rendering check and the unit tests (tests/)" ${PROJECT_IS_TOP_LEVEL})
option(AFFRESCOUI_BUILD_FUZZERS "Build the libFuzzer targets (tests/fuzz); needs clang, instruments the whole build" OFF)
option(AFFRESCOUI_BUILD_BENCHMARKS "Build the benchmarks (tests/perf)" OFF)

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
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL ${AFFRESCOUI_INSTALL} CACHE BOOL "" FORCE) # installed static libraries link it
# The release archive, pinned by its hash like the other dependencies (a git tag can be moved)
FetchContent_Declare(glfw
	URL https://github.com/glfw/glfw/releases/download/${AFFRESCOUI_GLFW_VERSION}/glfw-${AFFRESCOUI_GLFW_VERSION}.zip
	URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9 SYSTEM)
FetchContent_MakeAvailable(glfw)
# Warnings as errors are for our code: GLFW's own warnings (e.g. sprintf, deprecated in recent macOS SDKs) must not
# fail the build
set_target_properties(glfw PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

# nlohmann/json: the documents (format, vcs and channel modules; the rendering engine does without)
set(AFFRESCOUI_JSON_VERSION 3.11.3)
set(JSON_Install ${AFFRESCOUI_INSTALL} CACHE BOOL "" FORCE) # core/Json.hpp includes it
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v${AFFRESCOUI_JSON_VERSION}/json.tar.xz
	URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d SYSTEM)
FetchContent_MakeAvailable(json)

# Strict warnings for our C++ code only (not for glad.c or dependencies)
if(MSVC)
	set(AFFRESCOUI_WARNINGS /W4 /permissive-)
else()
	set(AFFRESCOUI_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast
		-Wnon-virtual-dtor -Woverloaded-virtual)
endif()

# The library is a set of modules, each its own target. They depend on each other only as shown here; anything
# crossing a module boundary goes through the interfaces in core (README.md, "Architecture").
#
#   core     interfaces + Signal: Executor, Channel, VersionControl, DocumentRules   (header-only)
#   render   rendering engine: windows, panels, text, OpenGL                         core
#   format   JSON screen format: rules, expressions, building panels, actions        core, render
#   vcs      version control: commits, branches, permission rules, review, change sets  core
#   channel  communication: process/stdio channels, the live protocol                core
#   affrescoui  affresco::Screen, which puts them together                         all of the above
function(affrescoui_module name)
	add_library(affrescoui_${name} ${ARGN})
	add_library(affrescoui::${name} ALIAS affrescoui_${name})
	target_include_directories(affrescoui_${name} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules/${name}/include>
		PRIVATE modules/${name}/src)
	target_compile_features(affrescoui_${name} PUBLIC cxx_std_23)
	set_target_properties(affrescoui_${name} PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${AFFRESCOUI_WARNINGS}>)
endfunction()

# core: the contracts between modules. JSON is opt-in: the rendering engine doesn't see it.
add_library(affrescoui_core INTERFACE)
add_library(affrescoui::core ALIAS affrescoui_core)
target_include_directories(affrescoui_core INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules/core/include>)
target_compile_features(affrescoui_core INTERFACE cxx_std_23)

# render. Vendored in external/: glad (OpenGL 4.1 core loader) and stb_truetype (font rasteriser).
affrescoui_module(render
	modules/render/src/Application.cpp
	modules/render/src/TextEditor.cpp
	modules/render/src/Color.cpp
	modules/render/src/Controls.cpp
	modules/render/src/Font.cpp
	modules/render/src/Label.cpp
	modules/render/src/Panel.cpp
	modules/render/src/Stack.cpp
	modules/render/src/TextArea.cpp
	modules/render/src/Window.cpp
	modules/render/src/gl/Context.cpp
	modules/render/src/gl/QuadRenderer.cpp
	modules/render/src/gl/Shader.cpp
	external/glad/src/glad.c
	external/stb/stb_truetype.c)
target_include_directories(affrescoui_render SYSTEM PRIVATE external/glad/include external/stb)
target_link_libraries(affrescoui_render PUBLIC affrescoui::core PRIVATE glfw)
target_compile_definitions(affrescoui_render PRIVATE GLFW_INCLUDE_NONE)

affrescoui_module(format
	modules/format/src/Expression.cpp
	modules/format/src/Rules.cpp
	modules/format/src/Runtime.cpp
	modules/format/src/Build.cpp
	modules/format/src/Properties.cpp
	modules/format/src/Events.cpp
	modules/format/src/Steps.cpp
	modules/format/src/Checker.cpp
	modules/format/src/CheckerNodes.cpp
	modules/format/src/CheckerValues.cpp)
target_link_libraries(affrescoui_format PUBLIC affrescoui::core affrescoui::render nlohmann_json::nlohmann_json)

affrescoui_module(vcs modules/vcs/src/Repository.cpp modules/vcs/src/RepositoryHistory.cpp
	modules/vcs/src/HistoryStore.cpp modules/vcs/src/Permissions.cpp modules/vcs/src/Sha256.cpp)
target_link_libraries(affrescoui_vcs PUBLIC affrescoui::core nlohmann_json::nlohmann_json)

affrescoui_module(channel modules/channel/src/Channels.cpp modules/channel/src/Session.cpp modules/channel/src/Sockets.cpp)
target_link_libraries(affrescoui_channel PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
if(WIN32)
	target_link_libraries(affrescoui_channel PRIVATE ws2_32 advapi32) # Winsock; the socket files' access lists
endif()

# affrescoui: the whole library
add_library(affrescoui modules/screen/src/Screen.cpp modules/screen/src/OffscreenImages.cpp modules/screen/src/Png.cpp)
add_library(affrescoui::affrescoui ALIAS affrescoui)
target_include_directories(affrescoui PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/modules/screen/include>)
set_target_properties(affrescoui PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(affrescoui PRIVATE ${AFFRESCOUI_WARNINGS})
target_link_libraries(affrescoui PUBLIC affrescoui::core affrescoui::render affrescoui::format affrescoui::vcs affrescoui::channel)

# cmake --install: the libraries, their headers and the AffrescoUI package for find_package
if(AFFRESCOUI_INSTALL)
	include(cmake/AffrescoUIInstall.cmake)
endif()

if(AFFRESCOUI_BUILD_EXAMPLES)
	# affrescoui_host renders a screen and accepts peers; affrescoui_studio is a client for it (itself an AffrescoUI screen)
	foreach(example host studio)
		add_executable(affrescoui_${example} examples/${example}.cpp)
		if(example STREQUAL "host")
			target_sources(affrescoui_host PRIVATE examples/CodeEditor.cpp) # the Forge template's "code" type
		endif()
		target_link_libraries(affrescoui_${example} PRIVATE affrescoui)
		target_compile_options(affrescoui_${example} PRIVATE ${AFFRESCOUI_WARNINGS})
		target_compile_definitions(affrescoui_${example} PRIVATE AFFRESCOUI_EXAMPLES_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
	endforeach()
endif()

if(AFFRESCOUI_BUILD_TESTS)
	# Plays the scenarios in tests/render/scenarios.json; check.py compares the frames with the golden images
	add_executable(affrescoui_render_runner tests/render/runner.cpp examples/CodeEditor.cpp)
	target_include_directories(affrescoui_render_runner PRIVATE examples)
	target_link_libraries(affrescoui_render_runner PRIVATE affrescoui nlohmann_json::nlohmann_json)
	target_include_directories(affrescoui_render_runner PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
	target_compile_options(affrescoui_render_runner PRIVATE ${AFFRESCOUI_WARNINGS})

	enable_testing()
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_FOUND)
		add_test(NAME rendering COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/render/check.py
		         --build ${CMAKE_CURRENT_BINARY_DIR})
	endif()

	# Unit tests for the modules that need no window (tests/unit). doctest is one header, used without its CMake
	# project (SOURCE_SUBDIR has none); its main is in main.cpp.
	FetchContent_Declare(doctest URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
		URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70 SOURCE_SUBDIR doctest)
	FetchContent_MakeAvailable(doctest)

	add_executable(affrescoui_unit_tests tests/unit/main.cpp tests/unit/core.cpp tests/unit/vcs.cpp
		tests/unit/history.cpp tests/unit/expressions.cpp tests/unit/rules.cpp tests/unit/channel.cpp tests/unit/sockets.cpp
		tests/unit/screen.cpp tests/unit/text.cpp tests/unit/painter.cpp tests/unit/checker.cpp tests/unit/png.cpp
		modules/screen/src/Png.cpp)
	target_sources(affrescoui_unit_tests PRIVATE tests/unit/nesting.cpp)
	# Expression.hpp, Utf8.hpp, Sha256.hpp and Png.hpp stay private
	target_include_directories(affrescoui_unit_tests PRIVATE modules/format/src modules/render/src modules/vcs/src
		modules/screen/src)
	target_compile_definitions(affrescoui_unit_tests PRIVATE
		AFFRESCOUI_FONTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/fonts")
	target_include_directories(affrescoui_unit_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
	target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui) # all modules; screen.cpp loads screens, no window
	set_target_properties(affrescoui_unit_tests PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_unit_tests PRIVATE ${AFFRESCOUI_WARNINGS})
	# The checker's tests read the shipped templates, which must have no findings
	target_compile_definitions(affrescoui_unit_tests PRIVATE
		AFFRESCOUI_SCREENS_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets/screens")
	add_test(NAME unit COMMAND affrescoui_unit_tests)

	# Tests that need a window (an OpenGL context), so a display or Xvfb, like the rendering check: timers, review
	# steps, shapes, pictures
	add_executable(affrescoui_window_tests tests/unit/main.cpp tests/unit/timers.cpp tests/unit/review.cpp
		tests/unit/shapes.cpp tests/unit/images.cpp)
	target_include_directories(affrescoui_window_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR}
		$<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
	target_compile_definitions(affrescoui_window_tests PRIVATE GLFW_INCLUDE_NONE)
	target_link_libraries(affrescoui_window_tests PRIVATE affrescoui)
	set_target_properties(affrescoui_window_tests PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_window_tests PRIVATE ${AFFRESCOUI_WARNINGS})
	add_test(NAME window COMMAND affrescoui_window_tests)
endif()

if(AFFRESCOUI_BUILD_FUZZERS)
	add_subdirectory(tests/fuzz)
endif()

if(AFFRESCOUI_BUILD_BENCHMARKS)
	# Speed, allocations and memory of the workloads in tests/perf (its README); CI counts instructions with callgrind,
	# using valgrind's header when it is installed
	add_executable(affrescoui_bench tests/perf/bench.cpp tests/perf/allocations.cpp tests/perf/screens.cpp
		tests/perf/engine.cpp examples/CodeEditor.cpp)
	target_include_directories(affrescoui_bench PRIVATE examples modules/format/src) # Expression.hpp stays private
	target_include_directories(affrescoui_bench SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
	target_link_libraries(affrescoui_bench PRIVATE affrescoui)
	target_compile_definitions(affrescoui_bench PRIVATE
		AFFRESCOUI_EXAMPLES_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets"
		AFFRESCOUI_PERF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/perf")
	set_target_properties(affrescoui_bench PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_bench PRIVATE ${AFFRESCOUI_WARNINGS})
endif()
