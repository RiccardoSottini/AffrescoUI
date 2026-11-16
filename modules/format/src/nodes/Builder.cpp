/// @file
/// @brief format::Builder: components, styles, repeats, scoped variables and "when", on nodes compiled once per
/// version of the document.

#include "Builder.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Tells whether a character can be in a name of a path: a letter or '_', or a digit after the first.
/// @param c The character.
/// @param first Whether it is the name's first.
/// @return true if it can.
bool isNameChar(char c, bool first) {
	const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	return c == '_' || letter || (!first && c >= '0' && c <= '9');
}

/// @brief Reads a name in a path, moving past it.
/// @param path The path.
/// @param i Where the name starts; then where it ends.
/// @return The name, or "" if none starts there.
std::string_view nameAt(std::string_view path, std::size_t& i) {
	const std::size_t start = i;
	while(i < path.size() && isNameChar(path[i], i == start)) ++i;
	return path.substr(start, i - start);
}

/// @brief Reads an index in a path, "[2]" after its '[', moving past its ']'.
/// @param path The path.
/// @param i Where the digits start; then after the ']'.
/// @return The digits, or "" if there are none or no ']' follows.
std::string_view indexAt(std::string_view path, std::size_t& i) {
	const std::size_t start = i;
	while(i < path.size() && path[i] >= '0' && path[i] <= '9') ++i;
	const std::string_view digits = path.substr(start, i - start);
	if(i >= path.size() || path[i] != ']') return {};
	++i;
	return digits;
}

/// @brief Where a repeat's list lies in the state, when its "in" is a plain path there: "state.columns[2].cards", or
/// a path from an item that lies in the state ("column.cards").
/// @param path "in" as a path, "{{...}}".
/// @param locals The variables in scope.
/// @return The place, as a JSON Pointer, or std::nullopt for any other "in".
std::optional<std::string> placeOf(std::string_view path, const Scope& locals) {
	path.remove_prefix(2);
	path.remove_suffix(2);
	std::size_t i = 0;
	const std::string_view root = nameAt(path, i);
	std::string pointer;
	if(root != "state") {
		const std::string* origin = root.empty() ? nullptr : locals.originOf(root);
		if(!origin) return std::nullopt;
		pointer = *origin;
	}
	while(i < path.size()) {
		const char c = path[i++];
		std::string_view key;
		if(c == '.') key = nameAt(path, i);
		else if(c == '[') key = indexAt(path, i);
		if(key.empty()) return std::nullopt;
		appendKey(pointer, key);
	}
	return pointer;
}

/// @brief Reports what is wrong with a "repeat" that didn't compile, as building it reads it.
/// @param repeat The "repeat" value.
/// @param at The node's place.
/// @throws DocumentError Always.
[[noreturn]] void reportRepeat(const json& repeat, const std::string& at) {
	object(repeat, at + ".repeat");
	if(repeat.contains("for")) static_cast<void>(string(repeat["for"], at + ".repeat.for"));
	if(repeat.contains("index")) static_cast<void>(string(repeat["index"], at + ".repeat.index"));
	fail(at + ".repeat", "needs \"in\": the list to repeat over");
}

} // namespace

std::vector<json> Builder::values(const CompiledNode& node, const Scope& locals, const std::string& path) const {
	std::vector<json> result(node.evaluatedCount);
	if(result.empty()) return result;
	const Context context = engine_.document.context(locals, emptyObject());
	for(const CompiledProperty& property : node.properties)
		if(property.evaluated != CompiledProperty::noValue)
			result[property.evaluated] = property.value.evaluate(context, path, property.key);
	return result;
}

void Builder::check(const json& node, Scope& layer, const std::string& at) const {
	const DocumentContext& document = engine_.document;
	const json none = json::object();
	const json& rules = object(node["check"], at + ".check");
	// The value checked: "value", else the node's bound value
	const json value = rules.contains("value") ? document.evaluate(rules["value"], layer, none, at + ".check.value")
	                   : node.contains("bind") && node["bind"].is_string()
	                       ? format::lookup(text(document.evaluate(node["bind"], layer, none, at + ".bind")),
	                                        document.context(layer, none), at + ".bind")
	                       : json();
	layer.vars["value"] = value;
	const bool valid =
	    !rules.contains("rule") || truthy(document.evaluate(rules["rule"], layer, none, at + ".check.rule"));
	layer.vars["valid"] = valid;
	layer.vars["error"] =
	    valid ? json("") : document.evaluate(rules.value("message", json("Invalid")), layer, none, at + ".check");
}

