/// @file
/// @brief Sinopia's screen (examples/sinopia): it listens for an agent, so its permissions send an agent's changes to
/// review and let only the Studio review them.

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <fstream>
#include <string>

using affresco::json;

namespace {

/// @brief Sinopia's screen as a repository holds it.
/// @return It.
json sinopiaScreen() {
	std::ifstream file(std::string(AFFRESCOUI_EXAMPLES_DIRECTORY) + "/sinopia/index.screen.json");
	REQUIRE(file);
	json screen = json::parse(file);
	screen.erase("include"); // a Repository does not merge included files; the permissions are in this one
	return screen;
}

/// @brief A revision that switches the theme.
/// @param author Its author.
/// @return It.
json switchTheme(const std::string& author) {
	return {{"author", author},
	        {"patch", json::array({{{"op", "replace"}, {"path", "/state/theme"}, {"value", "dark"}}})}};
}

} // namespace

TEST_CASE("sinopia: an agent's changes wait for the Studio's review, and the agent can't review them") {
	const affresco::format::Rules rules;
	const affresco::vcs::Repository repository(rules, sinopiaScreen());
	CHECK(repository.judge(switchTheme("studio")).decision == "allow");
	CHECK(repository.judge(switchTheme("agent/claude-code")).decision == "ask");
	CHECK(repository.judge(switchTheme("human/ann")).decision == "ask");
	CHECK(repository.mayReview("studio"));
	CHECK_FALSE(repository.mayReview("agent/claude-code"));
	CHECK_FALSE(repository.mayReview("human/ann"));
	// Nor can an agent's revision take the permissions away
	const json drop = {{"author", "agent/claude-code"},
	                   {"patch", json::array({{{"op", "remove"}, {"path", "/permissions"}}})}};
	CHECK(repository.judge(drop).decision == "deny");
}
