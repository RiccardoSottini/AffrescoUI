/// @file
/// @brief affrescoui_tempera_scenarios: plays Tempera's scenarios (tempera.scenarios.json) with its natives, headless.
//
//   affrescoui_tempera_scenarios [FILE...]
//
// Each scenario's screen gets a session of its own with the sample document. The action "agentProposesFilterPanel"
// submits agent-filter-panel.revision.json as the agent would through agent.sock, so a scenario can review it.

#include "tempera/Tempera.hpp"

#include <affresco/Screen.hpp>
#include <affresco/automation/Scenarios.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace automation = affresco::automation;

/// @brief The folder of Tempera's files.
/// @return It.
std::filesystem::path temperaDirectory() { return AFFRESCOUI_TEMPERA_DIRECTORY; }

/// @brief Reads a whole text file.
/// @param path The file.
/// @return Its text.
/// @throws affresco::DocumentError If it can't be read.
std::string readText(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	if(!in) throw affresco::DocumentError("can't read " + path.string());
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

/// @brief Prepares a screen as affrescoui_tempera does, with no window and no executor, plus the agent's proposal.
/// @param screen The screen.
/// @param session Receives this screen's session (the one before it is released).
void prepare(affresco::Screen& screen, std::shared_ptr<tempera::Session>& session) {
	session = std::make_shared<tempera::Session>(affresco::imaging::Limits{});
	tempera::registerTempera(screen, session, nullptr);
	screen.onAction("agentProposesFilterPanel", [&screen](affresco::Panel&) {
		const affresco::RevisionResult result =
		    screen.submitRevision(readText(temperaDirectory() / "agent-filter-panel.revision.json"));
		if(!result.error.empty()) std::cerr << "agent's revision: " << result.error << '\n';
	});
}

/// @brief Plays the files and prints each failure.
/// @param files The scenario files.
/// @return How many failures there were.
/// @throws affresco::DocumentError If a file is malformed.
std::size_t playAll(const std::vector<std::filesystem::path>& files) {
	std::shared_ptr<tempera::Session> session; // the session of the screen playing
	automation::Options options;
	options.pictures = false;
	options.prepare = [&session](affresco::Screen& screen) { prepare(screen, session); };
	std::size_t failed = 0, played = 0;
	for(const std::filesystem::path& file : files) {
		const automation::Report report = automation::run(automation::ScenarioFile::load(file.string()), options);
		for(const automation::ScenarioResult& scenario : report.scenarios) {
			++played;
			for(const automation::Failure& failure : scenario.failures) {
				++failed;
				std::cout << "FAILED  " << scenario.name << ", step " << failure.step << ": " << failure.problem
				          << '\n';
			}
		}
	}
	std::cout << played << " scenarios, " << failed << " failed\n";
	return failed;
}

} // namespace

/// @brief Plays Tempera's scenarios.
/// @param argc The number of arguments.
/// @param argv The scenario files; none: tempera.scenarios.json.
/// @return 0 if every scenario passed, 1 otherwise.
int main(int argc, char** argv) {
	try {
		std::vector<std::filesystem::path> files(argv + 1, argv + argc);
		if(files.empty()) files.push_back(temperaDirectory() / "tempera.scenarios.json");
		return playAll(files) ? EXIT_FAILURE : EXIT_SUCCESS;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_tempera_scenarios: " << error.what() << '\n';
	}
	return EXIT_FAILURE;
}
