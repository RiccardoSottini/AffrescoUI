/// @file
/// @brief format::CompiledCache: the document's nodes with their components instantiated and their styles merged,
/// compiled once per version of the document.

#include "CompiledCache.hpp"
#include "Library.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Puts a use's children wherever a component has {"slot": true}, searching its node depth first (not inside
/// other uses). Every slot gets them, so a component can show them in several places (a dock's areas, each showing
/// what is active in it), as the checker walks every slot.
/// @param node The component's node, or a node inside it.
/// @param children The use's children.
/// @return true if at least one slot took them.
bool fillSlot(json& node, const json& children) {
	if(!node.is_object() || !node.contains("children") || !node["children"].is_array()) return false;
	json& list = node["children"];
	bool placed = false;
	json replaced = json::array();
	for(json& child : list) {
		if(child.is_object() && child.value("slot", json(false)) == true) {
			replaced.insert(replaced.end(), children.begin(), children.end());
			placed = true;
			continue;
		}
		if(!child.contains("use") && fillSlot(child, children)) placed = true;
		replaced.push_back(std::move(child));
	}
	list = std::move(replaced);
	return placed;
}

/// @brief Tells whether a node sets its own position or size (otherwise a screen fills its outlet).
/// @param node The node.
/// @return true if it has layout properties.
bool hasLayout(const json& node) {
	static constexpr std::array<const char*, 6> keys = {"x", "y", "width", "height", "anchors", "fill"};
	return std::ranges::any_of(keys, [&node](const char* key) { return node.contains(key); });
}

/// @brief A node's "children", where it is.
/// @param node The node.
/// @return The array as written, or nullptr.
const json* childrenOf(const json& node) {
	const auto it = node.find("children");
	return it == node.end() ? nullptr : &*it;
}

} // namespace

CompiledCache::Cache& CompiledCache::cache() const {
	const json* document = &engine_.versions.document();
	if(cache_.document != document) {
		forgetVersion();
		cache_.document = document;
	}
	return cache_;
}

void CompiledCache::forgetVersion() const noexcept { cache_ = Cache{}; }

// The node's properties on top of its styles' (in order: later styles override earlier ones)
json CompiledCache::withStyles(const json& node, const std::string& path, int depth) const {
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

	for(const auto& [key, value] : node.items()) // the node's own keys win
		if(key != "style") merged[key] = value;
	return merged;
}

CompiledRef CompiledCache::compiled(const json& node, const std::string& path, bool fill) const {
	Cache& cached = cache();
	auto& map = fill ? cached.screens : cached.nodes;
	if(const auto it = map.find(&node); it != map.end()) return it->second;
	json merged = withStyles(withoutChildren(object(node, path)), path);
	if(fill && !hasLayout(merged)) merged["fill"] = 0;
	CompiledRef result = compileNode(std::move(merged), childrenOf(node));
	map.emplace(&node, result);
	return result;
}

CompiledRef CompiledCache::compiledOverlay(const json& node, const std::string& path) const {
	Cache& cached = cache();
	if(const auto it = cached.overlays.find(&node); it != cached.overlays.end()) return it->second;
	const auto placed = [](json& merged) {
		json& overlay = merged["overlay"];
		if(overlay.is_null()) overlay = json::object();
		if(overlay.is_object() && !overlay.contains("placement")) overlay["placement"] = "window";
	};
	CompiledRef result;
	if(object(node, path).contains("use")) { // an overlay that is a component: its node, as resolve() makes it
		json raw = node;
		for(int depth = 0; raw.contains("use"); ++depth) {
			if(depth >= maxComponentDepth) fail(path, "components nested too deeply (a component uses itself?)");
			raw = instantiateComponent(raw, path);
		}
		json merged = withStyles(raw, path);
		placed(merged);
		result = compileNode(std::make_shared<const json>(std::move(merged))); // it holds the use's children
	} else {
		json merged = withStyles(withoutChildren(node), path);
		placed(merged);
		result = compileNode(std::move(merged), childrenOf(node));
	}
	cached.overlays.emplace(&node, result);
	return result;
}

json CompiledCache::instantiateComponent(const json& use, const std::string& path) const {
	const std::string name = string(use["use"], path + ".use");
	const json& document = engine_.versions.document();
	const bool own = document.contains("components") && document["components"].contains(name);
	const json* builtIn = own ? nullptr : libraryComponent(name); // the document's own win
	if(!own && !builtIn) fail(path + ".use", "unknown component '" + name + "'");
	const json& component = builtIn ? *builtIn : object(document["components"][name], "components." + name);
	if(!component.contains("node")) fail("components." + name, "a component needs a \"node\"");

	json result = object(component["node"], "components." + name + ".node");
	for(const auto& [key, value] : use.items())
		if(key != "use" && key != "with" && key != "children") result[key] = value;

	// Parameters, evaluated in this order in one layer, each seeing the ones before it: what the use passes (in the
	// caller's scope), then the component's defaults it didn't pass (they may read the passed ones), then the
	// component node's own "with" (what it passes when it is itself a use of another component, or its own variables):
	// it reads this component's parameters, and a name it shares with one of them replaces it.
	json with = use.contains("with") ? object(use["with"], path + ".with") : json::object();
	if(component.contains("params"))
		for(const auto& [key, value] : object(component["params"], "components." + name + ".params").items())
			if(!with.contains(key)) with[key] = value;
	if(result.contains("with"))
		for(const auto& [key, value] : object(result["with"], "components." + name + ".node.with").items()) {
			with.erase(key);
			with[key] = value;
		}
	result["with"] = std::move(with);

	// The use's children go where the component has {"slot": true} (nothing does if it has none)
	const json children = use.contains("children") ? use["children"] : json::array();
	if(!fillSlot(result, children) && use.contains("children")) result["children"] = children;
	return result;
}

CompiledRef CompiledCache::styled(const CompiledRef& node, const json& style, const std::string& at) const {
	Styled& choices = cache().styled[node.get()];
	const auto it = std::ranges::find(choices, style, &Styled::value_type::first);
	if(it != choices.end()) return it->second;

	json chosen = node->own; // the node with the style this instance chose
	if(style.is_null() || (style.is_string() && style.get_ref<const std::string&>().empty())) chosen.erase("style");
	else chosen["style"] = style;
	CompiledRef result = compileNode(withStyles(chosen, at), node->children, node->keep);
	choices.emplace_back(style, result); // kept: the children cache knows its children by their place
	return result;
}

CompiledRef CompiledCache::resolve(const json& child, const std::string& at) const {
	Cache& cached = cache();
	if(const auto it = cached.children.find(&child); it != cached.children.end()) return it->second;
	CompiledRef result;
	if(child.contains("use")) {
		json raw = child;
		for(int depth = 0; raw.contains("use"); ++depth) {
			if(depth >= maxComponentDepth) fail(at, "components nested too deeply (a component uses itself?)");
			raw = instantiateComponent(raw, at);
		}
		result = compileNode(std::make_shared<const json>(withStyles(raw, at))); // it holds the use's children
	} else if(child.contains("style") && !isDynamic(child["style"])) {
		result = compileNode(withStyles(withoutChildren(child), at), childrenOf(child));
	} else {
		result = compileNode(withoutChildren(child), childrenOf(child));
	}
	cached.children.emplace(&child, result);
	return result;
}

} // namespace affresco::format
