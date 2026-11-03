#pragma once

/// @file
/// @brief Window::Asking: the question Window::confirm() asks, drawn by the window above everything a screen draws.

#include "affresco/render/Label.hpp"
#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Window.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace affresco {

/// @brief The question shown: its panels, which no screen holds, and who gets the answer.
///
/// Invariants: every pointer points into backdrop, which the window owns and lays out last. Main thread only.
struct Window::Asking {
	std::uint64_t id = 0;               ///< See Window::confirm().
	std::function<void(bool)> answered; ///< Gets the answer.
	std::unique_ptr<Panel> backdrop;    ///< Covers the window, and holds the rest.
	Panel* card = nullptr;              ///< The question's box, in the middle.
	TextEditor* text = nullptr;         ///< What it is about, read only.
	Label* accept = nullptr;            ///< The button that says yes.
	Label* decline = nullptr;           ///< The button that says no.
	Panel* pressedOn = nullptr;         ///< The button a click began on, while it lasts.
	std::optional<Key> pressedKey;      ///< The answering key pressed while it was shown, until let go.
	double shownAt = 0.0;               ///< When it was shown, by the window's clock: a yes waits a moment.
};

} // namespace affresco
