#pragma once

#include "sgl/Font.hpp"
#include "sgl/Panel.hpp"

#include <limits>
#include <memory>
#include <string>
#include <string_view>

namespace sgl {

// Panel that draws text. Transparent background by default; uses the window's default font unless given one.
class Label : public Panel {
public:
	Label();
	explicit Label(std::string_view text);

	void setText(std::string_view utf8);
	[[nodiscard]] std::string text() const; // UTF-8
	[[nodiscard]] const std::u32string& codepoints() const noexcept { return text_; }

	void setFont(std::shared_ptr<const Font> font);
	[[nodiscard]] const Font* font() const noexcept; // own font, else the window's default, else nullptr

	void setTextColor(Color color);
	[[nodiscard]] Color textColor() const noexcept { return textColor_; }

	void setPadding(float padding); // pixels between the edges and the text
	[[nodiscard]] float padding() const noexcept { return padding_; }

	void setAlignment(Align horizontal, Align vertical);
	void setWrap(bool wrap); // break lines that don't fit the width

	// Size the label to its text (plus padding) instead of width/height; with wrap, lines break at `maxWidth`
	void setFitToText(bool fit, float maxWidth = std::numeric_limits<float>::infinity());

protected:
	void prepareLayout() override;
	void paint(Painter& painter) const override;

	[[nodiscard]] Rect contentRect() const noexcept; // frame minus padding

	std::u32string text_; // one element per codepoint

private:
	std::shared_ptr<const Font> font_;
	Color textColor_ = colors::black;
	float padding_ = 0.0f;
	Align horizontal_ = Align::Start;
	Align vertical_ = Align::Start;
	bool wrap_ = false;
	bool fitToText_ = false;
	float fitMaxWidth_ = std::numeric_limits<float>::infinity();
};

} // namespace sgl
