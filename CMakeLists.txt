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

# nlohmann/json: parses screen files (used only inside the library, not exposed by its headers)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
	URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d SYSTEM)
FetchContent_MakeAvailable(json)

# Library. Vendored in external/: glad (OpenGL 4.1 core loader) and stb_truetype (font rasteriser).
add_library(sglgui
	src/Application.cpp
	src/Color.cpp
	src/Font.cpp
	src/Label.cpp
	src/Panel.cpp
	src/Screen.cpp
	src/Stack.cpp
	src/TextArea.cpp
	src/Window.cpp
	src/gl/Context.cpp
	src/gl/QuadRenderer.cpp
	src/gl/Shader.cpp
	external/glad/src/glad.c
	external/stb/stb_truetype.c)
add_library(sglgui::sglgui ALIAS sglgui)

target_compile_features(sglgui PUBLIC cxx_std_23)
set_target_properties(sglgui PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(sglgui PUBLIC include PRIVATE src)
target_include_directories(sglgui SYSTEM PRIVATE external/glad/include external/stb)
target_link_libraries(sglgui PRIVATE glfw nlohmann_json::nlohmann_json)
target_compile_definitions(sglgui PRIVATE GLFW_INCLUDE_NONE)

# Strict warnings for our C++ code only (not for glad.c or dependencies)
if(MSVC)
	set(SGLGUI_WARNINGS /W4 /permissive-)
else()
	set(SGLGUI_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast
		-Wnon-virtual-dtor -Woverloaded-virtual)
endif()
target_compile_options(sglgui PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${SGLGUI_WARNINGS}>)

if(SGLGUI_BUILD_EXAMPLES)
	foreach(example chatbot_demo screen_viewer)
		add_executable(${example} examples/${example}.cpp)
		target_link_libraries(${example} PRIVATE sglgui)
		target_compile_options(${example} PRIVATE ${SGLGUI_WARNINGS})
		target_compile_definitions(${example} PRIVATE SGLGUI_EXAMPLES_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/examples/assets")
	endforeach()
endif()
