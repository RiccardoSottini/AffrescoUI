/// @file
/// @brief The flexible and grid layout keys: readers for their values, the keys every node has, and the property sets
/// of rows, columns and grids.

#include "LayoutProperties.hpp"
#include "NodeKeys.hpp"
#include "PropertyValues.hpp"

#include "affresco/render/Stack.hpp"

#include <cmath>
#include <exception>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

namespace affresco::format {

using expect::boolean;
using expect::fail;
using expect::integer;
using expect::object;
using expect::string;

namespace {

/// @brief Reads "start", "center", "end" or "stretch".
/// @param value The value.
/// @param at Its place, for errors.
/// @return The alignment.
/// @throws DocumentError If it is something else.
CrossAlign crossAlign(const json& value, const std::string& at) {
	const std::string name = string(value, at);
	if(name == "start") return CrossAlign::Start;
	if(name == "center") return CrossAlign::Center;
	if(name == "end") return CrossAlign::End;
	if(name == "stretch") return CrossAlign::Stretch;
	fail(at, R"(expected "start", "center", "end" or "stretch")");
}

/// @brief Reads a justification.
/// @param value The value.
/// @param at Its place, for errors.
/// @return The justification.
/// @throws DocumentError If it is something else.
Justify justification(const json& value, const std::string& at) {
	const std::string name = string(value, at);
	if(name == "start") return Justify::Start;
	if(name == "center") return Justify::Center;
	if(name == "end") return Justify::End;
	if(name == "space-between") return Justify::SpaceBetween;
	if(name == "space-around") return Justify::SpaceAround;
	if(name == "space-evenly") return Justify::SpaceEvenly;
	fail(at, R"(expected "start", "center", "end", "space-between", "space-around" or "space-evenly")");
}

/// @brief Reads a writing direction: "ltr", "rtl", "auto" (the locale's) or "inherit".
/// @param value The value.
/// @param at Its place, for errors.
/// @return The direction.
/// @throws DocumentError If it is something else.
WritingDirection writingDirection(const json& value, const std::string& at) {
	const std::string name = string(value, at);
	if(name == "ltr") return WritingDirection::LeftToRight;
	if(name == "rtl") return WritingDirection::RightToLeft;
	if(name == "auto") return WritingDirection::Locale;
	if(name == "inherit") return WritingDirection::Inherit;
	fail(at, R"(expected "ltr", "rtl", "auto" (the locale's) or "inherit")");
}

/// @brief Reads a number that ends a text, after its unit was taken off.
/// @param text The number.
/// @return It, or std::nullopt if the text isn't exactly a finite number of 0 or more.
std::optional<float> plainNumber(std::string_view text) {
	try {
		std::size_t used = 0;
		const std::string digits(text);
		const float amount = std::stof(digits, &used);
		if(used == digits.size() && std::isfinite(amount) && amount >= 0.0f) return amount;
		// NOLINTNEXTLINE(bugprone-empty-catch): not a number, reported by the caller like any other malformed size
	} catch(const std::exception&) {
	}
	return std::nullopt;
}

/// @brief Reads one bound of a track: 120 or "120px", "25%", "1fr" or "auto".
/// @param value The value.
/// @param at Its place, for errors.
/// @return The bound.
/// @throws DocumentError If it is none of those.
TrackSize trackSize(const json& value, const std::string& at) {
	if(value.is_number()) return {pixels(value, at), TrackUnit::Pixels};
	if(value.is_string()) {
		const std::string_view text = value.get_ref<const std::string&>();
		if(text == "auto") return {0.0f, TrackUnit::Content};
		const auto unit = [&](std::string_view suffix, TrackUnit kind) -> std::optional<TrackSize> {
			if(!text.ends_with(suffix)) return std::nullopt;
			const auto amount = plainNumber(text.substr(0, text.size() - suffix.size()));
			return amount ? std::optional(TrackSize{*amount, kind}) : std::nullopt;
		};
		for(const auto& [suffix, kind] : {std::pair{std::string_view("fr"), TrackUnit::Fraction},
		                                  std::pair{std::string_view("%"), TrackUnit::Percent},
		                                  std::pair{std::string_view("px"), TrackUnit::Pixels}})
			if(const auto size = unit(suffix, kind)) return *size;
		if(const auto amount = plainNumber(text)) return {*amount, TrackUnit::Pixels};
	}
	fail(at, R"(expected a track size: pixels (120), a percentage ("25%"), a fraction ("1fr") or "auto")");
}

/// @brief Reads one track: a size, or "minmax(minimum, maximum)".
/// @param value The value.
/// @param at Its place, for errors.
/// @return The track.
/// @throws DocumentError If it is malformed.
Track track(const json& value, const std::string& at) {
	if(value.is_string() && value.get_ref<const std::string&>().starts_with("minmax(")) {
		const std::string& text = value.get_ref<const std::string&>();
		const std::size_t comma = text.find(',');
		if(!text.ends_with(')') || comma == std::string::npos)
			fail(at, R"text(expected "minmax(minimum, maximum)", e.g. "minmax(100, 1fr)")text");
		const auto trimmed = [](std::string_view part) {
			while(!part.empty() && part.front() == ' ') part.remove_prefix(1);
			while(!part.empty() && part.back() == ' ') part.remove_suffix(1);
			return json(std::string(part));
		};
		const std::string_view inside(text.data() + 7, text.size() - 8);
		return Track::between(trackSize(trimmed(inside.substr(0, comma - 7)), at),
		                      trackSize(trimmed(inside.substr(comma - 6)), at));
	}
	const TrackSize size = trackSize(value, at);
	if(size.unit == TrackUnit::Fraction) return Track::fraction(size.value); // from 0, so equal fractions stay equal
	return {size, size};
}

/// @brief Reads a grid cell: { "column", "row", "columnSpan", "rowSpan" }, each a whole number from 1.
/// @param value The value.
/// @param at Its place, for errors.
/// @return The cell.
/// @throws DocumentError If it is malformed.
GridCell cell(const json& value, const std::string& at) {
	GridCell result;
	for(const auto& [key, number] : object(value, at).items()) {
		const std::string where = concatenate(at, ".", key);
		const int amount = integer(number, where);
		if(amount < 1 || std::cmp_greater(amount, maxGridTracks))
			fail(where, std::format("expected a whole number from 1 to {}", maxGridTracks));
		if(key == "column") result.column = amount;
		else if(key == "row") result.row = amount;
		else if(key == "columnSpan") result.columnSpan = amount;
		else if(key == "rowSpan") result.rowSpan = amount;
		else fail(where, "unknown property (expected column, row, columnSpan or rowSpan)");
	}
	return result;
}

/// @brief Reads an aspect ratio: a number, or "16/9".
/// @param value The value.
/// @param at Its place, for errors.
/// @return Width divided by height, more than 0.
/// @throws DocumentError If it is malformed.
float aspectRatio(const json& value, const std::string& at) {
	if(value.is_string()) {
		const std::string& text = value.get_ref<const std::string&>();
		if(const std::size_t slash = text.find('/'); slash != std::string::npos) {
			std::string_view width(text.data(), slash), height(text.data() + slash + 1, text.size() - slash - 1);
			while(width.ends_with(' ')) width.remove_suffix(1);
			while(height.starts_with(' ')) height.remove_prefix(1);
			const auto w = plainNumber(width), h = plainNumber(height);
			if(w && h && *w > 0.0f && *h > 0.0f) return *w / *h;
		}
	} else if(value.is_number()) {
		const float ratio = finiteNumber(value, at);
		if(ratio > 0.0f) return ratio;
	}
	fail(at, R"(expected a ratio of width to height: a number more than 0, or "16/9")");
}

/// @brief Reads a number of 0 or more.
/// @param value The value.
/// @param at Its place, for errors.
/// @return The number.
/// @throws DocumentError If it is not one.
float nonNegative(const json& value, const std::string& at) {
	const float amount = finiteNumber(value, at);
	if(amount < 0.0f) fail(at, "expected a number, 0 or more");
	return amount;
}

/// @brief Reads an optional length: a length, or the word that means none.
/// @param value The value.
/// @param none "none" or "auto".
/// @param at Its place, for errors.
/// @return The length, or std::nullopt for @p none.
std::optional<Length> optionalLength(const json& value, std::string_view none, const std::string& at) {
	if(value.is_string() && value.get_ref<const std::string&>() == none) return std::nullopt;
	return length(value, at);
}

/// @brief Applies the keys of a row, a column or a stack that its set holds (padding and fit among them); justify
/// only when given, since the runtime's reset (and "from") set it otherwise.
/// @param panel The panel, a Stack.
/// @param values The keys the node gives.
/// @param reader Not used: these keys have no colours or fonts.
/// @param path Its place, for errors.
void applyStack(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	static_cast<void>(reader);
	auto* stack = dynamic_cast<Stack*>(&panel);
	if(!stack) return;
	const auto at = [&](const char* key) { return concatenate(path, ".", key); };
	if(values.contains("justify")) stack->setJustify(justification(values["justify"], at("justify")));
	stack->setAlignItems(values.contains("alignItems")
	                         ? std::optional(crossAlign(values["alignItems"], at("alignItems")))
							 : std::nullopt);
	stack->setRowGap(values.contains("rowGap") ? std::optional(pixels(values["rowGap"], at("rowGap"))) : std::nullopt);
	stack->setColumnGap(values.contains("columnGap") ? std::optional(pixels(values["columnGap"], at("columnGap")))
	                                                 : std::nullopt);
	stack->setPadding(values.contains("padding") ? pixels(values["padding"], at("padding")) : 0.0f);
	stack->setFit(values.contains("fit") && boolean(values["fit"], at("fit")));
}

/// @brief Applies the keys of a grid that its set holds, each to its default when the node doesn't give it.
/// @param panel The panel, a Grid.
/// @param values The keys the node gives.
/// @param reader Not used: these keys have no colours or fonts.
/// @param path Its place, for errors.
void applyGrid(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	static_cast<void>(reader);
	auto* grid = dynamic_cast<Grid*>(&panel);
	if(!grid) return;
	const auto at = [&](const char* key) { return concatenate(path, ".", key); };
	grid->setRows(values.contains("rows") ? gridTracks(values["rows"], at("rows")) : std::vector<Track>{});
	grid->setRowGap(values.contains("rowGap") ? std::optional(pixels(values["rowGap"], at("rowGap"))) : std::nullopt);
	grid->setColumnGap(values.contains("columnGap") ? std::optional(pixels(values["columnGap"], at("columnGap")))
	                                                : std::nullopt);
	grid->setJustifyItems(values.contains("justifyItems") ? crossAlign(values["justifyItems"], at("justifyItems"))
	                                                      : CrossAlign::Stretch);
	grid->setAlignItems(values.contains("alignItems") ? crossAlign(values["alignItems"], at("alignItems"))
	                                                  : CrossAlign::Start);
}

} // namespace

std::vector<PropertySpec> layoutItemProperties() {
	return {{"maxWidth"},    {"maxHeight"},
	        {"aspectRatio"}, {"shrink", ValueKind::Number},
	        {"basis"},       {"alignSelf", ValueKind::Text},
	        {"cell"},        {"dir", ValueKind::Text}};
}

bool layoutItemProperty(Panel& panel, std::string_view key, const json& value, const std::string& at) {
	if(key == "maxWidth") panel.setMaxWidth(optionalLength(value, "none", at));
	else if(key == "maxHeight") panel.setMaxHeight(optionalLength(value, "none", at));
	else if(key == "aspectRatio") panel.setAspectRatio(aspectRatio(value, at));
	else if(key == "shrink") panel.setShrink(nonNegative(value, at));
	else if(key == "basis") panel.setBasis(optionalLength(value, "auto", at));
	else if(key == "alignSelf") panel.setAlignSelf(crossAlign(value, at));
	else if(key == "cell") panel.setGridCell(cell(value, at));
	else if(key == "dir") panel.setWritingDirection(writingDirection(value, at));
	else return false;
	return true;
}

void resetLayoutItem(Panel& panel) {
	panel.setMaxSize(std::nullopt, std::nullopt);
	panel.setAspectRatio(0.0f);
	panel.setShrink(0.0f);
	panel.setBasis(std::nullopt);
	panel.setAlignSelf(std::nullopt);
	panel.setGridCell({});
	panel.setWritingDirection(WritingDirection::Inherit);
}

std::shared_ptr<const PropertySet> stackProperties(std::initializer_list<PropertySpec> applied) {
	auto set = std::make_shared<PropertySet>();
	set->properties = applied;
	for(PropertySpec spec : {PropertySpec{"justify", ValueKind::Text}, PropertySpec{"alignItems", ValueKind::Text},
	                         PropertySpec{"rowGap", ValueKind::Number}, PropertySpec{"columnGap", ValueKind::Number},
	                         PropertySpec{"padding", ValueKind::Number}, PropertySpec{"fit", ValueKind::Boolean}})
		set->properties.push_back(std::move(spec));
	set->apply = applyStack;
	set->everyUpdate = true; // each key set once per update, so nothing changes when the node doesn't
	return set;
}

std::shared_ptr<const PropertySet> gridProperties() {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"columns"},
	                   {"gap", ValueKind::Number},
	                   {"rows"},
	                   {"rowGap", ValueKind::Number},
	                   {"columnGap", ValueKind::Number},
	                   {"justifyItems", ValueKind::Text},
	                   {"alignItems", ValueKind::Text}};
	set->apply = applyGrid;
	set->everyUpdate = true;
	return set;
}

std::vector<Track> gridTracks(const json& value, const std::string& at) {
	if(value.is_number()) {
		const int columns = integer(value, at);
		if(columns < 1 || std::cmp_greater(columns, maxGridTracks))
			fail(at, std::format("expected 1 to {} columns", maxGridTracks));
		return std::vector<Track>(static_cast<std::size_t>(columns), Track::fraction(1.0f));
	}
	if(!value.is_array() || value.empty() || value.size() > maxGridTracks)
		fail(at, std::format(R"(expected a number of equal columns, or a list of 1 to {} tracks, e.g. [200, "1fr"])",
		                     maxGridTracks));
	std::vector<Track> tracks;
	tracks.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) tracks.push_back(track(value[i], std::format("{}[{}]", at, i)));
	return tracks;
}

} // namespace affresco::format
