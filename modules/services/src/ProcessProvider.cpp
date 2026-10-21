/// @file
/// @brief The `process` provider: a granted program run once per call, its input written, its output lines streamed.

#include "Process.hpp"
#include "Providers.hpp"

#include <stdexcept>

namespace affresco::services {

namespace {

constexpr std::size_t mostAtOnce = 16;                   ///< Runs at once, per service.
constexpr std::size_t longestLine = std::size_t{64} << 10; ///< A longer line arrives in pieces of 64 KiB.
constexpr std::size_t largestInput = std::size_t{1} << 20; ///< `with.input`, at most 1 MiB.

/// @brief Turns output into lines, each one event {line}; a piece over longestLine is sent as it is.
class LineEvents {
public:
	/// @brief Sends to a call.
	/// @param call The call.
	explicit LineEvents(Call& call) noexcept : call_(call) {}

	/// @brief Adds output.
	/// @param bytes What the program wrote next.
	void add(std::string_view bytes) {
		for(const char character : bytes) {
			if(character == '\n') {
				flush();
				continue;
			}
			line_ += character;
			if(line_.size() == longestLine) flush();
		}
	}
	/// @brief Sends what is left (a last line without a line break).
	void finish() {
		if(!line_.empty()) flush();
	}

private:
	/// @brief Sends the line (without a `\r` at its end) and starts the next.
	void flush() {
		if(!line_.empty() && line_.back() == '\r') line_.pop_back();
		call_.event({{"line", isUtf8(line_) ? line_ : std::string("(not UTF-8 text)")}});
		line_.clear();
	}

	Call& call_;       ///< See the constructor.
	std::string line_; ///< The line so far.
};

/// @brief Runs the program once for a call: on its worker thread; the call or the service stopping ends it.
/// @param launch What to run.
/// @param stop The worker's stop token.
/// @param call The call: its input, its events and its result {code}.
void runOnce(const Launch& launch, const std::stop_token& stop, Call& call) {
	std::unique_ptr<ChildProcess> child;
	try {
		child = ChildProcess::start(launch);
	} catch(const std::runtime_error& error) {
		return call.fail("failed", error.what());
	}
	ChildProcess& running = *child;
	const EitherStop stopping(call.stopToken(), stop, [&running] { (void)running.end(); });
	const std::string input = call.arguments().is_object() ? call.arguments().value("input", std::string()) : "";
	std::jthread writer([&running, &input] {
		if(!input.empty()) (void)running.write(input);
		running.closeInput();
	});
	LineEvents lines(call);
	char buffer[16384];
	for(std::ptrdiff_t count; (count = running.read(buffer, sizeof buffer)) > 0;)
		lines.add(std::string_view(buffer, static_cast<std::size_t>(count)));
	writer.join();
	if(stopping.requested()) return call.fail("cancelled", "the run was stopped");
	lines.finish();
	call.finish({{"code", running.wait()}});
}

/// @brief One `process` service: a program and its runs.
class ProcessService final : public Service {
public:
	/// @brief Serves a program.
	/// @param launch What each run starts.
	explicit ProcessService(Launch launch) : launch_(std::move(launch)) {}

	std::optional<std::string> invoke(const MethodSpec&, std::shared_ptr<Call> call) override {
		const bool started = tasks_.run(
		    [launch = launch_, call](const std::stop_token& stop) { runOnce(launch, stop, *call); });
		return started ? std::nullopt : std::optional<std::string>("the service is busy (16 runs at once)");
	}

private:
	Launch launch_;           ///< See the constructor.
	Tasks tasks_{mostAtOnce}; ///< The runs (last, so they end first).
};

/// @brief The `process` provider.
class ProcessProvider final : public Provider {
public:
	std::string_view name() const override { return "process"; }
	std::string_view about() const override { return "a program the host allows, run once per call"; }
	json configurationSchema() const override {
		return {{"type", "object"},
		        {"required", {"process"}},
		        {"additionalProperties", false},
		        {"properties", {{"process", {{"type", "string"}}}, {"root", {{"type", "string"}}}}}};
	}
	std::vector<MethodSpec> methods(const json&) const override {
		return {{"run", Effect::Run, true, false,
		         {{"type", "object"},
		          {"properties", {{"input", {{"type", "string"}, {"maxLength", largestInput}}}}},
		          {"additionalProperties", false}},
		         "runs the program (in the root, if one is named) with an input: each output line is an event {line}; "
		         "{code}"}};
	}
	std::unique_ptr<Service> create(const json&, ServiceContext context) override {
		const ProcessGrant& grant = *context.grant.process;
		return std::make_unique<ProcessService>(Launch{grant.program, grant.arguments, context.grant.root, grant.environment});
	}
};

} // namespace

std::unique_ptr<Provider> makeProcessProvider() { return std::make_unique<ProcessProvider>(); }

} // namespace affresco::services
