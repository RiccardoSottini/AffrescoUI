/// @file
/// @brief Modello's workspace, headless: every scenario of examples/modello/modello.scenarios.json, played with
/// Modello's natives registered, and an agent that proposes a workspace panel through review.

#include "HistoryFiles.hpp" // TemporaryFolder

#include "Modello.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

using affresco::json;
using affresco::test::TemporaryFolder;
namespace automation = affresco::automation;

namespace {

/// @brief Modello's folder in the source tree.
/// @return It.
std::filesystem::path modelloFolder() { return std::filesystem::path(AFFRESCOUI_MODELLO_SAMPLES).parent_path(); }

/// @brief The panel the agent proposes: a "Notes" column under the properties.
/// @return Its revision, as the agent would send it.
json agentPanelRevision() {
	const json panel = {{"id", "agent-notes"},
	                    {"type", "label"},
	                    {"font", "small"},
	                    {"width", "100%"},
	                    {"height", 22},
	                    {"text", "Notes: {{len(computed.scene.nodes)}} objects in the scene"}};
	return {{"author", "agent"},
	        {"message", "Add a notes panel to the properties"},
	        {"add", {{"#properties/children/-", panel}}}};
}

} // namespace

TEST_CASE("scene3d modello: the workspace's scenarios pass headless with Modello's natives") {
	const TemporaryFolder data;
	automation::Options options;
	options.pictures = false;
	options.prepare = [&data](affresco::Screen& screen) {
		modello::registerModello(screen, std::make_shared<modello::Model>(modelloFolder() / "samples", data.path()));
		// The agent's side of "modello-agent-panel": a revision that waits for review, as one sent on agent.sock would
		screen.onAction("testAgentProposesPanel",
		                [&screen](affresco::Panel&) { (void)screen.submitRevision(agentPanelRevision().dump()); });
	};
	const automation::Report report =
	    automation::run(automation::ScenarioFile::load(modelloFolder() / "modello.scenarios.json"), options);
	CHECK(report.scenarios.size() >= 25);
	for(const automation::ScenarioResult& scenario : report.scenarios) {
		std::string problems = scenario.name;
		for(const automation::Failure& failure : scenario.failures)
			problems += "\n  step " + std::to_string(failure.step) + ": " + failure.problem;
		CHECK_MESSAGE(scenario.passed(), problems);
	}
}
