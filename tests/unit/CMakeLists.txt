# Unit tests (README.md). Every .cpp here is a unit test file and every .cpp in window/ a window test file: the globs
# pick a new one up when the build next runs, with no edit to this file. doctest is one header, used without its CMake
# project (SOURCE_SUBDIR has none); its main is in main.cpp, which both programs share.
FetchContent_Declare(doctest URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
	URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70 SOURCE_SUBDIR doctest)
FetchContent_MakeAvailable(doctest)

# Unit tests for the modules that need no window
file(GLOB affrescoui_unit_test_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# forge.cpp tests the demo IDE's plain C++ (examples/forge), which needs no window
set(affrescoui_forge_dir ${PROJECT_SOURCE_DIR}/examples/forge)
add_executable(affrescoui_unit_tests ${affrescoui_unit_test_files} ${PROJECT_SOURCE_DIR}/modules/screen/src/Png.cpp
	${affrescoui_forge_dir}/features/palette/FileIndex.cpp ${affrescoui_forge_dir}/features/themes/ThemeImport.cpp
	${affrescoui_forge_dir}/features/themes/IconTheme.cpp ${affrescoui_forge_dir}/features/terminal/TerminalModel.cpp
	${affrescoui_forge_dir}/features/editor/Buffers.cpp ${affrescoui_forge_dir}/features/search/Search.cpp
	${affrescoui_forge_dir}/features/scm/GitStatus.cpp ${affrescoui_forge_dir}/features/scm/GitArguments.cpp
	${affrescoui_forge_dir}/features/scm/LineDiff.cpp ${affrescoui_forge_dir}/features/scm/Scm.cpp)
# Expression.hpp, Utf8.hpp, Sha256.hpp, Lines.hpp, Png.hpp and automation's Internal.hpp and PngReader.hpp stay private
target_include_directories(affrescoui_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/modules/format/src
	${PROJECT_SOURCE_DIR}/modules/render/src ${PROJECT_SOURCE_DIR}/modules/vcs/src
	${PROJECT_SOURCE_DIR}/modules/channel/src ${PROJECT_SOURCE_DIR}/modules/screen/src
	${PROJECT_SOURCE_DIR}/modules/automation/src ${PROJECT_SOURCE_DIR}/modules/net/src
	${PROJECT_SOURCE_DIR}/modules/services/src ${affrescoui_forge_dir})
target_compile_definitions(affrescoui_unit_tests PRIVATE
	AFFRESCOUI_FONTS_DIR="${PROJECT_SOURCE_DIR}/examples/assets/fonts"
	AFFRESCOUI_TEST_FONTS_DIR="${PROJECT_SOURCE_DIR}/tests/render/fonts")
target_include_directories(affrescoui_unit_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
target_include_directories(affrescoui_unit_tests SYSTEM PRIVATE ${AFFRESCOUI_GLAD_INCLUDE_DIRS}) # gles.cpp: gl/Shader.hpp
target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui::automation affrescoui_vterm) # all modules; screen.cpp loads screens, no window
set_target_properties(affrescoui_unit_tests PROPERTIES CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}) # build/, where the docs and tools/quality run it
target_compile_options(affrescoui_unit_tests PRIVATE ${AFFRESCOUI_WARNINGS})
# The checker's tests read the shipped templates, which must have no findings
target_compile_definitions(affrescoui_unit_tests PRIVATE
	AFFRESCOUI_SCREENS_DIRECTORY="${PROJECT_SOURCE_DIR}/examples/assets/screens"
	AFFRESCOUI_PROTOTYPES_DIR="${PROJECT_SOURCE_DIR}/rfcs/prototypes") # services.cpp starts the JSON-RPC prototype
# document_server.cpp tests the document server (examples/server), which needs no window
target_sources(affrescoui_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/examples/server/DocumentServer.cpp
	${PROJECT_SOURCE_DIR}/examples/server/TokenFile.cpp ${PROJECT_SOURCE_DIR}/examples/server/UserInfoVerifier.cpp)
target_include_directories(affrescoui_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/examples/server)
add_test(NAME unit COMMAND affrescoui_unit_tests WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

# Tests that need a window (an OpenGL context), so a display or Xvfb, like the rendering check: timers, review
# steps, shapes, pictures. Helpers.hpp is shared with the unit tests.
file(GLOB affrescoui_window_test_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/window/*.cpp)
add_executable(affrescoui_window_tests ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${affrescoui_window_test_files})
# glyphs.cpp draws with render's private gl::QuadRenderer, so it sees render's src and glad
target_include_directories(affrescoui_window_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/modules/render/src)
target_include_directories(affrescoui_window_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR}
	$<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES> ${AFFRESCOUI_GLAD_INCLUDE_DIRS})
target_compile_definitions(affrescoui_window_tests PRIVATE GLFW_INCLUDE_NONE
	AFFRESCOUI_FONTS_DIR="${PROJECT_SOURCE_DIR}/examples/assets/fonts"
	AFFRESCOUI_TEST_FONTS_DIR="${PROJECT_SOURCE_DIR}/tests/render/fonts")
target_link_libraries(affrescoui_window_tests PRIVATE affrescoui::automation) # and through it, the library
set_target_properties(affrescoui_window_tests PROPERTIES CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}) # build/, where the docs and tools/quality run it
target_compile_options(affrescoui_window_tests PRIVATE ${AFFRESCOUI_WARNINGS})
add_test(NAME window COMMAND affrescoui_window_tests WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
