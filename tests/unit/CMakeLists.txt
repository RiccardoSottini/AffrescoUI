# Unit tests (README.md). Every .cpp here is a unit test file and every .cpp in window/ a window test file: the globs
# pick a new one up when the build next runs, with no edit to this file. doctest is one header, used without its CMake
# project (SOURCE_SUBDIR has none); its main is in main.cpp, which both programs share.
cmake_language(DEFER CALL affrescoui_hide_test_windows) # AFFRESCOUI_HIDDEN=1 for its tests
FetchContent_Declare(doctest URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
	URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70 SOURCE_SUBDIR doctest)
FetchContent_MakeAvailable(doctest)

# Unit tests for the modules that need no window
file(GLOB affrescoui_unit_test_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
# second_window.cpp draws (images of panels, second windows): it is a window test, below
list(REMOVE_ITEM affrescoui_unit_test_files ${CMAKE_CURRENT_SOURCE_DIR}/second_window.cpp)
# audio_*.cpp test the audio module, which AFFRESCOUI_AUDIO=OFF leaves out
if(NOT AFFRESCOUI_AUDIO)
	list(FILTER affrescoui_unit_test_files EXCLUDE REGEX "/audio_[^/]*\\.cpp$")
endif()
add_executable(affrescoui_unit_tests ${affrescoui_unit_test_files} ${PROJECT_SOURCE_DIR}/modules/screen/src/Png.cpp)
# Expression.hpp, Utf8.hpp, Sha256.hpp, Lines.hpp, Png.hpp and automation's Internal.hpp and PngReader.hpp stay private
target_include_directories(affrescoui_unit_tests PRIVATE $<TARGET_PROPERTY:affrescoui_format,INCLUDE_DIRECTORIES>
	${PROJECT_SOURCE_DIR}/modules/render/src ${PROJECT_SOURCE_DIR}/modules/vcs/src
	${PROJECT_SOURCE_DIR}/modules/channel/src ${PROJECT_SOURCE_DIR}/modules/screen/src
	${PROJECT_SOURCE_DIR}/modules/automation/src ${PROJECT_SOURCE_DIR}/modules/net/src
	${PROJECT_SOURCE_DIR}/modules/services/src
	${PROJECT_SOURCE_DIR}/apps/shared) # Templates.hpp
target_compile_definitions(affrescoui_unit_tests PRIVATE
	AFFRESCOUI_FONTS_DIR="${PROJECT_SOURCE_DIR}/examples/shared/fonts"
	AFFRESCOUI_TEST_FONTS_DIR="${PROJECT_SOURCE_DIR}/tests/render/fonts")
target_include_directories(affrescoui_unit_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
target_include_directories(affrescoui_unit_tests SYSTEM PRIVATE ${AFFRESCOUI_GLAD_INCLUDE_DIRS}) # gles.cpp: gl/Shader.hpp
target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui::automation) # all modules; screen.cpp loads screens, no window
target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui::export) # pdf_export.cpp: PDF of a headless window's panels
target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui::data) # data_*.cpp: the record store and migrations
set_target_properties(affrescoui_unit_tests PROPERTIES CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}) # build/, where the docs and tools/quality run it
target_compile_options(affrescoui_unit_tests PRIVATE ${AFFRESCOUI_WARNINGS})
# The checker's tests read the shipped templates, which must have no findings
target_compile_definitions(affrescoui_unit_tests PRIVATE
	AFFRESCOUI_EXAMPLES_DIRECTORY="${PROJECT_SOURCE_DIR}/examples"
	AFFRESCOUI_PROTOTYPES_DIR="${PROJECT_SOURCE_DIR}/rfcs/prototypes" # services.cpp starts the JSON-RPC prototype
	AFFRESCOUI_FAKE_ADAPTER="${PROJECT_SOURCE_DIR}/tests/dap/fake_adapter.py" # dap.cpp starts the fake debug adapter
	AFFRESCOUI_FAKE_LANGUAGE_SERVER="${PROJECT_SOURCE_DIR}/tests/lsp/fake_server.py") # lsp_service.cpp, the fake server
