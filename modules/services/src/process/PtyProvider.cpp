/// @file
/// @brief The `pty` provider: a granted program on a terminal of its own, its output streamed as text, what is typed
/// written to it, resized and signalled.

#include "Providers.hpp"
#include "process/Pty.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace affresco::services {

namespace {

constexpr std::size_t mostSessions = 8;                      ///< Terminals open at once, per service.
constexpr std::size_t largestRead = std::size_t{16} << 10;   ///< An event's `data`, at most 16 KiB of output.
constexpr std::size_t largestWrite = std::size_t{64} << 10;  ///< A write's `data`, at most 64 KiB.
constexpr std::size_t mostPending = std::size_t{1} << 20;    ///< Bytes typed but not yet taken, at most 1 MiB.
constexpr std::size_t bytesPerSecond = std::size_t{4} << 20; ///< Output streamed, at most 4 MiB a second.
constexpr int largestSide = 1000;                            ///< Columns and rows, at most 1000.

/// @brief The length of the UTF-8 sequence a lead byte starts.
/// @param lead The byte.
/// @return 1 to 4, or 0 if it can't start one.
[[nodiscard]] std::size_t sequenceLength(unsigned char lead) noexcept {
	if(lead < 0x80) return 1;
	if(lead >= 0xC2 && lead <= 0xDF) return 2;
	if(lead >= 0xE0 && lead <= 0xEF) return 3;
	if(lead >= 0xF0 && lead <= 0xF4) return 4;
	return 0;
}

/// @brief Whether every byte continues a UTF-8 sequence (10xxxxxx).
/// @param bytes The bytes.
/// @return Whether they do.
[[nodiscard]] bool continues(std::string_view bytes) noexcept {
	return std::ranges::all_of(bytes, [](char byte) { return (static_cast<unsigned char>(byte) & 0xC0) == 0x80; });
}

} // namespace

/// @brief Takes the text from output that arrives in pieces: a sequence split across two pieces waits for the rest,
/// and bytes that aren't UTF-8 become U+FFFD, so every event is valid JSON text.
/// @param carry Bytes left from the piece before; on return, the incomplete sequence at the end of this one.
/// @param bytes The piece.
/// @return The text.
[[nodiscard]] std::string takeTerminalText(std::string& carry, std::string_view bytes) {
	std::string input = carry + std::string(bytes), text;
	carry.clear();
	text.reserve(input.size());
	for(std::size_t at = 0; at < input.size();) {
		const std::size_t length = sequenceLength(static_cast<unsigned char>(input[at]));
		if(length != 0 && at + length > input.size() && continues(std::string_view(input).substr(at + 1))) {
			carry = input.substr(at); // the rest arrives with the next piece
			break;
		}
		if(length != 0 && at + length <= input.size() && isUtf8(std::string_view(input).substr(at, length))) {
			text.append(input, at, length);
			at += length;
			continue;
		}
		text += "\xEF\xBF\xBD";
		++at;
	}
	return text;
}

namespace {

/// @brief A terminal's size from a call's arguments.
/// @param with The arguments: {cols, rows}, each 1..1000 (the schema checked them).
/// @return The size; 80 by 24 where one is missing.
[[nodiscard]] TerminalSize sizeOf(const json& with) {
	const auto side = [&](const char* name, int fallback) {
		return static_cast<std::uint16_t>(std::clamp(with.value(name, fallback), 1, largestSide));
	};
	return {side("cols", 80), side("rows", 24)};
}

/// @brief One open terminal: its program, the call it streams to, and the threads that read it and write to it.
class Session {
public:
	/// @brief Starts the threads.
	/// @param id Its id, sent as the first event {session}.
	/// @param terminal The terminal.
	/// @param call The `open` call: its events {data}, and its result {code}.
	Session(std::uint64_t id, std::unique_ptr<PseudoTerminal> terminal, std::shared_ptr<Call> call)
	    : terminal_(std::move(terminal)), call_(std::move(call)) {
		writer_ = std::jthread([this](const std::stop_token& stop) { writeAll(stop); });
		reader_ = std::jthread([this, id](const std::stop_token& stop) { readAll(id, stop); });
	}
	/// @brief Stops both threads and ends the program (the call is cancelled), then waits for them.
	~Session() {
		reader_.request_stop(); // first, so the reader sees a stop, not an end, and the call is cancelled
		writer_.request_stop();
		(void)terminal_->end();
	}
	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;
	Session(Session&&) = delete;
	Session& operator=(Session&&) = delete;

