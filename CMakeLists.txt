cmake_minimum_required(VERSION 3.25)
project(AffrescoUI VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimised unless asked otherwise (-DCMAKE_BUILD_TYPE=Debug): the runtime evaluates the whole screen on every update
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug, Release, RelWithDebInfo or MinSizeRel" FORCE)
endif()

option(AFFRESCOUI_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
option(AFFRESCOUI_BUILD_TESTS "Build the rendering check (tests/render)" ${PROJECT_IS_TOP_LEVEL})

include(FetchContent)

# GLFW: windows, OpenGL context, input
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE SYSTEM)
FetchContent_MakeAvailable(glfw)

# nlohmann/json: the documents (format, vcs and channel modules; the rendering engine does without)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
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
	target_include_directories(affrescoui_${name} PUBLIC modules/${name}/include PRIVATE modules/${name}/src)
	target_compile_features(affrescoui_${name} PUBLIC cxx_std_23)
	set_target_properties(affrescoui_${name} PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(affrescoui_${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${AFFRESCOUI_WARNINGS}>)
endfunction()

# core: the contracts between modules. JSON is opt-in: the rendering engine doesn't see it.
add_library(affrescoui_core INTERFACE)
add_library(affrescoui::core ALIAS affrescoui_core)
target_include_directories(affrescoui_core INTERFACE modules/core/include)
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
	modules/format/src/Steps.cpp)
target_link_libraries(affrescoui_format PUBLIC affrescoui::core affrescoui::render nlohmann_json::nlohmann_json)

affrescoui_module(vcs modules/vcs/src/Repository.cpp modules/vcs/src/Permissions.cpp modules/vcs/src/Sha256.cpp)
target_link_libraries(affrescoui_vcs PUBLIC affrescoui::core nlohmann_json::nlohmann_json)

affrescoui_module(channel modules/channel/src/Channels.cpp modules/channel/src/Session.cpp modules/channel/src/Sockets.cpp)
target_link_libraries(affrescoui_channel PUBLIC affrescoui::core nlohmann_json::nlohmann_json)

# affrescoui: the whole library
add_library(affrescoui modules/screen/src/Screen.cpp)
add_library(affrescoui::affrescoui ALIAS affrescoui)
target_include_directories(affrescoui PUBLIC modules/screen/include)
set_target_properties(affrescoui PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(affrescoui PRIVATE ${AFFRESCOUI_WARNINGS})
target_link_libraries(affrescoui PUBLIC affrescoui::core affrescoui::render affrescoui::format affrescoui::vcs affrescoui::channel)

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
endif()
