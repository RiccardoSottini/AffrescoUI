#pragma once

/// @file
/// @brief format::TerminalSessions: the screen's terminals, each one's screen (TerminalModel.hpp) and pty, kept by
/// the ids the `terminal` nodes and steps give them; their programs run on a service of the `pty` provider.

#include "TerminalModel.hpp"

#include <affresco/core/Json.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace affresco::format {

class ServiceCalls;
class TerminalView;

/// @brief The most terminals a screen keeps open.
constexpr std::size_t mostTerminals = 16;

/// @brief One terminal: its screen, its pty, and the view that shows it, if one does.
struct TerminalSession {
	std::unique_ptr<TerminalModel> model; ///< The screen and scrollback.
	std::string service;                  ///< The service its program runs on (the first view's `service`).
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

/// @brief The open terminals, by id. Main thread only; the engine owns it, and the views share it.
class TerminalSessions final : public std::enable_shared_from_this<TerminalSessions> {
public:
	/// @brief No terminals yet.
	/// @param calls The engine's service calls: their services run the programs, their executor (the main thread's)
	/// takes the programs' output; without one no program starts.
	explicit TerminalSessions(ServiceCalls& calls) : calls_(&calls) {}
	/// @brief Ends every program.
	~TerminalSessions() {
		for(auto& [id, session] : sessions_) session->stop.request_stop();
	}
	TerminalSessions(const TerminalSessions&) = delete;
	TerminalSessions& operator=(const TerminalSessions&) = delete;
	TerminalSessions(TerminalSessions&&) = delete;
	TerminalSessions& operator=(TerminalSessions&&) = delete;

	TerminalSession* attach(const std::string& id, const std::string& service, TerminalView& view, int rows,
	                        int columns);
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
	void forget() noexcept;
	void received(std::uint64_t serial, const json& event);
	void ended(std::uint64_t serial, std::optional<int> code, const std::string& error);

private:
	void start(TerminalSession& session);
	void write(const TerminalSession& session, std::string_view bytes);
	[[nodiscard]] TerminalSession* bySerial(std::uint64_t serial) noexcept;

	ServiceCalls* calls_;                                              ///< See the constructor; nullptr after forget().
	std::uint64_t serials_ = 0;                                        ///< Open calls made so far.
	std::map<std::string, std::unique_ptr<TerminalSession>> sessions_; ///< The terminals.
	std::map<std::string, std::string> queued_; ///< Lines to run in terminals not shown yet, by id.
};

} // namespace affresco::format