	/// @brief Queues what is typed.
	/// @param bytes The bytes.
	/// @return false if over 1 MiB would be waiting.
	[[nodiscard]] bool type(std::string_view bytes) {
		{
			const std::scoped_lock guard(lock_);
			if(pending_.size() + bytes.size() > mostPending) return false;
			pending_ += bytes;
		}
		wake_.notify_one();
		return true;
	}
	/// @brief The terminal.
	[[nodiscard]] PseudoTerminal& terminal() noexcept { return *terminal_; }
	/// @brief Whether the program ended and its call finished.
	[[nodiscard]] bool done() const noexcept { return done_; }

private:
	/// @brief Writes what is typed, in order, until the session ends.
	/// @param stop Says it ends.
	void writeAll(const std::stop_token& stop) {
		while(true) {
			std::string bytes;
			{
				std::unique_lock guard(lock_);
				if(!wake_.wait(guard, stop, [this] { return !pending_.empty(); })) return;
				bytes.swap(pending_);
			}
			if(!terminal_->write(bytes)) return;
		}
	}
	/// @brief Streams the output as events {data}, then finishes the call with {code}.
	/// @param id The session's id.
	/// @param stop Says the session ends.
	void readAll(std::uint64_t id, const std::stop_token& stop) {
		PseudoTerminal& terminal = *terminal_;
		const EitherStop stopping(call_->stopToken(), stop, [&terminal] { (void)terminal.end(); });
		call_->event({{"session", id}});
		std::string carry;
		char buffer[largestRead];
		auto window = std::chrono::steady_clock::now();
		std::size_t streamed = 0;
		for(std::size_t count; (count = terminal.read(buffer, sizeof buffer)) > 0;) {
			if(std::string text = takeTerminalText(carry, std::string_view(buffer, count)); !text.empty())
				call_->event({{"data", std::move(text)}});
			streamed += count;
			if(streamed >= bytesPerSecond / 10) { // at most a tenth of the second's bytes each tenth of a second
				std::this_thread::sleep_until(window + std::chrono::milliseconds(100));
				window = std::chrono::steady_clock::now();
				streamed = 0;
			}
		}
		if(stopping.requested()) call_->fail("cancelled", "the terminal was closed");
		else call_->finish({{"code", terminal.wait()}});
		done_ = true;
	}

