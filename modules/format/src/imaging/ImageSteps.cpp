/// @file
/// @brief The "image" step: runs one operation (ImageOperations.hpp) on an image the "images" section declares, then
/// publishes the image's snapshot to its "into" state path and its thumbnails to the image cache.

#include "Engine.hpp"
#include "ImageOperations.hpp"

namespace affresco::format {

namespace {

/// @brief The name of the image a step names: its "of", or the only image the screen declares.
/// @param s The step.
/// @param images The "images" section.
/// @return The name.
/// @throws DocumentError If it names none, or one not declared.
[[nodiscard]] std::string imageNameOf(const Running& s, const json& images) {
	const std::string& where = s.where;
	if(!s.step.contains("of")) {
		if(images.size() != 1) fail(where, "\"of\" names the image when a screen declares more than one");
		return images.begin().key();
	}
	std::string name = string(s.value("of"), where + ".of");
	if(!images.contains(name)) fail(where + ".of", "no image \"" + name + "\" in the \"images\" section");
	return name;
}

/// @brief Writes an image's snapshot to its "into" state path, and its thumbnails to the image cache.
/// @param s The step.
/// @param image The image.
void publish(const Running& s, editing::ImageDocument& image) {
	Engine& engine = s.steps.engine();
	editing::updateThumbnails(image, engine.document.images());
	if(image.definition.into.empty()) return;
	const std::string at = "images." + image.name + ".into";
	const json::json_pointer pointer =
	    statePointer(image.definition.into, engine.document.context(s.locals, s.event), at);
	json snapshot = editing::snapshot(image);
	expect::shallowEnough(snapshot, pointerDepth(pointer), at);
	statePlace(engine.versions.state(), pointer, at) = std::move(snapshot);
	engine.document.markStateChanged(pointer);
}

/// @brief Runs an "image" step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runImageStep(const Running& s) {
	const json& step = s.step;
	if(!step.contains("image")) return std::nullopt;
	const std::string& where = s.where;
	Engine& engine = s.steps.engine();
	const std::string operation = string(step["image"], where + ".image");
	const editing::ImageOperation* found = editing::findImageOperation(operation);
	if(found == nullptr) fail(where + ".image", "unknown image operation \"" + operation + "\"");
	const json& document = engine.versions.document();
	const auto images = document.find("images");
	if(images == document.end() || !images->is_object() || images->empty())
		fail(where, "an \"image\" step needs an \"images\" section");
	const std::string name = imageNameOf(s, *images);
	editing::ImageDocument& image =
	    engine.types.imageDocuments()->image(name, (*images)[name], "images." + name);
	const json with = step.contains("with") ? s.value("with") : json::object();
	if(!with.is_object()) fail(where + ".with", "expected an object");
	std::optional<std::string> problem;
	try {
		found->run(image, with, engine.window);
	} catch(const std::exception& error) {
		problem = error.what();
		image.message = *problem;
	}
	publish(s, image);
	if(!problem) return true;
	if(!step.contains("onError")) fail(where + ".image", operation + ": " + *problem);
	const Scope scope{&s.locals, nullptr, {{"error", *problem}}, {}, {}};
	return s.run(step["onError"], scope, where + ".onError");
}

} // namespace

void registerImageSteps(StepRegistry& registry) {
	registry.add({"image", {{"image", {"of", "with", "onError"}}}, runImageStep});
}

} // namespace affresco::format
