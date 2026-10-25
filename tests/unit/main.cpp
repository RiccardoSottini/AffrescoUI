/// @file
/// @brief The unit tests' entry point: doctest's main, which runs every TEST_CASE in tests/unit, with the shipped kits
/// and themes installed (modules/themes), as a program's entry point installs them.
///
/// Usage: affrescoui_unit_tests [doctest options], e.g. `--test-case="vcs*"` or `--list-test-cases`.

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <affresco/themes/Themes.hpp>

/// @brief Installs the kits and themes, then runs the tests.
/// @param argc The argument count.
/// @param argv The arguments (doctest's options).
/// @return doctest's exit status.
int main(int argc, char** argv) {
	affresco::themes::install();
	return doctest::Context(argc, argv).run();
}
