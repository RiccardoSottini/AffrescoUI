/// @file
/// @brief The rendering check's runner: plays scenario files with the automation module (affresco::automation) and
/// prints one line per picture, then the totals, as check.py has always printed them.
///
/// Usage: affrescoui_render_runner [--goldens DIR] [--out DIR] [--update] [--only NAME] FILE...
///
/// Each file is one area's (scenarios/kanban.json, ...), or a template's own scenarios (examples/assets/screens/
/// *.scenarios.json). A picture with no golden image yet is recorded from this run; --update records them all.
/// Frames that differ are written to --out with a diff (changed pixels in red). Exits with 1 if anything failed.

#include "CodeEditor.hpp"

#include <affresco/Screen.hpp>
#include <affresco/automation/Scenarios.hpp>

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace automation = affresco::automation;

/// @brief Prints a report the way check.py always has: "same", "golden" or "FAILED" per picture, and the failures of
/// the other steps.
/// @param report The report of one file.
/// @param frames Counts the pictures compared or recorded.
/// @return How many failures it has.
std::size_t print(const automation::Report& report, std::size_t& frames) {
	std::size_t failed = 0;
	for(const automation::ScenarioResult& scenario : report.scenarios) {
		for(const automation::Picture& picture : scenario.pictures) {
			++frames;
			if(picture.outcome == automation::Picture::Outcome::Same) std::cout << "same    " << picture.name << '\n';
			else if(picture.outcome == automation::Picture::Outcome::Recorded) std::cout << "golden  " << picture.name << '\n';
		}
		for(const automation::Failure& failure : scenario.failures) {
			++failed;
			std::cout << "FAILED  " << scenario.name << (failure.step ? ", step " + std::to_string(failure.step) : "")
			          << ": " << failure.problem << '\n';
		}
	}
	return failed;
}

} // namespace

int main(int argc, char** argv) {
	automation::Options options;
	options.policy = automation::Goldens::RecordMissing;
	options.prepare = [](affresco::Screen& screen) { examples::registerCodeEditor(screen); }; // as affrescoui_host
	std::vector<std::string> files;
	const std::vector<std::string_view> args(argv + 1, argv + argc);
	for(std::size_t i = 0; i < args.size(); ++i) {
		const bool valued = args[i] == "--goldens" || args[i] == "--out" || args[i] == "--only";
		if(valued && i + 1 == args.size()) {
			std::cerr << "affrescoui_render_runner: " << args[i] << " needs a value\n";
			return 2;
		}
		if(args[i] == "--goldens") options.goldens = args[++i];
		else if(args[i] == "--out") options.output = args[++i];
		else if(args[i] == "--only") options.only.emplace_back(args[++i]);
		else if(args[i] == "--update") options.policy = automation::Goldens::Update;
		else files.emplace_back(args[i]);
	}
	if(files.empty()) {
		std::cerr << "usage: affrescoui_render_runner [--goldens DIR] [--out DIR] [--update] [--only NAME] FILE...\n";
		return 2;
	}

	std::size_t frames = 0, failed = 0;
	std::set<std::string, std::less<>> names; // a name is the prefix of its golden images: unique across the files
	try {
		for(const std::string& file : files) {
			const automation::ScenarioFile scenarios = automation::ScenarioFile::load(file);
			for(const automation::Scenario& scenario : scenarios.scenarios)
				if(!names.insert(scenario.name).second)
					throw affresco::DocumentError("scenario '" + scenario.name + "' is in two files; the second is " + file);
			failed += print(automation::run(scenarios, options), frames);
		}
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_render_runner: " << error.what() << '\n';
		return EXIT_FAILURE;
	}
	std::cout << frames << " frames, " << failed << " failed\n";
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
