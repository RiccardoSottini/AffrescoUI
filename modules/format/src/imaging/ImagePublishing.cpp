/// @file
/// @brief The runtime's side of the images it edits: their declarations and their publisher (ImagePublishing.hpp).

#include "ImagePublishing.hpp"

#include "Engine.hpp"
#include "ImageDocuments.hpp"
#include "NodeKeys.hpp"

#include <string>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Writes an image's snapshot to its "into" state path, and its thumbnails to the image cache.
/// @param engine The runtime.
/// @param image The image.
/// @throws DocumentError If the snapshot can't be written there.
void publish(Engine& engine, editing::ImageDocument& image) {
	editing::updateThumbnails(image, engine.document.images());
	if(image.definition.into.empty()) return;
	const std::string at = "images." + image.name + ".into";
	const Scope none{};
	const json::json_pointer pointer =
	    statePointer(image.definition.into, engine.document.context(none, emptyObject()), at);
	json snapshot = editing::snapshot(image);
	expect::shallowEnough(snapshot, pointerDepth(pointer), at);
	statePlace(engine.versions.state(), pointer, at) = std::move(snapshot);
	engine.document.markStateChanged(pointer);
	engine.reconciler.requestStateUpdate();
}

} // namespace

void attachImageDocuments(Engine& engine) {
	engine.types.imageDocuments()->attach(
	    [&engine]() -> const json* {
		    const json& document = engine.versions.document();
		    const auto images = document.find("images");
		    return images == document.end() ? nullptr : &*images;
	    },
	    [&engine](editing::ImageDocument& image) { publish(engine, image); });
}

void detachImageDocuments(Engine& engine) noexcept {
	if(const auto& images = engine.types.imageDocuments()) images->detach();
}

} // namespace affresco::format
