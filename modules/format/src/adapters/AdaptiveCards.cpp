/// @file
/// @brief The Adaptive Cards 1.6 adapter's elements (AdaptiveCards.hpp): the card, text, images, containers, column
/// sets, fact sets and the card's `${...}` templating.

#include "AdaptiveCards.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace affresco::format::adapters::detail {

using expect::fail;

namespace {

/// @brief A TextBlock's size.
/// @param size "small", "default", "medium", "large" or "extraLarge".
/// @return Its text size.
[[nodiscard]] TextSize textSize(const std::string& size) noexcept {
	if(size == "small") return TextSize::Small;
	if(size == "medium" || size == "large") return TextSize::Large;
	if(size == "extraLarge") return TextSize::Title;
	return TextSize::Normal;
}

/// @brief A TextBlock's colour.
/// @param color "default", "accent", "good", "warning", "attention", "dark" or "light".
/// @param subtle Its "isSubtle".
/// @return A palette colour.
[[nodiscard]] std::string textColor(const std::string& color, bool subtle) {
	if(color == "accent" || color == "good" || color == "warning" || color == "attention") return "@" + color;
	return subtle || color == "light" ? "@muted" : "@text";
}

/// @brief An Image's size in pixels.
/// @param size "small", "medium", "large", "auto" or "stretch".
/// @return Its height (and width).
[[nodiscard]] float imageSize(const std::string& size) noexcept {
	if(size == "small") return 40;
	if(size == "large") return 160;
	if(size == "medium") return 80;
	return 120;
}

/// @brief Whether a `${...}` expression is a plain path into the card's data (`name`, `$root.user.name`).
/// @param expression The expression, without `${` and `}`.
/// @return Whether it is.
[[nodiscard]] bool plainPath(std::string_view expression) noexcept {
	if(expression.empty() || expression.front() == '.' || expression.back() == '.') return false;
	return std::ranges::all_of(expression, [](char c) {
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
	});
}

/// @brief A plain path as a state expression: `$root.user.name` and `user.name` are `state.data.user.name`.
/// @param expression The path.
/// @return The expression.
[[nodiscard]] std::string dataExpression(std::string_view expression) {
	std::vector<std::string> segments{"data"};
	for(std::size_t start = 0; start < expression.size();) {
		const std::size_t end = std::min(expression.find('.', start), expression.size());
		segments.emplace_back(expression.substr(start, end - start));
		start = end + 1;
	}
	return statePath(segments);
}

/// @brief Width spec of a Column: a weight (a number, "stretch", "auto") or pixels ("80px").
struct ColumnWidth {
	float weight = 1; ///< Its share of what fixed columns leave.
	float pixels = 0; ///< Its width in pixels, when fixed (else 0).
};

/// @brief A Column's "width".
/// @param width The value.
/// @return Its spec.
[[nodiscard]] ColumnWidth columnWidth(const json& width) {
	if(width.is_number() && width.get<double>() > 0) return {std::min(width.get<float>(), 100.0f), 0};
	const std::string text = textOf(width);
	if(text.ends_with("px")) {
		try {
			return {0, std::clamp(std::stof(text.substr(0, text.size() - 2)), 1.0f, 4096.0f)};
		} catch(const std::exception&) {
			return {}; // not a number of pixels: automatic
		}
	}
	return {};
}

/// @brief The input elements it converts.
constexpr std::array<std::string_view, 6> inputTypes = {"Input.Text", "Input.Number", "Input.Date",
                                                        "Input.Time", "Input.Toggle", "Input.ChoiceSet"};

/// @brief A RichTextBlock's text: its "inlines" (texts, or TextRuns' "text") joined.
/// @param element The element.
/// @return The text, before its templates are expanded.
std::string inlinesText(const json& element) {
	std::string text;
	for(const json& run : element.value("inlines", json::array()))
		text += run.is_string() ? run.get<std::string>() : textOf(run.is_object() ? run.value("text", json()) : json());
	return text;
}

} // namespace

Converted CardConverter::convert(const json& card) {
	if(!card.is_object() || card.value("type", json()) != "AdaptiveCard") fail("card", "expected an AdaptiveCard");
	const std::string version = textOf(card.value("version", json()), "1.0");
	if(version.size() > 3 || version > std::string(adaptiveCardsVersion))
		out_.warn("card.version", "newer than " + std::string(adaptiveCardsVersion) + ": converted as such");
	if(card.contains("$data")) {
		if(card["$data"].is_object()) out_.state()["data"] = card["$data"];
		else out_.warn("card.$data", "left out: only an object of data is kept");
	}
	std::vector<Built> parts;
	parts.push_back(elements(card.value("body", json::array()), "card.body", out_.contentWidth(), 1));
	for(Built& built : actions(card.value("actions", json()), "card.actions", out_.contentWidth(), 1))
		parts.push_back(std::move(built));
	if(!out_.state().contains("inputs")) out_.state()["inputs"] = json::object();
	return {out_.screen(column("card", std::move(parts), 12), "Adaptive Card",
	                    "Converted from Adaptive Cards " + std::string(adaptiveCardsVersion)),
	        std::move(out_.warnings()),
	        {}};
}

