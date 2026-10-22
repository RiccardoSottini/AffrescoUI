/// @file
/// @brief Migrating version 1 documents to version 2 (Migrate.hpp): one walk over the document that renames the older
/// keys of rows, columns and stacks, rewrites `len(` in expressions, and follows the patches inside it.

#include "affresco/format/Migrate.hpp"

#include "ExpressionMachine.hpp"
#include "Reads.hpp"
#include "affresco/format/NodeIds.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>

namespace affresco::format {

namespace {

/// Styles naming styles that the migration follows to find a node's type, as the runtime allows.
constexpr std::size_t maxStyleDepth = 16;

/// The formats a document can have, with their kinds.
constexpr std::array<std::pair<std::string_view, DocumentKind>, 5> formats = {
    {{"affrescoui-screen", DocumentKind::Screen},
     {"affrescoui-scenarios", DocumentKind::Scenarios},
     {"affrescoui-changes", DocumentKind::Changes},
     {"affrescoui-feature", DocumentKind::Feature},
     {"affrescoui-catalog", DocumentKind::Catalog}}};

/// The sections only a screen or its include part has: a document without a "format" that has one is a part.
constexpr std::array<std::string_view, 6> screenSections = {"root", "screens", "templates", "components", "styles",
                                                            "actions"};

/// The members of a screen whose values are data, not content: nothing in them is rewritten.
constexpr std::array<std::string_view, 2> dataSections = {"state", "strings"};

/// @brief Tells whether a character can be part of a name in an expression, or end the path before one.
/// @param c The character.
/// @return true for a letter, a digit, '_', '$' or '.'.
bool namePart(char c) noexcept {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$' || c == '.';
}

/// @brief Rewrites the calls of `len` in one expression to `length`, outside quoted text.
/// @param expression The expression, between `{{` and `}}`.
/// @return It rewritten, or nothing if it has no call of `len`.
std::optional<std::string> renameLen(std::string_view expression) {
	std::string result;
	bool changed = false;
	char quote = 0;
	for(std::size_t i = 0; i < expression.size(); ++i) {
		const char c = expression[i];
		if(quote) {
			result += c;
			if(c == '\\' && i + 1 < expression.size()) result += expression[++i];
			else if(c == quote) quote = 0;
			continue;
		}
		if(c == '\'' || c == '"') quote = c;
		if(expression.substr(i).starts_with("len") && (i == 0 || !namePart(expression[i - 1]))) {
			std::size_t next = i + 3;
			while(next < expression.size() && (expression[next] == ' ' || expression[next] == '\t')) ++next;
			if(next < expression.size() && expression[next] == '(') {
				result += "length";
				i += 2;
				changed = true;
				continue;
			}
		}
		result += c;
	}
	if(!changed) return std::nullopt;
	return result;
}

/// @brief Rewrites the calls of `len` in the placeholders of a string.
/// @param text The string.
/// @return It rewritten, or nothing if none of its placeholders calls `len`.
std::optional<std::string> renameLenIn(std::string_view text) {
	std::string result;
	bool changed = false;
	std::size_t done = 0;
	for(std::size_t start; (start = text.find("{{", done)) != std::string_view::npos;) {
		const std::size_t end = detail::closing(text, start + 2);
		if(end == std::string_view::npos) break;
		const std::string_view expression = text.substr(start + 2, end - start - 2);
		result += text.substr(done, start + 2 - done);
		if(const auto renamed = renameLen(expression)) {
			result += *renamed;
			changed = true;
		} else result += expression;
		done = end;
	}
	if(!changed) return std::nullopt;
	result += text.substr(done);
	return result;
}

/// @brief Tells whether a patch's path names a key of a node or a style, whose "from" is a row's, a column's or a
/// stack's: a node place (isNodePlace()), "#id" or a style ("/styles/<name>").
/// @param holder The path without its last segment.
/// @return true if it does.
bool holdsLayoutKeys(std::string_view holder) {
	if(holder.starts_with('#')) return holder.find('/') == std::string_view::npos;
	if(holder.starts_with("/styles/")) return holder.find('/', 8) == std::string_view::npos;
	return isNodePlace(holder);
}

/// @brief Tells whether a path of a patch is inside a screen's data sections.
/// @param path The path.
/// @return true if it is "/state", "/strings" or inside one.
bool inData(std::string_view path) {
	for(std::string_view section : dataSections)
		if(path.size() > section.size() && path[0] == '/' && path.substr(1, section.size()) == section &&
		   (path.size() == section.size() + 1 || path[section.size() + 1] == '/'))
			return true;
	return false;
}

/// @brief One migration: the walk and what it found.
class Migration {
public:
	/// @brief Starts one.
	/// @param styles The document's styles, to find a node's type through its style (null if it has none).
	explicit Migration(const json& styles) : styles_(styles) {}

