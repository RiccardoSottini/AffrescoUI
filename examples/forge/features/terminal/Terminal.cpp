/// @file
/// @brief Forge's terminal (Terminal.hpp): the sessions, their calls to the `terminal` service, and the widget.

#include "features/terminal/Terminal.hpp"

#include "features/terminal/TerminalModel.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/core/Services.hpp>
#include <affresco/render/RichText.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace forge {

namespace fs = std::filesystem;
using affresco::Color;
using affresco::json;
using affresco::Key;

namespace {

/// @brief The service the terminals run on, as terminal.json declares it.
constexpr std::string_view serviceName = "terminal";
/// @brief The most bytes one `write` call carries (the provider takes at most 64 KiB characters).
constexpr std::size_t mostWriteBytes = 16 * 1024;
/// @brief The status of a terminal in a host that starts no program.
constexpr std::string_view notStarted = "Terminals start when Forge runs.";

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
	void send(TerminalSession& session, std::string bytes);
	void resize(TerminalSession& session, int rows, int columns);
	void close(const std::string& id);
	void clear(const std::string& id);
	[[nodiscard]] json info(const std::string& id);
	void received(std::uint64_t serial, const json& event);
	void ended(std::uint64_t serial, std::optional<int> code, const std::string& error);

private:
	void start(TerminalSession& session);
	void write(const TerminalSession& session, std::string_view bytes);
	[[nodiscard]] TerminalSession* bySerial(std::uint64_t serial) noexcept;

	affresco::Screen& screen_;                                         ///< See the constructor.
	affresco::Executor* executor_;                                     ///< See the constructor.
	std::uint64_t serials_ = 0;                                        ///< Open calls made so far.
	std::map<std::string, std::unique_ptr<TerminalSession>> sessions_; ///< The terminals.
};

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
	void progress(json) override {}
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
	void fail(std::string, std::string message, json) override {
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
	void progress(json) override {}
	/// @brief Ignored.
	void event(json) override {}
	/// @brief Ignored.
	void finish(json) override {}
	/// @brief Ignored: the session's own call says when the terminal ends.
	void fail(std::string, std::string, json) override {}

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

	void apply(const affresco::Screen& screen, const json& properties);
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
	[[nodiscard]] Color foregroundOf(const TerminalCell& cell) const noexcept;
	[[nodiscard]] std::optional<Color> backgroundOf(const TerminalCell& cell) const noexcept;

	std::shared_ptr<TerminalSessions> sessions_; ///< See the constructor.
	std::string id_;                             ///< The `session` property.
	std::string attached_;                       ///< The session it attached to ("" before): a closed one isn't remade.
	std::shared_ptr<const affresco::Font> font_; ///< The `font` property; the window's if none.
	Color textColor_ = Color::hex(0xcccccc);     ///< The `textColor` property.
	Color cursorColor_ = Color::hex(0xaeafad);   ///< The `cursorColor` property.
	std::size_t scrollback_ = defaultScrollbackLines; ///< The `scrollback` property.
	bool allowTitle_ = false;                         ///< The `allowTitle` property.
	std::size_t scrolledBack_ = 0;                    ///< Lines scrolled back from the bottom.
	float wheel_ = 0.0f;                              ///< Wheel pixels not yet a whole line.
};

