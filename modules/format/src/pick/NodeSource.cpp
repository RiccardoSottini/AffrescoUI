/// @file
/// @brief Where a node is written: sourceOf() (Runtime::source()), and the `source` of a pick record. For a program,
/// the definition that built the node (lisp::nodeOrigins(), a second build run only when asked); for a JSON document,
/// the node itself and the file that holds it (Runtime::setFileOf()). It reads the versioned document, never a file.

#include "Composition.hpp"
#include "Engine.hpp"
#include "Picks.hpp"

#include "affresco/format/NodeIds.hpp"

#include <affresco/lisp/Document.hpp>

#include <cstdint>

namespace affresco::format {

namespace {

constexpr std::size_t maxAt = 512;           ///< The longest `at`, in bytes: an id, a pointer or an address is shorter.
constexpr std::size_t maxSourceText = 16384; ///< A source's text is cut to this many bytes.
constexpr std::string_view programPrefix = "/program/"; ///< How a definition's address starts.

/// @brief A key as a token of a JSON Pointer (RFC 6901): '~' becomes "~0" and '/' "~1".
/// @param key The key.
/// @return The token.
std::string pointerToken(std::string_view key) {
	std::string token;
	token.reserve(key.size());
	for(const char c : key) {
		if(c == '~') token += "~0";
		else if(c == '/') token += "~1";
		else token += c;
	}
	return token;
}

/// @brief A definition's address.
/// @param file The program's file, the key under `/program`.
/// @param definition The entry's name.
/// @return `/program/<file>/<definition>`, escaped as a JSON Pointer.
std::string addressOf(std::string_view file, std::string_view definition) {
	return std::string(programPrefix) + pointerToken(file) + "/" + pointerToken(definition);
}

/// @brief A text cut to a size at a character boundary.
/// @param text The text.
/// @param most The most bytes.
/// @param cut Set to true when it was cut.
/// @return The text.
std::string cutText(std::string text, std::size_t most, bool& cut) {
	cut = text.size() > most;
	return cutToCharacter(std::move(text), most);
}

/// @brief Where a definition's form starts in its text, and its head.
struct Head {
	std::string kind;         ///< The form's first word: `defview`, `defn`...; empty if the text holds no form.
	std::uint32_t line = 1;   ///< The line of its '(', from 1.
	std::uint32_t column = 1; ///< Its column, in characters, from 1.
};

/// @brief Reads where a definition's first form starts, past spaces and `;` comments, and its first word. Nothing is
/// run: it reads characters only.
/// @param text The definition's text.
/// @return The head.
Head headOf(std::string_view text) {
	Head head;
	std::size_t i = 0;
	for(; i < text.size() && text[i] != '('; ++i) {
		if(text[i] == ';') {
			while(i + 1 < text.size() && text[i + 1] != '\n') ++i;
		} else if(text[i] == '\n') {
			++head.line;
			head.column = 1;
		} else if((static_cast<unsigned char>(text[i]) & 0xC0U) != 0x80U) {
			++head.column;
		}
	}
	if(i >= text.size()) return head;
	std::size_t end = ++i;
	while(end < text.size() && text[end] != ' ' && text[end] != '\n' && text[end] != '\t' && text[end] != '(' &&
	      text[end] != ')')
		++end;
	head.kind = std::string(text.substr(i, end - i));
	return head;
}

/// @brief A definition's text in the versioned document.
/// @param engine The runtime.
/// @param file The program's file.
/// @param definition The entry's name.
/// @return The text, or nullptr.
const std::string* definitionText(const Engine& engine, const std::string& file, const std::string& definition) {
	const json& source = engine.versions.sourceDocument();
	const auto program = source.find("program");
	if(program == source.end() || !program->is_object()) return nullptr;
	const auto entries = program->find(file);
	if(entries == program->end() || !entries->is_object()) return nullptr;
	const auto text = entries->find(definition);
	return text != entries->end() && text->is_string() ? &text->get_ref<const std::string&>() : nullptr;
}

/// @brief The source of a definition's address.
/// @param engine The runtime.
/// @param at `/program/<file>/<name>`.
/// @return The source, without node and pointer: an address names a definition, not a node.
/// @throws DocumentError If the document has no such definition.
json definitionSource(const Engine& engine, std::string_view at) {
	const std::string where(at);
	std::string file;
	std::string definition;
	try {
		json::json_pointer pointer{where};
		definition = pointer.back();
		pointer.pop_back();
		file = pointer.back();
		pointer.pop_back();
		if(pointer.to_string() != "/program") fail(where, "expected /program/<file>/<name>");
	} catch(const json::exception&) {
		fail(where, "expected /program/<file>/<name>");
	}
	const std::string* text = definitionText(engine, file, definition);
	if(!text) fail(where, "no definition here");
	const Head head = headOf(*text);
	bool cut = false;
	json source = {{"notation", "lisp"},
	               {"address", addressOf(file, definition)},
	               {"kind", head.kind},
	               {"file", file},
	               {"line", head.line},
	               {"column", head.column},
	               {"shared", file.starts_with("../")},
	               {"text", cutText(*text, maxSourceText, cut)}};
	if(cut) source["cut"] = true;
	return source;
}

/// @brief Where a program built a node, by its id (lisp::nodeOrigins(): kept for a few sections, built when asked).
/// @param engine The runtime.
/// @param id The node's id.
/// @return Its origin, or std::nullopt for a document without a program, or a node the program didn't build.
std::optional<lisp::NodeOrigin> originOf(const Engine& engine, std::string_view id) {
	const json& source = engine.versions.sourceDocument();
	const auto program = source.find("program");
	if(id.empty() || program == source.end() || !program->is_object()) return std::nullopt;
	const std::shared_ptr<const lisp::NodeOrigins> origins = lisp::nodeOrigins(*program);
	if(!origins) return std::nullopt;
	const auto found = origins->find(id);
	if(found == origins->end()) return std::nullopt;
	return found->second;
}

/// @brief The `source` of a node a program built.
/// @param origin Where it built it.
/// @return `{"address", "kind", "file", "line", "column", "shared"}`.
json sourceOfOrigin(const lisp::NodeOrigin& origin) {
	return {{"address", addressOf(origin.file, origin.definition)},
	        {"kind", origin.kind},
	        {"file", origin.file},
	        {"line", origin.line},
	        {"column", origin.column},
	        {"shared", origin.file.starts_with("../")}};
}

/// @brief The node a JSON Pointer names in the document.
/// @param document The document.
/// @param at The pointer.
/// @return The node, or nullptr if the pointer is malformed, leads nowhere, or not to an object.
const json* nodeAt(const json& document, const std::string& at) {
	try {
		const json::json_pointer pointer{at};
		if(!document.contains(pointer)) return nullptr;
		const json& node = document.at(pointer);
		return node.is_object() ? &node : nullptr;
	} catch(const json::exception&) {
		return nullptr;
	}
}

} // namespace

std::optional<std::string> nodePointer(const json& document, std::string_view id) {
	if(id.empty()) return std::nullopt;
	if(std::optional<std::string> found = findNodeWithId(document, id)) return found;
	return findFeatureNode(document, id);
}

std::optional<json> programSource(const Engine& engine, std::string_view id) {
	const std::optional<lisp::NodeOrigin> origin = originOf(engine, id);
	return origin ? std::optional<json>(sourceOfOrigin(*origin)) : std::nullopt;
}

std::optional<std::string> definitionName(const Engine& engine, std::string_view id) {
	std::optional<lisp::NodeOrigin> origin = originOf(engine, id);
	return origin ? std::optional<std::string>(std::move(origin->definition)) : std::nullopt;
}

json documentSource(const Engine& engine, std::string_view node, std::string_view pointer) {
	json source = {{"address", node}};
	if(!engine.fileOf) return source;
	const FileOf file = engine.fileOf(pointer);
	if(file.name.empty()) return source;
	source["file"] = file.name;
	source["shared"] = file.shared;
	return source;
}

json sourceOf(Engine& engine, std::string_view at) {
	const std::string where(at.substr(0, maxAt));
	if(at.size() > maxAt) fail(where, "is longer than " + std::to_string(maxAt) + " bytes");
	if(at.starts_with(programPrefix)) return definitionSource(engine, at);

	const json& document = engine.versions.document();
	std::string pointer;
	std::string node = where;
	if(at.starts_with('#')) {
		std::optional<std::string> found = nodePointer(document, at.substr(1));
		if(!found) fail(where, "no node with this id");
		pointer = std::move(*found);
	} else if(at.starts_with('/')) {
		pointer = where;
	} else {
		fail(where, R"(expected a node ("#id"), a JSON Pointer or a definition's address ("/program/<file>/<name>"))");
	}
	const json* written = nodeAt(document, pointer);
	if(!written) fail(where, "no node here");
	const auto id = written->find("id");
	const bool hasId = id != written->end() && id->is_string() && !id->get_ref<const std::string&>().empty();
	if(hasId && !at.starts_with('#')) node = "#" + id->get<std::string>();

	const std::optional<lisp::NodeOrigin> origin =
	    hasId ? originOf(engine, id->get_ref<const std::string&>()) : std::nullopt;
	json source = origin ? sourceOfOrigin(*origin) : documentSource(engine, node, pointer);
	const std::string* definition = origin ? definitionText(engine, origin->file, origin->definition) : nullptr;
	std::string text = origin ? (definition ? *definition : std::string()) : written->dump(1, '\t');
	bool cut = false;
	source["node"] = node;
	source["pointer"] = pointer;
	source["notation"] = origin ? "lisp" : "json";
	source["text"] = cutText(std::move(text), maxSourceText, cut);
	if(cut) source["cut"] = true;
	return source;
}

} // namespace affresco::format
