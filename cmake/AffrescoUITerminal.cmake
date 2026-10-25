# The VT terminal emulator of the format's `terminal` node (modules/format/src/terminal), included by the root
# CMakeLists.txt: libvterm, downloaded as a release archive pinned by its hash, its sources compiled into the format
# module (its own Makefile isn't run). Its generated tables (src/encoding/*.inc, src/fullwidth.inc) ship in the archive.
set(AFFRESCOUI_VTERM_VERSION 0.3.3)
FetchContent_Declare(vterm
	URL https://launchpad.net/libvterm/trunk/v0.3/+download/libvterm-${AFFRESCOUI_VTERM_VERSION}.tar.gz
	URL_HASH SHA256=09156f43dd2128bd347cbeebe50d9a571d32c64e0cf18d211197946aff7226e0
	SOURCE_SUBDIR affrescoui-compiles-its-sources SYSTEM) # a folder it doesn't have: it has no CMakeLists.txt anyway
FetchContent_MakeAvailable(vterm)

set(AFFRESCOUI_VTERM_DIR ${vterm_SOURCE_DIR})
set(AFFRESCOUI_VTERM_SOURCES)
foreach(name encoding keyboard mouse parser pen screen state unicode vterm)
	list(APPEND AFFRESCOUI_VTERM_SOURCES ${vterm_SOURCE_DIR}/src/${name}.c)
endforeach()
# Its warnings are not ours to fix
set_source_files_properties(${AFFRESCOUI_VTERM_SOURCES} PROPERTIES COMPILE_OPTIONS $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>
	SKIP_LINTING ON)
