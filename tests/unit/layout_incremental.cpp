/// @file
/// @brief Incremental layout: a pass lays out only what changed (and what that moves), and gives exactly the frames a
/// full pass gives, whatever the changes.

#include "Headless.hpp"

#include <affresco/render/Label.hpp>
#include <affresco/render/Stack.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <random>
#include <vector>

using affresco::Panel;
using affresco::px;
using affresco::Rect;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A panel that counts how often a pass lays it out (arrange() runs once per layout of the panel).
class Counted final : public Panel {
public:
	int arranged = 0; ///< Times laid out.

protected:
	void arrange() override { ++arranged; }
};

/// @brief Every panel of a tree, depth first.
/// @param panel The tree.
/// @param out Receives the panels.
void collect(Panel& panel, std::vector<Panel*>& out) {
	out.push_back(&panel);
	for(const auto& child : panel.children()) collect(*child, out);
}

/// @brief The frames, bounds and visibility of every panel of a tree.
struct Snapshot {
	std::vector<Rect> frames; ///< Each panel's frame.
	std::vector<Rect> bounds; ///< Each panel's bounds.
	std::vector<bool> shown;  ///< Each panel's isShown().
};

/// @brief Takes a snapshot.
/// @param panels The panels.
/// @return Their layout results.
Snapshot snapshot(const std::vector<Panel*>& panels) {
	Snapshot result;
	for(const Panel* panel : panels) {
		result.frames.push_back(panel->frame());
		result.bounds.push_back(panel->bounds());
		result.shown.push_back(panel->isShown());
	}
	return result;
}

} // namespace

TEST_CASE("layout: a change lays out its own column again, not the column next to it") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Stack& row = window->root().add<affresco::Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setSize(px(400), px(300));
	affresco::Stack& left = row.add<affresco::Stack>();
	affresco::Stack& right = row.add<affresco::Stack>();
	std::vector<Counted*> leftCards, rightCards;
	for(affresco::Stack* column : {&left, &right}) {
		column->setSize(px(200), px(300));
		for(int i = 0; i < 5; ++i) {
			Counted& card = column->add<Counted>();
			card.setSize(px(200), px(40));
			(column == &left ? leftCards : rightCards).push_back(&card);
		}
	}
	window->render();
	for(Counted* card : rightCards) card->arranged = 0;
	for(Counted* card : leftCards) card->arranged = 0;

	leftCards[1]->setHeight(px(60)); // the cards after it move down
	window->render();
	CHECK(leftCards[0]->arranged == 0);
	CHECK(leftCards[1]->arranged == 1);
	CHECK(leftCards[4]->arranged == 1);
	CHECK(leftCards[4]->frame() == Rect{0, 180, 200, 220});
	for(const Counted* card : rightCards) CHECK(card->arranged == 0);

	// Nothing changed: no panel is laid out again, even when a pass runs
	window->invalidate();
	window->render();
	CHECK(rightCards[0]->arranged == 1); // Window::invalidate() lays every panel out
	static_cast<void>(window->panelAt({0, 0}));
	CHECK(rightCards[0]->arranged == 1);
}

TEST_CASE("layout: a change below a child doesn't arrange the children again; a change to a child does") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	Counted& outer = window->root().add<Counted>();
	outer.setSize(px(400), px(300));
	Panel& child = outer.add<Panel>();
	child.setSize(px(200), px(100));
	Panel& grandchild = child.add<Panel>();
	grandchild.setSize(px(50), px(50));
	window->render();
	outer.arranged = 0;

	grandchild.setHeight(px(60)); // the child measures as it did: its place stays
	window->render();
	CHECK(outer.arranged == 0);
	CHECK(grandchild.frame() == Rect{0, 0, 50, 60});

	child.setHeight(px(120));
	window->render();
	CHECK(outer.arranged == 1);
	CHECK(child.frame() == Rect{0, 0, 200, 120});
}

TEST_CASE("layout: a measured size is measured again only once its panel changes") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 100, clock);
	affresco::Stack& row = window->root().add<affresco::Stack>();
	row.setDirection(affresco::Direction::Row);
	row.setSize(px(400), px(100));
	const auto font = affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 15.0f);
	affresco::Label& label = row.add<affresco::Label>("Short");
	label.setFont(font);
	label.setFitToText(true);
	affresco::Label& other = row.add<affresco::Label>("Other");
	other.setFont(font);
	other.setFitToText(true);
	window->render();
	const float before = other.frame().left;

	label.setText("A much longer text");
	window->render();
	CHECK(other.frame().left > before); // the row measured the label again, and moved the next one
}

TEST_CASE("layout: incremental passes give the frames of a full pass, whatever changes (property test)") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(500, 400, clock);

	// Rows and columns three deep, with anchored, growing, wrapping, scrolling and hidden panels
	// NOLINTNEXTLINE(bugprone-random-generator-seed): a fixed seed, so a failing round can be played again
	std::mt19937 random(20261020);
	std::vector<Panel*> containers{&window->root()};
	for(int i = 0; i < 40; ++i) {
		Panel& parent = *containers[random() % containers.size()];
		const int kind = static_cast<int>(random() % 4);
		Panel& panel = kind == 0 ? parent.add<affresco::Stack>() : parent.add<Panel>();
		if(auto* stack = dynamic_cast<affresco::Stack*>(&panel)) {
			stack->setDirection(random() % 2 ? affresco::Direction::Row : affresco::Direction::Column);
			stack->setWrap(random() % 3 == 0);
			stack->setGap(static_cast<float>(random() % 8));
		}
		panel.setSize(px(static_cast<float>(20 + random() % 200)),
		              affresco::pct(static_cast<float>(10 + random() % 60)));
		if(kind == 1) {
			panel.setAnchor(affresco::Edge::Left, px(5));
			panel.setAnchor(affresco::Edge::Right, px(5));
		}
		if(kind == 2) panel.setScrollable(false, true);
		panel.setGrow(static_cast<float>(random() % 2));
		if(containers.size() < 15) containers.push_back(&panel);
	}
	std::vector<Panel*> panels;
	collect(window->root(), panels);
	window->render();

	for(int round = 0; round < 200; ++round) {
		CAPTURE(round);
		Panel& target = *panels[1 + random() % (panels.size() - 1)];
		switch(random() % 6) {
		case 0:
			target.setWidth(px(static_cast<float>(10 + random() % 220)));
			break;
		case 1:
			target.setHeight(affresco::pct(static_cast<float>(5 + random() % 80)));
			break;
		case 2:
			target.setVisible(!target.isVisible());
			break;
		case 3:
			target.setGrow(static_cast<float>(random() % 3));
			break;
		case 4:
			target.setMinWidth(px(static_cast<float>(random() % 120)));
			break;
		default:
			if(round % 20 == 0)
				window->setSize(400 + static_cast<int>(random() % 200), 300 + static_cast<int>(random() % 200));
			else target.scrollBy({0.0f, static_cast<float>(random() % 40) - 20.0f});
		}
		window->render();
		const Snapshot incremental = snapshot(panels);
		window->invalidate(); // every panel laid out again, from scratch
		window->render();
		const Snapshot full = snapshot(panels);
		CHECK(incremental.frames == full.frames);
		CHECK(incremental.bounds == full.bounds);
		CHECK(incremental.shown == full.shown);
	}
}
