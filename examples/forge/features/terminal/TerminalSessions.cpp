/// @file
/// @brief The terminal's sessions (TerminalSessions.hpp) and their calls to the `terminal` service.

#include "features/terminal/TerminalSessions.hpp"

#include "features/terminal/Terminal.hpp"
#include "features/terminal/TerminalView.hpp"

#include <affresco/core/Services.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace forge::terminal {

using affresco::json;

namespace {

/// @brief The service the terminals run on, as terminal.json declares it.
constexpr std::string_view serviceName = "terminal";
/// @brief The most bytes one `write` call carries (the provider takes at most 64 KiB characters).
constexpr std::size_t mostWriteBytes = std::size_t{16} * 1024;
/// @brief The status of a terminal in a host that starts no program.
constexpr std::string_view notStarted = "Terminals start when Forge runs.";

/// @brief The call that opens a pty: its events and its end go to the main thread, to the session that made it.
class OpenCall final : public affresco::services::Call {
public:
	/// @brief A call for a session.
	/// @param arguments `{cols, rows}`.
	/// @param stop Ends it.
	/// @param sessions The store (gone with the screen).
	/// @param executor The main thread's.
	/// @param serial The session's serial.
	OpenCall(json arguments, std::stop_token stop, std::weak_ptr<TerminalSessions> sessions,
	         affresco::Executor& executor, std::uint64_t serial)
	    : arguments_(std::move(arguments)), stop_(std::move(stop)), sessions_(std::move(sessions)), executor_(executor),
	      serial_(serial) {}

	/// @brief `{cols, rows}`.
	[[nodiscard]] const json& arguments() const noexcept override { return arguments_; }
	/// @brief Requested when the session closes.
	[[nodiscard]] std::stop_token stopToken() const noexcept override { return stop_; }
	/// @brief The provider reports none.
	void progress(json /*progress*/) override {}
	/// @brief Takes `{session}` or `{data}` to the session.
	/// @param event The event.
	void event(json event) override {
		post([event = std::move(event)](TerminalSessions& sessions, std::uint64_t serial) {
			sessions.received(serial, event);
		});
	}
	/// @brief The program exited: `{code}`.
	/// @param result The result.
	void finish(json result) override {
		const std::optional<int> code = result.contains("code") && result["code"].is_number_integer()
		                                    ? std::optional(result["code"].get<int>())
		                                    : std::nullopt;
		post([code](TerminalSessions& sessions, std::uint64_t serial) { sessions.ended(serial, code, ""); });
	}
	/// @brief The program couldn't start, or was ended.
	/// @param message Why.
	void fail(std::string /*code*/, std::string message, json /*data*/) override {
		post([message = std::move(message)](TerminalSessions& sessions, std::uint64_t serial) {
			sessions.ended(serial, std::nullopt, message);
		});
	}

private:
	/// @brief Runs a task on the main thread, if the store is still there.
	/// @param task Takes the store and the serial.
	template <typename Task> void post(Task task) {
		executor_.post([sessions = sessions_, serial = serial_, task = std::move(task)] {
			if(const std::shared_ptr<TerminalSessions> alive = sessions.lock()) task(*alive, serial);
		});
	}

