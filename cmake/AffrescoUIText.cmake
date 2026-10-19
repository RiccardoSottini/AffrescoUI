# The text dependencies of the render module, included by CMakeLists.txt: FreeType rasterises glyphs, HarfBuzz shapes
# text (decision D6). Both are downloaded as release archives pinned by their hashes, like the other dependencies, and
# built from source with nothing else: no zlib, libpng, bzip2 or brotli for FreeType (WOFF and PNG colour glyphs
# aren't read), and no FreeType, ICU or GLib for HarfBuzz (its own font functions and Unicode tables are used).

# FreeType. The second address is a mirror of the same archive.
set(AFFRESCOUI_FREETYPE_VERSION 2.14.3)
foreach(optional ZLIB BZIP2 PNG HARFBUZZ BROTLI)
	set(FT_DISABLE_${optional} ON CACHE BOOL "" FORCE)
endforeach()
# Installed with AffrescoUI, since the static render library leaves it to the application's link (like GLFW)
if(NOT AFFRESCOUI_INSTALL)
	set(SKIP_INSTALL_ALL ON) # read by FreeType's CMakeLists.txt only
endif()
FetchContent_Declare(freetype
	URL https://download.savannah.gnu.org/releases/freetype/freetype-${AFFRESCOUI_FREETYPE_VERSION}.tar.xz
	    https://sourceforge.net/projects/freetype/files/freetype2/${AFFRESCOUI_FREETYPE_VERSION}/freetype-${AFFRESCOUI_FREETYPE_VERSION}.tar.xz/download
	URL_HASH SHA256=36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f SYSTEM)
FetchContent_MakeAvailable(freetype)
unset(SKIP_INSTALL_ALL)
set_target_properties(freetype PROPERTIES COMPILE_WARNING_AS_ERROR OFF) # its warnings are not ours to fix

# HarfBuzz, built as its single amalgamated source file instead of through its own build: one target, no options
set(AFFRESCOUI_HARFBUZZ_VERSION 12.3.2)
FetchContent_Declare(harfbuzz
	URL https://github.com/harfbuzz/harfbuzz/releases/download/${AFFRESCOUI_HARFBUZZ_VERSION}/harfbuzz-${AFFRESCOUI_HARFBUZZ_VERSION}.tar.xz
	URL_HASH SHA256=6f6db164359a2da5a84ef826615b448b33e6306067ad829d85d5b0bf936f1bb8
	SOURCE_SUBDIR affrescoui-uses-the-amalgamated-source SYSTEM) # a folder it doesn't have: its CMakeLists.txt isn't run
FetchContent_MakeAvailable(harfbuzz)
add_library(affrescoui_harfbuzz STATIC ${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
target_include_directories(affrescoui_harfbuzz SYSTEM PUBLIC $<BUILD_INTERFACE:${harfbuzz_SOURCE_DIR}/src>)
set_target_properties(affrescoui_harfbuzz PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR OFF
	EXPORT_NAME harfbuzz)
if(MSVC)
	target_compile_options(affrescoui_harfbuzz PRIVATE /bigobj) # one source file with every part of HarfBuzz
endif()
