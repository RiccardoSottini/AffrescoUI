/// @file
/// @brief The A2UI 0.8 adapter: messages, the data model and bound values (A2ui.hpp), and fromA2ui().

#include "A2ui.hpp"

#include <algorithm>

namespace affresco::format::adapters {

namespace detail {

using expect::fail;

namespace {

/// @brief A data model path's names: `/a/b` is {"a", "b"}.
/// @param path The path.
/// @return Its names (none for `/` or an empty path).
[[nodiscard]] std::vector<std::string> segmentsOf(const std::string& path) {
	std::vector<std::string> segments;
	std::size_t start = 0;
	while(start <= path.size()) {
		const std::size_t end = std::min(path.find('/', start), path.size());
		if(end > start) segments.push_back(path.substr(start, end - start));
		start = end + 1;
	}
	return segments;
}

/// @brief The place of a data model path, made on the way: only objects are walked into, at most maxDepth deep.
/// @param model The data model.
/// @param segments The path's names.
/// @param at The path's place in the source, for the error.
/// @return The value at the path (null if it is new).
/// @throws DocumentError If the path is too deep, or goes through a value that isn't an object.
[[nodiscard]] json& placeOf(json& model, const std::vector<std::string>& segments, const std::string& at) {
	if(segments.size() > static_cast<std::size_t>(maxDepth))
		fail(at, "nests deeper than " + std::to_string(maxDepth) + " names");
	json* place = &model;
	for(const std::string& segment : segments) {
		if(!place->is_null() && !place->is_object()) fail(at, "goes through a value that isn't an object");
		place = &(*place)[stateKey(segment)];
	}
	return *place;
}

/// @brief Whether every key of an object is 0, 1, 2... in order: A2UI writes a list as such a map.
/// @param object The object.
/// @return Whether it is a list.
[[nodiscard]] bool listLike(const json& object) {
	std::size_t index = 0;
	for(const auto& [key, value] : object.items())
		if(key != std::to_string(index++)) return false;
	return !object.empty();
}

/// @brief The data model entries of a dataModelUpdate as JSON.
/// @param contents `[{"key", "valueString" | "valueNumber" | "valueBoolean" | "valueMap"}]`.
/// @param at Their place.
/// @param out The screen, for the limits.
/// @param depth How deeply they nest.
/// @return An object (or a list, for keys 0, 1, 2...).
[[nodiscard]] json entries(const json& contents, const std::string& at, Output& out, int depth) {
	if(!contents.is_array()) fail(at, "expected a list of entries");
	json object = json::object();
	for(std::size_t i = 0; i < contents.size(); ++i) {
		const std::string place = at + "[" + std::to_string(i) + "]";
		const json& entry = contents[i];
		out.enter(place, depth);
		if(!entry.is_object() || !entry.contains("key") || !entry["key"].is_string()) fail(place, "expected {key, value...}");
		json& value = object[stateKey(entry["key"].get<std::string>())];
		if(entry.contains("valueString")) value = expect::string(entry["valueString"], place + ".valueString");
		else if(entry.contains("valueNumber")) value = expect::number(entry["valueNumber"], place + ".valueNumber");
		else if(entry.contains("valueBoolean")) value = expect::boolean(entry["valueBoolean"], place + ".valueBoolean");
		else if(entry.contains("valueMap")) value = entries(entry["valueMap"], place + ".valueMap", out, depth + 1);
	}
	if(listLike(object)) {
		json list = json::array();
		for(auto& [key, value] : object.items()) list.push_back(std::move(value));
		return list;
	}
	return object;
}

/// @brief A JSON Pointer to a place in the state.
/// @param segments The names below `state.model`.
/// @return `/state/model/a/b`.
[[nodiscard]] std::string pointerOf(const std::vector<std::string>& segments) {
	std::string pointer = "/state/model";
	for(const std::string& segment : segments) pointer += "/" + stateKey(segment);
	return pointer;
}

} // namespace

void A2uiConverter::message(const json& message, const std::string& at) {
	if(!message.is_object() || message.size() != 1)
		fail(at, "expected one of surfaceUpdate, dataModelUpdate, beginRendering, deleteSurface");
	const std::string kind = message.begin().key();
	const json& body = message.begin().value();
	const std::string place = at + "." + kind;
	if(!body.is_object()) fail(place, "expected an object");
	if(kind == "surfaceUpdate") return surfaceUpdate(body, place);
	if(kind == "dataModelUpdate") return dataModelUpdate(body, place);
	if(kind == "beginRendering") {
		if(!ours(body, place)) return;
		root_ = expect::string(body.value("root", json()), place + ".root");
		if(body.contains("styles")) out_.warn(place + ".styles", "left out: the screen keeps its own palette");
		if(!document_) document_ = out_.screen(buildRoot(), "A2UI", "Converted from A2UI " + std::string(a2uiVersion));
		return;
	}
	if(kind == "deleteSurface") {
		if(ours(body, place)) out_.warn(place, "left out: the screen stays as it was");
		return;
	}
	fail(at, "unknown message '" + kind + "'");
}

bool A2uiConverter::ours(const json& body, const std::string& at) {
	const std::string id = body.contains("surfaceId") ? expect::string(body["surfaceId"], at + ".surfaceId") : "";
	if(surface_.empty()) surface_ = id.empty() ? "main" : id;
	if(id.empty() || id == surface_) return true;
	out_.warn(at, "left out: only the first surface ('" + surface_ + "') is converted");
	return false;
}

void A2uiConverter::surfaceUpdate(const json& update, const std::string& at) {
	if(!ours(update, at)) return;
	const json& components = update.value("components", json());
	if(!components.is_array()) fail(at + ".components", "expected a list of components");
	for(std::size_t i = 0; i < components.size(); ++i) {
		const std::string place = at + ".components[" + std::to_string(i) + "]";
		const json& entry = components[i];
		out_.enter(place, 1);
		if(!entry.is_object() || !entry.contains("id") || !entry.contains("component") ||
		   !entry["component"].is_object() || entry["component"].size() != 1)
			fail(place, R"(expected {"id", "component": {"Type": {...}}})");
		components_[expect::string(entry["id"], place + ".id")] = entry["component"];
	}
	if(!document_) return;
	json root = out_.root(buildRoot());
	json revision = {{"type", "revision"}, {"message", "A2UI surfaceUpdate"}, {"set", {{"/root", std::move(root)}}}};
	if(out_.state().contains("literals")) revision["set"]["/state/literals"] = out_.state()["literals"];
	revisions_.push_back(std::move(revision));
}

void A2uiConverter::dataModelUpdate(const json& update, const std::string& at) {
	if(!ours(update, at)) return;
	const std::vector<std::string> segments =
	    segmentsOf(update.contains("path") ? expect::string(update["path"], at + ".path") : "/");
	const json value = entries(update.value("contents", json()), at + ".contents", out_, 1);
	json set = json::object();
	json* place = &placeOf(out_.state()["model"], segments, at + ".path");
	if(segments.empty()) { // the root: each key, so keys not named stay
		for(const auto& [key, item] : value.items()) {
			(*place)[key] = item;
			set[pointerOf({key})] = item;
		}
	} else {
		*place = value;
		set[pointerOf(segments)] = value;
	}
	if(document_ && !set.empty())
		revisions_.push_back({{"type", "revision"}, {"message", "A2UI dataModelUpdate"}, {"set", std::move(set)}});
}

Built A2uiConverter::buildRoot() {
	out_.newBuild();
	visiting_.clear();
	return build(root_.empty() ? "root" : root_, out_.contentWidth(), {});
}

Converted A2uiConverter::finish() {
	if(components_.empty()) fail("a2ui", "no components (expected a surfaceUpdate)");
	if(!document_) {
		if(root_.empty() && !components_.contains("root")) root_ = components_.begin()->first;
		out_.warn("a2ui", "no beginRendering: rendered from '" + (root_.empty() ? "root" : root_) + "'");
		document_ = out_.screen(buildRoot(), "A2UI", "Converted from A2UI " + std::string(a2uiVersion));
	}
	for(json& revision : revisions_) revision["author"] = "a2ui";
	return {std::move(*document_), std::move(out_.warnings()), std::move(revisions_)};
}

BoundValue A2uiConverter::bound(const json& value, const std::string& at, const Scope& scope) {
	BoundValue result;
	if(!value.is_object()) return result;
	json literal;
	if(value.contains("literalString")) literal = expect::string(value["literalString"], at + ".literalString");
	else if(value.contains("literalNumber")) literal = expect::number(value["literalNumber"], at + ".literalNumber");
	else if(value.contains("literalBoolean"))
		literal = expect::boolean(value["literalBoolean"], at + ".literalBoolean");
	if(!value.contains("path")) {
		result.characters = textOf(literal).size();
		result.value = literal.is_string() ? out_.literal(literal.get<std::string>()) : literal;
		return result;
	}
	const std::string path = expect::string(value["path"], at + ".path");
	const std::string expression = pathExpression(path, scope);
	result.value = "{{" + expression + "}}";
	if(expression.starts_with("state.")) result.path = expression;
	if(expression.starts_with("state.") && !literal.is_null()) { // A2UI: a path with a literal starts it there
		json& place = placeOf(out_.state()["model"], segmentsOf(path), at + ".path");
		if(place.is_null()) place = literal;
	}
	result.characters = std::max<std::size_t>(textOf(literal).size(), 16);
	return result;
}

std::string A2uiConverter::pathExpression(const std::string& path, const Scope& scope) {
	std::vector<std::string> segments = segmentsOf(path);
	if(scope.inTemplate && !path.starts_with('/')) {
		std::string expression = "item";
		for(const std::string& segment : segments) expression += "." + stateKey(segment);
		return expression;
	}
	segments.insert(segments.begin(), "model");
	return statePath(segments);
}

std::optional<std::string> A2uiConverter::bindable(const BoundValue& value, const std::string& id,
                                                   const json& initial) {
	if(value.path) return value.path;
	if(value.value.is_string() && value.value.get_ref<const std::string&>().starts_with("{{item")) return std::nullopt;
	json& input = out_.state()["inputs"][stateKey(id)];
	input = value.value.is_null() ? initial : value.value;
	return "state.inputs." + stateKey(id);
}

} // namespace detail

Converted fromA2ui(std::string_view text, const Options& options) {
	if(text.size() > maxInputBytes) expect::fail("a2ui", "larger than " + std::to_string(maxInputBytes) + " bytes");
	detail::A2uiConverter converter(options);
	std::optional<json> whole;
	try {
		whole = parseUntrusted(text);
	} catch(const json::parse_error&) { // JSON Lines: one message per line
	}
	if(whole && whole->is_array()) {
		if(whole->size() > maxElements) expect::fail("a2ui", "more than " + std::to_string(maxElements) + " messages");
		for(std::size_t i = 0; i < whole->size(); ++i) converter.message((*whole)[i], "a2ui[" + std::to_string(i) + "]");
		return converter.finish();
	}
	if(whole) {
		converter.message(*whole, "a2ui");
		return converter.finish();
	}
	std::size_t line = 0;
	for(std::size_t start = 0; start < text.size(); ++line) {
		const std::size_t end = std::min(text.find('\n', start), text.size());
		const std::string_view content = text.substr(start, end - start);
		start = end + 1;
		if(content.find_first_not_of(" \t\r") == std::string_view::npos) continue;
		const std::string at = "a2ui line " + std::to_string(line + 1);
		if(line >= maxElements) expect::fail(at, "more than " + std::to_string(maxElements) + " messages");
		try {
			converter.message(parseUntrusted(content), at);
		} catch(const json::parse_error& error) {
			expect::fail(at, std::string("not JSON: ") + error.what());
		}
	}
	return converter.finish();
}

} // namespace affresco::format::adapters
