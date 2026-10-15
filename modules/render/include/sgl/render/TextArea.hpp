#pragma once

#include "sgl/render/Label.hpp"

#include <string>
#include <string_view>

namespace sgl {

// Single-line text input: collects what is typed while it has focus (click it to focus it).
// Shows a placeholder when empty, a caret when focused, and scrolls so the end of the text stays visible.
// Cmd+V / Ctrl+V pastes (line breaks become spaces).
class TextArea : public Label {
public:
	TextArea();

	void clear() noexcept;

	void setPlaceholder(std::string_view utf8);
	void setPlaceholderColor(Color color);

	Signal<const std::string&> onTextChanged; // after the user edits the text (not after setText)

protected:
	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	std::u32string placeholder_;
	Color placeholderColor_ = Color::hex(0x9ca3af);
};

} // namespace sgl
