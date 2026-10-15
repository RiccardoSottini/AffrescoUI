cmake_minimum_required(VERSION 3.25)
project(sGLGUI VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SGLGUI_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})

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
	set(SGLGUI_WARNINGS /W4 /permissive-)
else()
	set(SGLGUI_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast
		-Wnon-virtual-dtor -Woverloaded-virtual)
endif()

# The library is a set of modules, each its own target. They depend on each other only as shown here; anything
# crossing a module boundary goes through the interfaces in core (README.md, "Architecture").
#
#   core     interfaces + Signal: Executor, Channel, VersionControl, DocumentRules   (header-only)
#   render   rendering engine: windows, panels, text, OpenGL                         core
#   format   JSON screen format: rules, expressions, building panels, actions        core, render
#   vcs      version control: history, permissions, review, change sets              core
#   channel  communication: process/stdio channels, the live protocol                core
#   sglgui   sgl::Screen, which puts them together                                   all of the above
function(sglgui_module name)
	add_library(sglgui_${name} ${ARGN})
	add_library(sglgui::${name} ALIAS sglgui_${name})
	target_include_directories(sglgui_${name} PUBLIC modules/${name}/include PRIVATE modules/${name}/src)
	target_compile_features(sglgui_${name} PUBLIC cxx_std_23)
	set_target_properties(sglgui_${name} PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_options(sglgui_${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${SGLGUI_WARNINGS}>)
endfunction()

# core: the contracts between modules. JSON is opt-in: the rendering engine doesn't see it.
add_library(sglgui_core INTERFACE)
add_library(sglgui::core ALIAS sglgui_core)
target_include_directories(sglgui_core INTERFACE modules/core/include)
target_compile_features(sglgui_core INTERFACE cxx_std_23)

# render. Vendored in external/: glad (OpenGL 4.1 core loader) and stb_truetype (font rasteriser).
sglgui_module(render
	modules/render/src/Application.cpp
	modules/render/src/Color.cpp
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
target_include_directories(sglgui_render SYSTEM PRIVATE external/glad/include external/stb)
target_link_libraries(sglgui_render PUBLIC sglgui::core PRIVATE glfw)
target_compile_definitions(sglgui_render PRIVATE GLFW_INCLUDE_NONE)

sglgui_module(format modules/format/src/Expression.cpp modules/format/src/Rules.cpp modules/format/src/Runtime.cpp)
target_link_libraries(sglgui_format PUBLIC sglgui::core sglgui::render nlohmann_json::nlohmann_json)

sglgui_module(vcs modules/vcs/src/Repository.cpp)
target_link_libraries(sglgui_vcs PUBLIC sglgui::core nlohmann_json::nlohmann_json)

sglgui_module(channel modules/channel/src/Channels.cpp modules/channel/src/Session.cpp modules/channel/src/Sockets.cpp)
target_link_libraries(sglgui_channel PUBLIC sglgui::core nlohmann_json::nlohmann_json)

# sglgui: the whole library
add_library(sglgui modules/screen/src/Screen.cpp)
add_library(sglgui::sglgui ALIAS sglgui)
target_include_directories(sglgui PUBLIC modules/screen/include)
set_target_properties(sglgui PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(sglgui PRIVATE ${SGLGUI_WARNINGS})
target_link_libraries(sglgui PUBLIC sglgui::core sglgui::render sglgui::format sglgui::vcs sglgui::channel)

if(SGLGUI_BUILD_EXAMPLES)
	# sglgui_host renders a screen and accepts peers; sglgui_studio is a client for it (itself an sGLGUI screen)
	foreach(example host studio)
		add_executable(sglgui_${example} examples/${example}.cpp)
		target_link_libraries(sglgui_${example} PRIVATE sglgui)
		target_compile_options(sglgui_${example} PRIVATE ${SGLGUI_WARNINGS})
		target_compile_definitions(sglgui_${example} PRIVATE SGLGUI_EXAMPLES_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
	endforeach()
endif()
