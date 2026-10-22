#pragma once

/// @file
/// @brief What the adapters share: the screen they write (its palette, fonts and state), their limits, ids, safe
/// text, placeholders and warnings, and nodes with the height they need (a container doesn't size itself to its
/// children, so every node an adapter writes carries its height).

#include "affresco/format/Adapters.hpp"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format::adapters::detail {

/// @brief A node, and the height it was given.
struct Built {
	json node;        ///< The node.
	float height = 0; ///< Its height, in pixels.
};

/// @brief Text sizes, as the fonts a converted screen has.
enum class TextSize {
	Small,  ///< Captions and facts: 12 pixels.
	Normal, ///< Body text: 14 pixels.
	Large,  ///< Headings: 18 pixels.
	Title   ///< Titles: 22 pixels.
};

/// @brief The screen one conversion writes: its state, warnings, limits and node ids.
class Output {
public:
	/// @brief Starts an empty screen.
	/// @param options How it is written.
	explicit Output(Options options);

	/// @brief Counts an element, and checks the limits.
	/// @param at Its place in the source, for the error.
	/// @param depth How deeply it nests.
	/// @throws DocumentError If it nests deeper than maxDepth, or the conversion passed maxElements.
	void enter(const std::string& at, int depth);
	/// @brief Notes what was left out or shown as a placeholder, once for each place and message.
	/// @param at Its place in the source.
	/// @param message What happened to it.
	void warn(const std::string& at, const std::string& message);
	/// @brief An id no other node of this build has.
	/// @param base What it is made from (sanitized).
	/// @return `base`, or `base-2`, `base-3`...
	[[nodiscard]] std::string uniqueId(std::string_view base);
	/// @brief Forgets the ids given so far: a new build of the root (A2UI's updates) gives them again.
	void newBuild() { ids_ = {"root", "surface", "content"}; }

	/// @brief Text from the source, as a value the screen shows as written: text with "{{" is kept in the state and
	/// shown through a binding, since state values are never evaluated.
	/// @param text The text.
	/// @return The text, or `"{{state.literals.kN}}"`.
	[[nodiscard]] json literal(const std::string& text);
	/// @brief A label showing text from the source as written (see literal()), wrapped to a width.
	/// @param text The text.
	/// @param width The width it wraps to.
	/// @param size Its size.
	/// @param color Its colour, a colour name (`"@text"`) or `#rrggbb`.
	/// @return The label, as tall as its text is estimated to be.
	[[nodiscard]] Built label(const std::string& text, float width, TextSize size, std::string_view color = "@text");
	/// @brief A label showing a binding the adapter wrote (`"{{state.model.name}}"`).
	/// @param binding The binding.
	/// @param characters How long its text is expected to be, for its height.
	/// @param width The width it wraps to.
	/// @param size Its size.
	/// @param color Its colour.
	/// @return The label.
	[[nodiscard]] Built boundLabel(const std::string& binding, std::size_t characters, float width, TextSize size,
	                               std::string_view color = "@text") const;
	/// @brief A visible placeholder for what has no equivalent, with a warning.
	/// @param what What it stands for ("Video", "Action.ToggleVisibility").
	/// @param at Its place in the source.
	/// @param width Its width.
	/// @return The placeholder.
	[[nodiscard]] Built placeholder(const std::string& what, const std::string& at, float width);

	/// @brief The state the screen starts with ("model", "data", "inputs", "literals"...).
	[[nodiscard]] json& state() noexcept { return state_; }
	/// @brief The warnings, in the order they were noted.
	[[nodiscard]] std::vector<std::string>& warnings() noexcept { return warnings_; }
	/// @brief The width the content wraps to: the window's, inside the surface's margins.
	[[nodiscard]] float contentWidth() const noexcept;
	/// @brief Wraps a built body in the screen document.
	/// @param body The content, placed in a scrolling column.
	/// @param title The window's title, unless the options name one.
	/// @param note What the screen was made from.
	/// @return The document.
	[[nodiscard]] json screen(Built body, std::string_view title, const std::string& note) const;
	/// @brief The root node for a body, as screen() writes it (A2UI's updates set it again).
	/// @param body The content.
	/// @return The root.
	[[nodiscard]] static json root(Built body);

private:
	Options options_;                    ///< See Options.
	json state_ = json::object();        ///< See state().
	std::vector<std::string> warnings_;  ///< See warnings().
	std::set<std::string> ids_;          ///< Ids given in this build.
	std::size_t elements_ = 0;           ///< Elements converted so far.
	std::size_t literals_ = 0;           ///< Texts kept in state.literals so far.
};

/// @brief A text size's height in pixels.
/// @param size The size.
/// @return 12, 14, 18 or 22.
[[nodiscard]] float pixels(TextSize size) noexcept;
/// @brief Children one below the other, in a column as tall as they are.
/// @param id The column's id.
/// @param children The children.
/// @param gap Pixels between them.
/// @return The column.
[[nodiscard]] Built column(const std::string& id, std::vector<Built> children, float gap = 8);
/// @brief Children side by side, sharing the width by their "grow", in a row as tall as the tallest.
/// @param id The row's id.
/// @param children The children (each without a "width" given `"basis": 0`, and a "grow" of 1 unless it has one).
/// @param gap Pixels between them.
/// @return The row.
[[nodiscard]] Built row(const std::string& id, std::vector<Built> children, float gap = 8);
/// @brief A name from the source as a state key or an id: letters, digits and `_` kept, the rest `_`, never empty
/// and never starting with a digit.
/// @param name The name.
/// @return The key.
[[nodiscard]] std::string stateKey(std::string_view name);
/// @brief A list of names as a state path below `state.`: `state.model.user.name`.
/// @param segments The names (each made a stateKey()).
/// @return The path.
[[nodiscard]] std::string statePath(const std::vector<std::string>& segments);
/// @brief Whether a link from the source may be opened or fetched: https only, and nothing that could hide another
/// scheme or host (a user name before '@'), or be read as a binding (spaces, control characters, a backslash, braces,
/// quotes, angle brackets).
/// @param url The link.
/// @return Whether it is allowed.
[[nodiscard]] bool allowedUrl(std::string_view url) noexcept;
/// @brief Whether an image path from the source is a relative file inside the screen's folder.
/// @param path The path.
/// @return Whether it is.
[[nodiscard]] bool relativeFile(std::string_view path) noexcept;
/// @brief A JSON string, or the text of another value, for the source's own fields.
/// @param value The value.
/// @param fallback What to give if it is neither text nor a number.
/// @return The text.
[[nodiscard]] std::string textOf(const json& value, const std::string& fallback = "");

} // namespace affresco::format::adapters::detail
