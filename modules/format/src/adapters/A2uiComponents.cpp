/// @file
/// @brief The A2UI 0.8 adapter's components (A2ui.hpp): each standard catalog component as the screen's own types.

#include "A2ui.hpp"

#include <algorithm>

namespace affresco::format::adapters::detail {

namespace {

/// @brief An A2UI Text's usageHint as a text size.
/// @param hint "h1" to "h5", "caption" or "body".
/// @return The size.
[[nodiscard]] TextSize sizeOf(const std::string& hint) {
	if(hint == "h1") return TextSize::Title;
	if(hint == "h2" || hint == "h3") return TextSize::Large;
	if(hint == "caption") return TextSize::Small;
	return TextSize::Normal;
}

/// @brief An A2UI Image's usageHint as a height (and, for icons and avatars, a width).
/// @param hint "icon", "avatar", "smallFeature", "mediumFeature", "largeFeature" or "header".
/// @return The height in pixels.
[[nodiscard]] float imageHeight(const std::string& hint) {
	if(hint == "icon") return 24;
	if(hint == "avatar") return 48;
	if(hint == "smallFeature") return 64;
	if(hint == "mediumFeature") return 120;
	if(hint == "largeFeature") return 200;
	return 160;
}

/// @brief A component reference: a child's id.
/// @param body The component's properties.
/// @param key The key that names it ("child").
/// @return The id, or "" if there is none.
[[nodiscard]] std::string childOf(const json& body, const char* key) {
	return body.contains(key) && body[key].is_string() ? body[key].get<std::string>() : "";
}

/// @brief An A2UI Divider: a line across, or a short one down when its "axis" is "vertical".
/// @param body The component's properties.
/// @return The line.
Built divider(const json& body) {
	const bool vertical = body.value("axis", json()) == "vertical";
	Built line{{{"height", vertical ? 24 : 1}, {"color", "@divider"}}, vertical ? 24.0f : 1.0f};
	if(vertical) line.node["width"] = 1;
	return line;
}

} // namespace

Built A2uiConverter::build(const std::string& id, float width, const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'";
	out_.enter(at, scope.depth + 1);
	const auto found = components_.find(id);
	if(found == components_.end()) return out_.placeholder("missing component '" + id + "'", at, width);
	if(visiting_.contains(id)) return out_.placeholder("component '" + id + "' inside itself", at, width);
	visiting_.insert(id);
	const auto entry = found->second.begin();
	const std::string& type = entry.key();
	const json& body = entry.value();
	Built built = body.is_object() ? buildType(type, body, id, width, {scope.inTemplate, scope.depth + 1})
	                               : out_.placeholder(type + " without properties", at, width);
	visiting_.erase(id);
	if(!built.node.contains("id")) built.node["id"] = out_.uniqueId(id);
	return built;
}

Built A2uiConverter::buildType(const std::string& type, const json& body, const std::string& id, float width,
                               const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'";
	if(type == "Text") return text(body, at, width, scope);
	if(type == "Image") return image(body, at, width, scope);
	if(type == "Row") return container(body, id, true, width, scope);
	if(type == "Column") return container(body, id, false, width, scope);
	if(type == "List") return container(body, id, body.value("direction", json()) == "horizontal", width, scope);
	if(type == "Card") return card(body, id, width, scope);
	if(type == "Tabs") return tabs(body, id, width, scope);
	if(type == "Button") return button(body, id, width, scope);
	if(type == "CheckBox") return checkBox(body, id, width, scope);
	if(type == "TextField") return textField(body, id, width, scope);
	if(type == "MultipleChoice") return multipleChoice(body, id, scope);
	if(type == "Slider") return slider(body, id, scope);
	if(type == "Divider") return divider(body);
	return out_.placeholder(type, at, width); // Icon, Video, AudioPlayer, Modal, DateTimeInput, or another catalog's
}

Built A2uiConverter::text(const json& body, const std::string& at, float width, const Scope& scope) {
	const BoundValue value = bound(body.value("text", json()), at + ".text", scope);
	const TextSize size = sizeOf(textOf(body.value("usageHint", json())));
	if(value.value.is_string())
		return out_.boundLabel(value.value.get<std::string>(), value.characters, width, size);
	return out_.label(textOf(value.value), width, size);
}

Built A2uiConverter::image(const json& body, const std::string& at, float width, const Scope& scope) {
	const json& url = body.value("url", json());
	if(!url.is_object() || !url.contains("literalString") || !url["literalString"].is_string()) {
		(void)bound(url, at + ".url", scope);
		return out_.placeholder("Image from the data model", at, width);
	}
	const std::string link = url["literalString"].get<std::string>();
	const std::string hint = textOf(body.value("usageHint", json()));
	const float height = imageHeight(hint);
	Built built{{{"type", "image"}, {"height", height}}, height};
	if(allowedUrl(link)) {
		built.node["source"] = link;
		out_.warn(at, "a remote image: shown only when the host provides it (Screen::setImage)");
	} else if(relativeFile(link)) {
		built.node["file"] = link;
	} else {
		return out_.placeholder("Image (only https links and files inside the screen's folder)", at, width);
	}
	if(hint == "icon" || hint == "avatar") built.node["width"] = height;
	const std::string fit = textOf(body.value("fit", json()));
	built.node["fit"] = fit == "cover" || fit == "fill" ? fit : "contain";
	return built;
}

Built A2uiConverter::container(const json& body, const std::string& id, bool across, float width,
                               const Scope& scope) {
	const json& children = body.value("children", json());
	if(children.is_object() && children.contains("template")) return repeated(children["template"], id, across, width, scope);
	std::vector<std::string> ids;
	if(children.is_object() && children.contains("explicitList") && children["explicitList"].is_array())
		for(const json& child : children["explicitList"])
			if(child.is_string()) ids.push_back(child.get<std::string>());
	const float gap = 8;
	const float each = across && !ids.empty() ? (width - gap * static_cast<float>(ids.size() - 1)) / static_cast<float>(ids.size()) : width;
	std::vector<Built> built;
	built.reserve(ids.size());
	for(const std::string& child : ids) built.push_back(build(child, std::max(each, 16.0f), scope));
	return across ? row(out_.uniqueId(id), std::move(built), gap) : column(out_.uniqueId(id), std::move(built), gap);
}

Built A2uiConverter::repeated(const json& from, const std::string& id, bool across, float width,
                              const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'.children.template";
	if(!from.is_object() || !from.contains("componentId") || !from.contains("dataBinding"))
		expect::fail(at, "expected {componentId, dataBinding}");
	const std::string list = pathExpression(expect::string(from["dataBinding"], at + ".dataBinding"), scope);
	json data = out_.state();
	if(list.starts_with("state."))
		for(std::size_t start = 6; start < list.size();) {
			const std::size_t end = std::min(list.find('.', start), list.size());
			data = data.is_object() ? data.value(list.substr(start, end - start), json()) : json();
			start = end + 1;
		}
	Built item = build(expect::string(from["componentId"], at + ".componentId"), across ? width / 3 : width,
	                   {true, scope.depth});
	item.node["repeat"] = {{"for", "item"}, {"in", "{{" + (data.is_object() ? "values(" + list + ")" : list) + "}}"},
	                       {"index", "index"}};
	if(across) return row(out_.uniqueId(id), {std::move(item)});
	const float count = std::max(1.0f, static_cast<float>(data.is_structured() ? data.size() : 1));
	const float height = item.height * count + 8 * (count - 1);
	Built built = column(out_.uniqueId(id), {std::move(item)});
	built.height = height;
	built.node["height"] = height;
	return built;
}

Built A2uiConverter::card(const json& body, const std::string& id, float width, const Scope& scope) {
	Built child = build(childOf(body, "child"), width - 24, scope);
	child.node["fill"] = 12;
	const float height = child.height + 24;
	return {{{"id", out_.uniqueId(id)}, {"height", height}, {"color", "@card"}, {"radius", 10}, {"borderWidth", 1},
	         {"borderColor", "@divider"}, {"children", json::array({std::move(child.node)})}},
	        height};
}

Built A2uiConverter::tabs(const json& body, const std::string& id, float width, const Scope& scope) {
	const std::string key = "state.tabs." + stateKey(id);
	out_.state()["tabs"][stateKey(id)] = 0;
	const json& items = body.value("tabItems", json::array());
	std::vector<Built> titles;
	std::vector<Built> contents;
	float tallest = 0;
	for(std::size_t i = 0; i < (items.is_array() ? items.size() : 0); ++i) {
		const std::string at = "a2ui component '" + id + "'.tabItems[" + std::to_string(i) + "]";
		const BoundValue title = bound(items[i].value("title", json()), at + ".title", scope);
		Built tab = out_.boundLabel(textOf(title.value), title.characters, width / 4, TextSize::Normal);
		const std::string chosen = key + " == " + std::to_string(i);
		tab.node.update({{"id", out_.uniqueId(id + "_tab")}, {"height", 32}, {"padding", 6}, {"radius", 6},
		                 {"align", "center"}, {"focusable", true}, {"role", "tab"}, {"cursor", "hand"},
		                 {"color", "{{" + chosen + " ? @accent : @card}}"},
		                 {"textColor", "{{" + chosen + " ? @onAccent : @text}}"},
		                 {"onClick", {{"set", key}, {"to", i}}}});
		tab.height = 32;
		titles.push_back(std::move(tab));
		Built content = build(childOf(items[i], "child"), width, scope);
		content.node["when"] = "{{" + chosen + "}}";
		tallest = std::max(tallest, content.height);
		contents.push_back(std::move(content));
	}
	contents.insert(contents.begin(), row(out_.uniqueId(id + "_bar"), std::move(titles), 4));
	Built built = column(out_.uniqueId(id), std::move(contents));
	built.height = 32 + 8 + tallest;
	built.node["height"] = built.height;
	return built;
}

Built A2uiConverter::button(const json& body, const std::string& id, float width, const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'";
	const bool primary = body.value("primary", json()) == true;
	Built child = build(childOf(body, "child"), width - 16, scope);
	if(child.node.value("type", json()) == "label") child.node["textColor"] = primary ? "@onAccent" : "@accent";
	child.node["fill"] = 8;
	const float height = child.height + 16;
	Built built{{{"id", out_.uniqueId(id)}, {"height", height}, {"color", primary ? "@accent" : "@card"}, {"radius", 6},
	             {"borderWidth", 1}, {"borderColor", "@accent"}, {"focusable", true}, {"role", "button"},
	             {"cursor", "hand"}, {"children", json::array({std::move(child.node)})}},
	            height};
	if(body.contains("action")) built.node["onClick"] = actionSteps(body["action"], id, at + ".action", scope);
	else out_.warn(at, "a Button without an action");
	return built;
}

json A2uiConverter::actionSteps(const json& action, const std::string& id, const std::string& at,
                                const Scope& scope) {
	expect::object(action, at);
	json context = json::object();
	const json& entries = action.value("context", json::array());
	for(std::size_t i = 0; i < (entries.is_array() ? entries.size() : 0); ++i) {
		const json& entry = entries[i];
		const std::string place = at + ".context[" + std::to_string(i) + "]";
		if(!entry.is_object() || !entry.contains("key") || !entry["key"].is_string()) expect::fail(place, "expected {key, value}");
		context[stateKey(entry["key"].get<std::string>())] = bound(entry.value("value", json()), place + ".value", scope).value;
	}
	return json::array({{{"emit", "userAction"},
	                     {"data",
	                      {{"name", out_.literal(textOf(action.value("name", json())))},
	                       {"surfaceId", out_.literal(surface_)},
	                       {"sourceComponentId", out_.literal(id)},
	                       {"context", std::move(context)}}}}});
}

Built A2uiConverter::checkBox(const json& body, const std::string& id, float width, const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'";
	const BoundValue label = bound(body.value("label", json()), at + ".label", scope);
	const BoundValue value = bound(body.value("value", json()), at + ".value", scope);
	json box = {{"type", "checkbox"}, {"id", out_.uniqueId(id + "_box")}, {"width", 20}, {"height", 20}};
	if(const auto path = bindable(value, id, false)) box["bind"] = *path;
	else box["checked"] = value.value;
	Built text = out_.boundLabel(textOf(label.value), label.characters, width - 28, TextSize::Normal);
	return row(out_.uniqueId(id), {Built{std::move(box), 20}, std::move(text)});
}

Built A2uiConverter::textField(const json& body, const std::string& id, float width, const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'";
	if(body.value("textFieldType", json()) == "obscured") return out_.placeholder("TextField (obscured)", at, width);
	const BoundValue label = bound(body.value("label", json()), at + ".label", scope);
	const BoundValue value = bound(body.value("text", json()), at + ".text", scope);
	json field = {{"type", "textarea"}, {"id", out_.uniqueId(id + "_field")}, {"height", 32}, {"padding", 8},
	              {"color", "@card"}, {"textColor", "@text"}, {"borderWidth", 1}, {"borderColor", "@fieldBorder"},
	              {"radius", 6}};
	if(const auto path = bindable(value, id, "")) field["bind"] = *path;
	else field["text"] = value.value;
	Built caption = out_.boundLabel(textOf(label.value), label.characters, width, TextSize::Small, "@muted");
	return column(out_.uniqueId(id), {std::move(caption), Built{std::move(field), 32}}, 4);
}

Built A2uiConverter::multipleChoice(const json& body, const std::string& id, const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'";
	const json& options = body.value("options", json::array());
	json items = json::array();
	for(std::size_t i = 0; i < (options.is_array() ? options.size() : 0); ++i) {
		const std::string place = at + ".options[" + std::to_string(i) + "]";
		out_.enter(place, scope.depth + 1);
		const json& label = options[i].value("label", json());
		const std::string value = textOf(options[i].value("value", json()), std::to_string(i));
		items.push_back({{"id", value}, {"label", textOf(label.is_object() ? label.value("literalString", json()) : json(), value)}});
	}
	out_.state()["choices"][stateKey(id)] = items; // in the state, so the labels are never evaluated
	const bool single = body.value("maxAllowedSelections", json()) == 1;
	const BoundValue selections = bound(body.value("selections", json()), at + ".selections", scope);
	const float height = 28 * std::max<float>(1, static_cast<float>(items.size()));
	json list = {{"type", "list"}, {"id", out_.uniqueId(id)}, {"items", "{{state.choices." + stateKey(id) + "}}"},
	             {"selection", single ? "single" : "multiple"}, {"rowHeight", 28}, {"height", height},
	             {"color", "@card"}, {"borderWidth", 1}, {"borderColor", "@fieldBorder"}, {"textColor", "@text"}};
	if(const auto path = bindable(selections, id, single ? json("") : json::array())) list["bind"] = *path;
	return {std::move(list), height};
}

Built A2uiConverter::slider(const json& body, const std::string& id, const Scope& scope) {
	const std::string at = "a2ui component '" + id + "'";
	const BoundValue value = bound(body.value("value", json()), at + ".value", scope);
	const json low = body.value("minValue", json(0));
	const json high = body.value("maxValue", json(100));
	json node = {{"type", "slider"}, {"id", out_.uniqueId(id)}, {"height", 24},
	             {"min", low.is_number() ? low : json(0)}, {"max", high.is_number() ? high : json(100)}};
	if(const auto path = bindable(value, id, node["min"])) node["bind"] = *path;
	else node["value"] = value.value;
	return {std::move(node), 24};
}

} // namespace affresco::format::adapters::detail
