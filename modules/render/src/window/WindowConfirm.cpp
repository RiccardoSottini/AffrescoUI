/// @file
/// @brief Window::confirm(): a question the window asks the person itself, drawn above everything a screen draws, and
/// answered only by the person's own press.

#include "Confirmation.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace affresco {

namespace {

constexpr float cardWidth = 640.0f;   ///< The widest the question's box is.
constexpr float cardHeight = 480.0f;  ///< The tallest it is.
constexpr float margin = 16.0f;       ///< The least room around it, and inside it.
constexpr float buttonWidth = 112.0f; ///< A button's width.
constexpr float buttonHeight = 32.0f; ///< A button's height.
/// How long the question is shown before a press can say yes: the person's next press may already be on its way when
/// it comes up (the second click of a double click, Enter typed on after the evaluating key), and a yes is read first.
constexpr double yesDelay = 0.5;

/// @brief Makes one of the question's two buttons.
/// @param card The box it goes in.
/// @param text What it says, which is also its accessible name.
/// @param right Its distance from the box's right edge.
/// @param strong Whether it is the one that says yes, drawn filled.
/// @return It.
Label& button(Panel& card, const std::string& text, float right, bool strong) {
	auto& made = card.add<Label>();
	made.setText(text);
	made.setAccessibleRole("button");
	made.setAccessibleLabel(text);
	made.setAlignment(Align::Center, Align::Center);
	made.setCornerRadii(CornerRadii::all(6.0f));
	made.setColor(strong ? colors::accent : colors::track);
	made.setTextColor(strong ? colors::white : colors::black);
	made.setCursor(Cursor::Hand);
	made.setSize(px(buttonWidth), px(buttonHeight));
	made.setAnchor(Edge::Right, px(right));
	made.setAnchor(Edge::Bottom, px(margin));
	return made;
}

/// @brief Makes a line of the question's text.
/// @param card The box it goes in.
/// @param text What it says.
/// @param top Its distance from the box's top.
/// @param height Its height.
/// @return It.
Label& line(Panel& card, const std::string& text, float top, float height) {
	auto& made = card.add<Label>();
	made.setText(text);
	made.setWrap(true);
	made.setColor(colors::transparent);
	made.setTextColor(colors::black);
	made.setAnchor(Edge::Left, px(margin));
	made.setAnchor(Edge::Right, px(margin));
	made.setAnchor(Edge::Top, px(top));
	made.setHeight(px(height));
	return made;
}

/// @brief Tells whether a panel is another or inside it.
/// @param panel The panel, or nullptr.
/// @param other The other.
/// @return true if it is.
bool within(const Panel* panel, const Panel* other) noexcept {
	for(; panel; panel = panel->parent())
		if(panel == other) return true;
	return false;
}

} // namespace

std::uint64_t Window::confirm(const Confirmation& question, std::function<void(bool accepted)> answered) {
	if(asking_) return 0;
	auto asking = std::make_unique<Asking>();
	asking->id = nextConfirmation_++;
	asking->shownAt = clock().seconds();
	asking->answered = std::move(answered);
	asking->backdrop = std::make_unique<Panel>();
	asking->backdrop->attachTo(this);
	asking->backdrop->setColor(colors::black.withAlpha(0.45f));
	Panel& card = asking->backdrop->add<Panel>();
	card.setColor(colors::white);
	card.setCornerRadii(CornerRadii::all(10.0f));
	card.setAccessibleRole("alertdialog");
	card.setAccessibleLabel(question.title);
	asking->card = &card;
	line(card, question.title, margin, 24.0f);
	line(card, question.message, margin + 28.0f, 40.0f);
	auto& text = card.add<TextEditor>();
	text.setText(question.text);
	text.setReadOnly(true);
	text.setWrap(true);        // nothing past the right edge
	text.setLineNumbers(true); // how long it is, at a glance
	text.setAnchor(Edge::Left, px(margin));
	text.setAnchor(Edge::Right, px(margin));
	text.setAnchor(Edge::Top, px(margin + 76.0f));
	text.setAnchor(Edge::Bottom, px(margin * 2.0f + buttonHeight));
	asking->text = &text;
	asking->accept = &button(card, question.accept, margin, true);
	asking->decline = &button(card, question.decline, margin * 1.5f + buttonWidth, false);
	asking_ = std::move(asking);
	// What a screen showed under the pointer stops: the question has the input now
	cancelTimer(std::exchange(tooltipTimer_, 0));
	tooltipShown_ = false;
	grabbed_ = nullptr;
	setHovered(nullptr);
	invalidate();
	return asking_->id;
}

