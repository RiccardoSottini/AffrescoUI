/// @file
/// @brief `affrescoui test`: plays a screen's scenarios (modules/automation) and reports pass and fail compactly.

#include "Cli.hpp"

#include <affresco/automation/Scenarios.hpp>
#include <affresco/core/Log.hpp>

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
	return options;
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
			report = automation::run(automation::ScenarioFile::load(file), options);
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
