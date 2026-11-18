/// @file
/// @brief Unit tests for the build of a later version of a program in part (Program::rebuild()): only the definitions
/// that changed run, and those that use them, and the result is what a build in full gives. The first case is the
/// one that decides: every example, each changed in a few ways, built in part and in full.

#include "LispApp.hpp"

#include <affresco/affresco.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using affresco::json;
namespace lisp = affresco::lisp;

namespace {

/// @brief The limits a revision's build has (Document.hpp).
/// @return Them.
lisp::Limits buildLimits() {
	lisp::Limits limits;
	limits.budget.operations = lisp::maxBuildOperations;
	limits.budget.bytes = lisp::maxBuildBytes;
	return limits;
}

/// @brief What a build gave, as one value to compare: the tree and all a build says beside it.
/// @param program The program, built.
/// @param tree Its tree.
/// @return It.
json resultOf(const lisp::Program& program, const json& tree) {
	json result = {{"tree", tree}, {"undefined", json::array()}, {"truth", json::array()}};
	for(const bool truth : {false, true})
		for(const lisp::UndefinedName& name : truth ? program.truthTests() : program.undefinedNames())
			result[truth ? "truth" : "undefined"].push_back(
			    {name.name, name.file, name.line, name.column, name.definition});
	for(const lisp::Extension& extension : program.extensions())
		result["extensions"].push_back(
		    {extension.kind, extension.name, extension.targets, extension.doc, extension.author, extension.authors});
	result["used"] = program.namesUsed();
	for(const auto& [name, calls] : program.namedCalls()) result["named"][name] = {calls.texts, calls.otherwise};
	result["defaulted"] = program.defaulted();
	return result;
}

/// @brief Builds a section in full. @param section It. @return What it gave, or the error's message.
json buildInFull(const json& section) {
	try {
		lisp::Program program = lisp::programOf(section);
		const json tree = program.build(buildLimits());
		return resultOf(program, tree);
	} catch(const std::exception& error) {
		return {{"error", error.what()}};
	}
}

/// @brief Builds a section in part from an earlier build. @param previous The earlier build's record.
/// @param section The section. @param whole Set to whether it ran every definition after all.
/// @return What it gave, or the error's message.
json buildFrom(const lisp::BuildRecord& previous, const json& section, bool& whole) {
	try {
		lisp::Program program = lisp::programOf(section);
		const lisp::RebuildStatistics before = lisp::rebuildStatistics();
		const json tree = program.rebuild(previous, buildLimits());
		whole = lisp::rebuildStatistics().inPart == before.inPart;
		return resultOf(program, tree);
	} catch(const std::exception& error) {
		whole = true;
		return {{"error", error.what()}};
	}
}

/// @brief The `program` section of each example written as a program.
/// @return By folder name.
std::vector<std::pair<std::string, json>> examplePrograms() {
	std::vector<std::pair<std::string, json>> programs;
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	std::vector<std::filesystem::path> folders;
	for(const auto& folder : std::filesystem::directory_iterator(examples))
		if(std::filesystem::is_directory(folder.path() / "lisp")) folders.push_back(folder.path());
	std::ranges::sort(folders);
	for(const std::filesystem::path& folder : folders) {
		affresco::Screen app = affresco::Screen::fromApp(folder / "lisp", {}, affresco::Screen::History::InMemory);
		const json& document = app.versions().sourceDocument();
		if(document.contains("program")) programs.emplace_back(folder.filename().string(), document["program"]);
	}
	return programs;
}

/// @brief A change of a program: one entry's new text, or an entry gone.
struct Change {
	std::string what;                ///< What it is, for the report.
	std::string file;                ///< The entry's file.
	std::string name;                ///< The entry's name.
	std::optional<std::string> text; ///< Its new text, or std::nullopt to remove it.
};

/// @brief Replaces the first time a text has a part. @param text The text. @param part The part.
/// @param with What replaces it. @return The text, or std::nullopt if it hasn't the part.
std::optional<std::string> replaced(std::string text, std::string_view part, std::string_view with) {
	const std::size_t at = text.find(part);
	if(at == std::string::npos) return std::nullopt;
	return text.replace(at, part.size(), with);
}

/// @brief A few changes of each kind the brief names, of a program's entries: a function's body, a view's property,
/// a command's title, an added definition and a removed one.
/// @param section The section.
/// @return The changes.
std::vector<Change> changesOf(const json& section) {
	std::vector<Change> changes;
	std::map<std::string, int> counts;
	const auto add = [&](const std::string& what, const std::string& file, const std::string& name,
	                     std::optional<std::string> text) {
		if(counts[what]++ < 3) changes.push_back({what, file, name, std::move(text)});
	};
	std::string firstFile;
	for(const auto& [file, entries] : section.items())
		for(const auto& [name, value] : entries.items()) {
			const std::string text = value.get<std::string>();
			if(firstFile.empty()) firstFile = file;
			if(text.starts_with("(defn ")) {
				const std::size_t end = text.rfind(')');
				add("a function's body", file, name, text.substr(0, end) + " nil" + text.substr(end));
				if(counts["a removed function"] < 1 && name.find('.') == std::string::npos)
					add("a removed function", file, name, std::nullopt);
			}
			if(text.starts_with("(defview ") || text.starts_with("(defcomponent ") || text.starts_with("(defscreen "))
				if(auto changed = replaced(text, ":text \"", ":text \"Changed "))
					add("a view's property", file, name, std::move(changed));
			if(text.starts_with("(defcommand "))
				if(auto changed = replaced(text, ":title \"", ":title \"Changed "))
					add("a command's title", file, name, std::move(changed));
			if(text.starts_with("(defstate "))
				add("a state's value", file, name, text.substr(0, text.rfind(')')) + " :changed true)");
		}
	if(!firstFile.empty())
		add("an added definition", firstFile, "rebuild-added", "(defn rebuild-added \"Added.\" [x] (str x \"!\"))");
	return changes;
}

/// @brief A section with a change made. @param section The section. @param change The change. @return It.
json changed(json section, const Change& change) {
	if(change.text) section[change.file][change.name] = *change.text;
	else section[change.file].erase(change.name);
	return section;
}

/// @brief An interpreter that has loaded a program as Programs::load() loads it, purely, its values worked out.
/// @param program The program.
/// @return It, or nullptr if it can't be loaded.
std::unique_ptr<lisp::Interpreter> loadedInterpreter(const lisp::Program& program) {
	auto interpreter = std::make_unique<lisp::Interpreter>(buildLimits());
	interpreter->setPure(true);
	try {
		program.load(*interpreter, false);
		interpreter->workOutDefinitions();
	} catch(const std::exception&) {
		return nullptr;
	}
	interpreter->setPure(false);
	return interpreter;
}

/// @brief What an interpreter has defined, as one value to compare: each definition, in order, what it says of
/// itself, and the value of its name. @param interpreter It. @return It.
json definitionsOf(lisp::Interpreter& interpreter) {
	json definitions = json::array();
	for(const std::string& name : interpreter.definitionNames()) {
		const std::optional<lisp::DefinitionInfo> info = interpreter.describe(name);
		json definition = {name};
		if(info)
			definition.push_back(json::array(
			    {info->kind, info->doc, info->author, *info->place.file, info->place.line, info->place.column}));
		if(const std::optional<lisp::Value> value = interpreter.global(name)) {
			definition.push_back(interpreter.print(*value));
			definition.push_back(lisp::Interpreter::authorOf(*value));
		}
		definitions.push_back(std::move(definition));
	}
	return definitions;
}

/// @brief A section split from a program's text, its one file `app.lisp`. @param text The text. @return It.
json sectionOf(const std::string& text) { return lisp::sourceDocument(text, "app.lisp")["program"]; }

/// @brief The definitions an interpreter of a version has once changed in place into the next, or nullptr if
/// Program::redefine() declined or failed. @param before The earlier version, built. @param after The later.
/// @return Its definitions (definitionsOf()), or null.
json redefinedFrom(const lisp::Program& before, const json& after) {
	const std::unique_ptr<lisp::Interpreter> interpreter = loadedInterpreter(before);
	if(!interpreter) return nullptr;
	try {
		lisp::Program next = lisp::programOf(after);
		static_cast<void>(next.rebuild(*before.record(), buildLimits()));
		interpreter->setPure(true);
		if(!next.redefine(*interpreter, before, *next.record(), buildLimits())) return nullptr;
		interpreter->setPure(false);
	} catch(const std::exception&) {
		return nullptr;
	}
	return definitionsOf(*interpreter);
}

} // namespace

