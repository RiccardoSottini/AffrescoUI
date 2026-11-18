/// @file
/// @brief Unit tests for the build of a later version of a program in part (Program::rebuild()): only the definitions
/// that changed run, and those that use them, and the result is what a build in full gives. The first case is the
/// one that decides: every example, each changed in a few ways, built in part and in full.

#include <affresco/affresco.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Program.hpp>

#include <doctest/doctest.h>

#include <filesystem>
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
