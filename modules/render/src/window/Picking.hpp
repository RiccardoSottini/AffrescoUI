#pragma once

/// @file
/// @brief Window::Picking: the pick Window::pick() runs, and detail::PickLayer, what it draws above everything a screen
/// draws but the question confirm() asks.

#include "affresco/render/Label.hpp"
#include "affresco/render/Window.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace affresco {

namespace detail {

/// @brief What a pick draws: an outline over the panel offered, following its corners, a caption beside it, and a
/// hint at the top centre. It knows panels and text, nothing of what built them, and is never hit: the window lays it
/// out after the tree and draws it, and hit tests never reach it. Kept apart from the input handling, so a highlight
/// can be drawn without a pick running. Main thread only.
class PickLayer final : public Panel {
public:
	/// @brief Makes the layer, its parts hidden.
	/// @param color The outline's colour; the tint is it at 15%, and the caption's background it.
	explicit PickLayer(Color color);

	/// @brief Places the parts over a window; the window lays the layer out next (Window::layoutPick()).
	/// @param target The panel outlined, or nullptr for none; a panel not shown isn't outlined.
	/// @param caption The text beside it; empty for none.
	/// @param hint The text at the top centre; empty for none.
	/// @param hintTop Where the hint's top goes, in window units.
	/// @param window The window's size.
	void place(const Panel* target, const std::string& caption, const std::string& hint, float hintTop, Vec2 window);

	/// @brief The outline, for tests.
	[[nodiscard]] const Panel& highlight() const noexcept { return *highlight_; }
	/// @brief The caption, a polite live region, for tests.
	[[nodiscard]] const Label& caption() const noexcept { return *caption_; }
	/// @brief The hint, for tests.
	[[nodiscard]] const Label& hint() const noexcept { return *hint_; }

private:
	/// @brief Places the caption beside a frame: above it, or below where there is no room, inside the window.
	/// @param target The frame it goes beside.
	/// @param window The window's size.
	void placeCaption(const Rect& target, Vec2 window);

	Panel* highlight_ = nullptr; ///< The outline and its tint; a child, never null.
	Label* caption_ = nullptr;   ///< The text beside it; a child, never null.
	Label* hint_ = nullptr;      ///< The text at the top centre; a child, never null.
};

} // namespace detail

/// @brief The pick running: what it offers, the panel offered now, the press on its way, and what it draws.
///
/// Invariants: candidate and pressedOn point into the window's tree, and forget() nulls them before their panel goes;
/// options.start is used once, by pick(), and nulled. Main thread only.
struct Window::Picking {
	std::uint64_t id = 0;                     ///< See Window::pick().
	PickOptions options;                      ///< What it offers and shows.
	std::function<void(Panel*)> done;         ///< Gets what was picked.
	Panel* candidate = nullptr;               ///< The panel offered now, or nullptr.
	std::string caption;                      ///< Its caption, asked of options.caption when it changes.
	Panel* pressedOn = nullptr;               ///< The panel offered where a left press began, while it lasts.
	std::optional<Key> pressedKey;            ///< The key that picks or cancels, pressed while picking, until let go.
	bool rightPressed = false;                ///< The right button was pressed while picking, and not let go yet.
	std::unique_ptr<detail::PickLayer> layer; ///< What it draws.

	/// @brief Offers a panel, asking its caption.
	/// @param panel The panel, or nullptr to offer none.
	/// @return Whether it changed, so the layer is laid out again.
	bool offer(Panel* panel);
};

} // namespace affresco
