/// @file
/// @brief format::editing: the "image" step's history and layer operations: undo, redo, history, clearHistory,
/// addLayer, deleteLayer, duplicateLayer, selectLayer, addMask, deleteMask, moveLayer and setLayer.

#include "ImageOperations.hpp"

#include "affresco/imaging/Project.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco::format::editing {

namespace {

/// @brief Any step or position past a list's length is the same as its end, so one this far can't overflow a sum.
constexpr double farthest = 1e9;

/// @brief undo: takes back the last step.
/// @param image The image.
void undo(ImageDocument& image, const json&, Window*) {
	settle(image);
	image.canvas->document().undo();
}

/// @brief redo: does again the step last taken back.
/// @param image The image.
void redo(ImageDocument& image, const json&, Window*) {
	settle(image);
	image.canvas->document().redo();
}

/// @brief history: goes to a place in the history (0 before the first step; past the last means the last).
/// @param image The image.
/// @param with position.
void goToStep(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const double position = std::clamp(numberOf(with, "position", 0.0), 0.0, farthest);
	image.canvas->document().goTo(static_cast<std::size_t>(position));
}

/// @brief clearHistory: forgets every step, so the image as it is can't be undone.
/// @param image The image.
void clearHistory(ImageDocument& image, const json&, Window*) {
	settle(image);
	image.canvas->document().clearHistory();
}

/// @brief addLayer: adds a pixel layer, a group or an adjustment layer above the active one.
/// @param image The image.
/// @param with kind ("pixel", "group" or an adjustment's kind), name.
/// @throws DocumentError If the kind is unknown or the name too long.
void addLayer(ImageDocument& image, const json& with, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	const std::string kind = nameOf(with, "kind", "pixel");
	const std::string name = nameOf(with, "name");
	if(kind == "pixel") {
		document.addPixelLayer(name.empty() ? "Layer " + std::to_string(document.structure().nextId - 1) : name);
	} else if(kind == "group") {
		document.addGroup(name.empty() ? "Group" : name);
	} else {
		const auto adjustment = imaging::adjustmentKindOf(kind);
		if(!adjustment) throw DocumentError("Unknown layer kind " + kind);
		imaging::Adjustment settings;
		settings.kind = *adjustment;
		document.addAdjustment(name.empty() ? kind : name, settings);
	}
}

/// @brief Makes an operation that settles, finds the layer of "id" and does one thing to it.
/// @tparam Body What it does: void(imaging::Document&, std::uint64_t).
/// @return The runner.
template <auto Body> [[nodiscard]] constexpr ImageOperationRunner onLayer() {
	return [](ImageDocument& image, const json& with, Window*) {
		settle(image);
		Body(image.canvas->document(), layerOf(image, with));
	};
}

/// @brief moveLayer: moves a layer onto another's place in the list, or a step up or down.
/// @param image The image.
/// @param with id; then "onto" (the id of the layer whose place it takes) or "by" (+1 up, -1 down).
/// @throws DocumentError If a layer is unknown.
void moveLayer(ImageDocument& image, const json& with, Window*) {
	settle(image);
	imaging::Document& document = image.canvas->document();
	const std::uint64_t id = layerOf(image, with);
	const imaging::Layer& layer = *document.layer(id);
	const auto placeIn = [&](std::uint64_t parent) -> const std::vector<std::uint64_t>& {
		return parent == 0 ? document.structure().root : document.layer(parent)->children;
	};
	if(with.contains("onto")) {
		const std::uint64_t onto = layerOf(image, {{"id", with["onto"]}});
		if(onto == id) return;
		const imaging::Layer& target = *document.layer(onto);
		const auto& siblings = placeIn(target.parent);
		auto index = static_cast<std::size_t>(std::ranges::find(siblings, onto) - siblings.begin());
		// Document::moveLayer places it before the layer at index; taking the place of one above means after it.
		if(layer.parent == target.parent && std::cmp_less(std::ranges::find(siblings, id) - siblings.begin(), index))
			++index;
		document.moveLayer(id, target.parent, index);
		return;
	}
	const auto& siblings = placeIn(layer.parent);
	const auto index = static_cast<long>(std::ranges::find(siblings, id) - siblings.begin());
	const long by = std::lround(std::clamp(numberOf(with, "by", 1.0), -farthest, farthest));
	// Document::moveLayer places it before the layer at index, counted before it leaves, so up skips one more.
	document.moveLayer(id, layer.parent, static_cast<std::size_t>(std::max(0L, index + by + (by > 0 ? 1 : 0))));
}

/// @brief The history step's name of a change of a layer's settings.
/// @param with The settings changed.
/// @param blend Whether the blend mode changes.
/// @return The name.
[[nodiscard]] std::string layerChangeName(const json& with, bool blend) {
	return with.contains("opacity")      ? "Opacity Change"
	       : with.contains("adjustment") ? "Adjustment Change"
	       : with.contains("visible")    ? "Layer Visibility"
	       : blend                       ? "Blending Change"
	                                     : "Layer Properties";
}

/// @brief setLayer: changes a layer's settings; a slider's changes (opacity, an adjustment) coalesce into one step.
/// @param image The image.
/// @param with id, and any of name, visible, opacity (0 to 100), blend, lockPixels, lockPosition, lockAll, clipped,
/// maskEnabled, adjustment.
/// @throws DocumentError If the layer or the blend mode is unknown.
void setLayer(ImageDocument& image, const json& with, Window*) {
	settle(image);
	const std::uint64_t id = layerOf(image, with);
	std::optional<imaging::BlendMode> blend;
	if(with.contains("blend")) {
		blend = imaging::blendModeOf(textOf(with, "blend"));
		if(!blend) throw DocumentError("Unknown blend mode " + textOf(with, "blend"));
	}
	const std::optional<std::string> name = with.contains("name") ? std::optional(nameOf(with, "name")) : std::nullopt;
	const bool sliding = with.contains("opacity") || with.contains("adjustment");
	image.canvas->document().changeLayer(
	    id, layerChangeName(with, blend.has_value()),
	    [&](imaging::Layer& layer) {
		    if(name) layer.name = *name;
		    layer.visible = flagOf(with, "visible", layer.visible);
		    layer.opacity =
		        std::clamp(static_cast<float>(numberOf(with, "opacity", layer.opacity * 100.0) / 100.0), 0.0f, 1.0f);
		    if(blend) layer.blend = *blend;
		    layer.locks.pixels = flagOf(with, "lockPixels", layer.locks.pixels);
		    layer.locks.position = flagOf(with, "lockPosition", layer.locks.position);
		    layer.locks.all = flagOf(with, "lockAll", layer.locks.all);
		    layer.clipped = flagOf(with, "clipped", layer.clipped);
		    layer.maskEnabled = flagOf(with, "maskEnabled", layer.maskEnabled);
		    if(layer.kind == imaging::LayerKind::adjustment && with.contains("adjustment"))
			    layer.adjustment = imaging::adjustmentFromJson(with["adjustment"]);
	    },
	    sliding);
}

/// @brief deleteLayer's body.
/// @param document The document.
/// @param id The layer.
void deleteLayer(imaging::Document& document, std::uint64_t id) { document.deleteLayer(id); }
/// @brief duplicateLayer's body.
/// @param document The document.
/// @param id The layer.
void duplicateLayer(imaging::Document& document, std::uint64_t id) { document.duplicateLayer(id); }
/// @brief selectLayer's body.
/// @param document The document.
/// @param id The layer.
void selectLayer(imaging::Document& document, std::uint64_t id) { document.setActive(id); }
/// @brief addMask's body.
/// @param document The document.
/// @param id The layer.
void addMask(imaging::Document& document, std::uint64_t id) { document.addMask(id); }
/// @brief deleteMask's body.
/// @param document The document.
/// @param id The layer.
void deleteMask(imaging::Document& document, std::uint64_t id) { document.deleteMask(id); }

} // namespace