/* TerminalSessions */

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
void TerminalSessions::send(TerminalSession& session, std::string bytes) {
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

/* TerminalView */

/// @brief Applies the node's properties.
/// @param screen The screen, for the font and the colours.
/// @param properties `session`, `font`, `textColor`, `cursorColor`, `scrollback`, `allowTitle`.
void TerminalView::apply(const affresco::Screen& screen, const json& properties) {
	id_ = properties.value("session", std::string());
	const std::string fontName = properties.value("font", std::string());
	font_ = fontName.empty() ? nullptr : screen.font(fontName);
	textColor_ = screen.resolveColor(properties.value("textColor", std::string("#cccccc")));
	cursorColor_ = screen.resolveColor(properties.value("cursorColor", std::string("#aeafad")));
	scrollback_ = std::min(
	    mostScrollbackLines,
	    static_cast<std::size_t>(std::max(0.0, properties.value("scrollback", double(defaultScrollbackLines)))));
	allowTitle_ = properties.value("allowTitle", false);
	if(TerminalSession* shown = session()) configure(*shown->model);
	invalidate();
}

/// @brief The font it draws with.
/// @return The `font` property's, else the window's; nullptr before it has a window.
const affresco::Font* TerminalView::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

/// @brief One cell's size, in window units.
/// @return Its width and height; 0 without a font.
affresco::Vec2 TerminalView::cellSize() const noexcept {
	const affresco::Font* drawn = font();
	if(!drawn) return {0.0f, 0.0f};
	return {std::max(1.0f, drawn->measure(U"M")), std::max(1.0f, drawn->lineHeight())};
}

/// @brief The session it shows.
/// @return It, or nullptr (none attached, or closed).
TerminalSession* TerminalView::session() const noexcept {
	return attached_.empty() ? nullptr : sessions_->find(attached_);
}

/// @brief Gives a session's screen this view's settings.
/// @param model The screen.
void TerminalView::configure(TerminalModel& model) const {
	model.setScrollbackLimit(scrollback_);
	model.setAllowTitle(allowTitle_);
}

/// @brief Sizes the session to the view: attaches at the first layout, resizes the pty when the size changes.
void TerminalView::arrange() {
	const affresco::Vec2 cell = cellSize();
	const affresco::Rect& frame = bounds();
	if(cell.x <= 0.0f || frame.width() < cell.x || frame.height() < cell.y) return; // hidden: keep the size it had
	const int columns = std::clamp(static_cast<int>(frame.width() / cell.x), 1, mostTerminalCells);
	const int rows = std::clamp(static_cast<int>(frame.height() / cell.y), 1, mostTerminalCells);
	if(id_.empty()) return;
	if(id_ != attached_) {
		sessions_->detach(attached_, *this);
		attached_ = id_;
		scrolledBack_ = 0;
		if(TerminalSession* made = sessions_->attach(id_, *this, rows, columns)) configure(*made->model);
		return;
	}
	if(TerminalSession* shown = session()) sessions_->resize(*shown, rows, columns);
}

/// @brief Sends what a session's screen has for the program, and shows the bottom.
/// @param session The session.
void TerminalView::typed(TerminalSession& session) {
	sessions_->send(session, session.model->takeOutput());
	scrolledBack_ = 0;
	invalidatePaint();
}

/// @brief Whether a key stays a shortcut: with Super, or with Control and Shift together.
/// @param mods The modifiers.
/// @return Whether it does.
bool isShortcut(affresco::Mods mods) noexcept { return mods.super() || (mods.control() && mods.shift()); }

/// @brief Types a key: Control and a letter as its control code, Enter, Tab, the arrows and the other keys
/// TerminalModel::pressKey() knows as their sequences, Shift+Page Up and Down as scrolling back. Other keys, and
/// shortcuts, go to the document's keys.
/// @param event The event.
void TerminalView::keyEvent(const affresco::KeyEvent& event) {
	TerminalSession* shown = session();
	if(!shown || event.action == affresco::Action::Release || isShortcut(event.mods)) return Panel::keyEvent(event);
	const bool page = event.key == Key::PageUp || event.key == Key::PageDown;
	if(page && event.mods.shift() && !shown->model->alternateScreen()) {
		const long rows = shown->model->rows();
		return scrollLines(event.key == Key::PageUp ? rows : -rows);
	}
	const int code = static_cast<int>(event.key);
	if(event.mods.control() && code >= static_cast<int>(Key::A) && code <= static_cast<int>(Key::Z)) {
		shown->model->typeCharacter(static_cast<char32_t>(int{U'a'} + code - static_cast<int>(Key::A)), event.mods);
		return typed(*shown);
	}
	if(shown->model->pressKey(event.key, event.mods)) return typed(*shown);
	Panel::keyEvent(event);
}

/// @brief Types a character.
/// @param event The event.
void TerminalView::charEvent(const affresco::CharEvent& event) {
	TerminalSession* shown = session();
	if(!shown) return Panel::charEvent(event);
	shown->model->typeCharacter(event.codepoint);
	typed(*shown);
}

/// @brief Scrolls back by whole lines.
/// @param lines Lines towards the top; negative towards the bottom.
void TerminalView::scrollLines(long lines) {
	const TerminalSession* shown = session();
	if(!shown) return;
	const auto most = static_cast<long>(shown->model->scrollbackSize());
	scrolledBack_ = static_cast<std::size_t>(std::clamp(static_cast<long>(scrolledBack_) + lines, 0L, most));
	invalidatePaint();
}

/// @brief Scrolls the scrollback with the wheel (not the alternate screen, which has none).
/// @param delta Pixels; positive scrolls towards the end.
/// @return Whether it scrolled.
bool TerminalView::scrollContent(affresco::Vec2 delta) {
	const TerminalSession* shown = session();
	if(!shown || shown->model->alternateScreen() || shown->model->scrollbackSize() == 0) return false;
	wheel_ -= delta.y / cellSize().y;
	const float whole = std::trunc(wheel_);
	wheel_ -= whole;
	scrollLines(static_cast<long>(whole));
	return true;
}

/// @brief Draws the cursor as focused or not.
/// @param focused Whether it has the focus.
void TerminalView::focusEvent(bool focused) {
	invalidatePaint();
	Panel::focusEvent(focused);
}

/// @brief A cell's text colour.
/// @param cell The cell.
/// @return The colour.
Color TerminalView::foregroundOf(const TerminalCell& cell) const noexcept {
	if(cell.reverse)
		return cell.defaultBackground ? (color().a > 0.0f ? color() : Color::hex(0x1e1e1e))
		                              : Color::hex(cell.background);
	return cell.defaultForeground ? textColor_ : Color::hex(cell.foreground);
}

/// @brief A cell's background, if it isn't the view's.
/// @param cell The cell.
/// @return The colour, or std::nullopt.
std::optional<Color> TerminalView::backgroundOf(const TerminalCell& cell) const noexcept {
	if(cell.reverse) return cell.defaultForeground ? textColor_ : Color::hex(cell.foreground);
	if(cell.defaultBackground) return std::nullopt;
	return Color::hex(cell.background);
}

/// @brief Whether two cells are drawn in one run: the same colours and underline, both one column wide.
/// @param first The run's first cell.
/// @param next The next cell.
/// @return Whether they are.
bool sameRun(const TerminalCell& first, const TerminalCell& next) noexcept {
	return first.width == 1 && next.width == 1 && first.defaultForeground == next.defaultForeground &&
	       first.foreground == next.foreground && first.defaultBackground == next.defaultBackground &&
	       first.background == next.background && first.reverse == next.reverse && first.underline == next.underline;
}

/// @brief Draws a run of cells: their background, text and underline.
/// @param painter The painter.
/// @param cells The line's cells.
/// @param start The run's first column.
/// @param end Past its last column.
/// @param top The line's top.
void TerminalView::paintRun(affresco::Painter& painter, const std::vector<TerminalCell>& cells, int start, int end,
                            float top) const {
	const affresco::Vec2 cell = cellSize();
	const TerminalCell& first = cells[static_cast<std::size_t>(start)];
	const float left = bounds().left + static_cast<float>(start) * cell.x;
	const float right = left + static_cast<float>(end - start) * cell.x;
	if(const std::optional<Color> background = backgroundOf(first))
		painter.fillRect({left, top, right, top + cell.y}, *background);
	std::u32string text;
	bool blank = true;
	for(int column = start; column < end; ++column) {
		const char32_t character = cells[static_cast<std::size_t>(column)].character;
		blank = blank && (character == 0 || character == U' ');
		text.push_back(character == 0 ? U' ' : character);
	}
	const Color foreground = foregroundOf(first);
	const float baseline = top + font()->ascent();
	if(!blank) painter.drawText(*font(), text, {left, baseline}, foreground);
	if(first.underline) painter.fillRect({left, baseline + 1.0f, right, baseline + 2.0f}, foreground);
}

/// @brief Draws one line of the scrollback or the screen.
/// @param painter The painter.
/// @param model The screen.
/// @param line The line (TerminalModel::cell()).
/// @param top Its top.
void TerminalView::paintRow(affresco::Painter& painter, const TerminalModel& model, std::size_t line, float top) const {
	const int columns = model.columns();
	std::vector<TerminalCell> cells;
	cells.reserve(static_cast<std::size_t>(columns));
	for(int column = 0; column < columns; ++column) cells.push_back(model.cell(line, column));
	for(int start = 0; start < columns;) {
		const TerminalCell& first = cells[static_cast<std::size_t>(start)];
		int end = start + 1;
		if(first.width == 2) end = std::min(columns, start + 2);
		else
			while(end < columns && sameRun(first, cells[static_cast<std::size_t>(end)])) ++end;
		if(first.width != 0) paintRun(painter, cells, start, end, top);
		start = end;
	}
}

/// @brief Draws the cursor: a block while focused, an outline otherwise.
/// @param painter The painter.
/// @param model The screen.
/// @param first The first line shown.
void TerminalView::paintCursor(affresco::Painter& painter, const TerminalModel& model, std::size_t first) const {
	const TerminalCursor cursor = model.cursor();
	const std::size_t line = model.scrollbackSize() + static_cast<std::size_t>(cursor.row);
	if(!cursor.visible || line < first || line >= first + static_cast<std::size_t>(model.rows())) return;
	const affresco::Vec2 cell = cellSize();
	const float left = bounds().left + static_cast<float>(cursor.column) * cell.x;
	const float top = bounds().top + static_cast<float>(line - first) * cell.y;
	const affresco::Rect block{left, top, left + cell.x, top + cell.y};
	if(!hasFocus()) return painter.strokeRoundedRect(block, {}, 1.0f, cursorColor_);
	painter.fillRect(block, cursorColor_);
	const char32_t character = model.cell(line, cursor.column).character;
	if(character != 0)
		painter.drawText(*font(), std::u32string(1, character), {left, top + font()->ascent()},
		                 color().a > 0.0f ? color() : Color::hex(0x1e1e1e));
}

/// @brief Draws the background, the visible lines, the cursor, and the status when no program runs.
/// @param painter Clips to bounds().
void TerminalView::paint(affresco::Painter& painter) const {
	Panel::paint(painter);
	const TerminalSession* shown = session();
	if(!shown || !font()) return;
	const TerminalModel& model = *shown->model;
	const std::size_t rows = static_cast<std::size_t>(model.rows());
	const std::size_t back = std::min(scrolledBack_, model.scrollbackSize());
	const std::size_t first = model.scrollbackSize() - back; // the screen's top is line scrollbackSize()
	const float height = cellSize().y;
	for(std::size_t row = 0; row < rows; ++row)
		paintRow(painter, model, first + row, bounds().top + static_cast<float>(row) * height);
	paintCursor(painter, model, first);
	if(shown->status.empty()) return;
	Color dim = textColor_;
	dim.a *= 0.6f;
	painter.drawText(*font(), affresco::codepointsOf(shown->status),
	                 {bounds().left, bounds().bottom - height + font()->ascent()}, dim);
}

/// @brief The user's shell, as an absolute path.
/// @return $SHELL if it is an absolute path to a file, else /bin/sh; on Windows %COMSPEC%, else cmd.exe.
fs::path shellProgram() {
#ifdef _WIN32
	std::size_t length = 0;
	if(getenv_s(&length, nullptr, 0, "COMSPEC") == 0 && length > 0) {
		std::string value(length, '\0');
		if(getenv_s(&length, value.data(), value.size(), "COMSPEC") == 0) {
			value.resize(length > 0 ? length - 1 : 0);
			if(fs::path(value).is_absolute()) return value;
		}
	}
	return "C:\\Windows\\System32\\cmd.exe";
#else
	const char* shell = std::getenv("SHELL"); // NOLINT(concurrency-mt-unsafe): read once, at launch
	std::error_code error;
	if(shell && fs::path(shell).is_absolute() && fs::is_regular_file(shell, error)) return shell;
	return "/bin/sh";
#endif
}

/// @brief A call's `id` argument.
/// @param arguments The arguments, as JSON text.
/// @return The id, or "".
std::string idOf(const std::string& arguments) {
	const json with = affresco::parseUntrusted(arguments);
	return with.is_object() && with.contains("id") && with["id"].is_string() ? with["id"].get<std::string>() : "";
}

} // namespace

