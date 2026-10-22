/// @file
/// @brief The `process` provider: a granted program run once per call, its input written, its output lines streamed.

#include "process/Process.hpp"
#include "Providers.hpp"

#include <stdexcept>
#include <variant>

namespace affresco::services {

namespace {

constexpr std::size_t mostAtOnce = 16;                   ///< Runs at once, per service.
constexpr std::size_t longestLine = std::size_t{64} << 10; ///< A longer line arrives in pieces of 64 KiB.
constexpr std::size_t largestInput = std::size_t{1} << 20; ///< `with.input`, at most 1 MiB.
constexpr std::size_t largestOutput = std::size_t{8} << 20; ///< What a run with `collect` keeps, at most 8 MiB.
constexpr std::size_t mostArguments = 64;                  ///< `with.arguments`, at most 64.
constexpr std::size_t longestArgument = 4096;              ///< Each of them, at most 4 KiB.

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

/// @brief Keeps a run's whole output for its result (`collect`), up to largestOutput.
class Collected {
public:
	/// @brief Adds output.
	/// @param bytes What the program wrote next.
	/// @return Whether it still fits.
	[[nodiscard]] bool add(std::string_view bytes) {
		if(output_.size() + bytes.size() > largestOutput) return false;
		output_.append(bytes);
		return true;
	}
	/// @brief The output so far.
	/// @return It.
	[[nodiscard]] const std::string& output() const noexcept { return output_; }

private:
	std::string output_; ///< The output so far.
};

/// @brief Reads a run's output until the program closes it: as line events, or collected. A run that says too much
/// to collect is ended, and the rest of its output read and dropped.
/// @param running The program.
/// @param collect Whether the output is collected rather than sent as lines.
/// @param lines Gets the lines.
/// @param collected Gets the output.
/// @return Whether the collected output fits.
[[nodiscard]] bool readOutput(ChildProcess& running, bool collect, LineEvents& lines, Collected& collected) {
	bool fits = true;
	char buffer[16384];
	for(std::ptrdiff_t count; (count = running.read(buffer, sizeof buffer)) > 0;) {
		const std::string_view bytes(buffer, static_cast<std::size_t>(count));
		if(!collect) lines.add(bytes);
		else if(fits && !collected.add(bytes)) {
			fits = false;
			(void)running.end();
		}
	}
	return fits;
}

/// @brief Runs the program once for a call: on its worker thread; the call or the service stopping ends it.
/// @param launch What to run.
/// @param stop The worker's stop token.
/// @param call The call: its input, and its events and its result {code}, or its result {code, output} if it collects.
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
	const bool collect = call.arguments().is_object() && call.arguments().value("collect", false);
	LineEvents lines(call);
	Collected collected;
	const bool fits = readOutput(running, collect, lines, collected);
	writer.join();
	if(!fits) return call.fail("tooLarge", "the program wrote more than 8 MiB");
	if(stopping.requested()) return call.fail("cancelled", "the run was stopped");
	if(!collect) {
		lines.finish();
		return call.finish({{"code", running.wait()}});
	}
	if(!isUtf8(collected.output())) return call.fail("failed", "the program's output is not UTF-8 text");
	call.finish({{"code", running.wait()}, {"output", collected.output()}});
}

/// @brief Builds one call's run: the grant's launch, with the arguments the call adds as the host's shaper allows.
/// @param grant The grant: its shaper.
/// @param launch The grant's launch.
/// @param with The call's `with`.
/// @return The run's launch, or why it is refused.
[[nodiscard]] std::variant<Launch, std::string> shapeRun(const ProcessGrant& grant, Launch launch, const json& with) {
	const bool adds = with.is_object() && with.contains("arguments");
	if(!grant.shape) {
		if(adds) return std::string("this program takes no arguments from a call");
		return launch;
	}
	ProcessRun run{adds ? with.at("arguments").get<std::vector<std::string>>() : std::vector<std::string>{},
	               launch.directory};
	if(std::optional<std::string> refused = grant.shape(run)) return std::move(*refused);
	launch.arguments.insert(launch.arguments.end(), run.arguments.begin(), run.arguments.end());
	launch.directory = std::move(run.directory);
	return launch;
}

/// @brief One `process` service: a program and its runs.
class ProcessService final : public Service {
public:
	/// @brief Serves a program.
	/// @param grant The host's grant: its shaper.
	/// @param launch What each run starts, before the call's own arguments.
	ProcessService(ProcessGrant grant, Launch launch) : grant_(std::move(grant)), launch_(std::move(launch)) {}

	std::optional<std::string> invoke(const MethodSpec& /*method*/, std::shared_ptr<Call> call) override {
		std::variant<Launch, std::string> shaped = shapeRun(grant_, launch_, call->arguments());
		if(const std::string* refused = std::get_if<std::string>(&shaped)) return *refused;
		const bool started = tasks_.run([launch = std::get<Launch>(std::move(shaped)), call](const std::stop_token& stop) {
			runOnce(launch, stop, *call);
		});
		return started ? std::nullopt : std::optional<std::string>("the service is busy (16 runs at once)");
	}

private:
	ProcessGrant grant_;      ///< See the constructor.
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
	std::vector<MethodSpec> methods(const json& /*configuration*/) const override {
		return {{"run", Effect::Run, true, false,
		         {{"type", "object"},
		          {"properties",
		           {{"input", {{"type", "string"}, {"maxLength", largestInput}}},
		            {"arguments",
		             {{"type", "array"},
		              {"maxItems", mostArguments},
		              {"items", {{"type", "string"}, {"maxLength", longestArgument}}}}},
		            {"collect", {{"type", "boolean"}}}}},
		          {"additionalProperties", false}},
		         "runs the program (in the root, if one is named) with an input, and the arguments the host's rule allows: "
		         "each output line is an event {line}; {code}, or {code, output} with collect (8 MiB of UTF-8 at most)"}};
	}
	std::unique_ptr<Service> create(const json& /*configuration*/, ServiceContext context) override {
		const ProcessGrant& grant = *context.grant.process;
		return std::make_unique<ProcessService>(
		    grant, Launch{grant.program, grant.arguments, context.grant.root, grant.environment});
	}
};

} // namespace

std::unique_ptr<Provider> makeProcessProvider() { return std::make_unique<ProcessProvider>(); }

} // namespace affresco::services
