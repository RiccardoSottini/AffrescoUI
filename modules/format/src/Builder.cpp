/// @file
/// @brief format::Builder: components, styles, repeats, scoped variables and "when".

#include "Builder.hpp"
#include "Engine.hpp"

#include <format>
#include <functional>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Puts a use's children where a component has {"slot": true}, searching its node depth first (not inside
/// other uses).
/// @param node The component's node, or a node inside it.
/// @param children The use's children.
/// @return true once they are placed.
bool fillSlot(json& node, const json& children) {
	if(!node.is_object() || !node.contains("children") || !node["children"].is_array()) return false;
	json& list = node["children"];
	for(std::size_t i = 0; i < list.size(); ++i) {
		if(list[i].is_object() && list[i].value("slot", json(false)) == true) {
			json replaced = json::array();
			for(std::size_t j = 0; j < list.size(); ++j) {
				if(j == i) replaced.insert(replaced.end(), children.begin(), children.end());
				else replaced.push_back(list[j]);
			}
			list = std::move(replaced);
			return true;
		}
		if(!list[i].contains("use") && fillSlot(list[i], children)) return true;
	}
	return false;
}

} // namespace

// The node's properties on top of its styles' (in order: later styles override earlier ones)
json Builder::withStyles(const json& node, const std::string& path, int depth) const {
	object(node, path);
	if(!node.contains("style")) return node;
	if(depth > 16) fail(path + ".style", "styles nested too deeply (a style uses itself?)");

	const json& style = node["style"];
	if(isDynamic(style)) return node; // chosen per instance, once its variables are known (scoped())
	const json names = style.is_array() ? style : json::array({style});

	json merged = json::object();
	for(const json& name : names) {
		const std::string key = string(name, path + ".style");
		const json& document = engine_.versions.document();
		if(!document.contains("styles") || !document["styles"].contains(key))
			fail(path + ".style", "unknown style '" + key + "'");
		merged.update(withStyles(object(document["styles"][key], "styles." + key), "styles." + key, depth + 1));
	}

	json own = node;
	own.erase("style");
	merged.update(own);
	return merged;
}

json Builder::evaluateNode(const json& merged, const Scope& locals, const std::string& path) const {
	json result = json::object();
	for(const auto& [key, value] : merged.items()) {
		if(key == "children") continue; // built from the node itself (reconcileChildren), never copied
		result[key] = structural(key) || key == "type"
		                  ? value
		                  : engine_.document.evaluate(value, locals, json::object(), std::format("{}.{}", path, key));
	}
	return result;
}

