/// @file
/// @brief The "image" node type: an ImageView showing a file (decoded on a worker thread) or an image the host names,
/// fitted to the node.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/ImageView.hpp"

#include <array>
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

/// @brief Shows an image node's picture: the host's named image ("source"), else a file ("file"), else none.
/// @param panel The panel, an ImageView.
/// @param values Its file, source and fit, if given.
/// @param reader Reads image files and gives the image cache.
/// @param path The node's place, for errors.
void applyImage(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* view = dynamic_cast<ImageView*>(&panel);
	if(!view) return;
	view->setFit(values.contains("fit") ? fitOf(values["fit"], path + ".fit") : ImageFit::contain);
	if(values.contains("source")) view->showNamed(reader.images(), string(values["source"], path + ".source"));
	else if(values.contains("file")) view->showFile(reader.images(), reader.imageFile(values["file"], path + ".file"));
	else view->setBitmap(nullptr);
}

} // namespace

void registerImageType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"file", ValueKind::Text}, {"source", ValueKind::Text}, {"fit", ValueKind::Text}};
	set->apply = applyImage; // when the values change: a new file is asked for once
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const ImageView*>(&panel) != nullptr; };

	registry.add({"image", "A PNG or JPEG file, or an image the host names, fitted to the node",
	              [] { return std::make_unique<ImageView>(); }, set, false});
}

} // namespace affresco::format
