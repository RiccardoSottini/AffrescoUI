/// @file
/// @brief Window::pick(): the person points at one of the window's panels, in a mode the window owns, while the screen
/// hears no input; and detail::PickLayer, what the mode draws.

#include "Picking.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace affresco {

namespace {

constexpr float topMargin = 24.0f;     ///< The hint's distance from the window's top, as the full-screen hint's.
constexpr float hintGap = 8.0f;        ///< The room between the full-screen hint and the pick's, below it.
constexpr float captionGap = 4.0f;     ///< The room between the panel offered and its caption.
constexpr float outlineWidth = 2.0f;   ///< How thick the outline is.
constexpr float tintAlpha = 0.15f;     ///< How strong the tint over the panel offered is.
constexpr float captionWidth = 360.0f; ///< The widest the caption is before it wraps.
constexpr float hintWidth = 480.0f;    ///< The widest the hint is before it wraps.

/// @brief The panel a pick offers for a panel: what its caller's candidate gives, or the panel itself.
/// @param options The pick's options.
/// @param panel The panel, or nullptr.
/// @return The panel offered, or nullptr.
Panel* offered(const PickOptions& options, Panel* panel) {
	if(!panel) return nullptr;
	return options.candidate ? options.candidate(panel) : panel;
}

/// @brief Whether the keyboard can stop on a panel: it is shown, and offered for itself.
/// @param options The pick's options.
/// @param panel The panel.
/// @return true if it is.
bool accepted(const PickOptions& options, Panel& panel) {
	return panel.isShown() && offered(options, &panel) == &panel;
}

/// @brief Collects the panels accepted right below one: in its subtree, not inside another accepted one, in tree order.
/// @param options The pick's options.
/// @param top The panel.
/// @param out Gets them.
void collectBelow(const PickOptions& options, const Panel& top, std::vector<Panel*>& out) {
	for(const auto& child : top.children()) {
		if(!child->isShown()) continue;
		if(accepted(options, *child)) out.push_back(child.get());
		else collectBelow(options, *child, out);
	}
}

/// @brief Collects every panel accepted in a subtree, in tree order (Tab's order).
/// @param options The pick's options.
/// @param panel The subtree.
/// @param out Gets them.
void collectAll(const PickOptions& options, Panel& panel, std::vector<Panel*>& out) {
	if(!panel.isShown()) return;
	if(accepted(options, panel)) out.push_back(&panel);
	for(const auto& child : panel.children()) collectAll(options, *child, out);
}

/// @brief The nearest accepted ancestor of a panel.
/// @param options The pick's options.
/// @param panel The panel.
/// @return It, or nullptr.
Panel* acceptedAncestor(const PickOptions& options, const Panel& panel) {
	for(Panel* above = panel.parent(); above; above = above->parent())
		if(accepted(options, *above)) return above;
	return nullptr;
}

/// @brief The panel before or after one in a list, wrapping or not.
/// @param list The list.
/// @param from The panel.
/// @param forward true for the one after.
/// @param wrap Whether the end leads to the start, and the start to the end.
/// @return It; @p from at an end that doesn't wrap; the first (or last) if @p from isn't in the list; nullptr if it's
/// empty.
Panel* neighbour(const std::vector<Panel*>& list, Panel* from, bool forward, bool wrap) {
	if(list.empty()) return nullptr;
	const auto found = std::ranges::find(list, from);
	if(found == list.end()) return forward ? list.front() : list.back();
	const auto index = static_cast<std::size_t>(std::distance(list.begin(), found));
	if(forward) return index + 1U < list.size() ? list[index + 1U] : (wrap ? list.front() : from);
	return index > 0U ? list[index - 1U] : (wrap ? list.back() : from);
}

/// @brief Where a key moves the keyboard among the panels a pick accepts (ACCESSIBILITY.md, "Pointing with the
/// keyboard"): Up to the nearest accepted ancestor, Down to the first accepted descendant, Left and Right to the
/// accepted siblings (the panels accepted right below that ancestor), Tab and Shift+Tab through all, wrapping.
/// @param options The pick's options.
/// @param root The window's root panel.
/// @param from The panel offered now, or nullptr.
/// @param event The key.
/// @return The panel to offer, which may be @p from; std::nullopt for a key that moves nothing.
std::optional<Panel*> movedTo(const PickOptions& options, Panel& root, Panel* from, const KeyEvent& event) {
	const Key key = event.key;
	if(key != Key::Up && key != Key::Down && key != Key::Left && key != Key::Right && key != Key::Tab)
		return std::nullopt;
	std::vector<Panel*> list;
	if(!from || key == Key::Tab) {
		collectAll(options, root, list);
		return neighbour(list, from, !event.mods.shift(), true);
	}
	if(key == Key::Up) {
		Panel* above = acceptedAncestor(options, *from);
		return above ? above : from;
	}
	if(key == Key::Down) {
		collectBelow(options, *from, list);
		return list.empty() ? from : list.front();
	}
	if(const Panel* above = acceptedAncestor(options, *from)) collectBelow(options, *above, list);
	else if(accepted(options, root)) list.push_back(&root);
	else collectBelow(options, root, list);
	return neighbour(list, from, key == Key::Right, false);
}

/// @brief Where the keyboard starts (Window::pick()): the panel offered for the first of the start, the focused and the
/// hovered panels that offers a shown one, else the first panel accepted in tree order.
/// @param options The pick's options.
/// @param root The window's root panel.
/// @param starts The start, the focused and the hovered panels, each possibly nullptr.
/// @return It, or nullptr.
Panel* startingPanel(const PickOptions& options, Panel& root, std::initializer_list<Panel*> starts) {
	for(Panel* start : starts)
		if(Panel* offer = offered(options, start); offer && offer->isShown()) return offer;
	std::vector<Panel*> all;
	collectAll(options, root, all);
	return all.empty() ? nullptr : all.front();
}

/// @brief Makes one of the layer's labels: white text on a colour, sized to its text.
/// @param layer The layer.
/// @param background Its background.
/// @param maxWidth The widest it is before it wraps.
/// @return It, hidden.
Label& note(Panel& layer, Color background, float maxWidth) {
	auto& made = layer.add<Label>();
	made.setColor(background);
	made.setTextColor(colors::white);
	made.setPadding(6.0f);
	made.setCornerRadii(CornerRadii::all(4.0f));
	made.setFitToText(true, maxWidth);
	made.setWrap(true);
	made.setVisible(false);
	return made;
}

} // namespace