std::vector<ImageOperation> layerOperations() {
	return {
	    {"undo", {}, "Takes back the last step.", undo},
	    {"redo", {}, "Does again the step last taken back.", redo},
	    {"history", {"position"}, "Goes to a place in the history (0 is before the first step).", goToStep},
	    {"clearHistory", {}, "Forgets every step of the history.", clearHistory},
	    {"addLayer", {"kind", "name"}, "Adds a pixel layer, a group or an adjustment layer.", addLayer},
	    {"deleteLayer", {"id"}, "Deletes a layer (a group with its layers).", onLayer<deleteLayer>()},
	    {"duplicateLayer", {"id"}, "Duplicates a layer.", onLayer<duplicateLayer>()},
	    {"selectLayer", {"id"}, "Makes a layer the active one.", onLayer<selectLayer>()},
	    {"addMask", {"id"}, "Adds a mask to a layer.", onLayer<addMask>()},
	    {"deleteMask", {"id"}, "Deletes a layer's mask.", onLayer<deleteMask>()},
	    {"moveLayer", {"id", "onto", "by"}, "Moves a layer onto another's place, or a step up or down.", moveLayer},
	    {"setLayer",
	     {"id", "name", "visible", "opacity", "blend", "lockPixels", "lockPosition", "lockAll", "clipped",
	      "maskEnabled", "adjustment"},
	     "Changes a layer's settings.",
	     setLayer},
	};
}

} // namespace affresco::format::editing
