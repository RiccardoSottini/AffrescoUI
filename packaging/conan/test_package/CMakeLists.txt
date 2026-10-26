# A consumer of the Conan package, the way find_package users consume AffrescoUI: every target the installed package
# exports must be there (tools/release/packaging.py --check keeps this list equal to cmake/AffrescoUIInstall.cmake's).
cmake_minimum_required(VERSION 3.25)
project(AffrescoUITestPackage LANGUAGES CXX)

find_package(AffrescoUI CONFIG REQUIRED)

foreach(target affrescoui::affrescoui affrescoui::core affrescoui::data affrescoui::render affrescoui::format affrescoui::themes
	affrescoui::vcs affrescoui::channel affrescoui::net affrescoui::services affrescoui::automation
	affrescoui::harfbuzz affrescoui::regex affrescoui::imaging affrescoui::scene3d affrescoui::export
	affrescoui::harfbuzz_subset affrescoui::intelligence affrescoui::syntax)
	if(NOT TARGET ${target})
		message(FATAL_ERROR "find_package(AffrescoUI) defined no ${target}")
	endif()
endforeach()

# One module on its own, with no window or OpenGL, so the test runs anywhere
add_executable(test_package test_package.cpp)
target_link_libraries(test_package PRIVATE affrescoui::vcs)
target_compile_features(test_package PRIVATE cxx_std_23)