	/// @brief Migrates a value and everything in it.
	/// @param value The value.
	/// @param pointer Its JSON Pointer in the document as it was.
	/// @param document Whether it is a whole screen (or include part): its data sections are left.
	void value(json& value, const std::string& pointer, bool document = false) {
		if(value.is_string()) {
			if(auto renamed = renameLenIn(value.get_ref<const std::string&>()))
				replace(value, pointer, json(std::move(*renamed)), "len( is length( in version 2");
		} else if(value.is_array()) {
			for(std::size_t i = 0; i < value.size(); ++i) this->value(value[i], pointer + "/" + std::to_string(i));
		} else if(value.is_object()) {
			object(value, pointer, document);
		}
	}

	/// @brief Migrates a patch: its paths, then its values.
	/// @param patch The operations.
	/// @param pointer Its JSON Pointer.
	void patch(json& patch, const std::string& pointer) {
		if(!patch.is_array()) return;
		for(std::size_t i = 0; i < patch.size(); ++i)
			if(patch[i].is_object()) operation(patch[i], pointer + "/" + std::to_string(i));
	}

	/// @return What changed.
	std::vector<Rewrite>& rewrites() noexcept { return rewrites_; }

private:
	/// @brief Records that a value is replaced, and replaces it.
	/// @param value The value.
	/// @param pointer Its JSON Pointer.
	/// @param replacement The new value.
	/// @param what What changed, for the message.
	void replace(json& value, const std::string& pointer, json replacement, std::string_view what) {
		rewrites_.push_back({Rewrite::Kind::Replace, pointer, "", replacement, placeOf(pointer) + ": " + std::string(what)});
		value = std::move(replacement);
	}

	/// @brief The type of a node, as it gives it or through its style.
	/// @param node The node.
	/// @return Its type, or "" if neither gives one.
	std::string typeOf(const json& node) const {
		const json* styled = &node;
		for(std::size_t depth = 0; depth < maxStyleDepth; ++depth) {
			if(const auto type = styled->find("type"); type != styled->end() && type->is_string())
				return type->get<std::string>();
			const auto style = styled->find("style");
			if(style == styled->end() || !style->is_string() || !styles_.is_object() || !styles_.contains(*style))
				break;
			styled = &styles_[style->get<std::string>()];
			if(!styled->is_object()) break;
		}
		return "";
	}

	/// @brief Gives a member its version 2 key, or removes it if the node already has that key.
	/// @param object The node or the style.
	/// @param pointer Its JSON Pointer.
	/// @param from The version 1 key.
	/// @param to The version 2 key.
	void rename(json& object, const std::string& pointer, const std::string& from, const std::string& to) {
		if(!object.contains(from)) return;
		std::string at = pointer;
		appendKey(at, from);
		const std::string place = placeOf(pointer).empty() ? from : placeOf(pointer) + "." + from;
		if(object.contains(to)) {
			rewrites_.push_back({Rewrite::Kind::Remove, at, "", json(),
			                     place + ": removed (\"" + from + "\" is \"" + to + "\" in version 2, which it has)"});
			object.erase(from);
			return;
		}
		rewrites_.push_back(
		    {Rewrite::Kind::Rename, at, to, json(), place + ": \"" + from + "\" is \"" + to + "\" in version 2"});
		json renamed = json::object();
		for(auto& [key, member] : object.items()) renamed[key == from ? to : key] = std::move(member);
		object = std::move(renamed);
	}

	/// @brief Migrates an object: a node's or a style's older keys, a feature's "requires", a patch operation, then its
	/// members.
	/// @param object The object.
	/// @param pointer Its JSON Pointer.
	/// @param document Whether it is a whole screen: its data sections are left.
	void object(json& object, const std::string& pointer, bool document) {
		if(object.contains("op") && object["op"].is_string() && object.contains("path") && object["path"].is_string()) {
			operation(object, pointer);
			return;
		}
		if(document && !pointer.empty() && documentVersion(object) == 1)
			replace(object["version"], pointer + "/version", json(formatVersion), "version: 2");
		const std::string type = typeOf(object);
		if(type == "row" || type == "column" || type == "stack") rename(object, pointer, "from", "justify");
		if(type == "stack") rename(object, pointer, "align", "justify");
		if(const auto requires_ = object.find("requires"); requires_ != object.end() && requires_->is_object()) {
			if(const auto format = requires_->find("format"); format != requires_->end() && *format == 1)
				replace(*format, pointer + "/requires/format", json(formatVersion), "requires format 2");
		}
		for(auto& [key, member] : object.items()) {
			if(document && std::ranges::find(dataSections, key) != dataSections.end()) continue;
			std::string at = pointer;
			appendKey(at, key);
			if(key == "styles" && document && member.is_object()) {
				for(auto& [name, style] : member.items())
					if(style.is_object()) {
						std::string styleAt = at;
						appendKey(styleAt, name);
						if(!style.contains("type")) rename(style, styleAt, "from", "justify");
						value(style, styleAt);
					}
				continue;
			}
			if(key == "revision" && member.is_object()) compactEdits(member, at);
			value(member, at, key == "screen" && member.is_object());
		}
	}

