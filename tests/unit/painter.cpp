/// @file
/// @brief Unit tests for what a panel asks its Painter to draw: rounded corners, borders and shadows, and the
/// fallbacks of a Painter that only fills rectangles.

#include <affresco/render/Painter.hpp>
#include <affresco/render/Panel.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using affresco::Color;
using affresco::CornerRadii;
using affresco::Rect;

namespace {

/// @brief A Painter that only fills rectangles, and records them: the shape functions use their fallbacks.
class RectanglePainter : public affresco::Painter {
public:
	/// @brief Records a rectangle.
	/// @param rect The rectangle.
	/// @param color Ignored.
	void fillRect(const Rect& rect, Color color) override {
		static_cast<void>(color);
		calls.emplace_back("fillRect");
		rects.push_back(rect);
	}

	/// @brief Records that text was drawn.
	/// @param font Ignored.
	/// @param text Ignored.
	/// @param origin Ignored.
	/// @param color Ignored.
	void drawText(const affresco::Font& font, std::u32string_view text, affresco::Vec2 origin, Color color) override {
		static_cast<void>(font);
		static_cast<void>(text);
		static_cast<void>(origin);
		static_cast<void>(color);
		calls.emplace_back("drawText");
	}

	std::vector<std::string> calls; ///< What was drawn, in order.
	std::vector<Rect> rects;        ///< The rectangles filled, in order.
};

/// @brief A Painter that draws shapes too, and records which function each one used.
class ShapePainter final : public RectanglePainter {
public:
	/// @brief Records a rounded rectangle.
	/// @param rect Ignored.
	/// @param radii Ignored.
	/// @param color Ignored.
	void fillRoundedRect(const Rect& rect, const CornerRadii& radii, Color color) override {
		static_cast<void>(rect);
		static_cast<void>(radii);
		static_cast<void>(color);
		calls.emplace_back("fillRoundedRect");
	}

	/// @brief Records an outline.
	/// @param rect Ignored.
	/// @param radii Ignored.
	/// @param width Ignored.
	/// @param color Ignored.
	void strokeRoundedRect(const Rect& rect, const CornerRadii& radii, float width, Color color) override {
		static_cast<void>(rect);
		static_cast<void>(radii);
		static_cast<void>(width);
		static_cast<void>(color);
		calls.emplace_back("strokeRoundedRect");
	}

	/// @brief Records a shadow.
	/// @param rect Ignored.
	/// @param radii Ignored.
	/// @param shadow Ignored.
	void drawShadow(const Rect& rect, const CornerRadii& radii, const affresco::Shadow& shadow) override {
		static_cast<void>(rect);
		static_cast<void>(radii);
		static_cast<void>(shadow);
		calls.emplace_back("drawShadow");
	}
};

/// @brief A panel whose paint() a test can call.
class PaintablePanel final : public affresco::Panel {
public:
	/// @brief Paints the panel, as the renderer would.
	/// @param painter The painter.
	void paintWith(affresco::Painter& painter) const { paint(painter); }
};

} // namespace

TEST_CASE("render: a Painter that only fills rectangles draws shapes as rectangles, and no shadow") {
	RectanglePainter painter;
	const Rect rect{0.0f, 0.0f, 100.0f, 50.0f};

	painter.fillRoundedRect(rect, CornerRadii::all(8.0f), affresco::colors::black);
	CHECK(painter.rects == std::vector<Rect>{rect});

	painter.rects.clear();
	painter.strokeRoundedRect(rect, CornerRadii::all(8.0f), 2.0f, affresco::colors::black);
	CHECK(painter.rects == std::vector<Rect>{{0.0f, 0.0f, 100.0f, 2.0f},
	                                         {0.0f, 48.0f, 100.0f, 50.0f},
	                                         {0.0f, 2.0f, 2.0f, 48.0f},
	                                         {98.0f, 2.0f, 100.0f, 48.0f}});

	painter.calls.clear();
	painter.drawShadow(rect, {}, {{0.0f, 4.0f}, 8.0f, affresco::colors::black});
	CHECK(painter.calls.empty());
}

TEST_CASE("render: a panel without the new properties paints only its plain rectangle") {
	ShapePainter painter;
	const PaintablePanel panel;
	panel.paintWith(painter);
	CHECK(painter.calls == std::vector<std::string>{"fillRect"});
}

TEST_CASE("render: a panel paints its shadow, then its rounded fill, then its border") {
	ShapePainter painter;
	PaintablePanel panel;
	panel.setCornerRadii({.topLeft = 6.0f, .bottomRight = 12.0f});
	panel.setBorder(2.0f, affresco::colors::red);
	panel.setShadow({{0.0f, 4.0f}, 8.0f, affresco::colors::black.withAlpha(0.25f)});

	panel.paintWith(painter);
	CHECK(painter.calls == std::vector<std::string>{"drawShadow", "fillRoundedRect", "strokeRoundedRect"});
}

TEST_CASE("render: square corners keep the plain fill, and a transparent shadow is skipped") {
	ShapePainter painter;
	PaintablePanel panel;
	panel.setBorder(1.0f, affresco::colors::blue);
	panel.setShadow({{2.0f, 2.0f}, 4.0f, affresco::colors::transparent});

	panel.paintWith(painter);
	CHECK(painter.calls == std::vector<std::string>{"fillRect", "strokeRoundedRect"});
	CHECK(CornerRadii{}.isSquare());
	CHECK_FALSE(CornerRadii{.bottomLeft = 0.5f}.isSquare());
}