TEST_CASE("lisp rebuild: every example, changed, builds in part to what it builds in full") {
	std::size_t inPart = 0;
	std::size_t total = 0;
	for(const auto& [app, section] : examplePrograms()) {
		lisp::Program first = lisp::programOf(section);
		static_cast<void>(first.build(buildLimits()));
		REQUIRE(first.record());
		for(const Change& change : changesOf(section)) {
			INFO(app, ": ", change.what, " (", change.file, " ", change.name, ")");
			const json after = changed(section, change);
			bool whole = false;
			const json part = buildFrom(*first.record(), after, whole);
			CHECK(part == buildInFull(after));
			++total;
			if(!whole) ++inPart;
		}
	}
	MESSAGE(inPart, " of ", total, " changes built in part");
	CHECK(inPart * 2 > total); // most changes are of one definition and those that use it
}

TEST_CASE("lisp rebuild: an interpreter changed in place defines what a new one loading the later version defines") {
	std::size_t inPlace = 0;
	std::size_t total = 0;
	for(const auto& [app, section] : examplePrograms()) {
		lisp::Program first = lisp::programOf(section);
		static_cast<void>(first.build(buildLimits()));
		for(const Change& change : changesOf(section)) {
			INFO(app, ": ", change.what, " (", change.file, " ", change.name, ")");
			const json after = changed(section, change);
			lisp::Program fresh = lisp::programOf(after);
			const std::unique_ptr<lisp::Interpreter> loaded = loadedInterpreter(fresh);
			const json patched = redefinedFrom(first, after);
			++total;
			if(patched.is_null() || !loaded) continue; // declined: Programs loads a new interpreter
			CHECK(patched == definitionsOf(*loaded));
			++inPlace;
		}
	}
	MESSAGE(inPlace, " of ", total, " changes made in place");
	CHECK(inPlace * 2 > total);
}

