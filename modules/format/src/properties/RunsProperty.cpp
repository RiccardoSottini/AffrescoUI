/// @file
/// @brief The rich text properties of labels: "runs" (parts of the text with their own font, size, bold, italic,
/// colour, underline, strike, background, shadow, link or icon), "selectable" and "selectionColor".

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Label.hpp"
#include "affresco/render/TextArea.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>

namespace affresco::format {

namespace {

constexpr std::size_t runLimit = 256;                          ///< Runs one label may have.
constexpr std::size_t characterLimit = std::size_t{64} * 1024; ///< Code points of all of them.
constexpr std::size_t linkLimit = 2048;                        ///< Bytes of one link.
constexpr double smallestSize = 4.0;                           ///< A run's smallest "size", in pixels.
constexpr double largestSize = 256.0;                          ///< Its largest.

/// @brief The keys a run object may have.
constexpr std::array<std::string_view, 12> runKeys = {"text",  "icon",      "font",   "size",       "bold",   "italic",
                                                      "color", "underline", "strike", "background", "shadow", "link"};

/// @brief A run's font: the one it names or the label's, in the size and style it asks for.
/// @param run The run.
/// @param label The label.
/// @param reader Reads the font.
/// @param at The run's place, for errors.
/// @return The font, or nullptr for the label's own.
std::shared_ptr<const Font> runFont(const json& run, const Label& label, const PropertyReader& reader,
                                    const std::string& at) {
	std::shared_ptr<const Font> named = run.contains("font") ? reader.font(run["font"], at + ".font") : nullptr;
	const Font* base = named ? named.get() : label.font();
	const FontStyle given = base ? base->style() : FontStyle{}; // a bold font's runs stay bold unless they say
	const FontStyle style{run.contains("bold") ? boolean(run["bold"], at + ".bold") : given.bold,
	                      run.contains("italic") ? boolean(run["italic"], at + ".italic") : given.italic};
	float size = base ? base->pixelHeight() : 0.0f;
	if(run.contains("size")) {
		const json& value = run["size"];
		if(!value.is_number()) fail(at + ".size", "expected a number of pixels");
		size = static_cast<float>(std::clamp(value.get<double>(), smallestSize, largestSize));
	}
	if(!base || (size == base->pixelHeight() && style == base->style())) return named;
	if(auto variant = reader.fontVariant(named ? named.get() : nullptr, size, style, at)) return variant;
	return named;
}

/// @brief Reads one run: text, or an object with "text" or "icon" and its style.
/// @param run The run.
/// @param label The label, whose font a run without "font" is based on.
/// @param reader Reads colours and fonts.
/// @param at Its place, for errors.
/// @return The run.
/// @throws DocumentError If it is malformed.
TextRun readRun(const json& run, const Label& label, const PropertyReader& reader, const std::string& at) {
	TextRun result;
	if(!run.is_object()) {
		if(run.is_structured() || run.is_null()) fail(at, "expected text or a run object");
		result.text = codepointsOf(text(run));
		return result;
	}
	for(const auto& [key, value] : run.items())
		if(!std::ranges::contains(runKeys, key))
			fail(concatenate(at, ".", key),
			     "unknown run property (expected text, icon, font, size, bold, italic, color, underline, strike, "
			     "background, shadow or link)");
	if(run.contains("text")) result.text = codepointsOf(text(run["text"]));
	if(run.contains("icon")) result.icon = string(run["icon"], at + ".icon");
	if(run.contains("color")) result.color = reader.color(run["color"], at + ".color");
	if(run.contains("underline")) result.underline = boolean(run["underline"], at + ".underline");
	if(run.contains("strike")) result.strike = boolean(run["strike"], at + ".strike");
	// A null background or shadow is none, so a binding can switch it off
	if(run.contains("background") && !run["background"].is_null())
		result.background = reader.color(run["background"], at + ".background");
	if(run.contains("shadow") && !run["shadow"].is_null()) result.shadow = reader.color(run["shadow"], at + ".shadow");
	if(run.contains("link")) {
		result.link = text(run["link"]);
		if(result.link.size() > linkLimit) fail(at + ".link", std::format("at most {} bytes", linkLimit));
	}
	result.font = runFont(run, label, reader, at);
	return result;
}

/// @brief Reads "runs".
/// @param value The list.
/// @param label The label.
/// @param reader Reads colours and fonts.
/// @param at Its place, for errors.
/// @return The runs.
/// @throws DocumentError If it isn't a list of runs, or is over the limits.
std::vector<TextRun> readRuns(const json& value, const Label& label, const PropertyReader& reader,
                              const std::string& at) {
	if(!value.is_array()) fail(at, "expected a list of runs");
	if(value.size() > runLimit) fail(at, std::format("at most {} runs", runLimit));
	std::vector<TextRun> runs;
	runs.reserve(value.size());
	std::size_t characters = 0;
	for(std::size_t index = 0; index < value.size(); ++index) {
		runs.push_back(readRun(value[index], label, reader, std::format("{}[{}]", at, index)));
		characters += runs.back().text.size();
		if(characters > characterLimit) fail(at, std::format("at most {} characters in all", characterLimit));
	}
	return runs;
}

/// @brief Applies "runs", "selectable" and "selectionColor" to a label.
/// @param panel The panel, a Label.
/// @param values Those it has.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyRichText(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* label = dynamic_cast<Label*>(&panel);
	if(!label) return;
	label->setSelectable(values.contains("selectable") && boolean(values["selectable"], path + ".selectable"));
	if(values.contains("selectionColor"))
		label->setSelectionColor(reader.color(values["selectionColor"], path + ".selectionColor"));
	if(!values.contains("runs")) {
		if(!label->runs().empty()) label->setRuns({}); // removed: the text is empty until "text" sets it again
		return;
	}
	std::vector<TextRun> runs = readRuns(values["runs"], *label, reader, path + ".runs");
	// Set only when they change, so the theme's updates keep the layout and the selection
	if(runs != label->runs()) label->setRuns(std::move(runs));
}

} // namespace

void registerRichTextProperties(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"runs", ValueKind::Any, false},
	                   {"selectable", ValueKind::Boolean, false},
	                   {"selectionColor", ValueKind::Color}};
	set->apply = applyRichText;
	set->everyUpdate = true; // run colours follow the theme; the runs are set only when they change
	// Labels and the application's subclasses of them; not text inputs, which have their own editing
	set->appliesTo = [](const Panel& panel) {
		return dynamic_cast<const Label*>(&panel) != nullptr && dynamic_cast<const TextArea*>(&panel) == nullptr;
	};
	registry.addProperties(std::move(set));
}

} // namespace affresco::format
