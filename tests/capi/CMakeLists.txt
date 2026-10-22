# The C API's test (README.md): every function of affresco/affresco.h, called from C11 with strict warnings, so the
# header is checked as C too. It needs a display (a hidden window, and a context of its own for the embedded mode).
add_executable(affrescoui_capi_test capi_test.c)
set_target_properties(affrescoui_capi_test PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
	LINKER_LANGUAGE CXX)
if(NOT MSVC)
	target_compile_options(affrescoui_capi_test PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
		-Wstrict-prototypes -Wmissing-prototypes)
endif()
target_link_libraries(affrescoui_capi_test PRIVATE affrescoui::capi) # GLFW comes with it, for the embedded mode
target_include_directories(affrescoui_capi_test SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
# Its scratch folder: the screen file, its sockets
set(affrescoui_capi_scratch ${CMAKE_CURRENT_BINARY_DIR}/s)
file(MAKE_DIRECTORY ${affrescoui_capi_scratch})
add_test(NAME capi COMMAND affrescoui_capi_test ${affrescoui_capi_scratch})
# It passes enums values they don't have, as C may: under UBSan, C++ reading them as the enum fails the test
set_tests_properties(capi PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