namespace {

/// @brief A program of many functions that nothing else uses, and a screen.
/// @param count How many.
/// @param changed The function whose body is other, or -1.
/// @return Its section.
json manyFunctions(int count, int changed) {
	std::string text;
	for(int i = 0; i < count; ++i)
		text += "(defn f" + std::to_string(i) + " \"\" [] " + std::to_string(i == changed ? -1 : i) + ")\n";
	return sectionOf(text + "(defscreen main (column#c (label#l :text \"x\")))\n");
}

} // namespace

TEST_CASE("lisp rebuild: one of 200 functions changed compiles that one, and redefines that one") {
	lisp::Program first = lisp::programOf(manyFunctions(200, -1));
	static_cast<void>(first.build(buildLimits()));
	const json after = manyFunctions(200, 117);
	const lisp::RebuildStatistics before = lisp::rebuildStatistics();
	bool whole = true;
	const json part = buildFrom(*first.record(), after, whole);
	CHECK(lisp::rebuildStatistics().compiled - before.compiled == 1);
	CHECK_FALSE(whole);
	CHECK(part == buildInFull(after));
	const lisp::RebuildStatistics patching = lisp::rebuildStatistics();
	const json patched = redefinedFrom(first, after);
	REQUIRE_FALSE(patched.is_null());
	CHECK(lisp::rebuildStatistics().redefined - patching.redefined == 1);
	CHECK(lisp::rebuildStatistics().definedAgain - patching.definedAgain == 1);
	CHECK(patched == definitionsOf(*loadedInterpreter(lisp::programOf(after))));
}

TEST_CASE("lisp rebuild: a function a view calls while it builds, changed, builds the view again") {
	const auto program = [](const std::string& word) {
		return sectionOf("(defn word \"\" [] \"" + word +
		                 "\")\n(defn other \"\" [] 1)\n"
		                 "(defview caption \"\" [] `(label#cap :text ~(word)))\n"
		                 "(defscreen main (column#c ~(caption) (label#l :text \"x\")))\n");
	};
	const std::shared_ptr<const lisp::BuiltProgram> first = lisp::builtProgram(program("one"));
	const std::size_t inPart = lisp::rebuildStatistics().inPart;
	const std::shared_ptr<const lisp::BuiltProgram> second = lisp::builtProgram(program("two"));
	CHECK(lisp::rebuildStatistics().inPart == inPart + 1);
	CHECK(second->tree.dump().contains("\"two\""));
	CHECK(resultOf(lisp::programOf(program("two")), second->tree)["tree"] == buildInFull(program("two"))["tree"]);
	const std::vector<std::string> places = lisp::changedPlaces(*first, *second);
	CHECK(places == std::vector<std::string>{"/root"});
	CHECK(lisp::changedPlaces(*second, *lisp::builtProgram(program("two"))).empty());
}

