/// @file
/// @brief `affrescoui check`: a screen against the schema, the format's rules and the checker, without a window.

#include "Cli.hpp"
#include "Schema.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>

#include <algorithm>
#include <array>
#include <iostream>

namespace affresco::cli {

/// @brief The screen format's JSON Schema, built into the tool (schema/affrescoui-screen.schema.json; generated).
/// @return Its text.
std::string_view embeddedSchema();

namespace {

/// @brief One problem the check found.
struct Problem {
	std::string source;  ///< "json", "schema", "format" or "checker".
	std::string place;   ///< Where, if it is known apart from the message.
	std::string message; ///< What.
};

/// @brief Loads a screen as the library does, keeping its log quiet: the checker's warnings on load are reported by
/// the check itself.
/// @param text The file.
/// @param directory Where its relative paths ("include", fonts) are found.
/// @param problems Receives the error if it doesn't load.
/// @return The document and the state as loaded ("include"s in place), or std::nullopt.
std::optional<std::pair<json, json>> load(const std::string& text, const std::filesystem::path& directory,
                                          std::vector<Problem>& problems) {
	setLogSink([](LogLevel, std::string_view) {});
	try {
		const Screen screen = Screen::fromString(text, directory);
		setLogSink({});
		return std::pair(json::parse(screen.documentJson()), json::parse(screen.stateJson()));
	} catch(const DocumentError& error) {
		setLogSink({});
		problems.push_back({"format", "", error.what()});
	}
	return std::nullopt;
}

/// @brief Checks a screen file.
/// @param path The file.
/// @return What it found.
std::vector<Problem> problemsOf(const std::filesystem::path& path) {
	const std::string text = readFile(path);
	std::vector<Problem> problems;
	json document;
	try {
		document = parseUntrusted(text);
	} catch(const std::exception& error) {
		problems.push_back({"json", "", error.what()});
		return problems;
	}

	Schema schema(json::parse(embeddedSchema()));
	for(SchemaProblem& problem : schema.check(document))
		problems.push_back({"schema", std::move(problem.place), std::move(problem.problem)});

	const auto loaded = load(text, path.parent_path(), problems);
	const format::Rules rules;
	const json& checked = loaded ? loaded->first : document;
	for(std::string& finding :
	    rules.findings(checked, loaded ? loaded->second : document.value("state", json::object())))
		problems.push_back({"checker", "", std::move(finding)});
	return problems;
}

} // namespace

Exit checkScreen(std::span<const std::string_view> args) {
	if(std::ranges::contains(args, std::string_view("--compose"))) return checkComposition(args, registerApplicationTypes);
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	const Arguments arguments(args, {}, flags);
	const std::filesystem::path file = arguments.file();
	const std::vector<Problem> problems = problemsOf(file);

	if(arguments.flag("--json")) {
		json list = json::array();
		for(const Problem& problem : problems)
			list.push_back({{"source", problem.source}, {"place", problem.place}, {"message", problem.message}});
		std::cout << json{{"file", file.string()}, {"ok", problems.empty()}, {"problems", list}}.dump(1, '\t') << '\n';
	} else {
		for(const Problem& problem : problems)
			std::cout << file.string() << ": " << problem.source << ": "
			          << (problem.place.empty() ? "" : problem.place + ": ") << problem.message << '\n';
		if(problems.empty()) std::cout << file.string() << ": no problems\n";
	}
	return problems.empty() ? Exit::Success : Exit::Problems;
}

} // namespace affresco::cli
