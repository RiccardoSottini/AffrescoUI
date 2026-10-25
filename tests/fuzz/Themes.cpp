/// @file
/// @brief Installs the shipped kits and themes (modules/themes) before a fuzz target runs, as a program's entry point
/// does: each target is built with this file, so the documents it reads find the kits' components and tokens.

#include <affresco/themes/Themes.hpp>

namespace {

/// @brief Installs them when the target starts.
const bool installed = [] {
	affresco::themes::install();
	return true;
}();

} // namespace
