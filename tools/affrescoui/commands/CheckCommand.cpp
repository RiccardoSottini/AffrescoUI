/// @file
/// @brief `affrescoui check`: a screen against the schema, the format's rules and the checker, without a window; an
/// app's folder or app.json, its manifest and the capabilities its screen uses too.

#include "Cli.hpp"
#include "Schema.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/App.hpp>
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

/// @brief Checks an app (SCREEN_FORMAT.md, "Apps: app.json"): its manifest, its entry screen against the schema, the
/// app loaded as the host loads it (themes, models and seeds merged in, nothing kept on disk), the checker, and every
/// capability the screen uses that the manifest doesn't declare.
/// @param manifest The app.json file.
/// @return What it found.
std::vector<Problem> appProblems(const std::filesystem::path& manifest) {
	std::vector<Problem> problems;
	format::AppManifest app;
	try {
		app = format::parseApp(parseUntrusted(readFile(manifest)));
	} catch(const std::exception& error) {
		problems.push_back({"format", "", error.what()});
		return problems;
	}
	const std::filesystem::path entry = manifest.parent_path() / app.entry;
	try {
		Schema schema(json::parse(embeddedSchema()));
		for(SchemaProblem& problem : schema.check(parseUntrusted(readFile(entry))))
			problems.push_back({"schema", app.entry + ": " + problem.place, std::move(problem.problem)});
	} catch(const std::exception& error) {
		problems.push_back({"json", app.entry, error.what()});
		return problems;
	}
	setLogSink([](LogLevel, std::string_view) {});
	try {
		const Screen screen = Screen::fromApp(manifest, {}, Screen::History::InMemory);
		setLogSink({});
		const json document = json::parse(screen.documentJson());
		for(std::string& finding : format::Rules().findings(document, json::parse(screen.stateJson())))
			problems.push_back({"checker", app.entry, std::move(finding)});
		for(const std::string& key : format::undeclaredCapabilities(app, document))
			problems.push_back({"capabilities", app.entry,
			                    "the screen uses " + key + ", which app.json doesn't declare: it is never granted"});
	} catch(const DocumentError& error) {
		setLogSink({});
		problems.push_back({"format", "", error.what()});
	}
	return problems;
}

/// @brief Whether a file is an app's manifest, a folder an app's, or an app's entry screen (checked as the app, as
/// the host opens it).
/// @param path The file or folder.
/// @return The manifest, or "" if it isn't one.
std::filesystem::path manifestOf(const std::filesystem::path& path) {
	if(std::filesystem::is_directory(path)) return path / "app.json";
	if(path.filename() == "app.json") return path;
	try {
		return Screen::appOf(path);
	} catch(const DocumentError&) {
		return path.parent_path() / "app.json"; // the app's check says what is wrong with it
	}
}

} // namespace

Exit checkScreen(std::span<const std::string_view> args) {
	if(std::ranges::contains(args, std::string_view("--compose")))
		return checkComposition(args, registerApplicationTypes);
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	const Arguments arguments(args, {}, flags);
	const std::filesystem::path file = arguments.file();
	const std::filesystem::path manifest = manifestOf(file);
	const std::vector<Problem> problems = manifest.empty() ? problemsOf(file) : appProblems(manifest);

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