	json arguments_;                           ///< See arguments().
	std::stop_token stop_;                     ///< See stopToken().
	std::weak_ptr<TerminalSessions> sessions_; ///< Where the events go.
	affresco::Executor& executor_;             ///< See the constructor.
	std::uint64_t serial_;                     ///< See the constructor.
};

/// @brief A call whose result doesn't matter: `write`, `resize` and `signal` answer `{}` at once.
class QuietCall final : public affresco::services::Call {
public:
	/// @brief A call.
	/// @param arguments Its arguments.
	explicit QuietCall(json arguments) : arguments_(std::move(arguments)) {}
	/// @brief Its arguments.
	[[nodiscard]] const json& arguments() const noexcept override { return arguments_; }
	/// @brief Never requested.
	[[nodiscard]] std::stop_token stopToken() const noexcept override { return {}; }
	/// @brief Ignored.
	void progress(json /*progress*/) override {}
	/// @brief Ignored.
	void event(json /*event*/) override {}
	/// @brief Ignored.
	void finish(json /*result*/) override {}
	/// @brief Ignored: the session's own call says when the terminal ends.
	void fail(std::string /*code*/, std::string /*message*/, json /*data*/) override {}

private:
	json arguments_; ///< See arguments().
};

/// @brief Where a cut of UTF-8 text may go: at most @p most bytes in, never inside a character.
/// @param bytes The text.
/// @param most The most bytes before the cut.
/// @return The cut.
std::size_t cutBefore(std::string_view bytes, std::size_t most) noexcept {
	if(bytes.size() <= most) return bytes.size();
	std::size_t cut = most;
	while(cut > 0 && (static_cast<unsigned char>(bytes[cut]) & 0xC0U) == 0x80U) --cut;
	return cut == 0 ? most : cut;
}

} // namespace

/// @brief Shows a session in a view, making it (and starting its program) at its first use.
/// @param id The session.
/// @param view The view.
/// @param rows The view's rows.
/// @param columns The view's columns.
/// @return The session, or nullptr if mostTerminals are open.
TerminalSession* TerminalSessions::attach(const std::string& id, TerminalView& view, int rows, int columns) {
	TerminalSession* session = find(id);
	if(!session) {
		if(sessions_.size() >= mostTerminals) return nullptr;
		auto made = std::make_unique<TerminalSession>();
		made->model = std::make_unique<TerminalModel>(rows, columns);
		session = sessions_.emplace(id, std::move(made)).first->second.get();
		if(const auto queued = queued_.extract(id)) session->pendingInput = std::move(queued.mapped());
	}
	session->view = &view;
	resize(*session, rows, columns);
	if(!session->started) start(*session);
	return session;
}

/// @brief A view stops showing a session; it keeps running.
/// @param id The session.
/// @param view The view.
void TerminalSessions::detach(const std::string& id, const TerminalView& view) noexcept {
	if(TerminalSession* session = find(id); session && session->view == &view) session->view = nullptr;
}

/// @brief Starts a session's program, or says why it doesn't run.
/// @param session The session.
void TerminalSessions::start(TerminalSession& session) {
	session.started = true;
	if(!executor_) {
		session.status = notStarted;
		return;
	}
	session.serial = ++serials_;
	session.stop = std::stop_source();
	const json size = {{"cols", session.model->columns()}, {"rows", session.model->rows()}};
	auto call =
	    std::make_shared<OpenCall>(size, session.stop.get_token(), weak_from_this(), *executor_, session.serial);
	const std::optional<std::string> error = screen_.services().invoke(serviceName, "open", std::move(call));
	session.status = error ? *error : "Starting the shell…";
}

/// @brief Sends bytes to a session's program, or keeps them (bounded) until its pty is open.
/// @param session The session.
/// @param bytes Typed keys and replies to the program's queries.
void TerminalSessions::send(TerminalSession& session, const std::string& bytes) {
	if(bytes.empty()) return;
	if(!session.pty) {
		if(session.pendingInput.size() + bytes.size() <= mostPendingOutput) session.pendingInput += bytes;
		return;
	}
	for(std::string_view rest = bytes; !rest.empty();) {
		const std::size_t cut = cutBefore(rest, mostWriteBytes);
		write(session, rest.substr(0, cut));
		rest.remove_prefix(cut);
	}
}

/// @brief Runs a line in a terminal: types it with a newline, now or when a view first shows the terminal.
/// @param id The terminal.
/// @param line The line, without its newline.
void TerminalSessions::run(const std::string& id, const std::string& line) {
	if(TerminalSession* session = find(id)) return send(*session, line + "\n");
	if(queued_.size() >= mostTerminals && !queued_.contains(id)) return;
	std::string& queued = queued_[id];
	if(queued.size() + line.size() + 1 <= mostPendingOutput) queued += line + "\n";
}

