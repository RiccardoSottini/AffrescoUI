/// @file
/// @brief affrescoui_bottega: Bottega's demo, Sinopia's slide sorter installed into Tempera, with Tempera's natives.
//
//   affrescoui_bottega [FILE...]                         plays the scenarios (bottega.scenarios.json)
//   affrescoui_bottega check --compose SCREEN FEATURE …  affrescoui's check --compose, Tempera's scenarios included
//   affrescoui_bottega contributions SCREEN [--json]     affrescoui's contributions
//
// In the scenarios, the action "installSlideSorter" proves the sorter against the live document with
// tempera-sorter.adapter.json and submits the install as the "features" service would; "installSlideSorterBare"
// tries it with no adapter and "installSlideSorterBadly" with broken-sorter.adapter.json: both proofs fail, so
// nothing is submitted.

#include "Cli.hpp"
#include "CompositionProof.hpp"
#include "CompositionProofRevision.hpp"
#include "FeatureSignatures.hpp"
#include "tempera/Tempera.hpp"

#include <affresco/Screen.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/core/Json.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace automation = affresco::automation;
namespace cli = affresco::cli;
namespace format = affresco::format;
using affresco::json;

/// @brief The folder of Bottega's files.
/// @return It.
std::filesystem::path bottegaDirectory() { return AFFRESCOUI_BOTTEGA_DIRECTORY; }

/// @brief The slide sorter's manifest file.
/// @return It.
std::filesystem::path sorterManifest() {
	return bottegaDirectory().parent_path() / "sinopia" / "features" / "slide-sorter" / "feature.json";
}

/// @brief Proves the slide sorter on the screen's live document and, if the proof passes, submits the install.
/// @param screen The host screen.
/// @param adapter The adapter file in Bottega's folder, or empty for none.
void installSorter(affresco::Screen& screen, const std::string& adapter) {
	format::ProofInputs inputs;
	inputs.host = json::parse(screen.documentJson());
	inputs.manifest = affresco::parseUntrusted(cli::readFile(sorterManifest()));
	if(!adapter.empty()) inputs.adapter = affresco::parseUntrusted(cli::readFile(bottegaDirectory() / adapter));
	inputs.source = "app:sinopia";
	inputs.hostName = "app:tempera";
	inputs.digest = format::digestOf(cli::readFile(sorterManifest()));
	// the scenarios played here are the host's own: the proof's runner plays none
	const format::ProvenComposition proven = format::proveComposition(
	    inputs, [](const json&, const json&) { return std::vector<format::ScenarioOutcome>{}; });
	for(const std::string& line : format::proofReasons(proven.proof)) std::cout << "  " << line << '\n';
	if(proven.proof.value("passed", false) != true) return;
	const affresco::RevisionResult result = screen.submitRevision(
	    affresco::vcs::compositionRevision("slideSorter", proven.record, "features", inputs.host).dump());
	if(!result.error.empty()) std::cerr << "install: " << result.error << '\n';
}

/// @brief Prepares a screen as affrescoui_tempera_scenarios does, plus the install actions.
/// @param screen The screen.
/// @param session Receives this screen's session (the one before it is released).
void prepare(affresco::Screen& screen, std::shared_ptr<tempera::Session>& session) {
	session = std::make_shared<tempera::Session>(affresco::imaging::Limits{});
	tempera::registerTempera(screen, session, nullptr);
	screen.onAction("agentProposesFilterPanel", [&screen](affresco::Panel&) { // as affrescoui_tempera_scenarios
		const affresco::RevisionResult result = screen.submitRevision(
		    cli::readFile(std::filesystem::path(AFFRESCOUI_TEMPERA_DIRECTORY) / "agent-filter-panel.revision.json"));
		if(!result.error.empty()) std::cerr << "agent's revision: " << result.error << '\n';
	});
	screen.onAction("installSlideSorter",
	                [&screen](affresco::Panel&) { installSorter(screen, "tempera-sorter.adapter.json"); });
	screen.onAction("installSlideSorterBare", [&screen](affresco::Panel&) { installSorter(screen, ""); });
	screen.onAction("installSlideSorterBadly",
	                [&screen](affresco::Panel&) { installSorter(screen, "broken-sorter.adapter.json"); });
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

/// @brief Runs one of affrescoui's commands with Tempera's natives registered on every screen it plays.
/// @param args The command line after the program's name.
/// @return The command's exit status.
int runCommand(std::span<const std::string_view> args) {
	std::shared_ptr<tempera::Session> session;
	const auto natives = [&session](affresco::Screen& screen) { prepare(screen, session); };
	const cli::Exit exit = args.front() == "contributions" ? cli::listContributions(args.subspan(1))
	                                                       : cli::checkComposition(args.subspan(1), natives);
	return static_cast<int>(exit);
}

} // namespace

/// @brief Plays Bottega's scenarios, or runs check --compose or contributions with Tempera's natives.
/// @param argc The number of arguments.
/// @param argv The command, or the scenario files; none: bottega.scenarios.json.
/// @return 0 on success, 1 for failures or problems, 2 for a usage error, 3 when something can't be read.
int main(int argc, char** argv) {
	const std::vector<std::string_view> args(argv + 1, argv + argc);
	try {
		if(!args.empty() && (args.front() == "check" || args.front() == "contributions")) return runCommand(args);
		std::vector<std::filesystem::path> files(args.begin(), args.end());
		if(files.empty()) files.push_back(bottegaDirectory() / "bottega.scenarios.json");
		return playAll(files) ? EXIT_FAILURE : EXIT_SUCCESS;
	} catch(const cli::UsageError& error) {
		std::cerr << "affrescoui_bottega: " << error.what() << '\n';
		return static_cast<int>(cli::Exit::Usage);
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_bottega: " << error.what() << '\n';
	}
	return static_cast<int>(cli::Exit::Failure);
}
