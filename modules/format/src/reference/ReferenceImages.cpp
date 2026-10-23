/// @file
/// @brief The tables of SCREEN_FORMAT.md about the images a screen edits, generated from the library: the "image"
/// step's operations, and the names its values take (gestures, blend modes, adjustments, filters, gradient shapes,
/// limits).

#include "ImageDocuments.hpp"
#include "ImageOperations.hpp"
#include "Reference.hpp"

#include "affresco/imaging/Adjustment.hpp"
#include "affresco/imaging/Blend.hpp"
#include "affresco/imaging/Filter.hpp"
#include "affresco/imaging/Gradient.hpp"

#include <ranges>
#include <string>
#include <string_view>

namespace affresco::format {

namespace {

/// @brief Names written as code and separated by commas: "`a`, `b`".
/// @param names The names.
/// @return The text; "—" if there are none.
template <std::ranges::input_range Names> [[nodiscard]] std::string codeList(const Names& names) {
	std::string text;
	for(const std::string_view name : names) text += (text.empty() ? "`" : ", `") + std::string(name) + "`";
	return text.empty() ? "—" : text;
}

} // namespace

std::string imageOperationTable() {
	std::string text = "| Operation | Reads from `with` | What it does |\n|---|---|---|\n";
	for(const editing::ImageOperation& operation : editing::imageOperations())
		text += "| `" + std::string(operation.name) + "` | " + codeList(operation.with) + " | " +
		        std::string(operation.summary) + " |\n";
	return text;
}

std::string imageNameTable() {
	std::string limits;
	for(const auto& [name, value] : editing::libraryLimits())
		limits += (limits.empty() ? "`" : ", `") + std::string(name) + "` " + std::to_string(value);
	return "| Value | Names |\n|---|---|\n"
	       "| A tool's `gesture` | " +
	       codeList(editing::gestureNames()) +
	       " |\n"
	       "| A layer's `blend` | " +
	       codeList(imaging::blendModeNames()) +
	       " |\n"
	       "| `addLayer`'s `kind`, besides `pixel` and `group` | " +
	       codeList(imaging::adjustmentKindNames()) +
	       " |\n"
	       "| A filter's `kind` (`previewFilter`, `applyFilter`) | " +
	       codeList(imaging::filterKindNames()) +
	       " |\n"
	       "| The `shape` tool option (gradients) | " +
	       codeList(imaging::gradientShapeNames()) +
	       " |\n"
	       "| `limits`, with the library's values | " +
	       limits + " |\n";
}

} // namespace affresco::format