// The new variables form one layer over the parent's scope; with and let see the ones before them
CompiledRef Builder::scoped(const CompiledRef& node, SharedScope& scope, const std::string& at) const {
	const json& own = node->own;
	Scope layer{scope.get(), nullptr, json::object(), {}, {}};
	const Context context = engine_.document.context(layer, emptyObject()); // reads the layer as it grows
	if(node->malformedWith) object(own["with"], at + ".with");
	for(const CompiledVariable& variable : node->with)
		layer.vars[variable.name] = variable.value.evaluate(context, at, "with", variable.name);
	if(node->malformedLet) object(own["let"], at + ".let");
	for(const CompiledVariable& variable : node->let) // in order: each sees the ones before it
		layer.vars[variable.name] = variable.value.evaluate(context, at, "let", variable.name);
	if(own.contains("check")) check(own, layer, at);
	CompiledRef result = node;
	if(node->style) result = compiler_.styled(node, node->style->evaluate(context, at, "style"), at);
	if(!layer.vars.empty()) scope = extend(std::move(scope), std::move(layer.vars));
	return result;
}

json Builder::repeated(const CompiledRepeat& repeat, const Scope& locals, const std::string& at) const {
	const json list = repeat.list.evaluate(engine_.document.context(locals, emptyObject()), at, "repeat.in");
	if(!list.is_null() && !list.is_array()) fail(at + ".repeat.in", "expected a list, got " + list.dump());
	return list;
}

// A list that lies in the state, or a data model's rows, is read for its shape only: its items read what they read
// inside it, so a change inside one item updates what reads it and leaves the list alone (Reads.hpp, shapeKey)
json Builder::listOf(const CompiledRepeat& repeat, const Scope& locals, const std::string& at,
                     std::optional<std::string>& place) const {
	ReadSet* const outer = engine_.document.reads();
	const std::string* path = repeat.path ? &repeat.path->get_ref<const std::string&>() : nullptr;
	if(const std::optional<std::string_view> name = path ? modelName(*path) : std::nullopt) {
		const DataModel* model = engine_.models.find(*name);
		if(!model)
			fail(at + ".repeat.in", "no data model '" + std::string(*name) +
			                            "' (the application registers them; it has " + engine_.models.names() + ")");
		place = Models::placeOf(*name);
		if(outer) outer->addPath(*place + "/" + std::string(shapeKey));
		json rows = json::array();
		for(std::size_t k = 0, count = model->size(); k < count; ++k) rows.push_back(model->row(k));
		return rows;
	}
	ReadSet listReads;
	json list;
	{
		const DocumentContext::ReadScope scope(engine_.document, outer ? &listReads : nullptr);
		list = repeated(repeat, locals, at);
	}
	if(path && list.is_array() && repeat.item != repeat.index) place = placeOf(*path, locals);
	if(outer) {
		if(place) outer->addPath(*place + "/" + std::string(shapeKey));
		else outer->add(listReads);
	}
	return list;
}

void Builder::instances(const CompiledRef& node, const SharedScope& locals, const std::string& at,
                        std::vector<Item>& items) const {
	const auto add = [&](SharedScope scope, std::string where, json itemKey) {
		CompiledRef chosen = scoped(node, scope, where);
		if(chosen->when &&
		   !format::truthy(chosen->when->evaluate(engine_.document.context(*scope, emptyObject()), where, "when")))
			return;
		items.push_back({std::move(chosen), std::move(scope), std::move(where), std::move(itemKey)});
	};

	if(!node->own.contains("repeat")) {
		add(locals, at, nullptr);
		return;
	}
	const std::optional<CompiledRepeat>& compiled = node->repeat;
	if(!compiled) reportRepeat(node->own["repeat"], at);
	const CompiledRepeat& repeat = *compiled;
	std::optional<std::string> place;
	const json list = listOf(repeat, *locals, at, place);
	engine_.reconciler.counters().rowsExpanded += list.size(); // a null list has none
	for(std::size_t k = 0; k < list.size(); ++k) {             // a null list has none
		SharedScope scope = place ? extend(locals, {{repeat.item, list[k]}, {repeat.index, k}}, repeat.item,
		                                   *place + "/" + std::to_string(k))
		                          : extend(locals, {{repeat.item, list[k]}, {repeat.index, k}});
		std::string where = at + "[" + std::to_string(k) + "]";
		json itemKey = repeat.key
		                   ? repeat.key->evaluate(engine_.document.context(*scope, emptyObject()), where, "repeat.key")
		                   : json();
		add(std::move(scope), std::move(where), std::move(itemKey));
	}
}

// The children to build, after "use", "repeat", "with", "let", "check" and "when"
std::vector<Item> Builder::expand(const json& children, const SharedScope& locals, const std::string& path) const {
	std::vector<Item> items;
	if(!children.is_array()) fail(path + ".children", "expected an array");
	++engine_.reconciler.counters().listsExpanded;
	items.reserve(children.size());
	for(std::size_t i = 0; i < children.size(); ++i) {
		const std::string at = path + ".children[" + std::to_string(i) + "]";
		instances(compiler_.resolve(object(children[i], at), at), locals, at, items);
	}
	return items;
}

} // namespace affresco::format