/* What it draws */

detail::PickLayer::PickLayer(Color color)
	: highlight_(&add<Panel>()), caption_(&note(*this, color, captionWidth)),
	  hint_(&note(*this, Color::hex(0x111827), hintWidth)) { // the hint's colour as the full-screen hint's
	setColor(colors::transparent);
	highlight_->setColor(color.withAlpha(tintAlpha));
	highlight_->setBorder(outlineWidth, color);
	highlight_->setVisible(false);
	AccessibleState live;
	live.live = LiveRegion::Polite; // each new panel offered is read, the focus staying where it is
	caption_->setAccessibleState(live);
	hint_->setPadding(10.0f);
}

void detail::PickLayer::place(const Panel* target, const std::string& caption, const std::string& hint, float hintTop,
                              Vec2 window) {
	setSize(px(window.x), px(window.y));
	const bool outlined = target && target->isShown();
	highlight_->setVisible(outlined);
	caption_->setVisible(outlined && !caption.empty());
	if(outlined) { // on the panel's frame, following its corners, as the focus ring does
		const Rect& frame = target->frame();
		highlight_->setPosition(px(frame.left), px(frame.top));
		highlight_->setSize(px(frame.right - frame.left), px(frame.bottom - frame.top));
		highlight_->setCornerRadii(target->cornerRadii());
		if(!caption.empty()) {
			caption_->setText(caption);
			placeCaption(frame, window);
		}
	}
	hint_->setVisible(!hint.empty());
	if(!hint.empty()) {
		hint_->setText(hint);
		const float width = hint_->measuredWidth(window.x);
		hint_->setPosition(px(std::round(std::max((window.x - width) / 2.0f, 0.0f))), px(hintTop));
	}
}

void detail::PickLayer::placeCaption(const Rect& target, Vec2 window) {
	const Vec2 size{caption_->measuredWidth(window.x), caption_->measuredHeight(window.y)};
	float top = target.top - captionGap - size.y;
	if(top < 0.0f) top = target.bottom + captionGap; // no room above it
	top = std::clamp(top, 0.0f, std::max(window.y - size.y, 0.0f));
	const float left = std::clamp(target.left, 0.0f, std::max(window.x - size.x, 0.0f));
	caption_->setPosition(px(std::round(left)), px(std::round(top)));
}

bool Window::Picking::offer(Panel* panel) {
	if(panel == candidate) return false;
	candidate = panel;
	caption = panel && options.caption ? options.caption(*panel) : std::string{};
	return true;
}

/* The mode */

