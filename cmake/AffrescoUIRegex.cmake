# PCRE2, the regular expressions of the editor's find and of the search service, included by CMakeLists.txt. The
# release archive is pinned by its hash like the other dependencies and built from source as one static library,
# affrescoui_regex, with its 8-bit, Unicode matcher only: no JIT (a regular expression from a peer is interpreted with
# match limits, never compiled to machine code), no POSIX wrapper, no grep or test programs. The library also holds
# affresco::TextPattern (modules/core/include/affresco/core/TextPattern.hpp), the one wrapper render and services use.
set(AFFRESCOUI_PCRE2_VERSION 10.49)
FetchContent_Declare(pcre2
	URL https://github.com/PCRE2Project/pcre2/releases/download/pcre2-${AFFRESCOUI_PCRE2_VERSION}/pcre2-${AFFRESCOUI_PCRE2_VERSION}.tar.bz2
	URL_HASH SHA256=53c156e1ba416a20da8e65395daa132da0d80e76910424caca3fcdae7831d384
	SOURCE_SUBDIR affrescoui-builds-the-sources-itself SYSTEM) # a folder it doesn't have: its CMakeLists.txt isn't run
FetchContent_MakeAvailable(pcre2)

# The non-autotools build of its NON-AUTOTOOLS-BUILD file: the generic configuration and character tables, copied
set(AFFRESCOUI_PCRE2_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/pcre2-generated)
configure_file(${pcre2_SOURCE_DIR}/src/config.h.generic ${AFFRESCOUI_PCRE2_GENERATED}/config.h COPYONLY)
configure_file(${pcre2_SOURCE_DIR}/src/pcre2.h.generic ${AFFRESCOUI_PCRE2_GENERATED}/pcre2.h COPYONLY)
configure_file(${pcre2_SOURCE_DIR}/src/pcre2_chartables.c.dist ${AFFRESCOUI_PCRE2_GENERATED}/pcre2_chartables.c COPYONLY)
set(AFFRESCOUI_PCRE2_SOURCES ${AFFRESCOUI_PCRE2_GENERATED}/pcre2_chartables.c)
foreach(source auto_possess chkdint compile compile_cgroup compile_class config context convert dfa_match error extuni
		find_bracket jit_compile maketables match match_data match_next newline ord2utf pattern_info script_run serialize
		string_utils study substitute substring tables ucd valid_utf xclass)
	list(APPEND AFFRESCOUI_PCRE2_SOURCES ${pcre2_SOURCE_DIR}/src/pcre2_${source}.c)
endforeach()

add_library(affrescoui_regex STATIC ${AFFRESCOUI_PCRE2_SOURCES} modules/core/src/TextPattern.cpp)
add_library(affrescoui::regex ALIAS affrescoui_regex)
target_include_directories(affrescoui_regex PRIVATE ${AFFRESCOUI_PCRE2_GENERATED} ${pcre2_SOURCE_DIR}/src)
target_link_libraries(affrescoui_regex PUBLIC affrescoui_core)
target_compile_definitions(affrescoui_regex PRIVATE HAVE_CONFIG_H PCRE2_CODE_UNIT_WIDTH=8 PCRE2_STATIC SUPPORT_UNICODE
	HAVE_MEMMOVE HAVE_STDINT_H HAVE_INTTYPES_H HAVE_LIMITS_H HAVE_STRERROR)
set_target_properties(affrescoui_regex PROPERTIES C_STANDARD 99 CXX_EXTENSIONS OFF EXPORT_NAME regex)
# Its warnings are not ours to fix; the wrapper gets the strict ones. Visual Studio's projects can't keep a target's C
# and C++ options apart (both got /W4, and with warnings as errors PCRE2 failed), so there they are set file by file.
if(MSVC)
	set_source_files_properties(${AFFRESCOUI_PCRE2_SOURCES} PROPERTIES COMPILE_OPTIONS /w)
	set_source_files_properties(modules/core/src/TextPattern.cpp PROPERTIES COMPILE_OPTIONS "${AFFRESCOUI_WARNINGS}")
else()
	target_compile_options(affrescoui_regex PRIVATE $<$<COMPILE_LANGUAGE:C>:-w> $<$<COMPILE_LANGUAGE:CXX>:${AFFRESCOUI_WARNINGS}>)
endif()
