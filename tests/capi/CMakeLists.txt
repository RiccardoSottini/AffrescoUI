# The C API's test (README.md): every function of affresco/affresco.h, called from C11 with strict warnings, so the
# header is checked as C too. It needs a display (a hidden window, and a context of its own for the embedded mode).
cmake_language(DEFER CALL affrescoui_hide_test_windows) # AFFRESCOUI_HIDDEN=1 for its tests
add_executable(affrescoui_capi_test capi_test.c)
set_target_properties(affrescoui_capi_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
	LINKER_LANGUAGE CXX)
if(NOT MSVC)
	target_compile_options(affrescoui_capi_test PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
		-Wstrict-prototypes -Wmissing-prototypes)
endif()
target_link_libraries(affrescoui_capi_test PRIVATE affrescoui::capi) # GLFW comes with it, for the embedded mode
target_include_directories(affrescoui_capi_test SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(affrescoui_capi_test PRIVATE GLFW_INCLUDE_NONE) # GLFW's header only: no system GL headers
# Its scratch folder: the screen file, its sockets. A socket's path has a length limit (104 bytes on macOS), which a
# build folder deep in a worktree passes, so on POSIX it is a short folder in /tmp, one per build folder
set(affrescoui_capi_scratch ${CMAKE_CURRENT_BINARY_DIR}/s)
if(UNIX)
	string(MD5 affrescoui_capi_build ${CMAKE_CURRENT_BINARY_DIR})
	string(SUBSTRING ${affrescoui_capi_build} 0 12 affrescoui_capi_build)
	set(affrescoui_capi_scratch /tmp/afrcapi-${affrescoui_capi_build})
endif()
add_test(NAME capi-scratch COMMAND ${CMAKE_COMMAND} -E make_directory ${affrescoui_capi_scratch})
set_tests_properties(capi-scratch PROPERTIES FIXTURES_SETUP capi-scratch)
add_test(NAME capi COMMAND affrescoui_capi_test ${affrescoui_capi_scratch})
set_tests_properties(capi PROPERTIES FIXTURES_REQUIRED capi-scratch)
# It passes enums values they don't have, as C may: under UBSan, C++ reading them as the enum fails the test
set_tests_properties(capi PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
