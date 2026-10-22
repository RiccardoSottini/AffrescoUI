#pragma once

/// @file
/// @brief detail::PdfObjects: a PDF file's numbered objects, written in order, and its cross-reference table.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace affresco::detail {

/// @brief The objects of a PDF file, each written when it is defined, then the cross-reference table and trailer that
/// give each one's byte offset. Ids are reserved first, so objects can refer to ones defined later.
class PdfObjects {
public:
	/// @brief Starts a file: its header.
	/// @param limit The largest file, in bytes.
	explicit PdfObjects(std::size_t limit);

	/// @brief Gives a new object's id, to define later.
	/// @return It: 1 for the first.
	[[nodiscard]] std::uint32_t reserve();
	/// @brief Writes an object.
	/// @param id A reserved id, not yet defined.
	/// @param body What it is: a dictionary, an array, a number.
	/// @throws std::runtime_error Past the limit, or if the id is unknown or defined already.
	void define(std::uint32_t id, std::string_view body);
	/// @brief Writes a stream object.
	/// @param id A reserved id, not yet defined.
	/// @param dictionary The entries of its dictionary, without /Length (added).
	/// @param data Its bytes, as they are (no filter).
	/// @throws std::runtime_error As define().
	void defineStream(std::uint32_t id, std::string_view dictionary, std::span<const std::uint8_t> data);
	/// @brief Writes a stream object of text.
	/// @param id As defineStream().
	/// @param dictionary As defineStream().
	/// @param data Its bytes.
	/// @throws std::runtime_error As define().
	void defineStream(std::uint32_t id, std::string_view dictionary, std::string_view data);
	/// @brief How many bytes have been written.
	[[nodiscard]] std::size_t size() const noexcept { return out_.size(); }

	/// @brief Writes the cross-reference table and the trailer.
	/// @param root The catalog's id.
	/// @param info The information dictionary's id.
	/// @return The whole file.
	/// @throws std::runtime_error Past the limit, or if a reserved object wasn't defined.
	[[nodiscard]] std::vector<std::uint8_t> finish(std::uint32_t root, std::uint32_t info);

private:
	/// @brief Appends bytes, within the limit.
	/// @param text The bytes.
	void append(std::string_view text);
	/// @brief Starts an object: records its offset and writes "id 0 obj".
	/// @param id A reserved id, not yet defined.
	void begin(std::uint32_t id);

	std::vector<std::uint8_t> out_;    ///< The file so far.
	std::vector<std::size_t> offsets_; ///< By id - 1: where each object starts; 0 while it isn't defined.
	std::size_t limit_;                ///< The largest file.
};

} // namespace affresco::detail
