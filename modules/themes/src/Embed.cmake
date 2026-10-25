# Writes the themes' JSON files into a C++ source, as bytes (cmake -P, from CMakeLists.txt at build time):
#   -DBASE=<folder the paths are relative to> -DOUTPUT=<the .cpp> -DFILES=<path;path;...>
# Bytes rather than string literals: no length limit per literal and no escaping, on every compiler.
set(sources "")
set(entries "")
set(index 0)
foreach(path IN LISTS FILES)
	file(READ "${BASE}/${path}" bytes HEX)
	string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
	string(APPEND sources "constexpr unsigned char file${index}[] = {${bytes}};\n")
	string(APPEND entries "\tEmbeddedFile{\"${path}\", file${index}},\n")
	math(EXPR index "${index} + 1")
endforeach()
file(CONFIGURE OUTPUT "${OUTPUT}.new" @ONLY CONTENT "/// @file
/// @brief The themes' JSON files, as bytes (made by Embed.cmake at build time: edit the JSON files, not this).

#include \"Embedded.hpp\"

#include <array>

namespace affresco::themes {

namespace {

@sources@
/// @brief Every file, in the order CMakeLists.txt lists them.
constexpr std::array files{
@entries@};

} // namespace

std::span<const EmbeddedFile> embeddedFiles() noexcept { return files; }

} // namespace affresco::themes
")
# Only a changed text touches the source, so a build that changed no file compiles nothing
file(COPY_FILE "${OUTPUT}.new" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.new")
