#pragma once

/// @file
/// @brief TextArea: a single-line text input.

#include "affresco/render/Label.hpp"

#include <string>
#include <string_view>

namespace affresco {

/// @brief A single-line text input: collects what is typed while it has focus (click it to focus it).
///
/// Shows a placeholder when empty and a caret when focused, and scrolls so the end of the text stays visible.
/// Backspace deletes; Cmd+V / Ctrl+V pastes, with line breaks and tabs turned into spaces.
class TextArea : public Label {
public:
	/// @brief An empty input with a white background and an I-beam cursor.
	TextArea();

	/// @brief Empties the text, without firing onTextChanged.
	void clear() noexcept;

	/// @brief Sets the text shown while empty.
	/// @param utf8 UTF-8 text.
	void setPlaceholder(std::string_view utf8);

	/// @brief Sets the placeholder's colour.
	/// @param color The colour.
	void setPlaceholderColor(Color color);

	Signal<const std::string&> onTextChanged; ///< The user edited the text (not setText() or clear()); UTF-8.

protected:
	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	std::u32string placeholder_;                    ///< See setPlaceholder().
	Color placeholderColor_ = Color::hex(0x9ca3af); ///< See setPlaceholderColor().
};

} // namespace affresco
