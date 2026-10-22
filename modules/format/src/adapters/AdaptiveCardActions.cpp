/// @file
/// @brief The Adaptive Cards 1.6 adapter's inputs and actions (AdaptiveCards.hpp), and fromAdaptiveCard().

#include "AdaptiveCards.hpp"

#include <algorithm>

namespace affresco::format::adapters {

namespace detail {

namespace {

/// @brief An action's button colours by its "style".
struct ButtonColors {
	std::string background; ///< The button.
	std::string text;       ///< Its title.
};

/// @brief The colours of a button.
/// @param style "default", "positive" or "destructive".
/// @return Its colours.
[[nodiscard]] ButtonColors buttonColors(const std::string& style) {
	if(style == "positive") return {"@accent", "@onAccent"};
	if(style == "destructive") return {"@attention", "@onAccent"};
	return {"@card", "@accent"};
}

/// @brief The values of a multi-select ChoiceSet's "value": `"a,b"` is {"a", "b"}.
/// @param value The value.
/// @return The values.
[[nodiscard]] json splitValues(const std::string& value) {
	json values = json::array();
	for(std::size_t start = 0; start < value.size();) {
		const std::size_t end = std::min(value.find(',', start), value.size());
		if(end > start) values.push_back(value.substr(start, end - start));
		start = end + 1;
	}
	return values;
}

} // namespace

Built CardConverter::input(const std::string& type, const json& element, const std::string& at, float width) {
	const std::string id = textOf(element.value("id", json()));
	if(id.empty()) out_.warn(at, "an input without an id: kept as 'input'");
	const std::string key = stateKey(id.empty() ? "input" : id);
	if(type == "Input.Text" && element.value("style", json()) == "password")
		return out_.placeholder("Input.Text (password)", at, width);
	json& value = out_.state()["inputs"][key];
	std::vector<Built> parts;
	if(element.contains("label")) {
		const std::string label = field(element, "label", at);
		parts.push_back(out_.boundLabel(label, label.size(), width, TextSize::Small, "@muted"));
	}
	if(type == "Input.Toggle") {
		value = textOf(element.value("value", json())) == textOf(element.value("valueOn", json()), "true");
		const std::string title = field(element, "title", at);
		json box = {{"type", "checkbox"}, {"id", out_.uniqueId(key + "_box")}, {"width", 20}, {"height", 20},
		            {"bind", "state.inputs." + key}};
		parts.push_back(row(out_.uniqueId(key), {Built{std::move(box), 20},
		                                         out_.boundLabel(title, title.size(), width - 28, TextSize::Normal)}));
	} else if(type == "Input.ChoiceSet") {
		parts.push_back(choiceSet(element, key));
	} else {
		if(type != "Input.Text") out_.warn(at, type + " is a text field here: its value is text");
		value = textOf(element.value("value", json()));
		json field = {{"type", "textarea"}, {"id", out_.uniqueId(key)}, {"height", 32}, {"padding", 8},
		              {"color", "@card"}, {"textColor", "@text"}, {"borderWidth", 1}, {"borderColor", "@fieldBorder"},
		              {"radius", 6}, {"bind", "state.inputs." + key}};
		if(element.contains("placeholder")) field["placeholder"] = out_.literal(textOf(element["placeholder"]));
		parts.push_back({std::move(field), 32});
	}
	return column(out_.uniqueId(key + "_input"), std::move(parts), 4);
}

Built CardConverter::choiceSet(const json& element, const std::string& key) {
	const json& choices = element.value("choices", json::array());
	json items = json::array();
	for(std::size_t i = 0; i < (choices.is_array() ? choices.size() : 0); ++i) {
		const json& choice = choices[i];
		out_.enter("card choice '" + key + "'[" + std::to_string(i) + "]", 2);
		const std::string value = textOf(choice.is_object() ? choice.value("value", json()) : json(), std::to_string(i));
		items.push_back({{"id", value}, {"label", textOf(choice.is_object() ? choice.value("title", json()) : json(), value)}});
	}
	out_.state()["choices"][key] = items; // in the state, so the titles are never evaluated
	const bool multiple = element.value("isMultiSelect", json()) == true;
	const std::string value = textOf(element.value("value", json()));
	out_.state()["inputs"][key] = multiple ? splitValues(value) : json(value);
	const float height = 28 * std::max<float>(1, static_cast<float>(items.size()));
	return {{{"type", "list"}, {"id", out_.uniqueId(key + "_choices")}, {"items", "{{state.choices." + key + "}}"},
	         {"selection", multiple ? "multiple" : "single"}, {"rowHeight", 28}, {"height", height},
	         {"color", "@card"}, {"borderWidth", 1}, {"borderColor", "@fieldBorder"}, {"textColor", "@text"},
	         {"bind", "state.inputs." + key}},
	        height};
}

std::vector<Built> CardConverter::actions(const json& actions, const std::string& at, float width, int depth) {
	if(actions.is_null()) return {};
	if(!actions.is_array()) expect::fail(at, "expected a list of actions");
	std::vector<Built> buttons;
	std::vector<Built> cards;
	for(std::size_t i = 0; i < actions.size(); ++i) {
		const std::string place = at + "[" + std::to_string(i) + "]";
		out_.enter(place, depth);
		const json& action = expect::object(actions[i], place);
		const std::string type = textOf(action.value("type", json()));
		json steps = actionSteps(action, place);
		if(steps.is_null()) {
			buttons.push_back(out_.placeholder(type.empty() ? "an action without a type" : type, place, width));
			continue;
		}
		const std::string title = field(action, "title", place);
		const ButtonColors colors = buttonColors(textOf(action.value("style", json())));
		Built button = out_.boundLabel(title, title.size(), width, TextSize::Normal, colors.text);
		button.node.update({{"id", out_.uniqueId("action")}, {"height", 36}, {"padding", 8}, {"radius", 6},
		                    {"align", "center"}, {"wrap", false}, {"color", colors.background}, {"borderWidth", 1},
		                    {"borderColor", colors.background == "@card" ? "@accent" : colors.background},
		                    {"focusable", true}, {"role", "button"}, {"cursor", "hand"}, {"onClick", std::move(steps)}});
		button.height = 36;
		buttons.push_back(std::move(button));
		if(type != "Action.ShowCard") continue;
		const std::string shown = "state.cards.a" + std::to_string(actions_);
		const json card = expect::object(action.value("card", json()), place + ".card"); // a copy: value() is temporary
		std::vector<Built> parts{elements(card.value("body", json::array()), place + ".card.body", width - 24, depth + 1)};
		for(Built& part : this->actions(card.value("actions", json()), place + ".card.actions", width - 24, depth + 1))
			parts.push_back(std::move(part));
		Built inner = column(out_.uniqueId("card"), std::move(parts), 12);
		inner.node["fill"] = 12;
		const float height = inner.height + 24;
		cards.push_back({{{"id", out_.uniqueId("shown")}, {"height", height}, {"color", "@card"}, {"radius", 8},
		                  {"borderWidth", 1}, {"borderColor", "@divider"}, {"when", "{{" + shown + "}}"},
		                  {"children", json::array({std::move(inner.node)})}},
		                 height});
	}
	std::vector<Built> built;
	if(!buttons.empty()) built.push_back(row(out_.uniqueId("actions"), std::move(buttons)));
	for(Built& card : cards) built.push_back(std::move(card));
	return built;
}

json CardConverter::actionSteps(const json& action, const std::string& at) {
	const std::string type = textOf(action.value("type", json()));
	if(type == "Action.OpenUrl") {
		const std::string url = textOf(action.value("url", json()));
		if(allowedUrl(url)) return json::array({{{"emit", "openUrl"}, {"data", {{"url", url}}}}});
		out_.warn(at + ".url", "only https links are opened");
		return nullptr;
	}
	if(type != "Action.Submit" && type != "Action.Execute" && type != "Action.ShowCard") return nullptr;
	const std::string number = "a" + std::to_string(++actions_);
	if(type == "Action.ShowCard") {
		out_.state()["cards"][number] = false;
		return json::array({{{"toggle", "state.cards." + number}}});
	}
	if(!out_.state().contains("inputs")) out_.state()["inputs"] = json::object();
	json data = {{"inputs", "{{state.inputs}}"}};
	if(action.contains("data")) { // in the state, so it is never evaluated
		out_.state()["actions"][number] = action["data"];
		data["data"] = "{{state.actions." + number + "}}";
	}
	if(type == "Action.Execute") data["verb"] = out_.literal(textOf(action.value("verb", json())));
	return json::array({{{"emit", type == "Action.Submit" ? "submit" : "execute"}, {"data", std::move(data)}}});
}

void CardConverter::selectAction(json& node, const json& element, const std::string& at) {
	if(!element.contains("selectAction")) return;
	const std::string place = at + ".selectAction";
	const json& action = expect::object(element["selectAction"], place);
	json steps = action.value("type", json()) == "Action.ShowCard" ? json() : actionSteps(action, place);
	if(steps.is_null()) {
		out_.warn(place, textOf(action.value("type", json()), "an action") + " has no equivalent here: left out");
		return;
	}
	node.update({{"onClick", std::move(steps)}, {"focusable", true}, {"role", "button"}, {"cursor", "hand"}});
}

} // namespace detail

Converted fromAdaptiveCard(const json& card, const Options& options) {
	return detail::CardConverter(options).convert(card);
}

} // namespace affresco::format::adapters