void grantTerminal(affresco::Screen& screen, const fs::path& folder) {
#ifdef _WIN32
	screen.services().allowProcess("shell", shellProgram());
#else
	screen.services().allowProcess("shell", shellProgram(), {"-l"});
#endif
	screen.services().addRoot("terminal", folder.empty() ? fs::current_path() : folder);
}

void registerTerminal(affresco::Screen& screen, affresco::Executor* executor) {
	const auto sessions = std::make_shared<TerminalSessions>(screen, executor);
	screen.registerType(
	    "terminal", [sessions] { return std::make_unique<TerminalView>(sessions); },
	    {"session", "font", "textColor", "cursorColor", "scrollback", "allowTitle"},
	    [&screen](affresco::Panel& panel, const std::string& propertiesJson) {
		    dynamic_cast<TerminalView&>(panel).apply(screen, json::parse(propertiesJson)); // the factory made it
	    });
	screen.onCall("forge.terminal.close", [sessions](const std::string& arguments) {
		sessions->close(idOf(arguments));
		return json::object().dump();
	});
	screen.onCall("forge.terminal.clear", [sessions](const std::string& arguments) {
		sessions->clear(idOf(arguments));
		return json::object().dump();
	});
	screen.onCall("forge.terminal.info",
	              [sessions](const std::string& arguments) { return sessions->info(idOf(arguments)).dump(); });
}

} // namespace forge
