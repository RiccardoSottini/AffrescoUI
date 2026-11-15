/// @file
/// @brief The namespaces (Namespaces.hpp), how the machine resolves a qualified word (Machine::resolveQualified()), and
/// a file's `ns` form (reference/namespaces.md).

#include "Namespaces.hpp"

#include "Compiler.hpp"
#include "Machine.hpp"
#include "TopLevel.hpp"

#include <affresco/lisp/Interpreter.hpp>
#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <utility>

namespace affresco::lisp {

Namespaces::Namespaces()
    : spaces_{{"clojure.core", true}, {"clojure.set", false}, {"clojure.string", false}},
      aliases_{{"set", "clojure.set"}, {"str", "clojure.string"}, {"string", "clojure.string"}} {}

void Namespaces::add(std::string name, bool bare) { spaces_.insert_or_assign(std::move(name), bare); }

bool Namespaces::alias(std::string alias, std::string_view name) {
	if(!spaces_.contains(name)) return false;
	aliases_.insert_or_assign(std::move(alias), std::string(name));
	return true;
}

NamespaceFound Namespaces::find(std::string_view prefix, const AliasMap* aliases) const {
	std::string_view name = prefix;
	if(const auto own = aliases ? aliases->find(prefix) : AliasMap::const_iterator();
	   aliases && own != aliases->end()) {
		name = own->second; // a file's own first, and only through them a file of the program
		if(const auto program = programSpaces_.find(name); program != programSpaces_.end())
			return {program->first, true, &program->second};
	} else if(const auto alias = aliases_.find(prefix); alias != aliases_.end()) {
		name = alias->second;
	}
	const auto found = spaces_.find(name);
	return found == spaces_.end() ? NamespaceFound{} : NamespaceFound{found->first, found->second};
}

void Namespaces::clearProgram() {
	programSpaces_.clear();
	for(auto& [file, aliases] : fileAliases_) aliases.clear();
}

void Namespaces::addProgramSpace(std::string name, ProgramSpace space) {
	programSpaces_.insert_or_assign(std::move(name), std::move(space));
}

const ProgramSpace* Namespaces::programSpace(std::string_view name) const {
	const auto found = programSpaces_.find(name);
	return found == programSpaces_.end() ? nullptr : &found->second;
}

void Namespaces::setFileAliases(const std::string& file, AliasMap aliases) {
	fileAliases_.insert_or_assign(file, std::move(aliases)); // assigned in place: a pointer to it stays good
}

const AliasMap* Namespaces::fileAliases(const std::string* file) const {
	if(fileAliases_.empty()) return nullptr; // most programs: no file has an ns form
	const auto found = fileAliases_.find(file ? std::string_view(*file) : std::string_view());
	return found == fileAliases_.end() ? nullptr : &found->second;
}

std::optional<std::string_view> Machine::resolveQualified(std::string_view word, const AliasMap* aliases) {
	if(!isQualified(word)) return std::nullopt;
	const std::size_t slash = word.find('/');
	const std::string_view prefix = word.substr(0, slash);
	const NamespaceFound space = namespaces_.find(prefix, aliases);
	if(space.name.empty())
		throw Problem("\"" + notation::Lexer::excerpt(word) + "\": no namespace \"" + notation::Lexer::excerpt(prefix) +
		              "\" here; the language has clojure.core, clojure.string (str/) and clojure.set (set/)");
	const std::string_view name = word.substr(slash + 1);
	if(space.program && !space.program->names.contains(name))
		throw Problem(notation::Lexer::excerpt(word) + ": \"" + notation::Lexer::excerpt(name) +
		              "\" isn't defined in " + space.program->file);
	if(space.bare) return global(name).name;
	return global(std::string(space.name) + "/" + std::string(name)).name;
}

namespace {

/// The most specs one `ns` form requires: an app has tens of files.
constexpr std::size_t maxRequired = 256;

/// @brief Fails, naming the place.
/// @param file The file.
/// @param at What is wrong, or the form it is in.
/// @param message What is wrong.
[[noreturn]] void failAt(const std::string& file, const Value& at, const std::string& message) {
	throw LispError(file, at.place().line, at.place().column, "", message);
}

/// @brief A dotted name's segments.
/// @param name The name.
/// @return Them; one empty segment for an empty name.
std::vector<std::string_view> segmentsOf(std::string_view name) {
	std::vector<std::string_view> segments;
	for(std::size_t start = 0;;) {
		const std::size_t dot = name.find('.', start);
		segments.push_back(name.substr(start, dot - start));
		if(dot == std::string_view::npos) return segments;
		start = dot + 1;
	}
}

/// @brief Tells whether a word names a namespace: words between dots, none of them empty, with no slash.
/// @param name The word.
/// @return Whether it does.
bool isNamespaceName(std::string_view name) {
	if(name.empty() || name.find_first_of("/\\") != std::string_view::npos) return false;
	return std::ranges::none_of(segmentsOf(name), [](std::string_view segment) { return segment.empty(); });
}

/// @brief Reads a `:refer`'s vector of words.
/// @param file The file.
/// @param words The vector.
/// @param into Gets the words.
void readRefers(const std::string& file, const Value& words, std::vector<std::string>& into) {
	if(words.kind() == Kind::Keyword && words.text() == "all") failAt(file, words, "name what you refer");
	if(words.kind() != Kind::Vector) failAt(file, words, ":refer takes a vector of words: :refer [join]");
	for(std::size_t at = 0; at < words.count(); ++at) {
		const Value& word = words.at(at);
		if(word.kind() != Kind::Symbol || isQualified(word.text()))
			failAt(file, word, ":refer takes a vector of plain words: :refer [join]");
		into.emplace_back(word.text());
	}
}

/// @brief Reads what a spec of a `(:require ...)` names: a namespace's name or a file's path as a string.
/// @param file The file.
/// @param spec The spec.
/// @param own The file's own namespace.
/// @return The spec, with its name, path and place.
Required readRequiredTarget(const std::string& file, const Value& spec, std::string_view own) {
	const Value target = spec.kind() == Kind::Vector && spec.count() > 0 ? spec.at(0) : spec;
	const Value& placed = target.place().line != 0 ? target : spec;
	Required required;
	required.line = placed.place().line;
	required.column = placed.place().column;
	if(target.kind() == Kind::String && !target.text().empty()) {
		required.path = std::string(target.text()); // the loader checks it, as an include's
	} else if(target.kind() == Kind::Symbol && isNamespaceName(target.text())) {
		required.name = std::string(target.text());
		if(std::ranges::find(languageNamespaces, required.name) == languageNamespaces.end())
			required.path = namespacePath(required.name, own);
	} else {
		failAt(file, placed,
		       "(:require ...) takes namespaces, each a name, a file's path as a string, or a vector [name :as alias]");
	}
	return required;
}

/// @brief Reads one spec of a `(:require ...)`: a name, a string, or a vector `[name :as alias :refer [word...]]`.
/// @param file The file.
/// @param spec The spec.
/// @param own The file's own namespace.
/// @return It.
Required readRequired(const std::string& file, const Value& spec, std::string_view own) {
	const bool vector = spec.kind() == Kind::Vector;
	Required required = readRequiredTarget(file, spec, own);
	for(std::size_t at = 1; vector && at < spec.count(); at += 2) {
		const Value& option = spec.at(at);
		if(option.kind() != Kind::Keyword || (option.text() != "as" && option.text() != "refer"))
			failAt(file, option, "a required namespace takes :as alias and :refer [word...]");
		if(at + 1 >= spec.count()) failAt(file, option, ":" + std::string(option.text()) + " is missing its value");
		const Value& value = spec.at(at + 1);
		if(option.text() == "refer") {
			readRefers(file, value, required.refers);
		} else if(value.kind() != Kind::Symbol || value.text().contains('/') || !required.alias.empty()) {
			failAt(file, value, ":as takes one alias, a plain word: [clojure.string :as str]");
		} else {
			required.alias = std::string(value.text());
		}
	}
	return required;
}

/// @brief Reads a `(:require spec...)` clause into a declaration.
/// @param file The file.
/// @param clause The clause.
/// @param declaration Gets its specs.
void readRequire(const std::string& file, const Value& clause, NamespaceDeclaration& declaration) {
	if(clause.kind() != Kind::List || clause.count() == 0 || clause.at(0).kind() != Kind::Keyword ||
	   clause.at(0).text() != "require")
		failAt(file, clause, "ns takes (:require ...) only");
	for(std::size_t at = 1; at < clause.count(); ++at) {
		if(declaration.required.size() == maxRequired)
			failAt(file, clause.at(at), "ns requires at most " + std::to_string(maxRequired) + " namespaces");
		declaration.required.push_back(readRequired(file, clause.at(at), declaration.name));
	}
}

} // namespace

NamespaceDeclaration readNamespaceForm(const Value& form, const std::string& file) {
	if(form.count() < 2 || form.at(1).kind() != Kind::Symbol || !isNamespaceName(form.at(1).text()))
		failAt(file, form.count() < 2 ? form : form.at(1),
		       "ns takes the file's namespace first, words between dots: (ns app.parts.board ...)");
	NamespaceDeclaration declaration;
	declaration.name = std::string(form.at(1).text());
	if(std::ranges::find(languageNamespaces, declaration.name) != languageNamespaces.end())
		failAt(file, form.at(1), declaration.name + " is the language's: name the file otherwise");
	std::size_t at = 2;
	if(at < form.count() && form.at(at).kind() == Kind::String) ++at; // its docstring
	for(; at < form.count(); ++at) readRequire(file, form.at(at), declaration);
	return declaration;
}

std::string namespacePath(std::string_view name, std::string_view own) {
	std::vector<std::string_view> segments = segmentsOf(name);
	if(segments.size() >= 2) segments.erase(segments.begin()); // the app's
	std::string path;
	for(std::size_t up = segmentsOf(own).size(); up > 2; --up) path += "../"; // own's folder to the app's
	for(const std::string_view segment : segments) path.append(segment).append(1, '/');
	path.back() = '.';
	return path + "lisp";
}

namespace {

/// The files' namespaces and definitions, by file, as applyNamespaces() works them out.
using Definitions = std::map<std::string, std::set<std::string, std::less<>>, std::less<>>;

/// @brief The program's file a require names, as the program calls it.
/// @param file The requiring file, as the program calls it.
/// @param path The path it requires, relative to it.
/// @return The file.
std::string requiredFile(const std::string& file, const std::string& path) {
	return (std::filesystem::path(file).parent_path() / path).lexically_normal().generic_string();
}

/// @brief Checks a `:refer` of the language's namespace: the plain word must already mean the same function.
/// @param machine The machine.
/// @param file The file, for errors.
/// @param required The spec.
void checkLanguageRefers(const Machine& machine, const std::string& file, const Required& required) {
	const bool bare = required.name == "clojure.core";
	for(const std::string& word : required.refers) {
		const Global* plain = machine.findLanguageGlobal(word);
		const Global* qualified = bare ? plain : machine.findLanguageGlobal(required.name + "/" + word);
		if(!qualified)
			throw LispError(file, required.line, required.column, "",
			                "\"" + notation::Lexer::excerpt(word) + "\" isn't a function of " + required.name);
		if(!plain || plain->value.kind() != qualified->value.kind() ||
		   plain->value.pointer() != qualified->value.pointer()) {
			const std::string alias =
			    !required.alias.empty() ? required.alias : (required.name == "clojure.set" ? "set" : "str");
			throw LispError(file, required.line, required.column, "",
			                "\"" + notation::Lexer::excerpt(word) + "\" isn't a plain name here: write " + alias + "/" +
			                    notation::Lexer::excerpt(word));
		}
	}
}

/// @brief Adds what one spec of a file's program files gives to its aliases, checking what it can.
/// @param machine The machine.
/// @param file The requiring file.
/// @param required The spec.
/// @param names The namespace of each file of the program, by file.
/// @param definedIn Each file's definitions.
/// @param aliases Gets the spec's aliases.
void addProgramRequire(Machine& machine, const std::string& file, const Required& required,
                       const std::map<std::string, std::string, std::less<>>& names, const Definitions& definedIn,
                       AliasMap& aliases) {
	const std::string target = requiredFile(file, required.path);
	const auto declared = names.find(target);
	if(!required.name.empty() && (definedIn.contains(target) || declared != names.end()) &&
	   (declared == names.end() || declared->second != required.name))
		throw LispError(file, required.line, required.column, "",
		                required.name + " is required, and " + target + " doesn't declare it");
	// A file required by its path is labelled by its own ns form, or by its path
	const std::string space = !required.name.empty()    ? required.name
	                          : declared != names.end() ? declared->second
	                                                    : target;
	if(!machine.namespaces().programSpace(space)) {
		const auto defined = definedIn.find(target);
		machine.namespaces().addProgramSpace(
		    space, {target, defined != definedIn.end() ? defined->second : std::set<std::string, std::less<>>{}});
	}
	const ProgramSpace& program = *machine.namespaces().programSpace(space);
	for(const std::string& word : required.refers)
		if(!program.names.contains(word))
			throw LispError(file, required.line, required.column, "",
			                ":refer " + notation::Lexer::excerpt(word) + ": \"" + notation::Lexer::excerpt(word) +
			                    "\" isn't defined in " + program.file);
	if(!required.name.empty()) aliases.insert_or_assign(required.name, space);
	if(!required.alias.empty()) aliases.insert_or_assign(required.alias, space);
}

} // namespace

void applyNamespaces(Machine& machine, std::span<const std::pair<std::string, std::string>> declared,
                     const Definitions& definedIn) {
	machine.namespaces().clearProgram();
	if(declared.empty()) return;
	std::vector<std::pair<const std::string*, NamespaceDeclaration>> read;
	std::map<std::string, std::string, std::less<>> names; // each file's namespace
	for(const auto& [file, text] : declared) {
		const std::optional<Value> form = readEntry(machine.heap, text, file, 1, 1);
		if(!form || headWord(*form) != "ns") throw LispError(file, 1, 1, "ns", "a file's ns entry is one ns form");
		read.emplace_back(&file, readNamespaceForm(*form, file));
		names.emplace(file, read.back().second.name);
	}
	for(const auto& [file, declaration] : read) {
		AliasMap aliases;
		for(const Required& required : declaration.required) {
			if(!required.path.empty()) {
				addProgramRequire(machine, *file, required, names, definedIn, aliases);
				continue;
			}
			checkLanguageRefers(machine, *file, required);
			if(!required.alias.empty()) aliases.insert_or_assign(required.alias, required.name);
		}
		machine.namespaces().setFileAliases(*file, std::move(aliases));
	}
}

void declareNamespace(Machine& machine, const Value& form, const std::string& file) {
	const NamespaceDeclaration declaration = readNamespaceForm(form, file);
	AliasMap aliases;
	for(const Required& required : declaration.required) {
		if(!required.path.empty())
			throw LispError(file, required.line, required.column, "", "a file is required by the app that loads it");
		checkLanguageRefers(machine, file, required);
		if(!required.alias.empty()) aliases.insert_or_assign(required.alias, required.name);
	}
	machine.namespaces().setFileAliases(file, std::move(aliases));
}

} // namespace affresco::lisp
