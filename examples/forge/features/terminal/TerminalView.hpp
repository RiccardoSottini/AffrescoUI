#pragma once

/// @file
/// @brief The `terminal` widget (Terminal.hpp): shows a session of TerminalSessions.hpp, and types into it. Internal
/// to the terminal feature.

#include "features/terminal/TerminalModel.hpp"
#include "features/terminal/TerminalSessions.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Json.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace forge::terminal {

/// @brief The `terminal` widget: shows a session, and types into it.
class TerminalView final : public affresco::Panel {
public:
	/// @brief A view of no session yet.
	/// @param sessions The store.
	explicit TerminalView(std::shared_ptr<TerminalSessions> sessions) : sessions_(std::move(sessions)) {
		setCursor(affresco::Cursor::IBeam);
	}
	/// @brief Leaves its session running.
	~TerminalView() override { sessions_->detach(attached_, *this); }
	TerminalView(const TerminalView&) = delete;
	TerminalView& operator=(const TerminalView&) = delete;
	TerminalView(TerminalView&&) = delete;
	TerminalView& operator=(TerminalView&&) = delete;

	void apply(const affresco::Screen& screen, const affresco::json& properties);
	/// @brief Its session's screen changed: draws it again.
	void contentChanged() noexcept { invalidatePaint(); }

protected:
	void keyEvent(const affresco::KeyEvent& event) override;
	void charEvent(const affresco::CharEvent& event) override;
	bool scrollContent(affresco::Vec2 delta) override;
	void focusEvent(bool focused) override;
	void arrange() override;
	void paint(affresco::Painter& painter) const override;

private:
	[[nodiscard]] const affresco::Font* font() const noexcept;
	[[nodiscard]] affresco::Vec2 cellSize() const noexcept;
	[[nodiscard]] TerminalSession* session() const noexcept;
	void configure(TerminalModel& model) const;
	void typed(TerminalSession& session);
	void scrollLines(long lines);
	void paintRow(affresco::Painter& painter, const TerminalModel& model, std::size_t line, float top) const;
	void paintRun(affresco::Painter& painter, const std::vector<TerminalCell>& cells, int start, int end,
	              float top) const;
	void paintCursor(affresco::Painter& painter, const TerminalModel& model, std::size_t first) const;
	[[nodiscard]] affresco::Color foregroundOf(const TerminalCell& cell) const noexcept;
	[[nodiscard]] std::optional<affresco::Color> backgroundOf(const TerminalCell& cell) const noexcept;

	std::shared_ptr<TerminalSessions> sessions_; ///< See the constructor.
	std::string id_;                             ///< The `session` property.
	std::string attached_;                       ///< The session it attached to ("" before): a closed one isn't remade.
	std::shared_ptr<const affresco::Font> font_; ///< The `font` property; the window's if none.
	affresco::Color textColor_ = affresco::Color::hex(0xcccccc);   ///< The `textColor` property.
	affresco::Color cursorColor_ = affresco::Color::hex(0xaeafad); ///< The `cursorColor` property.
	std::size_t scrollback_ = defaultScrollbackLines;              ///< The `scrollback` property.
	bool allowTitle_ = false;                                      ///< The `allowTitle` property.
	std::size_t scrolledBack_ = 0;                                 ///< Lines scrolled back from the bottom.
	float wheel_ = 0.0f;                                           ///< Wheel pixels not yet a whole line.
};

} // namespace forge::terminal
