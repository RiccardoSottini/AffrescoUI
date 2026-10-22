#pragma once

/// @file
/// @brief The terminal's sessions (Terminal.hpp): each one's screen and pty, and the store that keeps them by id and
/// talks to the `terminal` service. Internal to the terminal feature.

#include "features/terminal/TerminalModel.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/core/Json.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace forge::terminal {

class TerminalView;

/// @brief One terminal: its screen, its pty, and the view that shows it, if one does.
struct TerminalSession {
	std::unique_ptr<TerminalModel> model; ///< The screen and scrollback.
	std::uint64_t pty = 0;                ///< The provider's session, once it said it; 0 before and after.
	std::uint64_t serial = 0;             ///< Which open call is this session's: a late event of another is dropped.
	std::string status;                   ///< Why no program runs (shown on the screen), or "".
	bool started = false;                 ///< Whether a program was asked for.
	bool running = false;                 ///< Whether it runs.
	std::optional<int> code;              ///< How it exited.
	std::stop_source stop;                ///< Ends the program.
	TerminalView* view = nullptr;         ///< The view showing it.
	std::string pendingInput;             ///< Typed before the pty was open, at most mostPendingOutput bytes.
};

/// @brief The open terminals, by the ids terminal.json gives them. Main thread only.
class TerminalSessions final : public std::enable_shared_from_this<TerminalSessions> {
public:
	/// @brief No terminals yet.
	/// @param screen The screen, whose services run the programs (it owns this store).
	/// @param executor The main thread's, or nullptr to start no program.
	TerminalSessions(affresco::Screen& screen, affresco::Executor* executor) : screen_(screen), executor_(executor) {}
	/// @brief Ends every program.
	~TerminalSessions() {
		for(auto& [id, session] : sessions_) session->stop.request_stop();
	}
	TerminalSessions(const TerminalSessions&) = delete;
	TerminalSessions& operator=(const TerminalSessions&) = delete;
	TerminalSessions(TerminalSessions&&) = delete;
	TerminalSessions& operator=(TerminalSessions&&) = delete;

	TerminalSession* attach(const std::string& id, TerminalView& view, int rows, int columns);
	void detach(const std::string& id, const TerminalView& view) noexcept;
	/// @brief A session.
	/// @param id Its id.
	/// @return It, or nullptr.
	[[nodiscard]] TerminalSession* find(const std::string& id) noexcept {
		const auto found = sessions_.find(id);
		return found == sessions_.end() ? nullptr : found->second.get();
	}
	void send(TerminalSession& session, const std::string& bytes);
	void resize(TerminalSession& session, int rows, int columns);
	void close(const std::string& id);
	void clear(const std::string& id);
	void run(const std::string& id, const std::string& line);
	[[nodiscard]] affresco::json info(const std::string& id);
	void received(std::uint64_t serial, const affresco::json& event);
	void ended(std::uint64_t serial, std::optional<int> code, const std::string& error);

private:
	void start(TerminalSession& session);
	void write(const TerminalSession& session, std::string_view bytes);
	[[nodiscard]] TerminalSession* bySerial(std::uint64_t serial) noexcept;

	affresco::Screen& screen_;                                         ///< See the constructor.
	affresco::Executor* executor_;                                     ///< See the constructor.
	std::uint64_t serials_ = 0;                                        ///< Open calls made so far.
	std::map<std::string, std::unique_ptr<TerminalSession>> sessions_; ///< The terminals.
	std::map<std::string, std::string> queued_; ///< Lines to run in terminals not shown yet, by id.
};

} // namespace forge::terminal
