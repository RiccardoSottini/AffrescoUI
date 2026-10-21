#pragma once

/// @file
/// @brief vector::XmlReader: the tags of an XML document, as much of XML as SVG icons need (no DTDs, no namespaces).

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::vector {

/// @brief An opening or closing tag.
struct XmlTag {
	std::string_view name;                                         ///< Its name.
	std::vector<std::pair<std::string_view, std::string>> attributes; ///< Its attributes, with entities replaced.
	bool closing = false;                                          ///< A closing tag (</name>).
	bool selfClosing = false;                                      ///< An empty element (<name/>).

	/// @brief An attribute's value.
	/// @param key Its name.
	/// @return The value, or nullopt if it has none.
	[[nodiscard]] std::optional<std::string_view> attribute(std::string_view key) const noexcept {
		for(const auto& [attributeName, value] : attributes)
			if(attributeName == key) return value;
		return std::nullopt;
	}
};

/// @brief Reads the tags of a document one by one, skipping text, comments, CDATA, declarations and processing
/// instructions. Throws std::runtime_error on broken markup.
class XmlReader {
public:
	/// @brief Reads a document.
	/// @param text The document; it must outlive the reader and its tags.
	explicit XmlReader(std::string_view text) noexcept : text_(text) {}

	/// @brief The next tag.
	/// @return The tag, or nullopt at the end of the document.
	[[nodiscard]] std::optional<XmlTag> next();

private:
	/// @brief Skips a construct that starts and ends with markers, if it starts here.
	/// @return true if skipped.
	bool skip(std::string_view start, std::string_view end);
	/// @brief Reads a tag, from its '<'.
	XmlTag tag();
	/// @brief Reads one attribute into a tag.
	void attribute(XmlTag& tag);
	/// @brief Reads a name here (letters, digits, '-', '_', ':', '.').
	std::string_view name();
	/// @brief Skips whitespace.
	void skipSpace() noexcept;
	/// @brief Throws, naming the problem and where it is.
	[[noreturn]] void fail(std::string_view problem) const;

	std::string_view text_; ///< The document.
	std::size_t at_ = 0;    ///< Where reading is.
};

} // namespace affresco::vector