json Builder::instantiateComponent(const json& use, const std::string& path) const {
	const std::string name = string(use["use"], path + ".use");
	const json& document = engine_.versions.document();
	if(!document.contains("components") || !document["components"].contains(name))
		fail(path + ".use", "unknown component '" + name + "'");
	const json& component = object(document["components"][name], "components." + name);
	if(!component.contains("node")) fail("components." + name, "a component needs a \"node\"");

	json result = object(component["node"], "components." + name + ".node");
	for(const auto& [key, value] : use.items())
		if(key != "use" && key != "with" && key != "children") result[key] = value;

	// Parameters: the component's defaults, then what the use passes (evaluated in the caller's scope)
	json with =
	    component.contains("params") ? object(component["params"], "components." + name + ".params") : json::object();
	if(use.contains("with")) with.update(object(use["with"], path + ".with"));
	result["with"] = std::move(with);

	// The use's children go where the component has {"slot": true}
	if(use.contains("children") && !fillSlot(result, use["children"])) result["children"] = use["children"];
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
NodeRef Builder::scoped(const NodeRef& shared, SharedScope& scope, const std::string& at) const {
	const DocumentContext& document = engine_.document;
	const json& node = *shared;
	const json none = json::object();
	Scope layer{scope.get(), nullptr, json::object()};
	if(node.contains("with"))
		for(const auto& [name, value] : object(node["with"], at + ".with").items())
			layer.vars[name] = document.evaluate(value, layer, none, std::format("{}.with.{}", at, name));
	if(node.contains("let")) // in order: each sees the ones before it
		for(const auto& [name, value] : object(node["let"], at + ".let").items())
			layer.vars[name] = document.evaluate(value, layer, none, std::format("{}.let.{}", at, name));
	if(node.contains("check")) check(node, layer, at);
	NodeRef result = shared;
	if(node.contains("style") && isDynamic(node["style"])) { // this instance's own copy, with its style
		json chosen = node;
		chosen["style"] = document.evaluate(node["style"], layer, none, at + ".style");
		const json& style = chosen["style"];
		if(style.is_null() || (style.is_string() && style.get_ref<const std::string&>().empty())) chosen.erase("style");
		result = std::make_shared<const json>(withStyles(chosen, at));
	}
	if(!layer.vars.empty()) scope = extend(std::move(scope), std::move(layer.vars));
	return result;
}

NodeRef Builder::resolve(const json& child, const std::string& at) const {
	if(child.contains("use")) {
		json raw = child;
		for(int depth = 0; raw.contains("use"); ++depth) {
			if(depth >= maxComponentDepth) fail(at, "components nested too deeply (a component uses itself?)");
			raw = instantiateComponent(raw, at);
		}
		return std::make_shared<const json>(withStyles(raw, at));
	}
	if(child.contains("style") && !isDynamic(child["style"]))
		return std::make_shared<const json>(withStyles(child, at));
	return borrow(child);
}

json Builder::repeated(const json& repeat, const Scope& locals, const std::string& at) const {
	if(!repeat.contains("in")) fail(at + ".repeat", "needs \"in\": the list to repeat over");
	// "in" is a path ("state.messages") or any value/expression
	const json& source = repeat["in"];
	const DocumentContext& document = engine_.document;
	const json list =
	    source.is_string() && source.get<std::string>().find("{{") == std::string::npos
	        ? document.evaluate("{{" + source.get<std::string>() + "}}", locals, json::object(), at + ".repeat.in")
	        : document.evaluate(source, locals, json::object(), at + ".repeat.in");
	if(!list.is_null() && !list.is_array()) fail(at + ".repeat.in", "expected a list, got " + list.dump());
	return list;
}

// The children to build, after "use", "repeat", "with", "let", "check" and "when"
std::vector<Item> Builder::expand(const json& children, const SharedScope& locals, const std::string& path) const {
	std::vector<Item> items;
	if(!children.is_array()) fail(path + ".children", "expected an array");

	for(std::size_t i = 0; i < children.size(); ++i) {
		const std::string at = path + ".children[" + std::to_string(i) + "]";
		const NodeRef shared = resolve(object(children[i], at), at);
		const json& merged = *shared;

		auto add = [&](SharedScope scope, const std::string& where, json itemKey) {
			NodeRef node = scoped(shared, scope, where);
			if(node->contains("when") &&
			   !format::truthy(engine_.document.evaluate((*node)["when"], *scope, json::object(), where + ".when")))
				return;
			items.push_back({std::move(node), std::move(scope), where, std::move(itemKey)});
		};

		if(!merged.contains("repeat")) {
			add(locals, at, nullptr);
			continue;
		}

		const json& repeat = object(merged["repeat"], at + ".repeat");
		const std::string name = repeat.contains("for") ? string(repeat["for"], at + ".repeat.for") : "item";
		const std::string indexName =
		    repeat.contains("index") ? string(repeat["index"], at + ".repeat.index") : "index";
		const json list = repeated(repeat, *locals, at);
		for(std::size_t k = 0; k < list.size(); ++k) { // a null list has none
			SharedScope scope = extend(locals, {{name, list[k]}, {indexName, k}});
			const std::string where = at + "[" + std::to_string(k) + "]";
			json itemKey = repeat.contains("key")
			                   ? engine_.document.evaluate(repeat["key"], *scope, json::object(), where + ".repeat.key")
			                   : json();
			add(std::move(scope), where, std::move(itemKey));
		}
	}

	return items;
}

} // namespace affresco::format
