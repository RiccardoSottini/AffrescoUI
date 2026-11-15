/// @file
/// @brief Documents that hold programs (Document.hpp): files read into documents, what a `program` section builds, and
/// the edit of one keyword's value in an entry's text.

#include <affresco/lisp/Document.hpp>

#include "Namespaces.hpp"
#include "Reader.hpp"
#include "TopLevel.hpp"

#include <affresco/notation/Lexer.hpp>
#include <affresco/notation/Notation.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

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
		if(token.kind >= TokenKind::Quote && token.kind <= TokenKind::Splice) continue; // the form it stands before
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

/// @brief Finds a keyword's value in one form, by tokens: among a definition form's options (firstOption()), or a
/// map's pairs when `maps` is true; each further keyword inside the map that is the previous one's value.
/// @param form The form's text.
/// @param keywords The keywords, without their ':'; at least one.
/// @param maps Whether a map at the top has keywords too.
/// @param where The entry's address, for errors.
/// @param missing Set to what is missing when there is no such value.
/// @return Its place in the form, or std::nullopt.
/// @throws LispError If the form is neither, or a keyword is there twice.
std::optional<Span> keywordIn(std::string_view form, std::span<const std::string> keywords, bool maps,
                              const std::string& where, std::string& missing) {
	Span span{0, form.size(), TokenKind::End};
	bool top = true;
	for(const std::string& keyword : keywords) {
		Lexer lexer(form.substr(span.start, span.end - span.start), true);
		if(!top && span.kind != TokenKind::OpenMap) {
			missing = "the value before :" + Lexer::excerpt(keyword) + " isn't a map";
			return std::nullopt;
		}
		const bool options = top && (!maps || Lexer(form, true).next().kind != TokenKind::OpenMap);
		const Token first = options ? firstOption(lexer, where) : (static_cast<void>(lexer.next()), lexer.next());
		const std::optional<Span> found = findKey(lexer, first, keyword, where);
		if(!found) {
			missing = "it has no keyword :" + Lexer::excerpt(keyword);
			return std::nullopt;
		}
		span = {span.start + found->start, span.start + found->end, found->kind};
		top = false;
	}
	return span;
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
	built->truth = program.truthTests();
	built->extensions = program.extensions();
	built->namesUsed = program.namesUsed();
	built->namedCalls = program.namedCalls();
	built->defaulted = program.defaulted();
	return built;
}

/// @brief Puts one written section into the built tree. A member both give must be the same, but one a `defdefaults`
/// gave, which the written one replaces.
/// @param tree The tree.
/// @param key The section's name.
/// @param written Its value.
/// @param defaulted The members a `defdefaults` gave.
/// @throws LispError If a member is both built and written differently.
void mergeWritten(json& tree, const std::string& key, const json& written,
                  const std::set<std::pair<std::string, std::string>>& defaulted) {
	const auto conflict = [&](const std::string& what) {
		return problemIn("", "\"" + what +
		                         "\" is built by the program and written in the document differently: change "
		                         "its definition under /program");
	};
	const auto found = tree.find(key);
	if(found == tree.end()) {
		tree[key] = written;
	} else if(*found == written) {                       // NOLINT(bugprone-branch-clone): nothing to do
	} else if(found->is_array() && written.is_array()) { // a list joins, as an include's does: `persist`
		for(const json& item : written)
			if(std::ranges::find(*found, item) == found->end()) found->push_back(item);
	} else if(found->is_object() && written.is_object()) {
		for(const auto& [member, value] : written.items()) {
			const auto built = found->find(member);
			if(built == found->end() || defaulted.contains({key, member})) (*found)[member] = value;
			else if(*built != value) throw conflict(std::format("{}.{}", key, member));
		}
	} else if(key != "format" && key != "version") {
		throw conflict(key);
	}
}