TEST_CASE("lisp rebuild: a function a view's expression calls by name, gone, builds the view again") {
	const json section = sectionOf("(defstate query \"\")\n(defn shout \"\" [x] (str x \"!\"))\n(defn other \"\" [] 1)\n"
	                               "(defview caption \"\" [] `(label#cap :text ~(shout state.query)))\n"
	                               "(defscreen main (column#c ~(caption)))\n");
	lisp::Program first = lisp::programOf(section);
	const json tree = first.build(buildLimits());
	REQUIRE(tree.dump().contains("lisp('shout'"));
	json after = section;
	after["app.lisp"].erase("shout");
	bool whole = false;
	const json part = buildFrom(*first.record(), after, whole);
	CHECK(part == buildInFull(after));
	CHECK(!part.dump().contains("lisp('shout'"));
	CHECK(!whole);
}

TEST_CASE("lisp rebuild: a macro changed builds everything, and the interpreter isn't changed in place") {
	const auto program = [](const std::string& body) {
		return sectionOf("(defmacro twice \"\" [x] " + body +
		                 ")\n(defn f \"\" [] (twice 1))\n"
		                 "(defscreen main (column#c (label#l :text \"x\")))\n");
	};
	lisp::Program first = lisp::programOf(program("`(+ ~x ~x)"));
	static_cast<void>(first.build(buildLimits()));
	const json after = program("`(* ~x 2)");
	bool whole = false;
	CHECK(buildFrom(*first.record(), after, whole) == buildInFull(after));
	CHECK(whole);
	CHECK(redefinedFrom(first, after).is_null());
}

TEST_CASE("lisp rebuild: a definition that doesn't compile is refused as a build in full refuses it") {
	const json section = sectionOf("(defn f \"\" [] 1)\n(defn g \"\" [] (f))\n"
	                               "(defscreen main (column#c (label#l :text \"x\")))\n");
	lisp::Program first = lisp::programOf(section);
	static_cast<void>(first.build(buildLimits()));
	json after = section;
	after["app.lisp"]["f"] = "(defn f \"\" [] (let [x] x))";
	bool whole = false;
	const json part = buildFrom(*first.record(), after, whole);
	REQUIRE(part.contains("error"));
	CHECK(part == buildInFull(after));
}

TEST_CASE("lisp rebuild: a revision of a running program redefines what it changed, by its author, the rest kept") {
	lispapp::App app(R"lisp(
(defn reach "The app's, with a control step." [] (call "hit"))
(defn other "" [] 1)
(defcommand c "" :title "C" [] (reach))
(defkey "ctrl+c" c)
(defscreen main (panel#root))
)lisp");
	const lisp::RebuildStatistics before = lisp::rebuildStatistics();
	app.write("other", R"lisp((defn other "An agent's." [] 2))lisp");
	CHECK(lisp::rebuildStatistics().redefined - before.redefined == 1);
	app.press(affresco::Key::C);
	CHECK(app.hits == 1); // reach is still the app's
	app.write("reach", R"lisp((defn reach "An agent's." [] (call "hit") 2))lisp");
	CHECK(lisp::rebuildStatistics().redefined - before.redefined == 2);
	app.errors.clear();
	app.press(affresco::Key::C);
	CHECK(app.hits == 1); // and now the agent's, who can't reach the host
	CHECK(app.reported("agent/x"));

	// Refused as a new interpreter refuses it, and the program running is untouched
	const std::optional<std::string> error =
	    app.repository.commit({{"author", "agent/x"},
		                       {"message", "m"},
		                       {"set", {{"/program/app.lisp/other", "(defn other \"\" [] (let [x] x))"}}}});
	CHECK(error.has_value());
	app.write("reach", R"lisp((defn reach "The person's." [] (call "hit")))lisp", "user");
	app.press(affresco::Key::C);
	CHECK(app.hits == 2);
}

TEST_CASE("lisp rebuild: a function running while its program changes finishes with the code it started with") {
	lispapp::App app(R"lisp(
(defn value "" [] 1)
(defcommand c "" :title "C" [] (call "hit") (set "state.got" (value)))
(defkey "ctrl+c" c)
(defscreen main :state {:got 0} (panel#root))
)lisp");
	bool once = false;
	app.runtime->onCall("hit", [&](const json&) {
		if(!std::exchange(once, true)) app.write("value", R"lisp((defn value "" [] 2))lisp", "app");
		return json();
	});
	const lisp::RebuildStatistics before = lisp::rebuildStatistics();
	app.press(affresco::Key::C);
	app.settle();
	CHECK(app.errors.empty());
	CHECK(app.state("got") == 1);
	CHECK(lisp::rebuildStatistics().redefined == before.redefined); // a new interpreter: the old one was running
	app.press(affresco::Key::C);
	CHECK(app.state("got") == 2);
}

