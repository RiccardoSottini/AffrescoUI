# Unit tests (README.md). Every .cpp here is a unit test file and every .cpp in window/ a window test file: the globs
# pick a new one up when the build next runs, with no edit to this file. doctest is one header, used without its CMake
# project (SOURCE_SUBDIR has none); its main is in main.cpp, which both programs share.
FetchContent_Declare(doctest URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
	URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70 SOURCE_SUBDIR doctest)
FetchContent_MakeAvailable(doctest)

# Unit tests for the modules that need no window
file(GLOB affrescoui_unit_test_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(affrescoui_unit_tests ${affrescoui_unit_test_files} ${PROJECT_SOURCE_DIR}/modules/screen/src/Png.cpp)
# Expression.hpp, Utf8.hpp, Sha256.hpp and Png.hpp stay private
target_include_directories(affrescoui_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/modules/format/src
	${PROJECT_SOURCE_DIR}/modules/render/src ${PROJECT_SOURCE_DIR}/modules/vcs/src
	${PROJECT_SOURCE_DIR}/modules/screen/src)
target_compile_definitions(affrescoui_unit_tests PRIVATE
	AFFRESCOUI_FONTS_DIR="${PROJECT_SOURCE_DIR}/examples/assets/fonts")
target_include_directories(affrescoui_unit_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui) # all modules; screen.cpp loads screens, no window
set_target_properties(affrescoui_unit_tests PROPERTIES CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}) # build/, where the docs and tools/quality run it
target_compile_options(affrescoui_unit_tests PRIVATE ${AFFRESCOUI_WARNINGS})
# The checker's tests read the shipped templates, which must have no findings
target_compile_definitions(affrescoui_unit_tests PRIVATE
	AFFRESCOUI_SCREENS_DIRECTORY="${PROJECT_SOURCE_DIR}/examples/assets/screens")
add_test(NAME unit COMMAND affrescoui_unit_tests WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

# Tests that need a window (an OpenGL context), so a display or Xvfb, like the rendering check: timers, review
# steps, shapes, pictures. Helpers.hpp is shared with the unit tests.
file(GLOB affrescoui_window_test_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/window/*.cpp)
add_executable(affrescoui_window_tests ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${affrescoui_window_test_files})
target_include_directories(affrescoui_window_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(affrescoui_window_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR}
	$<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(affrescoui_window_tests PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(affrescoui_window_tests PRIVATE affrescoui)
set_target_properties(affrescoui_window_tests PROPERTIES CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}) # build/, where the docs and tools/quality run it
target_compile_options(affrescoui_window_tests PRIVATE ${AFFRESCOUI_WARNINGS})
add_test(NAME window COMMAND affrescoui_window_tests WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
