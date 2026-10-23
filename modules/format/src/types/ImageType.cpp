/// @file
/// @brief The "image" node type: an ImageView showing a file (decoded on a worker thread), an inline `data:` image or
/// an image the host names, fitted to the node, with a region of it outlined if asked.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/ImageView.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Reads "fit".
/// @param value "contain", "cover" or "fill".
/// @param at Its place, for errors.
/// @return The fit.
/// @throws DocumentError If it is none of them.
ImageFit fitOf(const json& value, const std::string& at) {
	static constexpr std::array<std::pair<std::string_view, ImageFit>, 3> fits{
	    {{"contain", ImageFit::contain}, {"cover", ImageFit::cover}, {"fill", ImageFit::fill}}};
	const std::string name = string(value, at);
	for(const auto& [word, fit] : fits)
		if(word == name) return fit;
	fail(at, R"(expected "contain", "cover" or "fill")");
}

/// @brief Reads "highlight".
/// @param value null for none, or `[x, y, width, height]`: four finite numbers, in the image's pixels.
/// @param at Its place, for errors.
/// @return The region, or nullopt.
/// @throws DocumentError If it is neither.
std::optional<std::array<double, 4>> highlightOf(const json& value, const std::string& at) {
	if(value.is_null()) return std::nullopt;
	if(!value.is_array() || value.size() != 4) fail(at, "expected null or [x, y, width, height]");
	std::array<double, 4> region{};
	for(std::size_t i = 0; i < region.size(); ++i) {
		if(!value[i].is_number() || !std::isfinite(value[i].get<double>()))
			fail(concatenate(at, "[", std::to_string(i) + "]"), "expected a finite number");
		region[i] = value[i].get<double>();
	}
	return region;
}

/// @brief Shows an image node's picture: the host's named image or an inline `data:` one ("source"), else a file
/// ("file"), else none; and outlines its "highlight".
/// @param panel The panel, an ImageView.
/// @param values Its file, source, fit, highlight and highlightColor, if given.
/// @param reader Reads image files and gives the image cache.
/// @param path The node's place, for errors.
void applyImage(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* view = dynamic_cast<ImageView*>(&panel);
	if(!view) return;
	view->setFit(values.contains("fit") ? fitOf(values["fit"], path + ".fit") : ImageFit::contain);
	if(values.contains("source")) {
		const std::string source = string(values["source"], path + ".source");
		if(source.starts_with("data:")) view->showData(source, Bitmap::Limits{});
		else view->showNamed(reader.images(), source);
	} else if(values.contains("file"))
		view->showFile(reader.images(), reader.imageFile(values["file"], path + ".file"));
	else view->setBitmap(nullptr);
	view->setHighlight(values.contains("highlight") ? highlightOf(values["highlight"], path + ".highlight")
	                                                : std::nullopt);
	if(values.contains("highlightColor"))
		view->setHighlightColor(reader.color(values["highlightColor"], path + ".highlightColor"));
}

} // namespace

void registerImageType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"file", ValueKind::Text},
	                   {"source", ValueKind::Text},
	                   {"fit", ValueKind::Text},
	                   {"highlight", ValueKind::Any},
	                   {"highlightColor", ValueKind::Color}};
	set->apply = applyImage; // when the values change: a new file is asked for once
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const ImageView*>(&panel) != nullptr; };

	registry.add(
	    {"image",
		 "A PNG, JPEG or SVG file, an inline data: image or an image the host names, fitted to the node, with a "
		 "region outlined if asked",
		 [] { return std::make_unique<ImageView>(); }, set, false});
}

} // namespace affresco::format