void Window::dismissConfirmation(std::uint64_t id) {
	if(!asking_ || asking_->id != id) return;
	asking_.reset();
	invalidate();
}

Panel* Window::confirmation() const noexcept { return asking_ ? asking_->backdrop.get() : nullptr; }

void Window::layoutConfirmation() {
	if(!asking_) return;
	const auto width = static_cast<float>(width_), height = static_cast<float>(height_);
	const Rect area{0.0f, 0.0f, width, height};
	const float boxWidth = std::max(std::min(cardWidth, width - margin * 2.0f), 0.0f);
	const float boxHeight = std::max(std::min(cardHeight, height - margin * 2.0f), 0.0f);
	asking_->card->setPosition(px(std::round((width - boxWidth) / 2.0f)), px(std::round((height - boxHeight) / 2.0f)));
	asking_->card->setSize(px(boxWidth), px(boxHeight));
	asking_->backdrop->setSize(px(width), px(height));
	asking_->backdrop->layout(area, area, Vec2{width, height}, true);
	drawn_.push_back(asking_->backdrop.get()); // last: above everything else drawn
}

// A key answers when it is let go, if it was pressed while the question was shown: so the key that made the question
// come up can't answer it, and nothing of the answering key reaches the screen. Enter says yes only if pressed once the
// question has been shown a moment (yesDelay)
void Window::confirmationKey(const KeyEvent& event) {
	const Mods mods = event.mods;
	const bool plain = !mods.shift() && !mods.control() && !mods.alt() && !mods.super();
	const bool answers = (event.key == Key::Enter && plain) || event.key == Key::Escape;
	if(answers && event.action == Action::Press)
		asking_->pressedKey = event.key == Key::Enter && !confirmationReady() ? std::nullopt : std::optional(event.key);
	if(answers && event.action == Action::Release && asking_->pressedKey == event.key)
		return answerConfirmation(event.key == Key::Enter);
	Panel& text = *asking_->text; // through Panel, a friend: the editor keeps its handlers protected
	if(!answers && event.key != Key::Enter) text.keyEvent(event); // read, scroll, select and copy: it is read only
}

// A button answers when the click ends on the button it began on, so the whole click is the question's; the one that
// says yes, only a click begun once the question has been shown a moment (yesDelay)
void Window::confirmationButton(const MouseButtonEvent& event, Panel* hit) {
	if(event.button == MouseButton::Left) {
		Panel* on = within(hit, asking_->accept) && confirmationReady() ? asking_->accept
		            : within(hit, asking_->decline)                     ? asking_->decline
		                                                                : nullptr;
		if(event.action == Action::Press) asking_->pressedOn = on;
		if(event.action == Action::Release && on && on == std::exchange(asking_->pressedOn, nullptr))
			return answerConfirmation(on == asking_->accept);
	}
	if(event.action == Action::Press && within(hit, asking_->text)) grabbed_ = asking_->text;
	if(grabbed_ == asking_->text) {
		MouseButtonEvent local = event;
		local.position = asking_->text->toLocal(event.position).value_or(event.position);
		static_cast<Panel&>(*asking_->text).mouseButtonEvent(local);
	}
	if(event.action == Action::Release) grabbed_ = nullptr;
}

bool Window::confirmationReady() const noexcept { return asking_ && clock().seconds() - asking_->shownAt >= yesDelay; }

void Window::answerConfirmation(bool accepted) {
	if(!asking_ || !routingOwnPress()) return;
	std::function<void(bool)> answered = std::move(asking_->answered);
	// The panels go once the event is delivered: the one being pressed may be among them
	defer([asking = std::shared_ptr<Asking>(std::move(asking_))] { static_cast<void>(asking); });
	grabbed_ = nullptr;
	invalidate();
	if(answered) defer([answered = std::move(answered), accepted] { answered(accepted); });
}

} // namespace affresco
