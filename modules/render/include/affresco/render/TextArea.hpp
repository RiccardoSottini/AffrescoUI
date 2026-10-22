#pragma once

/// @file
/// @brief TextArea: a single-line text input.

#include "affresco/render/Label.hpp"

#include <cstddef>
#include <cstdint>
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

	/// @brief Which characters typing and pasting may add.
	enum class Accept : std::uint8_t {
		Any,          ///< Every character.
		Digits,       ///< 0 to 9.
		Letters,      ///< Letters of any script.
		Alphanumeric, ///< Letters and digits.
	};
	/// @brief Hides the text behind one dot per character, for passwords; assistive technology gets no value.
	/// @param secret Whether to hide it.
	void setSecret(bool secret);
	/// @brief Whether the text is hidden.
	/// @return See setSecret().
	[[nodiscard]] bool secret() const noexcept { return secret_; }
	/// @brief Limits how many characters typing and pasting may give the text (0: no limit).
	/// @param length The most characters.
	void setMaxLength(std::size_t length) noexcept { maxLength_ = length; }
	/// @brief The most characters, 0 for no limit.
	/// @return See setMaxLength().
	[[nodiscard]] std::size_t maxLength() const noexcept { return maxLength_; }
	/// @brief Limits which characters typing and pasting may add.
	/// @param accept The characters.
	void setAccept(Accept accept) noexcept { accept_ = accept; }
	/// @brief Whether typing or pasting may add a character.
	/// @param codepoint The character.
	/// @return True if setAccept() lets it in.
	[[nodiscard]] bool accepts(char32_t codepoint) const noexcept;

	Signal<const std::string&> onTextChanged; ///< The user edited the text (not setText() or clear()); UTF-8.

protected:
	/// @brief Adds typed or pasted characters, keeping those accepted and within the most characters.
	/// @param added The characters.
	/// @return Whether the text changed.
	bool insert(std::u32string_view added);
	/// @brief The characters painted: the text, or one dot per character when secret.
	/// @return The shown text.
	[[nodiscard]] std::u32string shownText() const;

	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	std::u32string placeholder_;             ///< See setPlaceholder().
	Color placeholderColor_ = colors::muted; ///< See setPlaceholderColor().
	bool secret_ = false;                    ///< See setSecret().
	std::size_t maxLength_ = 0;              ///< See setMaxLength().
	Accept accept_ = Accept::Any;            ///< See setAccept().
};

} // namespace affresco
