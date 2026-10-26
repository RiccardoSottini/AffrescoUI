/// @file
/// @brief `affrescoui test`: plays a screen's scenarios (modules/automation) and reports pass and fail compactly.

#include "Cli.hpp"

#include <affresco/automation/Scenarios.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/core/Portable.hpp>

#include <algorithm>
#include <array>
#include <iostream>

namespace affresco::cli {

namespace {

/// @brief Reads the options of a run from the command line.
/// @param arguments The command line.
/// @return The options.
/// @throws UsageError If --document is given with more than one file.
/// @throws std::runtime_error If the document or the output folder can't be read or made.
automation::Options optionsOf(const Arguments& arguments) {
	automation::Options options;
	options.prepare = registerApplicationTypes; // as affrescoui_launcher: the example apps' types, other types as panels
	if(const std::optional<std::string> goldens = arguments.value("--goldens")) options.goldens = *goldens;
	if(arguments.flag("--update")) options.policy = automation::Goldens::Update;
	if(const std::optional<std::string> only = arguments.value("--only")) options.only.push_back(*only);
	if(const std::optional<std::string> out = arguments.value("--out")) {
		options.output = *out;
		std::filesystem::create_directories(options.output);
	}
	if(const std::optional<std::string> document = arguments.value("--document")) {
		if(arguments.positional().size() != 1) throw UsageError("--document plays one scenarios file");
		options.document = readFile(*document);
	}
#ifdef __EMSCRIPTEN__
	options.pictures = false; // WebAssembly under Node: no OpenGL, so the scenarios that draw are left out
#else
	// A machine without OpenGL 4.1 (the Windows CI runners) says so as the rendering check's runner reads it
	if(environmentVariable("AFFRESCOUI_NO_OPENGL") == "1") options.pictures = false;
#endif
	return options;
}

/// @brief Plays a scenarios file. A run with no OpenGL leaves out the scenarios that draw (as affrescoui_launcher's
/// scenario check does), names how many in a line, and plays the rest, rather than failing those for want of it.
/// @param file The scenarios file.
/// @param options The run's options.
/// @param json Whether the report is JSON (the line about the scenarios left out is then not written).
/// @return The report.
/// @throws DocumentError If the scenarios file is malformed.
automation::Report playScenarios(const std::string& file, const automation::Options& options, bool json) {
	const automation::ScenarioFile loaded = automation::ScenarioFile::load(file);
	if(options.pictures) return automation::run(loaded, options);
	automation::Options drawless = options;
	drawless.only.clear();
	std::size_t left = 0;
	for(const automation::Scenario& scenario : loaded.scenarios) {
		if(!options.only.empty() && std::ranges::find(options.only, scenario.name) == options.only.end()) continue;
		if(scenario.needsPictures) ++left;
		else drawless.only.push_back(scenario.name);
	}
	if(left > 0 && !json)
		std::cout << loaded.path.filename().string() << ": " << left
		          << " left out: they draw pictures, and this run has no OpenGL\n";
	if(drawless.only.empty()) return {loaded.path, {}}; // all of them draw: an empty `only` would play every one
	return automation::run(loaded, drawless);
}

} // namespace

Exit testScreen(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 4> valued = {"--only", "--goldens", "--out", "--document"};
	static constexpr std::array<std::string_view, 3> flags = {"--json", "--update", "--verbose"};
	const Arguments arguments(args, valued, flags);
	if(arguments.positional().empty()) throw UsageError("expected one scenarios file or more");
	const automation::Options options = optionsOf(arguments);

	/// @brief Silences the library's log for the command's time (a channel that finds no host, the checker's
	/// warnings: noise next to the report), unless --verbose.
	struct QuietLog {
		/// @brief Silences the log.
		/// @param quiet false to leave it as it is.
		explicit QuietLog(bool quiet) {
			if(quiet) setLogSink([](LogLevel, std::string_view) {});
		}
		/// @brief Puts the default log back.
		~QuietLog() { setLogSink({}); }
		QuietLog(const QuietLog&) = delete;
		QuietLog& operator=(const QuietLog&) = delete;
		QuietLog(QuietLog&&) = delete;
		QuietLog& operator=(QuietLog&&) = delete;
	} const quiet(!arguments.flag("--verbose"));

	bool passed = true;
	for(const std::string& file : arguments.positional()) {
		automation::Report report;
		try {
			report = playScenarios(file, options, arguments.flag("--json"));
		} catch(const DocumentError& error) { // the scenarios file itself is malformed
			throw FileProblem(error.what());
		}
		passed = passed && report.failed() == 0;
		if(arguments.flag("--json")) std::cout << report.toJson().dump() << '\n'; // one line per file
		else std::cout << report.text();
	}
	return passed ? Exit::Success : Exit::Problems;
}

} // namespace affresco::cli
