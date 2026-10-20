/// @file
/// @brief format::CompiledNode: compiling a resolved node into slots and bindings.

#include "CompiledNode.hpp"

#include "NodeKeys.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>
#include <utility>

namespace affresco::format {

namespace {

/// @brief A key with the slot it sets.
struct NamedSlot {
	std::string_view key; ///< The key.
	Slot slot;            ///< Its slot.
};

/// The keys the runtime applies itself, with their slots (the handlers are found by their shape).
constexpr auto namedSlots = std::to_array<NamedSlot>({
    {"id", Slot::Id},
    {"color", Slot::Color},
    {"visible", Slot::Visible},
    {"cursor", Slot::Cursor},
    {"tooltip", Slot::Tooltip},
    {"disabled", Slot::Disabled},
    {"transition", Slot::Transition},
    {"scroll", Slot::Scroll},
    {"overlay", Slot::Overlay},
    {"radius", Slot::Radius},
    {"borderWidth", Slot::BorderWidth},
    {"borderColor", Slot::BorderColor},
    {"shadow", Slot::Shadow},
    {"x", Slot::Layout},
    {"y", Slot::Layout},
    {"width", Slot::Layout},
    {"height", Slot::Layout},
    {"minWidth", Slot::Layout},
    {"minHeight", Slot::Layout},
    {"grow", Slot::Layout},
    {"fill", Slot::Layout},
    {"anchors", Slot::Layout},
    {"textColor", Slot::TextColor},
    {"padding", Slot::Padding},
    {"wrap", Slot::Wrap},
    {"font", Slot::Font},
    {"align", Slot::Align},
    {"fitText", Slot::FitText},
    {"placeholder", Slot::Placeholder},
    {"placeholderColor", Slot::PlaceholderColor},
    {"gap", Slot::Gap},
    {"from", Slot::From},
    {"direction", Slot::Direction},
    {"columns", Slot::Columns},
    {"type", Slot::Value},
    {"autofocus", Slot::Value},
    {"locked", Slot::Value},
    {"checked", Slot::Value},
    {"value", Slot::Value},
    {"text", Slot::Value},
    {"hover", Slot::StateBlock},
    {"focused", Slot::StateBlock},
    {"dragging", Slot::StateBlock},
    {"dragOver", Slot::StateBlock},
    {"pressed", Slot::StateBlock},
    {"bind", Slot::Bind},
    {"caret", Slot::Caret},
    {"keys", Slot::Keys},
    {"drag", Slot::Drag},
    {"every", Slot::Every},
});

/// The structural keys the builder reads, never a property of the panel.
constexpr auto scopingKeys =
    std::to_array<std::string_view>({"children", "repeat", "when", "let", "check", "use", "with", "note", "slot"});

/// @brief Tells whether a slot's value is read as written, never evaluated.
/// @param slot The slot.
/// @return true for the handlers and the other structural keys.
bool structuralSlot(Slot slot) noexcept {
	return slot == Slot::Handler || slot == Slot::Bind || slot == Slot::Caret || slot == Slot::Keys ||
	       slot == Slot::Drag || slot == Slot::Every;
}

/// @brief Tells whether applying a property reads more than its value: the state, or the node's variables.
/// @param slot Its slot.
/// @param value Its value as written.
/// @return true for an overlay's "open" written as a state path, a computed "bind" or "caret", and an "every" whose
/// period is computed.
bool readsMore(Slot slot, const json& value) {
	if(slot == Slot::Overlay) return value.is_object() && value.contains("open") && value["open"].is_string();
	if(slot == Slot::Bind || slot == Slot::Caret) return isDynamic(value);
	if(slot == Slot::Every) return value.is_object() && value.contains("ms") && isDynamic(value["ms"]);
	return false;
}

/// @brief Compiles "with" or "let": its variables in order.
/// @param value The object.
/// @param into Gets the variables.
/// @return false if it isn't an object (the builder reports it where the node is built).
bool compileVariables(const json& value, std::vector<CompiledVariable>& into) {
	if(!value.is_object()) return false;
	for(const auto& [name, item] : value.items()) into.push_back({name, Binding(item)});
	return true;
}

/// @brief Compiles "repeat".
/// @param repeat The "repeat" value.
/// @return It, or std::nullopt if it is malformed (the builder reports it where the node is built).
std::optional<CompiledRepeat> compileRepeat(const json& repeat) {
	if(!repeat.is_object() || !repeat.contains("in")) return std::nullopt;
	CompiledRepeat result;
	if(repeat.contains("for")) {
		if(!repeat["for"].is_string()) return std::nullopt;
		result.item = repeat["for"].get<std::string>();
	}
	if(repeat.contains("index")) {
		if(!repeat["index"].is_string()) return std::nullopt;
		result.index = repeat["index"].get<std::string>();
	}
	// "in" is a path ("state.messages") or any value/expression
	const json& source = repeat["in"];
	if(source.is_string() && !source.get_ref<const std::string&>().contains("{{")) {
		result.path = std::make_unique<json>("{{" + source.get<std::string>() + "}}");
		result.list.emplace(*result.path);
	} else {
		result.list.emplace(source);
	}
	if(repeat.contains("key")) result.key.emplace(repeat["key"]);
	return result;
}

/// @brief Reads the scoping keys of a node: with, let, a dynamic style, when, repeat.
/// @param node The compiled node; its own copy is read.
void compileScope(CompiledNode& node) {
	const json& own = node.own;
	if(own.contains("with")) node.malformedWith = !compileVariables(own["with"], node.with);
	if(own.contains("let")) node.malformedLet = !compileVariables(own["let"], node.let);
	if(own.contains("style") && isDynamic(own["style"])) node.style.emplace(own["style"]);
	if(own.contains("when")) node.when.emplace(own["when"]);
	if(own.contains("repeat")) node.repeat = compileRepeat(own["repeat"]);
}

} // namespace

Slot slotOf(std::string_view key) {
	static const std::unordered_map<std::string_view, Slot> slots = [] { // built once, never changed
		std::unordered_map<std::string_view, Slot> result;
		for(const auto& [name, slot] : namedSlots) result.emplace(name, slot);
		return result;
	}();
	if(const auto it = slots.find(key); it != slots.end()) return it->second;
	const bool handler = key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
	return handler ? Slot::Handler : Slot::Other;
}

const CompiledProperty* CompiledNode::find(std::string_view key) const noexcept {
	const auto it = std::ranges::find(properties, key, &CompiledProperty::key);
	return it == properties.end() ? nullptr : &*it;
}

json withoutChildren(const json& node) {
	json result = json::object();
	for(const auto& [key, value] : node.items())
		if(key != "children") result[key] = value;
	return result;
}

CompiledRef compileNode(const NodeRef& node) {
	const auto children = node->find("children");
	return compileNode(withoutChildren(*node), children == node->end() ? nullptr : &*children, node);
}

CompiledRef compileNode(json keys, const json* children, NodeRef keep) {
	auto node = std::make_shared<CompiledNode>();
	if(const auto styled = keys.find("children"); styled != keys.end()) { // a style's children
		if(!children) {
			keep = std::make_shared<const json>(std::move(*styled));
			children = keep.get();
		}
		keys.erase(styled);
	}
	node->own = std::move(keys);
	node->children = children;
	node->keep = std::move(keep);

	const json& own = node->own; // its values stay where they are from here on: the bindings refer to them
	if(own.contains("type")) {
		node->typeIsText = own["type"].is_string();
		if(node->typeIsText) node->type = own["type"].get<std::string>();
	}
	node->hasId = own.contains("id");
	for(const auto& [key, value] : own.items()) {
		if(key == "style" || std::ranges::find(scopingKeys, key) != scopingKeys.end()) continue;
		const Slot slot = slotOf(key);
		const bool asWritten = structuralSlot(slot) || key == "type"; // never evaluated
		CompiledProperty property{key, slot, asWritten ? Binding::constant(value) : Binding(value)};
		if(property.value.dynamic()) property.evaluated = node->evaluatedCount++;
		node->hasStateBlocks = node->hasStateBlocks || slot == Slot::StateBlock;
		node->alwaysApply = node->alwaysApply || readsMore(slot, value);
		node->properties.push_back(std::move(property));
	}
	compileScope(*node);
	return node;
}

} // namespace affresco::format