/// @brief Writes one chunk to a session's pty.
/// @param session The session, with its pty open.
/// @param bytes At most mostWriteBytes.
void TerminalSessions::write(const TerminalSession& session, std::string_view bytes) {
	const json with = {{"session", session.pty}, {"data", std::string(bytes)}};
	(void)screen_.services().invoke(serviceName, "write", std::make_shared<QuietCall>(with)); // busy: dropped
}

/// @brief Resizes a session's screen and pty.
/// @param session The session.
/// @param rows Its rows.
/// @param columns Its columns.
void TerminalSessions::resize(TerminalSession& session, int rows, int columns) {
	if(session.model->rows() == rows && session.model->columns() == columns) return;
	session.model->resize(rows, columns);
	if(!session.pty) return;
	const json with = {{"session", session.pty}, {"cols", session.model->columns()}, {"rows", session.model->rows()}};
	(void)screen_.services().invoke(serviceName, "resize", std::make_shared<QuietCall>(with));
}

/// @brief Ends a session's program and forgets it.
/// @param id The session.
void TerminalSessions::close(const std::string& id) {
	const auto found = sessions_.find(id);
	if(found == sessions_.end()) return;
	found->second->stop.request_stop();
	if(found->second->view) found->second->view->contentChanged();
	sessions_.erase(found);
}

/// @brief Clears a session's scrollback, and asks its program to redraw.
/// @param id The session.
void TerminalSessions::clear(const std::string& id) {
	TerminalSession* session = find(id);
	if(!session) return;
	session->model->write("\x1b[3J");
	send(*session, "\x0c");
	if(session->view) session->view->contentChanged();
}

/// @brief What a session is doing.
/// @param id The session.
/// @return `{exists, running, title, status, code}`.
json TerminalSessions::info(const std::string& id) {
	const TerminalSession* session = find(id);
	if(!session) return {{"exists", false}, {"running", false}, {"title", ""}, {"status", ""}, {"code", nullptr}};
	return {{"exists", true},
	        {"running", session->running},
	        {"title", session->model->title()},
	        {"status", session->status},
	        {"code", session->code ? json(*session->code) : json(nullptr)}};
}

/// @brief The session an open call belongs to.
/// @param serial The call's serial.
/// @return The session, or nullptr if it was closed.
TerminalSession* TerminalSessions::bySerial(std::uint64_t serial) noexcept {
	const auto found =
	    std::ranges::find_if(sessions_, [serial](const auto& entry) { return entry.second->serial == serial; });
	return found == sessions_.end() ? nullptr : found->second.get();
}

/// @brief An event of a session's open call: its pty's id, or what the program wrote.
/// @param serial The call's serial.
/// @param event `{session}` or `{data}`.
void TerminalSessions::received(std::uint64_t serial, const json& event) {
	TerminalSession* session = bySerial(serial);
	if(!session || !event.is_object()) return;
	if(const auto pty = event.find("session"); pty != event.end() && pty->is_number_unsigned()) {
		session->pty = pty->get<std::uint64_t>();
		session->running = true;
		session->status.clear();
		send(*session, std::exchange(session->pendingInput, std::string()));
		resize(*session, session->model->rows(), session->model->columns());
	} else if(const auto data = event.find("data"); data != event.end() && data->is_string()) {
		session->model->write(data->get_ref<const std::string&>());
		send(*session, session->model->takeOutput());
	} else {
		return;
	}
	if(session->view) session->view->contentChanged();
}

/// @brief A session's program ended, or never started.
/// @param serial The call's serial.
/// @param code Its exit code, if it exited.
/// @param error Why it ended otherwise.
void TerminalSessions::ended(std::uint64_t serial, std::optional<int> code, const std::string& error) {
	TerminalSession* session = bySerial(serial);
	if(!session) return;
	session->pty = 0;
	session->running = false;
	session->code = code;
	session->status = code ? "The shell exited with code " + std::to_string(*code) + "." : error;
	if(session->view) session->view->contentChanged();
}

} // namespace forge::terminal
