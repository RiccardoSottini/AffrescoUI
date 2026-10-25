/// @file
/// @brief shellLine()'s line, run by a POSIX shell: every word arrives as it was, whatever it holds, and a line break
/// or carriage return is refused; and Forge types the debug adapter's line into `sh`, never the person's login shell
/// (which may be fish, csh or cmd.exe, where single quotes don't protect a word).

#include "Expression.hpp"
#include "process/Process.hpp" // modules/services/src

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

using affresco::json;

namespace {

/// @brief shellLine() of some words and variables, evaluated as a screen would.
/// @param words The words.
/// @param variables The variables, or null.
/// @return The line, or null.
json shellLineOf(const json& words, const json& variables = nullptr) {
	const json state = {{"words", words}, {"variables", variables}}, none, revision = 0;
	const affresco::format::Scope locals{nullptr, nullptr, {}, {}, {}};
	const affresco::format::Context context{state, none, revision, none, locals, none};
	return affresco::format::evaluate("{{shellLine(state.words, state.variables)}}", context, "test");
}

/// @brief Runs a line with `/bin/sh -c` and reads what it prints.
/// @param line The line.
/// @return Its output.
std::string runLine(const std::string& line) {
	const auto child = affresco::services::ChildProcess::start({"/bin/sh", {"-c", line}, {}, std::nullopt});
	child->closeInput();
	std::string output;
	char buffer[4096];
	for(std::ptrdiff_t got = 0; (got = child->read(buffer, sizeof buffer)) > 0;)
		output.append(buffer, static_cast<std::size_t>(got));
	CHECK(child->wait() == 0);
	return output;
}

} // namespace

#ifndef _WIN32
TEST_CASE("shellLine: a POSIX shell reads back every word as it was") {
	const std::vector<std::string> words = {
	    "plain", "a b", "it's", "'", "''", "\\", "a\\'b", "$HOME", "`id`", "$(id)", "!!", "!x", "*", "~", "#c",
	    ";", "&& x", "|", "\"q\"", "é ✓", "-n", "%d", ""};
	json line = {"printf", "[%s]"};
	std::string expected;
	for(const std::string& word : words) {
		line.push_back(word);
		expected += "[" + word + "]";
	}
	const json quoted = shellLineOf(line);
	REQUIRE(quoted.is_string());
	CHECK(runLine(quoted.get<std::string>()) == expected);

	const json withVariable = shellLineOf({"sh", "-c", "printf '[%s]' \"$X\""}, {{"X", "a'b $y `z`"}});
	REQUIRE(withVariable.is_string());
	CHECK(runLine(withVariable.get<std::string>()) == "[a'b $y `z`]");
}
#endif

TEST_CASE("shellLine: a word or value with a line break, a carriage return or another control character is refused") {
	for(const char* word : {"a\nb", "a\rb", "a\r\nb", "\tx", "x\x1b[2J", "x\x7f"}) {
		CAPTURE(word);
		CHECK(shellLineOf({"echo", word}).is_null());
		CHECK(shellLineOf({"echo"}, {{"X", word}}).is_null());
	}
}

TEST_CASE("shellLine: Forge types the debug adapter's line into the program `sh`, which it declares") {
	const std::filesystem::path forge = std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "forge";
	const json debug = json::parse(std::ifstream(forge / "features" / "debug" / "debug.json"));
	const json app = json::parse(std::ifstream(forge / "app.json"));
	CHECK(debug["services"]["debugTerminal"]["process"] == "sh");
	CHECK(app["capabilities"]["programs"].contains("sh"));
}
