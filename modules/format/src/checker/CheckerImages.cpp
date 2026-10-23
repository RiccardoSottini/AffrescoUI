/// @file
/// @brief The checker's look at the images a screen edits: the state paths the "images" section publishes to, and the
/// "image" step's operation, its arguments and the image it names.

#include "CheckerImpl.hpp"
#include "ImageOperations.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace affresco::format::checker {

namespace {

/// @brief The names of the image operations.
/// @return Them, in the reference's order.
[[nodiscard]] std::vector<std::string> operationNames() {
	std::vector<std::string> names;
	for(const editing::ImageOperation& operation : editing::imageOperations()) names.emplace_back(operation.name);
	return names;
}

/// @brief A value written as it is, not computed.
/// @param value The value.
/// @return Whether it is text without "{{".
[[nodiscard]] bool literalText(const json& value) { return value.is_string() && !isDynamic(value); }

} // namespace

// Each image's snapshot is published to its "into" path: the screen reads it there
void Checker::walkImages() {
	const json* images = section("images");
	if(images == nullptr) return;
	for(const auto& [name, image] : images->items())
		if(image.is_object() && image.contains("into") && literalText(image["into"]))
			write(image["into"], anything, Change::set, Scope(), std::format("images.{}.into", name));
}

void Checker::walkImageStep(const json& step, const std::string& place) {
	const json* images = section("images");
	if(images == nullptr || !images->is_object() || images->empty()) {
		report(place, "an \"image\" step needs an \"images\" section");
		return;
	}
	if(!step.contains("of") && images->size() != 1)
		report(place, "\"of\" names the image when a screen declares more than one");
	if(step.contains("of") && literalText(step["of"]))
		refer("image", "images", step["of"].get<std::string>(), "", place + ".of");
	const json& name = step["image"];
	if(!literalText(name)) return; // computed: not known before it runs
	const editing::ImageOperation* operation = editing::findImageOperation(name.get_ref<const std::string&>());
	if(operation == nullptr) {
		report(place + ".image", std::format("unknown image operation '{}'", name.get<std::string>()),
		       hint([&] { return hintFor(name.get<std::string>(), operationNames(), "", "expected:"); }));
		return;
	}
	const auto with = step.find("with");
	if(with == step.end() || !with->is_object()) return; // computed whole: its keys aren't known
	for(const auto& [key, value] : with->items())
		if(std::ranges::find(operation->with, key) == operation->with.end())
			report(std::format("{}.with.{}", place, key), std::format("'{}' doesn't read '{}'", operation->name, key),
			       hint([&] {
				       const std::vector<std::string> keys(operation->with.begin(), operation->with.end());
				       return hintFor(key, keys, "", "it reads:");
			       }));
}

} // namespace affresco::format::checker
