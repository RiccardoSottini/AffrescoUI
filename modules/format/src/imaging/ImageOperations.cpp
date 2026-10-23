/// @file
/// @brief format::editing: the table of the "image" step's operations, gathered once from their files.

#include "ImageOperations.hpp"

#include <algorithm>
#include <iterator>

namespace affresco::format::editing {

std::span<const ImageOperation> imageOperations() {
	static const std::vector<ImageOperation> operations = [] {
		std::vector<ImageOperation> all;
		for(auto group : {fileOperations(), layerOperations(), editOperations(), viewOperations()})
			std::ranges::move(group, std::back_inserter(all));
		return all;
	}();
	return operations;
}

const ImageOperation* findImageOperation(std::string_view name) {
	const auto operations = imageOperations();
	const auto found = std::ranges::find(operations, name, &ImageOperation::name);
	return found == operations.end() ? nullptr : &*found;
}

std::string nameOf(const json& with, std::string_view key, std::string_view fallback) {
	std::string name = textOf(with, key, fallback);
	if(name.size() > maxNameLength)
		throw DocumentError(std::string(key) + " is at most " + std::to_string(maxNameLength) + " bytes");
	return name;
}

} // namespace affresco::format::editing
