/// @file
/// @brief Each example holds its app twice, in a json and a lisp folder: both load as exactly the same document (a
/// program, written by hand, as the same app), and the lisp folder's app is in the Lisp notation.

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>
#include <affresco/notation/Notation.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

using affresco::Screen;

namespace {

/// @brief A value without its members named "title", at any depth.
/// @param value The value.
/// @return It, untitled.
affresco::json untitled(affresco::json value) {
	if(value.is_object()) {
		value.erase("title");
		for(auto& [key, member] : value.items()) member = untitled(member);
	} else if(value.is_array()) {
		for(auto& item : value) item = untitled(item);
	}
	return value;
}

} // namespace

TEST_CASE("lisp examples: every example is there in both notations, and loads as the same document") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int apps = 0;
	int programs = 0; // apps written as programs (RFC 0005): their lisp folder is a source of its own, beside the JSON
	for(const auto& entry : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_regular_file(entry.path() / "lisp" / "app.lisp")) continue;
		CAPTURE(entry.path().filename().string());
		Screen written = Screen::fromApp(entry.path() / "lisp", {}, Screen::History::InMemory);
		REQUIRE(written.app() != nullptr);
		CHECK(written.app()->entry.ends_with(".lisp"));
		if(!std::filesystem::is_regular_file(entry.path() / "json" / "app.json")) {
			// Lisp only: Calamaio
			CHECK(written.versions().sourceDocument().contains("program"));
			++programs;
			continue;
		}
		const Screen spelled = Screen::fromApp(entry.path() / "json", {}, Screen::History::InMemory);
		REQUIRE(spelled.app() != nullptr);
		CHECK(spelled.app()->id == written.app()->id);
		const affresco::json& source = written.versions().sourceDocument();
		const bool own =
		    source.contains("program") && std::ranges::any_of(source["program"].items(), [](const auto& file) {
			    return !file.key().starts_with("../"); // a program of its own, not only a shared add-on's
		    });
		if(own) {
			// A program builds what its JSON writes out, its logic in functions: the same app, not the same document.
			// The same window, but for its titles, which a program may give through its locales (t)
			CHECK(untitled(affresco::json::parse(written.documentJson())["window"]) ==
			      untitled(affresco::json::parse(spelled.documentJson())["window"]));
			++programs;
		} else {
			// The generated ones: their own document the same; what a shared add-on's program builds says the same
			// in Lisp forms, so its bindings' text differs from the JSON's (proven by affrescoui lisp-binding --prove,
			// and drawn the same by the lisp_parity check): the same sections, with the same members
			const affresco::json document = affresco::json::parse(written.documentJson());
			const affresco::json json = affresco::json::parse(spelled.documentJson());
			CHECK(document["root"] == json["root"]);
			for(const auto& [key, value] : json.items()) {
				CAPTURE(key);
				REQUIRE(document.contains(key));
				if(!value.is_object()) continue;
				for(const auto& [member, given] : value.items()) {
					CAPTURE(member);
					CHECK(document[key].contains(member));
				}
			}
			++apps;
		}
	}
	// The generated ones (Sign in, until it is written as a program) and the programs, all but Calamaio with their
	// JSON beside them
	CHECK(apps + programs >= 13);
	CHECK(programs >= 12);
}

TEST_CASE("lisp examples: a lisp folder holds Lisp, but for scenarios and a signed catalogue") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int documents = 0;
	for(const auto& example : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_directory(example.path() / "lisp")) continue;
		for(const auto& file : std::filesystem::recursive_directory_iterator(example.path() / "lisp")) {
			if(!file.is_regular_file() || file.path().extension() != ".json") {
				documents += file.is_regular_file() ? 1 : 0;
				continue;
			}
			CAPTURE(file.path().string());
			const std::string relative = file.path().lexically_relative(example.path() / "lisp").generic_string();
			CHECK((relative.ends_with(".scenarios.json") || relative.starts_with("catalog/")));
		}
	}
	CHECK(documents > 100);
}

TEST_CASE("lisp examples: a file found by its name reads the same in either notation") {
	namespace notation = affresco::notation;
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	const std::filesystem::path written = examples / "forge" / "json" / "features" / "palette";
	const std::filesystem::path lisp = examples / "forge" / "lisp" / "features" / "palette";
	CHECK(notation::namedDocument(written, "feature").filename() == "feature.json");
	CHECK(notation::namedDocument(lisp, "feature").filename() == "feature.lisp");
	CHECK(notation::namedDocument(lisp, "missing").filename() ==
	      "missing.json"); // the problem names what was looked for
	CHECK(notation::isNamedDocument("a/app.lisp", "app"));
	CHECK(notation::isNamedDocument("a/app.json", "app"));
	CHECK_FALSE(notation::isNamedDocument("a/app.txt", "app"));
	CHECK_FALSE(notation::isNamedDocument("a/myapp.lisp", "app"));
	const auto read = [](const std::filesystem::path& file) {
		std::ifstream stream(file, std::ios::binary);
		const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		return notation::readDocument(text, file);
	};
	CHECK(read(notation::namedDocument(lisp, "feature")) == read(notation::namedDocument(written, "feature")));
	// Tempera is a program, its lisp folder written by hand: its adapter still reads as its JSON one
	CHECK(read(examples / "tempera" / "lisp" / "sorter" / "sorter.adapter.lisp") ==
	      read(examples / "tempera" / "json" / "sorter" / "sorter.adapter.json"));
}