/// @brief Blanks a span of a text: each character but a line's end made a space, so every other form keeps its line and
/// column.
/// @param text The text.
/// @param start Where the span starts.
/// @param end Where it ends.
void blank(std::string& text, std::size_t start, std::size_t end) {
	for(std::size_t at = start; at < end; ++at)
		if(text[at] != '\n' && text[at] != '\r') text[at] = ' ';
}

/// @brief Tells whether a program file's first form is an `ns` form, by its first two tokens.
/// @param text The file's text.
/// @return Whether it is.
bool startsWithNamespace(std::string_view text) {
	try {
		notation::Lexer lexer(text, true);
		const notation::Token first = lexer.next();
		if(first.kind != notation::TokenKind::OpenNode) return false;
		const notation::Token second = lexer.next();
		return second.kind == notation::TokenKind::Word && second.text == "ns";
	} catch(const DocumentError&) {
		return false; // the reader says what is wrong
	}
}

/// @brief What a program file's text says beside its definitions: its include forms and its ns form.
struct FileForms {
	json includes = json::array(); ///< The files it names, in order: its include forms', then its ns form's.
	std::string declaration;       ///< Its ns form's text, with the comments above it; "" if it has none.
};

/// @brief Adds the files an ns form requires to a file's includes, after its own include forms', each once.
/// @param declaration What the form says.
/// @param includes The file's includes.
void addRequired(const NamespaceDeclaration& declaration, json& includes) {
	const auto named = [&includes](const std::string& path) {
		const std::string normal = std::filesystem::path(path).lexically_normal().generic_string();
		return std::ranges::any_of(includes, [&](const json& include) {
			return std::filesystem::path(include.get<std::string>()).lexically_normal().generic_string() == normal;
		});
	};
	for(const Required& required : declaration.required)
		if(!required.path.empty() && !named(required.path)) includes.push_back(required.path);
}

