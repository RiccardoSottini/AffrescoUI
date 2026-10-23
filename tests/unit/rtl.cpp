/// @file
/// @brief Unit tests for right-to-left layout and text: rows, anchors and positions mirrored under a right-to-left
/// panel or the window's locale, an icon marked mirror swapping left and right, and shaping's bidirectional order
/// (runs of Arabic and Latin text, and numbers, each in their own direction).

#include "Headless.hpp"

#include <affresco/render/Font.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Stack.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <array>
#include <filesystem>
#include <memory>

using affresco::Edge;
using affresco::Font;
using affresco::Label;
using affresco::Panel;
using affresco::px;
using affresco::Rect;
using affresco::Stack;
using affresco::WritingDirection;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// A label whose shown text (its icon's character, a space and the text) the test reads.
struct ShownLabel : Label {
	using Label::Label;
	using Label::shown;
};

/// @brief Roboto with the check's Arabic subset and Lucide's icons as fallbacks.
/// @return The font.
std::shared_ptr<const Font> mixedFont() {
	const std::filesystem::path fonts = AFFRESCOUI_FONTS_DIR;
	const std::array fallbacks{std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR) / "NotoSansArabic-Subset.ttf",
	                           fonts / "Lucide.ttf"};
	return Font::load(fonts / "Roboto-Regular.ttf", 16.0f, fallbacks);
}

/// @brief Adds a child of a given size.
/// @param parent The container.
/// @param width Its width.
/// @param height Its height.
/// @return The child.
Panel& child(Panel& parent, float width, float height) {
	Panel& panel = parent.add<Panel>();
	panel.setSize(px(width), px(height));
	return panel;
}

} // namespace

TEST_CASE("rtl: a right-to-left row places its children from the right, and anchors and positions mirror") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Panel& page = window->root().add<Panel>();
	page.setSize(px(200), px(100));
	page.setWritingDirection(WritingDirection::RightToLeft);
	Stack& row = page.add<Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setSize(px(200), px(20));
	row.setGap(10.0f);
	Panel& first = child(row, 40, 20);
	Panel& second = child(row, 30, 20);
	Panel& anchored = child(page, 50, 10);
	anchored.setAnchor(Edge::Left, px(10));
	anchored.setAnchor(Edge::Top, px(40));
	Panel& placed = child(page, 20, 10);
	placed.setPosition(px(5), px(60));
	window->render();
	CHECK(row.rightToLeft());
	CHECK(first.frame() == Rect{160, 0, 200, 20});
	CHECK(second.frame() == Rect{120, 0, 150, 20});
	CHECK(anchored.frame() == Rect{140, 40, 190, 50});
	CHECK(placed.frame() == Rect{175, 60, 195, 70});

	// A child set left to right inside it is laid out as usual
	row.setWritingDirection(WritingDirection::LeftToRight);
	window->render();
	CHECK(first.frame() == Rect{0, 0, 40, 20});
	CHECK(second.frame() == Rect{50, 0, 80, 20});
}

TEST_CASE("rtl: a panel set to the locale's direction follows the window's locale both ways") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Stack& row = window->root().add<Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setSize(px(200), px(20));
	row.setWritingDirection(WritingDirection::Locale);
	Panel& first = child(row, 40, 20);
	window->render();
	CHECK_FALSE(row.rightToLeft());
	CHECK(first.frame() == Rect{0, 0, 40, 20});

	window->setLocaleRightToLeft(true);
	window->render();
	CHECK(row.rightToLeft());
	CHECK(first.rightToLeft()); // children inherit
	CHECK(first.frame() == Rect{160, 0, 200, 20});

	window->setLocaleRightToLeft(false);
	window->render();
	CHECK(first.frame() == Rect{0, 0, 40, 20});
}

TEST_CASE("rtl: an icon marked mirror swaps left and right in right-to-left text, and only then") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	const auto font = mixedFont();
	const char32_t left = font->codepointNamed("chevron-left");
	const char32_t right = font->codepointNamed("chevron-right");
	REQUIRE(left != 0);
	REQUIRE(right != 0);
	ShownLabel& back = window->root().add<ShownLabel>("Back");
	back.setSize(px(120), px(30));
	back.setFont(font);
	back.setIcon("chevron-left");
	back.setIconMirrored(true);
	ShownLabel& plain = window->root().add<ShownLabel>("Plain");
	plain.setSize(px(120), px(30));
	plain.setFont(font);
	plain.setIcon("chevron-left");
	window->render();
	CHECK(back.shown().front() == left);

	window->root().setWritingDirection(WritingDirection::RightToLeft);
	window->render();
	CHECK(back.shown().front() == right);
	CHECK(plain.shown().front() == left); // not marked: drawn as it is
	CHECK(back.shown().substr(1) == U" Back");

	window->root().setWritingDirection(WritingDirection::LeftToRight);
	window->render();
	CHECK(back.shown().front() == left);
}

TEST_CASE("rtl: shaping orders runs by their levels: Arabic right to left, Latin and numbers left to right") {
	const auto font = mixedFont();
	// Latin first: a left-to-right paragraph, the Arabic word after "Anna " and drawn right to left
	{
		const std::u32string text = U"Anna كيف";
		const Font::ShapedText& shaped = font->shape(text);
		REQUIRE(shaped.carets.size() == text.size() + 1);
		CHECK(shaped.carets[0] == doctest::Approx(0.0f));
		CHECK(shaped.carets[5] > shaped.carets[6]); // inside the Arabic word, carets go right to left
		CHECK(shaped.carets[5] > shaped.carets[4]); // and the word is after "Anna"
	}
	// Arabic first: a right-to-left paragraph; the number keeps its digits left to right, left of the Arabic
	{
		const std::u32string text = U"كيف 12";
		const Font::ShapedText& shaped = font->shape(text);
		REQUIRE(shaped.carets.size() == text.size() + 1);
		CHECK(shaped.carets[0] > shaped.carets[2]); // the Arabic runs right to left
		CHECK(shaped.carets[4] < shaped.carets[5]); // "1" before "2", left to right
		CHECK(shaped.carets[5] < shaped.carets[0]); // the number is left of the Arabic
	}
	// Arabic, a separator, then Latin: the separator sits between the two, not on the Latin word's far side
	{
		const std::u32string text = U"كيف · Anna";
		const Font::ShapedText& shaped = font->shape(text);
		REQUIRE(shaped.carets.size() == text.size() + 1);
		CHECK(shaped.carets[6] < shaped.carets[10]); // "Anna" left to right
		CHECK(shaped.carets[4] > shaped.carets[10]); // the dot right of "Anna"
		CHECK(shaped.carets[4] < shaped.carets[2]);  // and left of the Arabic
	}
}