std::uint64_t Window::pick(PickOptions options, std::function<void(Panel* picked)> done) {
	if(picking_ || asking_) return 0;
	auto picking = std::make_unique<Picking>();
	picking->id = nextPick_++;
	picking->done = std::move(done);
	picking->layer = std::make_unique<detail::PickLayer>(options.color);
	picking->layer->attachTo(this);
	picking->options = std::move(options);
	picking_ = std::move(picking);
	updateLayout(); // what is shown now, for where the keyboard starts
	picking_->offer(startingPanel(picking_->options, root_, {picking_->options.start, focused_, hovered_}));
	picking_->options.start = nullptr; // used once
	grabbed_ = nullptr;                // a drag under way ends: its release goes to the pick
	requestLayout();
	updateCursor();
	return picking_->id;
}

void Window::cancelPick(std::uint64_t id) {
	if(picking_ && picking_->id == id) endPick();
	std::erase_if(pickResults_, [id](const PickResult& result) { return result.id == id; }); // ended, not yet told
}

void Window::setPickLayerVisible(bool visible) {
	pickLayerVisible_ = visible;
	requestLayout();
}

void Window::layoutPick() {
	if(!picking_ || !pickLayerVisible_) return;
	const auto width = static_cast<float>(width_), height = static_cast<float>(height_);
	// Below the full-screen hint, when both are shown
	const float top = fullscreenHintShown_ && fullscreenHint_ ? fullscreenHint_->frame().bottom + hintGap : topMargin;
	picking_->layer->place(picking_->candidate, picking_->caption, picking_->options.hint, top, Vec2{width, height});
	const Rect area{0.0f, 0.0f, width, height};
	picking_->layer->layout(area, area, Vec2{width, height}, true);
	drawn_.push_back(picking_->layer.get());
}

void Window::pickAt(Vec2 position) {
	if(picking_->offer(offered(picking_->options, panelAt(position)))) requestLayout();
}

// A key that picks or cancels acts when it is let go, if it was pressed while picking: so the key that started the
// mode can't end it, and nothing of the key reaches the screen
void Window::pickKey(const KeyEvent& event) {
	const Mods mods = event.mods;
	const bool plain = !mods.shift() && !mods.control() && !mods.alt() && !mods.super();
	const bool picks = plain && (event.key == Key::Enter || event.key == Key::KeypadEnter || event.key == Key::Space);
	if(picks || event.key == Key::Escape) {
		pickAnswerKey(event);
		return;
	}
	if(event.action == Action::Release) return;
	Picking& picking = *picking_;
	if(const std::optional<Panel*> next = movedTo(picking.options, root_, picking.candidate, event))
		if(picking.offer(*next)) requestLayout();
}

void Window::pickAnswerKey(const KeyEvent& event) {
	Picking& picking = *picking_;
	if(event.action == Action::Press) picking.pressedKey = event.key;
	if(event.action != Action::Release || std::exchange(picking.pressedKey, std::nullopt) != event.key) return;
	if(!routingOwnPress()) return;
	if(event.key == Key::Escape) finishPick(nullptr);
	else if(picking.candidate) finishPick(picking.candidate);
}

// A left click picks when it ends on the panel offered where it began, and a right click cancels when it ends: so the
// whole click is the pick's, and no release of it reaches the screen after the mode
void Window::pickButton(const MouseButtonEvent& event, Panel* hit) {
	Picking& picking = *picking_;
	if(event.button == MouseButton::Right) {
		if(event.action == Action::Press) picking.rightPressed = true;
		if(event.action == Action::Release && std::exchange(picking.rightPressed, false) && routingOwnPress())
			finishPick(nullptr);
		return;
	}
	if(event.button != MouseButton::Left) return;
	if(picking.offer(offered(picking.options, hit))) requestLayout();
	if(event.action == Action::Press) picking.pressedOn = picking.candidate;
	if(event.action != Action::Release) return;
	const Panel* const pressedOn = std::exchange(picking.pressedOn, nullptr);
	if(picking.candidate && picking.candidate == pressedOn && routingOwnPress()) finishPick(picking.candidate);
}

void Window::finishPick(Panel* picked) {
	const std::unique_ptr<Picking> ended = std::move(picking_); // what it drew goes now: nothing ever hits it
	requestLayout();
	updateCursor();
	if(!ended->done) return;
	// The callback runs once the event is delivered; what was picked may go before (forget())
	pickResults_.push_back({ended->id, picked});
	defer([this, id = ended->id, done = std::move(ended->done)] {
		const auto found = std::ranges::find(pickResults_, id, &PickResult::id);
		if(found == pickResults_.end()) return; // cancelled since (cancelPick())
		Panel* const panel = found->panel;
		pickResults_.erase(found);
		done(panel);
	});
}

void Window::endPick() {
	if(!picking_) return;
	picking_.reset();
	requestLayout();
	updateCursor();
}

} // namespace affresco
