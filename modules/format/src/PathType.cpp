/// @file
/// @brief The "path" node type: a PathView drawing SVG path data, filled and stroked, fitted to the node.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/PathView.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t dashLimit = 64; ///< The most dash and gap lengths.

/// @brief Reads a word that names one of a few choices.
/// @param value The word.
/// @param at Its place, for errors.
/// @param choices The words and what they mean.
/// @param expected What to say when it is none of them.
/// @return What the word means.
/// @throws DocumentError If it is none of them.
template <typename Choice, std::size_t count>
Choice choiceOf(const json& value, const std::string& at,
                const std::array<std::pair<std::string_view, Choice>, count>& choices, std::string_view expected) {
	const std::string name = string(value, at);
	for(const auto& [word, choice] : choices)
		if(word == name) return choice;
	fail(at, "expected " + std::string(expected));
}

/// @brief Reads "viewBox": four numbers, "x y width height", the width and height above zero.
/// @param value The text.
/// @param at Its place, for errors.
/// @return The box.
/// @throws DocumentError If it isn't four numbers or the box is empty.
Rect viewBoxOf(const json& value, const std::string& at) {
	const std::string text = string(value, at);
	std::array<float, 4> numbers{};
	std::size_t read = 0;
	const char* cursor = text.c_str();
	while(read < numbers.size()) {
		while(*cursor == ' ' || *cursor == ',') ++cursor;
		char* end = nullptr;
		const float number = std::strtof(cursor, &end);
		if(end == cursor || !std::isfinite(number)) break;
		numbers[read++] = number;
		cursor = end;
	}
	while(*cursor == ' ') ++cursor;
	if(read != numbers.size() || *cursor != '\0' || numbers[2] <= 0.0f || numbers[3] <= 0.0f)
		fail(at, R"(expected "x y width height", the width and height above zero)");
	return {numbers[0], numbers[1], numbers[2], numbers[3]};
}

/// @brief Reads "strokeDash": dash and gap lengths, at least zero, at most 64.
/// @param value The lengths.
/// @param at Its place, for errors.
/// @return The lengths.
/// @throws DocumentError If it isn't such a list.
std::vector<float> dashOf(const json& value, const std::string& at) {
	if(!value.is_array() || value.size() > dashLimit) fail(at, "expected a list of at most 64 lengths");
	std::vector<float> lengths;
	lengths.reserve(value.size());
	for(std::size_t index = 0; index < value.size(); ++index) {
		const double length = number(value[index], at + "[" + std::to_string(index) + "]");
		if(length < 0.0) fail(at + "[" + std::to_string(index) + "]", "expected a length of zero or more");
		lengths.push_back(static_cast<float>(length));
	}
	return lengths;
}

/// @brief Reads the stroke's shape: width, joins, caps and dashes.
/// @param values The node's values.
/// @param path The node's place, for errors.
/// @return The style.
StrokeStyle strokeOf(const json& values, const std::string& path) {
	static constexpr std::array<std::pair<std::string_view, LineJoin>, 3> joins{
	    {{"miter", LineJoin::miter}, {"round", LineJoin::round}, {"bevel", LineJoin::bevel}}};
	static constexpr std::array<std::pair<std::string_view, LineCap>, 3> caps{
	    {{"butt", LineCap::butt}, {"round", LineCap::round}, {"square", LineCap::square}}};
	StrokeStyle style;
	if(values.contains("strokeWidth")) {
		const double width = number(values["strokeWidth"], path + ".strokeWidth");
		if(width < 0.0) fail(path + ".strokeWidth", "expected a width of zero or more");
		style.width = static_cast<float>(width);
	}
	if(values.contains("strokeJoin"))
		style.join = choiceOf(values["strokeJoin"], path + ".strokeJoin", joins, R"("miter", "round" or "bevel")");
	if(values.contains("strokeCap"))
		style.cap = choiceOf(values["strokeCap"], path + ".strokeCap", caps, R"("butt", "round" or "square")");
	if(values.contains("strokeDash")) style.dash = dashOf(values["strokeDash"], path + ".strokeDash");
	return style;
}

/// @brief Draws a path node: its data ("d"), view box and fit, fill and stroke.
/// @param panel The panel, a PathView.
/// @param values Its values, if given.
/// @param reader Reads colours, through the theme.
/// @param path The node's place, for errors.
void applyPath(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	static constexpr std::array<std::pair<std::string_view, ImageFit>, 3> fits{
	    {{"contain", ImageFit::contain}, {"cover", ImageFit::cover}, {"fill", ImageFit::fill}}};
	static constexpr std::array<std::pair<std::string_view, FillRule>, 2> rules{
	    {{"nonzero", FillRule::nonZero}, {"evenodd", FillRule::evenOdd}}};
	auto* view = dynamic_cast<PathView*>(&panel);
	if(!view) return;
	try {
		view->setPathData(values.contains("d") ? string(values["d"], path + ".d") : std::string());
	} catch(const std::invalid_argument& error) {
		fail(path + ".d", error.what());
	}
	view->setViewBox(values.contains("viewBox") ? std::optional(viewBoxOf(values["viewBox"], path + ".viewBox"))
	                                            : std::nullopt);
	view->setFit(values.contains("fit")
	                 ? choiceOf(values["fit"], path + ".fit", fits, R"("contain", "cover" or "fill")")
					 : ImageFit::contain);
	view->setFillColor(values.contains("fillColor") ? reader.color(values["fillColor"], path + ".fillColor")
	                                                : colors::transparent);
	view->setFillRule(values.contains("fillRule")
	                      ? choiceOf(values["fillRule"], path + ".fillRule", rules, R"("nonzero" or "evenodd")")
						  : FillRule::nonZero);
	view->setStrokeColor(values.contains("stroke") ? reader.color(values["stroke"], path + ".stroke")
	                                               : colors::transparent);
	view->setStrokeStyle(strokeOf(values, path));
}

} // namespace

void registerPathType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"d", ValueKind::Text},
	                   {"viewBox", ValueKind::Text},
	                   {"fit", ValueKind::Text},
	                   {"fillColor", ValueKind::Color},
	                   {"fillRule", ValueKind::Text},
	                   {"stroke", ValueKind::Color},
	                   {"strokeWidth", ValueKind::Number},
	                   {"strokeJoin", ValueKind::Text},
	                   {"strokeCap", ValueKind::Text},
	                   {"strokeDash", ValueKind::Any}};
	set->apply = applyPath;
	set->everyUpdate = true; // colours follow the theme; the data is parsed again only when it changes
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const PathView*>(&panel) != nullptr; };

	registry.add({"path", "SVG path data, filled and stroked, fitted to the node",
	              [] { return std::make_unique<PathView>(); }, set, false});
}

} // namespace affresco::format