TEST_CASE("lisp examples: the two notations share one copy of an example's assets") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int assets = 0;
	for(const auto& example : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_directory(example.path() / "lisp")) continue;
		assets += std::filesystem::is_directory(example.path() / "assets") ? 1 : 0;
		for(const std::string notation : {"json", "lisp"}) {
			if(!std::filesystem::is_directory(example.path() / notation)) continue; // an app written as a program
			for(const auto& file : std::filesystem::recursive_directory_iterator(example.path() / notation)) {
				const std::string extension = file.path().extension().string();
				CAPTURE(file.path().string());
				// Documents and the scripts of the JSON folder, and the catalogue's line-ending rule: no picture or
				// sound
				CHECK((!file.is_regular_file() || extension == ".json" || extension == ".lisp" || extension == ".py" ||
				       file.path().filename() == ".gitattributes"));
			}
		}
	}
	CHECK(assets >= 5);
}

TEST_CASE("lisp examples: only a json or a lisp folder reads the assets folder beside it") {
	const std::filesystem::path assets = std::filesystem::absolute("/a/flow/assets"); // on Windows, with its drive
	CHECK(affresco::format::notationAssets("/a/flow/json") == assets);
	CHECK(affresco::format::notationAssets("/a/flow/lisp/") == assets);
	CHECK(affresco::format::notationAssets("/a/flow/json/.") == assets);
	CHECK(affresco::format::notationAssets("/a/flow").empty());
	CHECK(affresco::format::notationAssets("").empty());
	CHECK(affresco::format::notationAssets("/a/flow/parts").empty());
}

namespace {

/// @brief A name as a JSON Pointer's segment: "~" as "~0", "/" as "~1".
/// @param name The name.
/// @return The segment.
std::string segment(const std::string& name) {
	std::string escaped;
	for(const char c : name) escaped += c == '~' ? std::string("~0") : c == '/' ? std::string("~1") : std::string(1, c);
	return escaped;
}

} // namespace

// The apps that moved to programs kept their JSON permissions, whose ask rules name /actions: an agent's change to a
// function or a command is under /program, so in acceptEdits it was applied unreviewed, where JSON asked for it
TEST_CASE("lisp examples attack: an agent's change to an app's program is reviewed, as its actions were, in any mode") {
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	int programs = 0;
	for(const auto& entry : std::filesystem::directory_iterator(examples)) {
		if(!std::filesystem::is_regular_file(entry.path() / "lisp" / "app.lisp")) continue;
		CAPTURE(entry.path().filename().string());
		Screen screen = Screen::fromApp(entry.path() / "lisp", {}, Screen::History::InMemory);
		const affresco::json source = screen.versions().sourceDocument();
		if(!source.contains("program")) continue;
		// A function of the app's own program: what builds a part of the tree is judged by what it builds
		auto file = source["program"].end();
		affresco::json::const_iterator definition;
		for(auto at = source["program"].begin(); at != source["program"].end() && file == source["program"].end(); ++at)
			for(auto text = at->begin(); text != at->end(); ++text)
				if(!at.key().starts_with("../") &&
				   (text->get<std::string>().contains("\n(defn ") || text->get<std::string>().starts_with("(defn "))) {
					file = at;
					definition = text;
					break;
				}
		if(file == source["program"].end()) continue;
		screen.versions().setMode(affresco::Mode::AcceptEdits);
		const affresco::json revision = {{"author", "agent/x"},
		                                 {"message", "m"},
		                                 {"set",
		                                  {{"/program/" + segment(file.key()) + "/" + segment(definition.key()),
		                                    ";; changed\n" + definition->get<std::string>()}}}};
		const affresco::RevisionResult result = screen.submitRevision(revision.dump());
		CAPTURE(result.error);
		CHECK(result.status != affresco::RevisionStatus::Applied);
		++programs;
	}
	CHECK(programs >= 11);
}

namespace {

/// @brief A section's members that another document's section has too, in its order.
/// @param document The document.
/// @param other The other.
/// @param section The section.
/// @return Their names.
std::vector<std::string> sharedMembers(const affresco::json& document, const affresco::json& other,
                                       const char* section) {
	std::vector<std::string> names;
	if(!document.contains(section) || !other.contains(section)) return names;
	for(const auto& [name, unused] : document[section].items())
		if(other[section].contains(name)) names.push_back(name);
	return names;
}

/// @brief The first key the screen binds each action to, as the `commands` variable shows it.
/// @param document The document.
/// @return Each action's.
std::map<std::string, std::string> firstKeys(const affresco::json& document) {
	std::map<std::string, std::string> keys;
	for(const auto& [key, binding] : document.value("keys", affresco::json::object()).items())
		if(binding.is_string()) keys.try_emplace(binding.get<std::string>(), key);
	return keys;
}

} // namespace

TEST_CASE("lisp examples: a program lists what it orders as its JSON version does") {
	// A section's members come in the order of their definitions: the settings, the overlays (drawn in that order),
	// the watches, and the first key a palette shows for a command
	const std::filesystem::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	for(const char* app : {"flow", "haven", "arcadia", "orrery", "forge", "sinopia", "code-editor", "launcher"}) {
		const std::string name = app;
		CAPTURE(name);
		const affresco::json json = affresco::json::parse(
		    Screen::fromApp(examples / app / "json", {}, Screen::History::InMemory).documentJson());
		const affresco::json program = affresco::json::parse(
		    Screen::fromApp(examples / app / "lisp", {}, Screen::History::InMemory).documentJson());
		for(const char* section : {"settings", "overlays", "watch"}) {
			const std::string sectionName = section;
			CAPTURE(sectionName);
			CHECK(sharedMembers(program, json, section) == sharedMembers(json, program, section));
		}
		const std::map<std::string, std::string> written = firstKeys(json);
		for(const auto& [action, key] : firstKeys(program)) {
			CAPTURE(action);
			if(written.contains(action)) CHECK(written.at(action) == key);
		}
	}
}
