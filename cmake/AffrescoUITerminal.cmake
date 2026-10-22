# The terminal emulator of Forge's terminal panel (examples/forge/features/terminal), included by the root
# CMakeLists.txt: libvterm, downloaded as a release archive pinned by its hash and compiled into a small static library
# (its own Makefile isn't run). Its generated tables (src/encoding/*.inc, src/fullwidth.inc) ship in the archive.
set(AFFRESCOUI_VTERM_VERSION 0.3.3)
FetchContent_Declare(vterm
	URL https://launchpad.net/libvterm/trunk/v0.3/+download/libvterm-${AFFRESCOUI_VTERM_VERSION}.tar.gz
	URL_HASH SHA256=09156f43dd2128bd347cbeebe50d9a571d32c64e0cf18d211197946aff7226e0
	SOURCE_SUBDIR affrescoui-compiles-its-sources SYSTEM) # a folder it doesn't have: it has no CMakeLists.txt anyway
FetchContent_MakeAvailable(vterm)

set(vterm_sources)
foreach(name encoding keyboard mouse parser pen screen state unicode vterm)
	list(APPEND vterm_sources ${vterm_SOURCE_DIR}/src/${name}.c)
endforeach()
add_library(affrescoui_vterm STATIC ${vterm_sources})
target_include_directories(affrescoui_vterm SYSTEM PUBLIC ${vterm_SOURCE_DIR}/include)
set_target_properties(affrescoui_vterm PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
# Its warnings are not ours to fix
if(MSVC)
	set_source_files_properties(${vterm_sources} PROPERTIES COMPILE_OPTIONS "/w" SKIP_LINTING ON)
else()
	set_source_files_properties(${vterm_sources} PROPERTIES COMPILE_OPTIONS "-w" SKIP_LINTING ON)
endif()
set_target_properties(affrescoui_vterm PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