Built CardConverter::elements(const json& elements, const std::string& at, float width, int depth) {
	if(!elements.is_array()) fail(at, "expected a list of elements");
	std::vector<Built> built;
	for(std::size_t i = 0; i < elements.size(); ++i)
		for(Built& node : element(elements[i], at + "[" + std::to_string(i) + "]", width, depth))
			built.push_back(std::move(node));
	return column(out_.uniqueId("items"), std::move(built));
}

std::vector<Built> CardConverter::element(const json& element, const std::string& at, float width, int depth) {
	out_.enter(at, depth);
	if(!element.is_object() || !element.contains("type") || !element["type"].is_string())
		fail(at, "expected an element {\"type\", ...}");
	const std::string type = element["type"].get<std::string>();
	if(element.contains("$when")) out_.warn(at + ".$when", "left out: the element is always shown");
	if(element.contains("$data")) out_.warn(at + ".$data", "left out: the element reads the card's data");
	std::optional<std::vector<Built>> built = elementOfType(type, element, at, width, depth);
	if(!built) {
		const json& fallback = element.value("fallback", json());
		if(fallback == "drop") {
			out_.warn(at, type + " has no equivalent: dropped, as its fallback says");
			return {};
		}
		if(fallback.is_object()) return this->element(fallback, at + ".fallback", width, depth + 1);
		built = std::vector<Built>{out_.placeholder(type, at, width)};
	}
	if(element.value("isVisible", json()) == false)
		for(Built& node : *built) node.node["visible"] = false;
	return std::move(*built);
}

std::optional<std::vector<Built>> CardConverter::elementOfType(const std::string& type, const json& element,
                                                               const std::string& at, float width, int depth) {
	if(type == "TextBlock") return std::vector<Built>{textBlock(element, field(element, "text", at), width)};
	if(type == "RichTextBlock")
		return std::vector<Built>{textBlock(element, textOf(templated(inlinesText(element), at)), width)};
	if(type == "Image")
		return std::vector<Built>{image(element, at, width, imageSize(textOf(element.value("size", json()))))};
	if(type == "Container") return std::vector<Built>{container(element, at, width, depth)};
	if(type == "ColumnSet") return std::vector<Built>{columnSet(element, at, width, depth)};
	if(type == "FactSet") return std::vector<Built>{factSet(element, at, width)};
	if(type == "ImageSet") return std::vector<Built>{imageSet(element, at, depth)};
	if(type == "ActionSet") return actions(element.value("actions", json()), at + ".actions", width, depth + 1);
	if(std::ranges::contains(inputTypes, type)) return std::vector<Built>{input(type, element, at, width)};
	return std::nullopt;
}

Built CardConverter::imageSet(const json& element, const std::string& at, int depth) {
	std::vector<Built> images;
	const float size = imageSize(textOf(element.value("imageSize", json()), "medium"));
	const json& list = element.value("images", json::array());
	for(std::size_t i = 0; i < (list.is_array() ? list.size() : 0); ++i) {
		const std::string place = at + ".images[" + std::to_string(i) + "]";
		out_.enter(place, depth + 1);
		images.push_back(image(list[i], place, size, size));
	}
	return row(out_.uniqueId("images"), std::move(images));
}

Built CardConverter::textBlock(const json& element, const std::string& text, float width) {
	const TextSize size = textSize(textOf(element.value("size", json())));
	const std::string color = textColor(textOf(element.value("color", json())), element.value("isSubtle", json()) == true);
	Built built = out_.boundLabel(text, text.size(), width, size, color);
	built.node["id"] = out_.uniqueId(textOf(element.value("id", json()), "text"));
	const std::string alignment = textOf(element.value("horizontalAlignment", json()));
	if(alignment == "center" || alignment == "right")
		built.node["align"] = {{"horizontal", alignment == "center" ? "center" : "end"}, {"vertical", "start"}};
	return built;
}

Built CardConverter::image(const json& element, const std::string& at, float width, float size) {
	if(!element.is_object()) fail(at, "expected an Image");
	const std::string url = textOf(element.value("url", json()));
	const bool stretch = element.value("size", json()) == "stretch";
	Built built{{{"type", "image"}, {"id", out_.uniqueId(textOf(element.value("id", json()), "image"))},
	             {"height", size}, {"fit", stretch ? "cover" : "contain"}},
	            size};
	if(allowedUrl(url)) {
		built.node["source"] = url;
		out_.warn(at, "a remote image: shown only when the host provides it (Screen::setImage)");
	} else if(relativeFile(url)) {
		built.node["file"] = url;
	} else {
		return out_.placeholder("Image (only https links and files inside the screen's folder)", at, width);
	}
	if(!stretch) built.node["width"] = std::min(size, width);
	if(element.value("style", json()) == "person") built.node["radius"] = 999;
	if(element.contains("altText")) built.node["label"] = out_.literal(textOf(element["altText"]));
	selectAction(built.node, element, at);
	return built;
}

