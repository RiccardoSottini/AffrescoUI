#pragma once

/// @file
/// @brief format::TerminalView: the `terminal` node (TerminalType.cpp), which shows a session of
/// TerminalSessions.hpp and types into it.

#include "TerminalModel.hpp"
#include "TerminalSessions.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/render/Panel.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

class PropertyReader;

/// @brief The service a terminal's program runs on when its node names none.
constexpr std::string_view defaultService = "terminal";
/// @brief The longest session id.
constexpr std::size_t mostSessionIdBytes = 256;

/// @brief The `terminal` node: shows a session, and types into it.
class TerminalView final : public Panel {
public:
	/// @brief A view of no session yet.
	TerminalView() { setCursor(Cursor::IBeam); }
	/// @brief Leaves its session running.
	~TerminalView() override {
		if(sessions_) sessions_->detach(attached_, *this);
	}
	TerminalView(const TerminalView&) = delete;
	TerminalView& operator=(const TerminalView&) = delete;
	TerminalView(TerminalView&&) = delete;
	TerminalView& operator=(TerminalView&&) = delete;

	void apply(std::shared_ptr<TerminalSessions> sessions, const json& values, const PropertyReader& reader,
	           const std::string& path);
	/// @brief Its session's screen changed: draws it again.
	void contentChanged() noexcept { invalidatePaint(); }

protected:
	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	bool scrollContent(Vec2 delta) override;
	void focusEvent(bool focused) override;
	void arrange() override;
	void paint(Painter& painter) const override;

private:
	[[nodiscard]] const Font* font() const noexcept;
	[[nodiscard]] Vec2 cellSize() const noexcept;
	[[nodiscard]] TerminalSession* session() const noexcept;
	void configure(TerminalModel& model) const;
	void typed(TerminalSession& session);
	void scrollLines(long lines);
	void paintRow(Painter& painter, const TerminalModel& model, std::size_t line, float top) const;
	void paintRun(Painter& painter, const std::vector<TerminalCell>& cells, int start, int end, float top) const;
	void paintCursor(Painter& painter, const TerminalModel& model, std::size_t first) const;
	[[nodiscard]] Color foregroundOf(const TerminalCell& cell) const noexcept;
	[[nodiscard]] std::optional<Color> backgroundOf(const TerminalCell& cell) const noexcept;

	std::shared_ptr<TerminalSessions> sessions_; ///< The engine's terminals, from apply().
	std::string id_;                             ///< The `session` property.
	std::string service_;                        ///< The `service` property.
	std::string attached_;                       ///< The session it attached to ("" before): a closed one isn't remade.
	std::shared_ptr<const Font> font_;           ///< The `font` property; the window's if none.
	Color textColor_ = Color::hex(0xcccccc);     ///< The `textColor` property.
	Color cursorColor_ = Color::hex(0xaeafad);   ///< The `cursorColor` property.
	std::size_t scrollback_ = defaultScrollbackLines; ///< The `scrollback` property.
	bool allowTitle_ = false;                         ///< The `allowTitle` property.
	std::size_t scrolledBack_ = 0;                    ///< Lines scrolled back from the bottom.
	float wheel_ = 0.0f;                              ///< Wheel pixels not yet a whole line.
};

} // namespace affresco::format
