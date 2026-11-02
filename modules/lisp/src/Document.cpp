/// @file
/// @brief Documents that hold programs (Document.hpp): files read into documents, what a `program` section builds, and
/// the edit of one keyword's value in an entry's text.

#include <affresco/lisp/Document.hpp>

#include "Reader.hpp"

#include <affresco/notation/Lexer.hpp>
#include <affresco/notation/Notation.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <utility>

namespace affresco::lisp {

namespace {

using notation::Lexer;
using notation::Token;
using notation::TokenKind;

/// How many sections' builds a thread keeps: a revision judges two, a proposal's rebase two more.
constexpr std::size_t keptBuilds = 4;

/// What a program can't build: what is read when a file is, or what only a revision brings.
constexpr std::array<std::string_view, 5> unbuildable = {"include", "assets", "features", "program", "$schema"};

/// @brief An error about a definition, with no place in a file.
/// @param name The definition's address.
/// @param message What is wrong.
/// @return The error.
LispError problemIn(const std::string& name, const std::string& message) { return {"", 0, 0, name, message}; }

/// @brief Moves past one form.
/// @param lexer The lexer.
/// @param first The form's first token.
/// @param where The entry's address, for errors.
/// @return Where the form ends.
/// @throws LispError If the text ends first, or a closing bracket stands for a form.
std::size_t formEnd(Lexer& lexer, Token first, const std::string& where) {
	std::size_t depth = 0;
	for(Token token = first;; token = lexer.next()) {
		switch(token.kind) {
		case TokenKind::OpenNode:
		case TokenKind::OpenList:
		case TokenKind::OpenMap:
			++depth;
			break;
		case TokenKind::CloseNode:
		case TokenKind::CloseList:
		case TokenKind::CloseMap:
			if(depth == 0) throw problemIn(where, "expected a value before " + Lexer::shown(token));
			--depth;
			break;
		case TokenKind::End:
			throw problemIn(where, "the text ends inside a form");
		case TokenKind::Quote:
		case TokenKind::Backquote:
		case TokenKind::Unquote:
		case TokenKind::Splice:
			continue; // the form it stands before
		default:
			break;
		}
		if(depth == 0) return lexer.lastEnd();
	}
}

/// @brief A value's place in a text.
struct Span {
	std::size_t start = 0;           ///< Where it starts.
	std::size_t end = 0;             ///< Where it ends.
	TokenKind kind = TokenKind::End; ///< Its first token.
};

/// @brief Finds a key among the pairs a lexer is at: `:key value`, until something that isn't a key.
/// @param lexer The lexer, before the first pair.
/// @param first The token after it, or the first of the pairs.
/// @param key The key.
/// @param where The entry's address, for errors.
/// @return Its value's place, or std::nullopt if the pairs don't have it.
/// @throws LispError If they have it twice.
std::optional<Span> findKey(Lexer& lexer, Token first, std::string_view key, const std::string& where) {
	std::optional<Span> found;
	for(Token token = first; token.kind == TokenKind::Key || token.kind == TokenKind::String; token = lexer.next()) {
		const bool same =
		    token.kind == TokenKind::Key ? lexer.name(token) == key : lexer.unquote(token.text, token.offset) == key;
		const Token value = lexer.next();
		const Span span{value.offset, formEnd(lexer, value, where), value.kind};
		if(!same) continue;
		if(found) throw problemIn(where, ":" + Lexer::excerpt(key) + " is given twice");
		found = span;
	}
	return found;
}

/// @brief Moves past what comes before a definition form's options: its head, its name, its docstring, and for
/// `defcustom` its value.
/// @param lexer The lexer, at the start of the entry's text.
/// @param where The entry's address, for errors.
/// @return The first token after them.
/// @throws LispError If the form takes no options.
Token firstOption(Lexer& lexer, const std::string& where) {
	const Token open = lexer.next();
	const Token head = lexer.next();
	if(open.kind != TokenKind::OpenNode || head.kind != TokenKind::Word ||
	   (head.text != "defcommand" && head.text != "defscreen" && head.text != "defcustom"))
		throw problemIn(where, "only defcommand, defscreen and defcustom have keywords to edit");
	if(const Token name = lexer.next(); name.kind != TokenKind::Word)
		throw problemIn(where, "the form names no definition");
	Token token = lexer.next();
	if(head.text == "defcustom") {
		static_cast<void>(formEnd(lexer, token, where));
		token = lexer.next();
	}
	return token.kind == TokenKind::String ? lexer.next() : token;
}

/// @brief Refuses text that isn't exactly one form.
/// @param value The text.
/// @param where The entry's address, for errors.
/// @throws LispError If it isn't.
void expectOneForm(std::string_view value, const std::string& where) {
	Heap heap;
	Reader reader(heap, value, where);
	const auto read = reader.next();
	if(!read || read->start != 0 || read->end != value.size() || reader.next())
		throw problemIn(where, "a keyword's new value is exactly one form");
}

/// @brief Builds a section, under the limits.
/// @param section The section.
/// @return What it built.
/// @throws LispError If it can't be built, naming the definition.
std::shared_ptr<const BuiltProgram> buildSection(const json& section) {
	Program program = programOf(section);
	Limits limits;
	limits.budget.operations = maxBuildOperations;
	limits.budget.bytes = maxBuildBytes;
	auto built = std::make_shared<BuiltProgram>();
	built->tree = program.build(limits);
	built->undefined = program.undefinedNames();
	built->extensions = program.extensions();
	return built;
}

/// @brief Puts one written section into the built tree. A member both give must be the same.
/// @param tree The tree.
/// @param key The section's name.
/// @param written Its value.
/// @throws LispError If a member is both built and written differently.
void mergeWritten(json& tree, const std::string& key, const json& written) {
	const auto conflict = [&](const std::string& what) {
		return problemIn("", "\"" + what +
		                         "\" is built by the program and written in the document differently: change "
		                         "its definition under /program");
	};
	const auto found = tree.find(key);
	if(found == tree.end()) {
		tree[key] = written;
	} else if(*found == written) { // NOLINT(bugprone-branch-clone): nothing to do
	} else if(found->is_object() && written.is_object()) {
		for(const auto& [member, value] : written.items()) {
			const auto built = found->find(member);
			if(built == found->end()) (*found)[member] = value;
			else if(*built != value) throw conflict(std::format("{}.{}", key, member));
		}
	} else if(key != "format" && key != "version") {
		throw conflict(key);
	}
}

} // namespace

bool hasProgram(const json& document) { return document.is_object() && document.contains("program"); }

void checkProgramShape(const json& section) {
	if(!section.is_object()) expect::fail("program", "expected an object: file -> {definition -> its text}");
	std::size_t total = 0;
	for(const auto& [file, entries] : section.items()) {
		if(file.empty() || file.size() > maxEntryNameBytes) expect::fail("program", "a file's name is 1 to 1024 bytes");
		const std::string place = "program." + file;
		if(!entries.is_object()) expect::fail(place, "expected an object: definition -> its text");
		total += entries.size();
		if(total > maxProgramEntries)
			expect::fail("program", "more than " + std::to_string(maxProgramEntries) + " definitions");
		for(const auto& [name, text] : entries.items()) {
			if(name.empty() || name.size() > maxEntryNameBytes)
				expect::fail(place, "a definition's address is 1 to 1024 bytes");
			if(!text.is_string()) expect::fail(std::format("{}.{}", place, name), "expected the definition's text");
			if(text.get_ref<const std::string&>().size() > maxEntryBytes)
				expect::fail(std::format("{}.{}", place, name), "a definition's text is at most 1 MiB");
		}
	}
}

json sourceDocument(std::string_view text, const std::string& file) {
	Program program;
	program.addFile(text, file); // two definitions of one name are an error, which an object's keys wouldn't say
	json entries = json::object();
	for(const Entry& entry : program.entries()) entries[entry.name] = entry.text;
	return {{"format", "affrescoui-screen"}, {"version", 2}, {"program", {{file, std::move(entries)}}}};
}

json readDocument(std::string_view text, const std::filesystem::path& file, const std::string& name) {
	if(notation::notationOf(file) != notation::Notation::Lisp) return notation::readDocument(text, file);
	bool program = false;
	try {
		program = isProgram(text);
		// NOLINTNEXTLINE(bugprone-empty-catch): the notation's reader says what is wrong, naming the file
	} catch(const LispError&) {
	}
	if(!program) return notation::readDocument(text, file);
	return sourceDocument(text, name.empty() ? file.filename().generic_string() : name);
}

std::string jsonText(std::string_view text, const std::filesystem::path& file) {
	if(notation::notationOf(file) != notation::Notation::Lisp) return std::string(text);
	return readDocument(text, file).dump();
}

Program programOf(const json& section, const json& authors) {
	checkProgramShape(section);
	Program program;
	for(const auto& [file, entries] : section.items())
		for(const auto& [name, text] : entries.items()) {
			Entry entry;
			entry.file = file;
			entry.name = name;
			entry.text = text.get<std::string>();
			if(const json* of = authors.is_object() && authors.contains(file) ? &authors[file] : nullptr;
			   of && of->is_object() && of->contains(name) && (*of)[name].is_string())
				entry.author = (*of)[name].get<std::string>();
			program.addEntry(std::move(entry));
		}
	return program;
}

std::shared_ptr<const BuiltProgram> builtProgram(const json& section) {
	thread_local std::vector<std::pair<json, std::shared_ptr<const BuiltProgram>>> kept;
	const auto found = std::ranges::find_if(kept, [&](const auto& build) { return build.first == section; });
	if(found != kept.end()) {
		std::rotate(kept.begin(), found, found + 1);
		return kept.front().second;
	}
	std::shared_ptr<const BuiltProgram> built = buildSection(section);
	if(kept.size() >= keptBuilds) kept.pop_back();
	kept.emplace(kept.begin(), section, built);
	return built;
}

json buildDocument(const json& source) {
	const json& section = source.at("program");
	json tree = builtProgram(section)->tree;
	for(const std::string_view key : unbuildable)
		if(tree.contains(key))
			throw problemIn("", "a program can't build \"" + std::string(key) + "\": " +
			                        (key == "include" ? "a data file or an app lists a program's files"
			                                          : "a revision or a file brings it"));
	for(const auto& [key, value] : source.items())
		if(key != "program") mergeWritten(tree, key, value);
	tree["program"] = section;
	return tree;
}

std::string replaceKeyword(std::string_view text, std::span<const std::string> keywords, std::string_view value,
                           const std::string& where) {
	expectOneForm(value, where);
	Span span{0, text.size(), TokenKind::End};
	bool top = true;
	for(const std::string& keyword : keywords) {
		const std::string_view inside = text.substr(span.start, span.end - span.start);
		Lexer lexer(inside, true);
		if(!top && span.kind != TokenKind::OpenMap)
			throw problemIn(where, "the value before :" + Lexer::excerpt(keyword) + " isn't a map");
		const Token first = top ? firstOption(lexer, where) : (static_cast<void>(lexer.next()), lexer.next());
		const std::optional<Span> found = findKey(lexer, first, keyword, where);
		if(!found) throw problemIn(where, "it has no keyword :" + Lexer::excerpt(keyword));
		span = {span.start + found->start, span.start + found->end, found->kind};
		top = false;
	}
	std::string result(text.substr(0, span.start));
	result.append(value).append(text.substr(span.end));
	const std::vector<Entry> entries = splitEntries(result, "");
	if(entries.size() != 1 || entries.front().name != where)
		throw problemIn(where, "the edit would make the text something other than this one definition");
	return result;
}

std::string keywordValueText(const json& value) {
	if(value.is_string()) return value.dump(-1, ' ', false, json::error_handler_t::replace);
	if(value.is_object() && value.size() == 1 && value.contains("$form")) {
		if(!value["$form"].is_string()) expect::fail("$form", "expected the form's text");
		return value["$form"].get<std::string>();
	}
	std::string text = notation::write(value, notation::Notation::Lisp);
	while(text.ends_with('\n')) text.pop_back();
	return text;
}

} // namespace affresco::lisp