	/// @brief The version 2 path of a version 1 path that ends in a node's or a style's "from".
	/// @param path The path.
	/// @return It, ending in "justify", or nothing if it doesn't end in such a "from".
	static std::optional<std::string> renamedPath(const std::string& path) {
		const std::size_t last = path.rfind('/');
		if(last == std::string::npos || path.substr(last + 1) != "from" || !holdsLayoutKeys(path.substr(0, last)))
			return std::nullopt;
		return path.substr(0, last) + "/justify";
	}

	/// @brief Migrates a revision's compact edits' paths: the keys of "set", "add" and "move", the items of "remove"
	/// and the values of "move", and the values of all three.
	/// @param revision The revision.
	/// @param pointer Its JSON Pointer.
	void compactEdits(json& revision, const std::string& pointer) {
		for(const char* edits : {"set", "add", "move"}) {
			const auto found = revision.find(edits);
			if(found == revision.end() || !found->is_object()) continue;
			const std::string at = pointer + "/" + edits;
			json renamed = json::object();
			for(auto& [key, member] : found->items()) {
				std::string memberAt = at;
				appendKey(memberAt, key);
				value(member, memberAt); // here, while its pointer is the one the text has
				if(std::string(edits) == "move" && member.is_string())
					if(auto to = renamedPath(member.get<std::string>()))
						replace(member, memberAt, json(*to), "\"from\" is \"justify\" in version 2");
				const auto to = renamedPath(key);
				if(to && !found->contains(*to))
					rewrites_.push_back({Rewrite::Kind::Rename, memberAt, *to, json(),
					                     placeOf(at) + ": \"" + key + "\" is \"" + *to + "\" in version 2"});
				renamed[to && !found->contains(*to) ? *to : key] = std::move(member);
			}
			*found = std::move(renamed);
		}
		if(const auto remove = revision.find("remove"); remove != revision.end() && remove->is_array())
			for(std::size_t i = 0; i < remove->size(); ++i)
				if((*remove)[i].is_string())
					if(auto to = renamedPath((*remove)[i].get<std::string>()))
						replace((*remove)[i], pointer + "/remove/" + std::to_string(i), json(*to),
						        "\"from\" is \"justify\" in version 2");
	}

	/// @brief Migrates one patch operation: a path (and a move's "from") that ends in a node's "from", and its value.
	/// @param op The operation.
	/// @param pointer Its JSON Pointer.
	void operation(json& op, const std::string& pointer) {
		const auto renamePath = [&](const char* key) {
			const auto path = op.find(key);
			if(path == op.end() || !path->is_string()) return;
			if(auto to = renamedPath(path->get<std::string>()))
				replace(*path, pointer + "/" + key, json(*to), "\"from\" is \"justify\" in version 2");
		};
		const std::string path = op["path"].get<std::string>();
		renamePath("path");
		if(op["op"] == "move" || op["op"] == "copy") renamePath("from");
		if(op.contains("value") && !inData(path)) value(op["value"], pointer + "/value", path.empty());
	}

	const json& styles_;             ///< The document's styles.
	std::vector<Rewrite> rewrites_; ///< What changed.
};

} // namespace

DocumentKind documentKind(const json& document) noexcept {
	if(!document.is_object()) return DocumentKind::Other;
	const auto format = document.find("format");
	if(format == document.end())
		return std::ranges::any_of(screenSections, [&](std::string_view key) { return document.contains(key); })
		           ? DocumentKind::Screen
		           : DocumentKind::Other;
	for(const auto& [name, kind] : formats)
		if(*format == name) return kind;
	return DocumentKind::Other;
}

int documentVersion(const json& document) noexcept {
	if(!document.is_object()) return 0;
	const auto version = document.find("version");
	return version != document.end() && version->is_number_integer() ? version->get<int>() : 0;
}

std::vector<Rewrite> migrate(json& document) {
	const DocumentKind kind = documentKind(document);
	if(kind == DocumentKind::Other || documentVersion(document) > 1) return {};
	static const json none;
	const json styles = document.value("styles", json());
	Migration migration(styles.is_object() ? styles : none);
	if(kind != DocumentKind::Catalog) migration.value(document, "", kind == DocumentKind::Screen);
	if(documentVersion(document) == 1) {
		migration.rewrites().push_back({Rewrite::Kind::Replace, "/version", "", json(formatVersion), "version: 2"});
		document["version"] = formatVersion;
	}
	return std::move(migration.rewrites());
}

std::vector<Rewrite> migratePatch(json& patch, const std::string& pointer) {
	static const json none;
	Migration migration(none);
	migration.patch(patch, pointer);
	return std::move(migration.rewrites());
}

std::string removedKeyHint(std::string_view type, std::string_view key) {
	if(key == "from" && (type.empty() || type == "row" || type == "column" || type == "stack"))
		return "'from' was removed in version 2: use 'justify'";
	if(key == "align" && type == "stack") return "'align' on a stack was removed in version 2: use 'justify'";
	return "";
}

std::string removedFunctionHint(std::string_view name) {
	return name == "len" ? "'len' was removed in version 2: use 'length'" : "";
}

} // namespace affresco::format