Built CardConverter::container(const json& element, const std::string& at, float width, int depth) {
	const std::string style = textOf(element.value("style", json()), "default");
	const bool framed = style != "default";
	Built items = elements(element.value("items", json::array()), at + ".items", framed ? width - 24 : width, depth + 1);
	if(!framed) {
		selectAction(items.node, element, at);
		return items;
	}
	const bool known = style == "good" || style == "warning" || style == "attention" || style == "accent";
	items.node["fill"] = 12;
	const float height = items.height + 24;
	Built built{{{"id", out_.uniqueId(textOf(element.value("id", json()), "container"))}, {"height", height},
	             {"color", "@card"}, {"radius", 8}, {"borderWidth", 1},
	             {"borderColor", known ? "@" + style : std::string("@divider")},
	             {"children", json::array({std::move(items.node)})}},
	            height};
	selectAction(built.node, element, at);
	return built;
}

Built CardConverter::columnSet(const json& element, const std::string& at, float width, int depth) {
	const json& columns = element.value("columns", json::array());
	if(!columns.is_array()) fail(at + ".columns", "expected a list of columns");
	std::vector<ColumnWidth> widths;
	float fixed = 0;
	float weights = 0;
	for(const json& column : columns) {
		widths.push_back(columnWidth(column.is_object() ? column.value("width", json()) : json()));
		fixed += widths.back().pixels;
		weights += widths.back().weight;
	}
	const float gap = 8;
	const float flexible = std::max(16.0f, width - fixed - gap * static_cast<float>(std::max<std::size_t>(columns.size(), 1) - 1));
	std::vector<Built> built;
	for(std::size_t i = 0; i < columns.size(); ++i) {
		const std::string place = at + ".columns[" + std::to_string(i) + "]";
		out_.enter(place, depth + 1);
		if(!columns[i].is_object()) fail(place, "expected a Column");
		const ColumnWidth spec = widths[i];
		const float own = spec.pixels > 0 ? spec.pixels : flexible * spec.weight / std::max(weights, 1.0f);
		Built cell = elements(columns[i].value("items", json::array()), place + ".items", own, depth + 2);
		if(spec.pixels > 0) cell.node["width"] = spec.pixels;
		else cell.node["grow"] = spec.weight;
		selectAction(cell.node, columns[i], place);
		built.push_back(std::move(cell));
	}
	return row(out_.uniqueId(textOf(element.value("id", json()), "columns")), std::move(built), gap);
}

Built CardConverter::factSet(const json& element, const std::string& at, float width) {
	const json& facts = element.value("facts", json::array());
	std::vector<Built> lines;
	const float titleWidth = std::floor(width * 0.35f);
	for(std::size_t i = 0; i < (facts.is_array() ? facts.size() : 0); ++i) {
		const std::string place = at + ".facts[" + std::to_string(i) + "]";
		out_.enter(place, 2);
		if(!facts[i].is_object()) fail(place, "expected {title, value}");
		const std::string title = field(facts[i], "title", place);
		const std::string value = field(facts[i], "value", place);
		Built name = out_.boundLabel(title, title.size(), titleWidth, TextSize::Small, "@muted");
		name.node["width"] = titleWidth;
		Built shown = out_.boundLabel(value, value.size(), width - titleWidth - 8, TextSize::Small);
		lines.push_back(row(out_.uniqueId("fact"), {std::move(name), std::move(shown)}));
	}
	return column(out_.uniqueId(textOf(element.value("id", json()), "facts")), std::move(lines), 4);
}

json CardConverter::templated(const std::string& text, const std::string& at) {
	if(!text.contains("${")) return out_.literal(text);
	std::string result;
	bool plain = true;
	for(std::size_t start = 0; start < text.size() && plain;) {
		const std::size_t open = std::min(text.find("${", start), text.size());
		const std::string_view literal = std::string_view(text).substr(start, open - start);
		plain = literal.find_first_of("{}") == std::string_view::npos;
		result += literal;
		if(open == text.size()) break;
		const std::size_t close = text.find('}', open);
		std::string_view expression = std::string_view(text).substr(open + 2, close - open - 2);
		if(expression.starts_with("$root.")) expression.remove_prefix(6);
		plain = plain && close != std::string::npos && plainPath(expression);
		if(plain) result += "{{" + dataExpression(expression) + "}}";
		start = close + 1;
	}
	if(plain) return result;
	out_.warn(at, "an expression with no equivalent (only ${name} and ${a.b} of the card's data): shown as written");
	return out_.literal(text);
}

std::string CardConverter::field(const json& element, const char* key, const std::string& at) {
	const json& value = element.value(key, json());
	if(value.is_string()) return templated(value.get<std::string>(), at + "." + key).get<std::string>();
	return textOf(value);
}

} // namespace affresco::format::adapters::detail
