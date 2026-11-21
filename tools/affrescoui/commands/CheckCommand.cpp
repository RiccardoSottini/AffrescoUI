/// @file
/// @brief `affrescoui check`: a screen against the schema, the format's rules and the checker, then built in a headless
/// window (no display, no OpenGL) as a host shows it; an app's folder or app.json, its manifest and the capabilities
/// its screen uses too.

#include "Cli.hpp"
#include "Schema.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/App.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/notation/Notation.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <iostream>

namespace affresco::cli {

/// @brief The screen format's JSON Schema, built into the tool (schema/affrescoui-screen.schema.json; generated).
/// @return Its text.
std::string_view embeddedSchema();

namespace {

/// @brief One problem the check found.
struct Problem {
	std::string source;  ///< "json" (or "lisp", the file's notation), "schema", "format" or "checker".
	std::string place;   ///< Where, if it is known apart from the message.
	std::string message; ///< What.
};

/// @brief Builds a loaded screen as a host shows it, in a headless window: loading alone doesn't apply the nodes'
/// properties, so what only building rejects (a property a node's type doesn't have, a layout property on the root) is
/// a problem too, and so are the errors building logs (an expression that fails), but for a service provider a host
/// registers itself.
/// @param screen The screen, loaded.
/// @param problems Receives what building rejected or logged as an error.
void built(Screen screen, std::vector<Problem>& problems) {
	std::vector<std::string> errors;
	setLogSink([&errors](LogLevel level, std::string_view line) {
		// A host registers service providers of its own, which this tool hasn't: those are the host's to report
		if(level != LogLevel::Error || line.contains("no provider '")) return;
		if(std::ranges::find(errors, line) == errors.end()) errors.emplace_back(line);
	});
	try {
		automation::buildHeadless(std::move(screen));
	} catch(const std::exception& error) {
		errors.insert(errors.begin(), error.what());
	}
	setLogSink({});
	for(std::string& error : errors) problems.push_back({"format", "", std::move(error)});
}

/// @brief Reports a root typed as a container: the root is the window's own panel whatever its type, so a column's
/// arrangement silently wouldn't happen.
/// @param document The document, as loaded.
/// @param problems Receives the finding, if there is one.
void containerRoot(const json& document, std::vector<Problem>& problems) {
	const auto root = document.find("root");
	if(root == document.end() || !root->is_object() || !root->contains("type") || !(*root)["type"].is_string()) return;
	const std::string& type = (*root)["type"].get_ref<const std::string&>();
	if(type == "row" || type == "column" || type == "stack" || type == "grid")
		problems.push_back({"checker", "root",
		                    "a " + type + " root doesn't arrange its children: the root is a panel that always fills " +
		                        "the window; put the " + type + " inside it, with \"fill\": true"});
}

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
		Screen screen = Screen::fromString(text, directory);
		std::pair loaded(json::parse(screen.documentJson()), json::parse(screen.stateJson()));
		setLogSink({});
		built(std::move(screen), problems);
		return loaded;
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
	const notation::Notation spelling = notation::notationOf(path);
	std::vector<Problem> problems;
	json document;
	bool whole = true; // a program that includes its other files builds only with them: it is judged as loaded
	try {
		document = lisp::readDocument(text, path);
		whole = !lisp::hasProgram(document) || !document.contains("include");
		if(lisp::hasProgram(document) && whole) document = lisp::buildDocument(document); // what it builds is judged
	} catch(const std::exception& error) {
		problems.push_back({std::string(notation::nameOf(spelling)), "", error.what()});
		return problems;
	}

	Schema schema(json::parse(embeddedSchema()));
	// A program that includes its other files builds only with them, and they are included parts, which the schema
	// doesn't check in JSON either: it is judged as loaded, by the format's rules and the checker
	if(whole)
		for(SchemaProblem& problem : schema.check(document))
			problems.push_back({"schema", std::move(problem.place), std::move(problem.problem)});
	const auto loaded = load(lisp::jsonText(text, path), path.parent_path(), problems);