/// @brief Takes a program file's `(include "a.lisp" ...)` forms and its first form `(ns ...)` out of its text, each
/// character but a line's end made a space, so every other form keeps its line and column. They are read with the
/// file, and are no entry of the program: a revision has no way to add one, so none changes which files the loader
/// reads. An ns form takes the comments above it along, so that its text reads from the file's first line.
/// @param text The file's text.
/// @param file Its name, for errors.
/// @param forms Gets what they say.
/// @return The text without them.
/// @throws LispError If an include names something other than a file, or the ns form doesn't read.
std::string withoutFileForms(std::string_view text, const std::string& file, FileForms& forms) {
	std::string kept(text);
	Heap heap;
	Reader reader(heap, text, file);
	std::optional<NamespaceDeclaration> declared;
	for(bool first = true; const auto read = reader.next(); first = false) {
		const Value& form = read->form;
		const std::string_view head =
		    form.kind() == Kind::List && form.count() > 0 && form.at(0).kind() == Kind::Symbol ? form.at(0).text() : "";
		if(head == "ns" && first) { // elsewhere, the split into entries refuses it
			declared = readNamespaceForm(form, file);
			const std::size_t start = text.starts_with("\xEF\xBB\xBF") ? 3 : 0;
			const std::size_t end = pastSameLineComment(text, read->end);
			forms.declaration = std::string(text.substr(start, end - start));
			blank(kept, start, end);
			continue;
		}
		if(head != "include") continue;
		for(std::size_t at = 1; at < form.count(); ++at) {
			const Kind kind = form.at(at).kind();
			if((kind != Kind::String && kind != Kind::Symbol) || form.at(at).text().empty()) {
				const Place place = form.at(at).place().line != 0 ? form.at(at).place() : form.place();
				throw LispError(file, place.line, place.column, "", "include names files, each a string");
			}
			forms.includes.push_back(std::string(form.at(at).text()));
		}
		blank(kept, read->start, read->end);
	}
	if(declared) addRequired(*declared, forms.includes);
	return kept;
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

json sourceDocument(std::string_view text, const std::string& file, std::string_view declaration) {
	Program program; // two definitions of one name are an error, which an object's keys wouldn't say
	for(Entry& entry : splitEntries(text, file)) program.addEntryUnread(std::move(entry)); // the split read each
	json entries = json::object();
	if(!declaration.empty()) entries[std::string(namespaceEntry)] = declaration; // first, as in the file
	for(const Entry& entry : program.entries()) entries[entry.name] = entry.text;
	return {{"format", "affrescoui-screen"}, {"version", 2}, {"program", {{file, std::move(entries)}}}};
}

namespace {

/// Lisp files read, kept for the process by their exact text, file and name: an app reads its files each time it opens.
constexpr std::size_t keptDocumentBytes = 64U << 20U;

/// @brief The Lisp files read lately (readDocument() is pure: the same text, file and name give the same value).
class KeptDocuments {
public:
	/// @brief Finds one. @param key Its key. @return A copy of its value, or std::nullopt.
	std::optional<json> find(const std::string& key) {
		const std::scoped_lock lock(mutex_);
		const auto found = std::ranges::find(kept_, key, &Kept::key);
		if(found == kept_.end()) return std::nullopt;
		std::rotate(kept_.begin(), found, found + 1); // the most recent first
		return kept_.front().value;
	}
	/// @brief Keeps one, letting the least recent go past keptDocumentBytes. @param key Its key. @param value Its
	/// value.
	void keep(std::string key, const json& value) {
		const std::scoped_lock lock(mutex_);
		const std::size_t bytes = key.size() * 4; // its value costs a few times its text
		if(bytes > keptDocumentBytes / 4 || std::ranges::contains(kept_, key, &Kept::key)) return;
		kept_.insert(kept_.begin(), {std::move(key), value, bytes});
		bytes_ += bytes;
		while(bytes_ > keptDocumentBytes) {
			bytes_ -= kept_.back().bytes;
			kept_.pop_back();
		}
	}

private:
	/// @brief A file read.
	struct Kept {
		std::string key;   ///< Its name, file and text.
		json value;        ///< What it read to.
		std::size_t bytes; ///< What it costs.
	};
	std::mutex mutex_;       ///< Files are read on any thread.
	std::vector<Kept> kept_; ///< The most recent first: an app has tens of files.
	std::size_t bytes_ = 0;  ///< What they cost.
};

/// @brief readDocument() of a Lisp file, not kept.
/// @param text The text.
/// @param file The file.
/// @param name What a program calls it.
/// @return The value.
json readLispDocument(std::string_view text, const std::filesystem::path& file, const std::string& name);

} // namespace

json readDocument(std::string_view text, const std::filesystem::path& file, const std::string& name) {
	if(notation::notationOf(file) != notation::Notation::Lisp) return notation::readDocument(text, file);
	static KeptDocuments kept;
	std::string key = name;
	key.append(1, '\0').append(file.generic_string()).append(1, '\0').append(text);
	if(std::optional<json> found = kept.find(key)) return std::move(*found);
	json value = readLispDocument(text, file, name);
	kept.keep(std::move(key), value);
	return value;
}

namespace {

json readLispDocument(std::string_view text, const std::filesystem::path& file, const std::string& name) {
	// By tokens first: a data file's one form is then read once, by the notation's reader
	const std::optional<bool> byTokens = isProgramByTokens(text);
	bool program = byTokens.value_or(false);
	if(!byTokens) try {
			program = isProgram(text);
			// NOLINTNEXTLINE(bugprone-empty-catch): the notation's reader says what is wrong, naming the file
		} catch(const LispError&) {
		}
	if(program) {
		const std::string named = name.empty() ? file.filename().generic_string() : name;
		FileForms forms;
		const bool taken = text.contains("include") || startsWithNamespace(text);
		// The forms are taken out first: an argument beside the call that fills forms.declaration may be read before it
		const std::string kept = taken ? withoutFileForms(text, named, forms) : std::string(text);
		json document = sourceDocument(kept, named, forms.declaration);
		if(!forms.includes.empty()) document["include"] = std::move(forms.includes);
		return document;
	}
	bool runs = false;
	try {
		runs = byTokens ? firstUnquote(text).has_value() : needsRunning(text).has_value();
		// NOLINTNEXTLINE(bugprone-empty-catch): the notation's reader says what is wrong, naming the file
	} catch(const DocumentError&) {
	}
	return runs ? buildText(text, file.generic_string()) : notation::readDocument(text, file);
}

} // namespace

std::string jsonText(std::string_view text, const std::filesystem::path& file) {
	if(notation::notationOf(file) != notation::Notation::Lisp) return std::string(text);
	return readDocument(text, file).dump();
}

Program programOf(const json& section, const json& authors) {
	checkProgramShape(section);
	Program program;
	for(const auto& [file, entries] : section.items())
		for(const auto& [name, text] : entries.items()) {
			if(name == namespaceEntry) { // the file's ns form, which the loader read with it
				program.declareNamespace(file, text.get<std::string>());
				continue;
			}
			Entry entry;
			entry.file = file;
			entry.name = name;
			entry.text = text.get<std::string>();
			if(const json* of = authors.is_object() && authors.contains(file) ? &authors[file] : nullptr;
			   of && of->is_object() && of->contains(name) && (*of)[name].is_string())
				entry.author = (*of)[name].get<std::string>();
			program.addEntryUnread(std::move(entry)); // load() and build() read it, and refuse what addEntry() would
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

namespace {

/// @brief A `program` section as a Program whose entries each start on lines of their own: in each file, an entry
/// starts on the line after the previous one's last, so that a place names one entry (programOf() reads every entry
/// from line 1).
/// @param section The section, its shape checked.
/// @return The program, each entry read (Program::addEntry()), so that its kind is known.
/// @throws LispError If an entry isn't one form with its address, or a name is taken.
Program programWithOwnLines(const json& section) {
	Program program;
	for(const auto& [file, entries] : section.items()) {
		std::size_t line = 1;
		for(const auto& [name, text] : entries.items()) {
			if(name == namespaceEntry) {
				program.declareNamespace(file, text.get<std::string>());
				continue;
			}
			Entry entry;
			entry.file = file;
			entry.name = name;
			entry.text = text.get<std::string>();
			entry.line = line;
			line += static_cast<std::size_t>(std::ranges::count(entry.text, '\n')) + 1U;
			program.addEntry(std::move(entry));
		}
	}
	return program;
}

/// @brief A program's entries by file, each file's in the order of their start lines (programWithOwnLines()).
using EntriesByFile = std::map<std::string_view, std::vector<const Entry*>, std::less<>>;

/// @brief The entry whose lines hold a place a recording build noted.
/// @param files The program's entries by file; they start on lines of their own.
/// @param name The place's file.
/// @param line The place's line, as the entries' start lines count it.
/// @return The entry, or nullptr if none holds it.
const Entry* holderOf(const EntriesByFile& files, std::string_view name, std::size_t line) {
	const auto file = files.find(name);
	if(file == files.end()) return nullptr;
	const auto after =
	    std::ranges::upper_bound(file->second, line, std::less<>(), [](const Entry* entry) { return entry->line; });
	if(after == file->second.begin()) return nullptr;
	const Entry* holder = *std::prev(after);
	const auto lines = static_cast<std::size_t>(std::ranges::count(holder->text, '\n')) + 1U;
	return line < holder->line + lines ? holder : nullptr;
}

/// @brief Where each line of the entries' texts starts, worked out once per entry that holds a noted place, so that
/// finding many places in one large definition costs its text once.
class LineStarts {
public:
	/// @brief The byte where a line and a column of an entry's text are.
	/// @param entry The entry.
	/// @param line The line, from 1 at its text's first.
	/// @param column The column, from 1, in characters.
	/// @return The byte; the text's size if it is shorter.
	std::size_t offset(const Entry& entry, std::size_t line, std::size_t column) {
		auto [found, added] = starts_.try_emplace(&entry);
		std::vector<std::size_t>& starts = found->second;
		if(added) {
			starts.push_back(0);
			for(std::size_t at = 0; at < entry.text.size(); ++at)
				if(entry.text[at] == '\n') starts.push_back(at + 1U);
		}
		if(line == 0 || line > starts.size()) return entry.text.size();
		std::size_t at = starts[line - 1U];
		for(std::size_t columns = 1; columns < column && at < entry.text.size(); ++columns) {
			++at; // past a character: its first byte and those that continue it
			while(at < entry.text.size() && (static_cast<unsigned char>(entry.text[at]) & 0xC0U) == 0x80U) ++at;
		}
		return at;
	}

private:
	std::map<const Entry*, std::vector<std::size_t>> starts_; ///< By entry, where each line starts.
};

/// @brief How the node form at a byte of a text gives a node its id: written out, `(type#id` or `:id "id"` among its
/// properties, which come before its children; or as an expression, `:id ~name` or `:id ~(str "row" i)`.
/// @param text The text.
/// @param at Where the form starts.
/// @param id The id.
/// @return NodeOrigin::Id::Form if it writes that id, NodeOrigin::Id::Expression if an expression gives it, or
/// std::nullopt if no node form there has an id.
/// @throws DocumentError If the form doesn't read as tokens; it read in the build, so it does.
std::optional<NodeOrigin::Id> idOfForm(std::string_view text, std::size_t at, std::string_view id) {
	Lexer lexer(text.substr(at), true);
	if(lexer.next().kind != TokenKind::OpenNode) return std::nullopt;
	const Token head = lexer.next();
	if(head.kind != TokenKind::Word) return std::nullopt;
	if(const std::size_t hash = head.text.find('#'); hash != std::string_view::npos)
		return head.text.substr(hash + 1U) == id ? std::optional(NodeOrigin::Id::Form) : std::nullopt;
	for(Token key = lexer.next(); key.kind == TokenKind::Key; key = lexer.next()) {
		const Token value = lexer.next();
		if(lexer.name(key) != "id") {
			static_cast<void>(formEnd(lexer, value, ""));
			continue;
		}
		if(value.kind != TokenKind::String) return NodeOrigin::Id::Expression;
		return lexer.unquote(value.text, value.offset) == id ? std::optional(NodeOrigin::Id::Form) : std::nullopt;
	}
	return std::nullopt;
}

/// @brief Whether an id's text, in quotes, is written at a byte of a text: where a call hands it in.
/// @param text The text.
/// @param at Where it would start.
/// @param id The id.
/// @return true if it is.
/// @throws DocumentError If the text there doesn't read as a token; it read in the build, so it does.
bool writesText(std::string_view text, std::size_t at, std::string_view id) {
	Lexer lexer(text.substr(at), true);
	const Token token = lexer.next();
	return token.kind == TokenKind::String && lexer.unquote(token.text, token.offset) == id;
}

/// @brief What nodeOrigins() gives, built anew.
/// @param section The section.
/// @return The origins; empty if the section can't be built.
std::shared_ptr<const NodeOrigins> recordOrigins(const json& section) {
	auto origins = std::make_shared<NodeOrigins>();
	try {
		checkProgramShape(section);
		Program program = programWithOwnLines(section);
		Limits limits;
		limits.budget.operations = maxBuildOperations;
		limits.budget.bytes = maxBuildBytes;
		program.recordOrigins(true);
		static_cast<void>(program.build(limits));
		EntriesByFile files;
		for(const Entry& entry : program.entries()) files[entry.file].push_back(&entry);
		LineStarts lines;
		for(const NodePlace& noted : program.nodePlaces()) {
			const Entry* form = holderOf(files, noted.file, noted.line);
			if(form == nullptr) continue;
			const std::size_t line = noted.line - form->line + 1U;
			const std::optional<NodeOrigin::Id> id =
			    idOfForm(form->text, lines.offset(*form, line, noted.column), noted.id);
			if(!id) continue;
			// An expression that hands in an id's text a call wrote: the origin is that call's
			if(const Entry* call =
			       *id == NodeOrigin::Id::Expression ? holderOf(files, noted.idFile, noted.idLine) : nullptr) {
				const std::size_t callLine = noted.idLine - call->line + 1U;
				if(writesText(call->text, lines.offset(*call, callLine, noted.idColumn), noted.id)) {
					origins->emplace(noted.id, NodeOrigin{call->file, call->name, call->kind,
					                                      static_cast<std::uint32_t>(callLine), noted.idColumn,
					                                      NodeOrigin::Id::Call});
					continue;
				}
			}
			origins->emplace(noted.id, NodeOrigin{form->file, form->name, form->kind, static_cast<std::uint32_t>(line),
			                                      noted.column, *id});
		}
	} catch(const LispError&) { // builtProgram() reports it, with the lines as the history has them
		origins->clear();
	} catch(const DocumentError&) {
		origins->clear();
	}
	return origins;
}

} // namespace

std::shared_ptr<const NodeOrigins> nodeOrigins(const json& section) {
	thread_local std::vector<std::pair<json, std::shared_ptr<const NodeOrigins>>> kept;
	const auto found = std::ranges::find_if(kept, [&](const auto& origins) { return origins.first == section; });
	if(found != kept.end()) {
		std::rotate(kept.begin(), found, found + 1);
		return kept.front().second;
	}
	std::shared_ptr<const NodeOrigins> origins = recordOrigins(section);
	if(kept.size() >= keptBuilds) kept.pop_back();
	kept.emplace(kept.begin(), section, origins);
	return origins;
}

json buildDocument(const json& source) {
	const json& section = source.at("program");
	const std::shared_ptr<const BuiltProgram> built = builtProgram(section);
	json tree = built->tree;
	for(const std::string_view key : unbuildable)
		if(tree.contains(key))
			throw problemIn("", "a program can't build \"" + std::string(key) + "\": " +
			                        (key == "include" ? "a data file or an app lists a program's files"
			                                          : "a revision or a file brings it"));
	for(const auto& [key, value] : source.items())
		if(key != "program") mergeWritten(tree, key, value, built->defaulted);
	tree["program"] = section;
	return tree;
}

std::string replaceKeyword(std::string_view text, std::span<const std::string> keywords, std::string_view value,
                           const std::string& where) {
	expectOneForm(value, where);
	std::string missing;
	const std::optional<Span> span = keywordIn(text, keywords, false, where, missing);
	if(!span) throw problemIn(where, missing);
	std::string result(text.substr(0, span->start));
	result.append(value).append(text.substr(span->end));
	const std::vector<Entry> entries = splitEntries(result, "");
	if(entries.size() != 1 || entries.front().name != where)
		throw problemIn(where, "the edit would make the text something other than this one definition");
	return result;
}

std::optional<std::pair<std::size_t, std::size_t>> findKeyword(std::string_view text,
                                                               std::span<const std::string> keywords) {
	if(keywords.empty()) return std::nullopt;
	Lexer lexer(text, true);
	for(Token token = lexer.next(); token.kind != TokenKind::End; token = lexer.next()) {
		const std::size_t start = token.offset;
		const std::size_t end = formEnd(lexer, token, "");
		const std::string_view form = text.substr(start, end - start);
		Lexer head(form, true);
		const Token open = head.next();
		const Token word = head.next();
		const bool options = open.kind == TokenKind::OpenNode && word.kind == TokenKind::Word &&
		                     (word.text == "defcommand" || word.text == "defscreen" || word.text == "defcustom");
		if(open.kind != TokenKind::OpenMap && !options) continue;
		std::string missing;
		if(const std::optional<Span> span = keywordIn(form, keywords, true, "", missing))
			return std::pair{start + span->start, start + span->end};
	}
	return std::nullopt;
}

std::optional<std::string> withKeyword(std::string_view text, std::span<const std::string> keywords,
                                       std::string_view value) {
	expectOneForm(value, "");
	const auto span = findKeyword(text, keywords);
	if(!span) return std::nullopt;
	std::string result(text.substr(0, span->first));
	result.append(value).append(text.substr(span->second));
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