# document_server.cpp tests the document server (apps/server), which needs no window
target_sources(affrescoui_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/apps/server/DocumentServer.cpp
	${PROJECT_SOURCE_DIR}/apps/server/TokenFile.cpp ${PROJECT_SOURCE_DIR}/apps/server/UserInfoVerifier.cpp)
target_include_directories(affrescoui_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/apps/server)
# scene3d*.cpp test the 3D module on Modello's sample models (examples/modello/samples)
target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui::scene3d)
if(AFFRESCOUI_AUDIO)
	target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui::audio)
endif()
target_compile_definitions(affrescoui_unit_tests PRIVATE
	AFFRESCOUI_MODELLO_SAMPLES="${PROJECT_SOURCE_DIR}/examples/modello/samples")
target_include_directories(affrescoui_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/examples ${PROJECT_SOURCE_DIR}/examples/shared)
# intelligence*.cpp test the providers, settings and agent (modules/intelligence)
target_link_libraries(affrescoui_unit_tests PRIVATE affrescoui::intelligence)
target_compile_definitions(affrescoui_unit_tests PRIVATE AFFRESCOUI_EXAMPLES_ASSETS="${PROJECT_SOURCE_DIR}/examples")
add_test(NAME unit COMMAND affrescoui_unit_tests WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

# Tests that need a window (an OpenGL context), so a display or Xvfb, like the rendering check: timers, review
# steps, shapes, pictures. Helpers.hpp is shared with the unit tests.
file(GLOB affrescoui_window_test_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/window/*.cpp)
add_executable(affrescoui_window_tests ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${affrescoui_window_test_files}
	${CMAKE_CURRENT_SOURCE_DIR}/second_window.cpp)
# glyphs.cpp draws with render's private gl::QuadRenderer, so it sees render's src and glad
target_include_directories(affrescoui_window_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/modules/render/src)
target_include_directories(affrescoui_window_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR}
	$<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES> ${AFFRESCOUI_GLAD_INCLUDE_DIRS})
target_compile_definitions(affrescoui_window_tests PRIVATE GLFW_INCLUDE_NONE
	AFFRESCOUI_FONTS_DIR="${PROJECT_SOURCE_DIR}/examples/shared/fonts"
	AFFRESCOUI_TEST_FONTS_DIR="${PROJECT_SOURCE_DIR}/tests/render/fonts")
target_link_libraries(affrescoui_window_tests PRIVATE affrescoui::automation) # and through it, the library
set_target_properties(affrescoui_window_tests PROPERTIES CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}) # build/, where the docs and tools/quality run it
target_compile_options(affrescoui_window_tests PRIVATE ${AFFRESCOUI_WARNINGS})
add_test(NAME window COMMAND affrescoui_window_tests WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
# window/accessibility_*.cpp read a window's tree through the platform's API: on macOS, NSAccessibility (Objective-C++),
# on Windows, UI Automation, and on Linux, AT-SPI when libatspi is installed
if(TARGET affrescoui_accesskit)
	target_compile_definitions(affrescoui_window_tests PRIVATE AFFRESCOUI_ACCESSIBILITY)
	if(APPLE)
		set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/window/accessibility_mac.cpp PROPERTIES LANGUAGE OBJCXX
			COMPILE_OPTIONS -fobjc-arc)
		target_link_libraries(affrescoui_window_tests PRIVATE "-framework AppKit")
	elseif(WIN32)
		target_link_libraries(affrescoui_window_tests PRIVATE ole32 oleaut32 uiautomationcore)
	else()
		find_package(PkgConfig QUIET)
		if(PKG_CONFIG_FOUND)
			# GObject too: the test releases AT-SPI's objects with g_object_unref, and atspi-2.pc doesn't link it
			pkg_check_modules(AFFRESCOUI_ATSPI QUIET IMPORTED_TARGET atspi-2 gobject-2.0)
		endif()
		if(AFFRESCOUI_ATSPI_FOUND)
			target_compile_definitions(affrescoui_window_tests PRIVATE AFFRESCOUI_ATSPI_TEST)
			target_link_libraries(affrescoui_window_tests PRIVATE PkgConfig::AFFRESCOUI_ATSPI)
		endif()
	endif()
endif()