	std::unique_ptr<PseudoTerminal> terminal_; ///< See the constructor.
	std::shared_ptr<Call> call_;               ///< See the constructor.
	std::mutex lock_;                          ///< Guards pending_.
	std::condition_variable_any wake_;         ///< Says something was typed.
	std::string pending_;                      ///< Typed, not yet written.
	std::atomic<bool> done_{false};            ///< See done().
	std::jthread writer_;                      ///< Writes pending_.
	std::jthread reader_;                      ///< Streams the output (last, so it is joined first).
};

/// @brief One `pty` service: a program and the terminals it runs on. Main thread only.
class PtyService final : public Service {
public:
	/// @brief Serves a program.
	/// @param launch What each terminal starts.
	explicit PtyService(Launch launch) : launch_(std::move(launch)) {}

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		if(method.name == "open") return open(std::move(call));
		const json& with = call->arguments();
		const auto found = sessions_.find(with.value("session", std::uint64_t{0}));
		if(found == sessions_.end() || found->second->done()) return "with.session: no such terminal is open";
		Session& session = *found->second;
		if(method.name == "write") {
			if(!session.type(with.value("data", std::string()))) return "the terminal is busy (1 MiB not yet taken)";
		} else if(method.name == "resize") {
			session.terminal().resize(sizeOf(with));
		} else {
			session.terminal().signal(signalNamed(with.value("name", std::string())));
		}
		call->finish(json::object());
		return std::nullopt;
	}

private:
	/// @brief Opens a terminal for an `open` call.
	/// @param call The call.
	/// @return A reason it was refused, or std::nullopt.
	std::optional<std::string> open(std::shared_ptr<Call> call) {
		std::erase_if(sessions_, [](const auto& entry) { return entry.second->done(); });
		if(sessions_.size() == mostSessions) return "the service is busy (8 terminals at once)";
		std::unique_ptr<PseudoTerminal> terminal;
		try {
			terminal = PseudoTerminal::start(launch_, sizeOf(call->arguments()));
		} catch(const std::runtime_error& error) {
			call->fail("failed", error.what());
			return std::nullopt;
		}
		const std::uint64_t id = ++opened_;
		sessions_.emplace(id, std::make_unique<Session>(id, std::move(terminal), std::move(call)));
		return std::nullopt;
	}
	/// @brief The signal a name asks for (the schema allows only these four).
	/// @param name "interrupt", "terminate", "hangup" or "kill".
	/// @return The signal.
	[[nodiscard]] static TerminalSignal signalNamed(std::string_view name) noexcept {
		if(name == "terminate") return TerminalSignal::Terminate;
		if(name == "hangup") return TerminalSignal::Hangup;
		if(name == "kill") return TerminalSignal::Kill;
		return TerminalSignal::Interrupt;
	}

	Launch launch_;                                              ///< See the constructor.
	std::uint64_t opened_ = 0;                                   ///< Terminals opened so far, to number them.
	std::map<std::uint64_t, std::unique_ptr<Session>> sessions_; ///< The open terminals (last, so they end first).
};

/// @brief The schema of a method's arguments: an object with these properties, all required.
/// @param properties The properties.
/// @return The schema.
[[nodiscard]] json argumentsOf(json properties) {
	json required = json::array();
	for(const auto& [name, schema] : properties.items()) required.push_back(name);
	return {{"type", "object"},
	        {"required", required},
	        {"properties", std::move(properties)},
	        {"additionalProperties", false}};
}

/// @brief The `pty` provider.
class PtyProvider final : public Provider {
public:
	std::string_view name() const override { return "pty"; }
	std::string_view about() const override { return "a program the host allows, on a terminal of its own"; }
	json configurationSchema() const override {
		return {{"type", "object"},
		        {"required", {"process"}},
		        {"additionalProperties", false},
		        {"properties", {{"process", {{"type", "string"}}}, {"root", {{"type", "string"}}}}}};
	}
	std::vector<MethodSpec> methods(const json&) const override {
		const json side = {{"type", "integer"}, {"minimum", 1}, {"maximum", largestSide}};
		const json session = {{"type", "integer"}, {"minimum", 1}};
		const json names = {{"enum", {"interrupt", "terminate", "hangup", "kill"}}};
		return {{"open", Effect::Run, true, false, argumentsOf({{"cols", side}, {"rows", side}}),
		         "starts the program on a terminal (in the root, if one is named): a first event {session}, then its "
		         "output as events {data}; {code}"},
		        {"write", Effect::Run, true, false,
		         argumentsOf({{"session", session}, {"data", {{"type", "string"}, {"maxLength", largestWrite}}}}),
		         "types into an open terminal: {}"},
		        {"resize", Effect::Run, true, false,
		         argumentsOf({{"session", session}, {"cols", side}, {"rows", side}}), "resizes an open terminal: {}"},
		        {"signal", Effect::Run, true, false, argumentsOf({{"session", session}, {"name", names}}),
		         "signals the program on an open terminal: {}"}};
	}
	std::unique_ptr<Service> create(const json&, ServiceContext context) override {
		const ProcessGrant& grant = *context.grant.process;
		return std::make_unique<PtyService>(
		    Launch{grant.program, grant.arguments, context.grant.root, grant.environment});
	}
};

} // namespace

std::unique_ptr<Provider> makePtyProvider() { return std::make_unique<PtyProvider>(); }

} // namespace affresco::services
