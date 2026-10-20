/// @file
/// @brief Fuzzes scenarios files (modules/automation): an agent can write one and have it played, so reading it and
/// playing its steps headless on the Kanban template must never crash, whatever it asks for.
///
/// The input is a scenarios file. Reading it gives scenarios or throws a DocumentError; playing them gives a report,
/// every failure with a problem to show. Pictures are off (no OpenGL here), so a scenario that draws fails to start.

#include "Fuzz.hpp"

#include <affresco/automation/Scenarios.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

/// @brief The Kanban template's text, which every scenario plays on.
/// @return It.
const std::string& kanbanText() {
	static const std::string text = affresco::fuzz::kanbanTemplate().dump();
	return text;
}

} // namespace

/// @brief libFuzzer's set-up: a quiet log.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace automation = affresco::automation;
	automation::ScenarioFile file;
	try {
		// Next to the template, so its fonts are found
		file = automation::ScenarioFile::parse(affresco::fuzz::text(data, size),
		                                       std::filesystem::path(AFFRESCOUI_FUZZ_TEMPLATE).parent_path() /
		                                           "fuzz.scenarios.json");
	} catch(const affresco::DocumentError&) {
		return 0;
	}
	automation::Options options;
	options.pictures = false;
	options.document = kanbanText();
	const automation::Report report = automation::run(file, options);
	if(report.scenarios.size() != file.scenarios.size()) affresco::fuzz::broken("every scenario is reported");
	for(const automation::ScenarioResult& scenario : report.scenarios)
		for(const automation::Failure& failure : scenario.failures)
			if(failure.problem.empty()) affresco::fuzz::broken("every failure says what went wrong");
	static_cast<void>(report.toJson().dump()); // the report can always be written
	return 0;
}