	const format::Rules rules;
	const json& checked = loaded ? loaded->first : document;
	for(std::string& finding :
	    rules.findings(checked, loaded ? loaded->second : document.value("state", json::object())))
		problems.push_back({"checker", "", std::move(finding)});
	containerRoot(checked, problems);
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
		app = format::parseApp(notation::readDocument(readFile(manifest), manifest));
	} catch(const std::exception& error) {
		problems.push_back({"format", "", error.what()});
		return problems;
	}
	const std::filesystem::path entry = manifest.parent_path() / app.entry;
	try {
		Schema schema(json::parse(embeddedSchema()));
		json screen = lisp::readDocument(readFile(entry), entry);
		// A program that includes its other files builds only with them: it is judged as loaded, as an include is
		const bool whole = !lisp::hasProgram(screen) || !screen.contains("include");
		if(lisp::hasProgram(screen) && whole) screen = lisp::buildDocument(screen);
		if(whole)
			for(SchemaProblem& problem : schema.check(screen))
				problems.push_back({"schema", app.entry + ": " + problem.place, std::move(problem.problem)});
	} catch(const std::exception& error) {
		problems.push_back({"json", app.entry, error.what()});
		return problems;
	}
	setLogSink([](LogLevel, std::string_view) {});
	try {
		Screen screen = Screen::fromApp(manifest, {}, Screen::History::InMemory);
		setLogSink({});
		const json document = json::parse(screen.documentJson());
		for(std::string& finding : format::Rules().findings(document, json::parse(screen.stateJson())))
			problems.push_back({"checker", app.entry, std::move(finding)});
		containerRoot(document, problems);
		for(const std::string& key : format::undeclaredCapabilities(app, document))
			problems.push_back({"capabilities", app.entry,
			                    "the screen uses " + key + ", which app.json doesn't declare: it is never granted"});
		built(std::move(screen), problems);
	} catch(const DocumentError& error) {
		setLogSink({});
		problems.push_back({"format", "", error.what()});
	}
	return problems;
}

/// @brief Where a program's functions test a path of the running app with the language's truth, where "", 0 and []
/// are true (lisp::Program::truthTests()): notes for a person to look at, not problems.
/// @param file The screen, or an app's folder or manifest.
/// @param manifest The app's manifest, or "".
/// @return One line each: the place, the definition and the path.
std::vector<std::string> truthNotes(const std::filesystem::path& file, const std::filesystem::path& manifest) {
	setLogSink([](LogLevel, std::string_view) {});
	std::vector<std::string> notes;
	try {
		const Screen screen = manifest.empty()
		                          ? Screen::fromString(lisp::jsonText(readFile(file), file), file.parent_path())
		                          : Screen::fromApp(manifest, {}, Screen::History::InMemory);
		const json document = json::parse(screen.documentJson());
		if(lisp::hasProgram(document))
			for(const lisp::UndefinedName& test : lisp::builtProgram(lisp::effectiveProgram(document))->truth) // features' too
				notes.push_back(
				    test.name == "+"
				        ? std::format("{}: in {}, line {}: + is given a text: an expression joins texts with it, a "
				                      "function can't: write (str ...)",
				                      test.file, test.definition, test.line)
				        : std::format("{}: in {}, line {}: {} is tested with Lisp's truth (\"\", 0 and [] are "
				                            "true): if it can be one, write (truthy? {}), (some? ...) or "
				                            "(not (empty? ...))",
				                            test.file, test.definition, test.line, test.name, test.name));
	} catch(const std::exception& error) {
		notes.push_back(std::string("can't load it: ") + error.what());
	}
	setLogSink({});
	return notes;
}

/// @brief Whether a file is an app's manifest, a folder an app's, or an app's entry screen (checked as the app, as
/// the host opens it).
/// @param path The file or folder.
/// @return The manifest, or "" if it isn't one.
std::filesystem::path manifestOf(const std::filesystem::path& path) {
	if(std::filesystem::is_directory(path)) return notation::namedDocument(path, "app");
	if(notation::isNamedDocument(path, "app")) return path;
	try {
		return Screen::appOf(path);
	} catch(const DocumentError&) {
		return notation::namedDocument(path.parent_path(), "app"); // the app's check says what is wrong with it
	}
}

} // namespace

Exit checkScreen(std::span<const std::string_view> args) {
	if(std::ranges::contains(args, std::string_view("--compose")))
		return checkComposition(args, registerApplicationTypes);
	static constexpr std::array<std::string_view, 2> flags = {"--json", "--truth"};
	const Arguments arguments(args, {}, flags);
	const std::filesystem::path file = arguments.file();
	const std::filesystem::path manifest = manifestOf(file);
	if(arguments.flag("--truth")) { // notes, not problems: what a person decides
		const std::vector<std::string> notes = truthNotes(file, manifest);
		for(const std::string& note : notes) std::cout << note << '\n';
		if(notes.empty()) std::cout << file.string() << ": no function tests a path with Lisp's truth\n";
		return Exit::Success;
	}
	std::vector<Problem> problems = manifest.empty() ? problemsOf(file) : appProblems(manifest);
	if(problems.empty() && !std::filesystem::is_directory(file) && !notation::isNamedDocument(file, "app"))
		for(std::string& problem : requiredStringsProblems(file))
			problems.push_back({"strings", "", std::move(problem)});

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
