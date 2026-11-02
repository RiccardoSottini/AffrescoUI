/// @file
/// @brief Fuzzes the Lisp language (modules/lisp): its reader, compiler, macro expansion and machine on any text, under
/// low limits, and its templates against the notation's reader.
///
/// Any text builds, or gives a LispError naming a place, whose message stays short, within the budget. What the
/// notation's reader reads, a template either builds to the same tree, key order included, or refuses for a word the
/// language runs. The same text builds to the same tree twice. A program's entries hold the whole of its text.

#include "Fuzz.hpp"

#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <affresco/notation/Notation.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using affresco::json;

namespace {

/// Longest error message, without its place: what it quotes of the text is cut short.
constexpr std::size_t maxMessage = 512;

/// @brief Low limits, so that inputs reach them and each run stays short.
/// @return The limits.
affresco::lisp::Limits lowLimits() {
	affresco::lisp::Limits limits;
	limits.budget.operations = 200000;
	limits.budget.bytes = 4U << 20U;
	limits.callDepth = 2000;
	limits.macroOperations = 50000;
	return limits;
}

/// @brief Builds a text.
/// @param text The text.
/// @return The tree, or std::nullopt if it is refused, which can only be with a LispError.
std::optional<json> built(std::string_view text) {
	try {
		return affresco::lisp::buildText(text, "fuzz", lowLimits());
	} catch(const affresco::lisp::LispError& error) {
		if(error.message().size() > maxMessage) affresco::fuzz::broken("an error quotes only the start of the text");
		return std::nullopt;
	}
}

/// @brief What the notation's reader reads, a template gives the same, unless it uses a word the language runs.
/// @param text The text.
/// @param tree What the template built, if it did.
void agreesWithTheNotation(std::string_view text, const std::optional<json>& tree) {
	json read;
	try {
		read = affresco::notation::read(text, affresco::notation::Notation::Lisp);
	} catch(const affresco::notation::NotationError&) {
		return;
	}
	if(tree && !affresco::lisp::isProgram(text) && tree->dump() != read.dump())
		affresco::fuzz::broken("a template builds to the tree the notation's reader reads");
}

/// @brief A program's entries, side by side, hold every form and comment of its text.
/// @param text The text.
void entriesKeepTheText(std::string_view text) {
	try {
		if(!affresco::lisp::isProgram(text)) return;
		std::size_t at = 0;
		for(const affresco::lisp::Entry& entry : affresco::lisp::splitEntries(text, "fuzz")) {
			at = text.find(entry.text, at);
			if(at == std::string_view::npos) affresco::fuzz::broken("an entry's text is exact");
			at += entry.text.size();
		}
	} catch(const affresco::lisp::LispError& error) { // it can't be read, or a form names no definition
		if(error.message().size() > maxMessage) affresco::fuzz::broken("an error quotes only the start of the text");
	}
}

/// @brief The keyword edit leaves every other byte, and gives one definition of the same address, or refuses.
/// @param text The text.
void keywordEditKeepsTheRest(std::string_view text) {
	try {
		const std::vector<affresco::lisp::Entry> entries = affresco::lisp::splitEntries(text, "fuzz");
		if(entries.size() != 1) return;
		const std::string value = "\"edited\"";
		const std::vector<std::string> keyword = {"title"};
		const std::string edited =
		    affresco::lisp::replaceKeyword(entries.front().text, keyword, value, entries.front().name);
		const std::string_view original = entries.front().text;
		bool kept = false; // the value may be in the text already: one place must be the edit's
		for(std::size_t at = edited.find(value); at != std::string::npos && !kept; at = edited.find(value, at + 1))
			kept = original.starts_with(edited.substr(0, at)) && original.ends_with(edited.substr(at + value.size()));
		if(!kept) affresco::fuzz::broken("every byte but the value stays");
		const std::vector<affresco::lisp::Entry> again = affresco::lisp::splitEntries(edited, "fuzz");
		if(again.size() != 1 || again.front().name != entries.front().name)
			affresco::fuzz::broken("the edit gives the same definition");
		// NOLINTNEXTLINE(bugprone-empty-catch): refusing it is the rule
	} catch(const affresco::DocumentError&) {
	}
}

/// @brief A document holding the text as its entry: its build gives the same tree twice, or refuses, and read as a
/// document again it gives itself.
/// @param text The text.
void documentsBuildAlike(std::string_view text) {
	try {
		const json source = affresco::lisp::sourceDocument(text, "fuzz.lisp");
		const json built = affresco::lisp::buildDocument(source);
		if(built != affresco::lisp::buildDocument(built)) affresco::fuzz::broken("a built document gives itself");
		// NOLINTNEXTLINE(bugprone-empty-catch): refusing it is the rule
	} catch(const affresco::DocumentError&) {
	}
}

} // namespace

/// @brief libFuzzer's entry point: builds the input and runs it at the top level.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	const std::optional<json> tree = built(text);
	if(tree != built(text)) affresco::fuzz::broken("the same text builds to the same tree");
	agreesWithTheNotation(text, tree);
	entriesKeepTheText(text);
	keywordEditKeepsTheRest(text);
	documentsBuildAlike(text);
	affresco::lisp::Interpreter interpreter(lowLimits());
	try {
		const affresco::lisp::Value value = interpreter.eval(text, "fuzz", "fuzz");
		static_cast<void>(interpreter.print(value));
		static_cast<void>(interpreter.toTree(value));
	} catch(const affresco::lisp::LispError& error) { // refusing it is the rule
		if(error.message().size() > maxMessage) affresco::fuzz::broken("an error quotes only the start of the text");
	}
	return 0;
}
