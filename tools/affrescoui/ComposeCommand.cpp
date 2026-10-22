/// @file
/// @brief `affrescoui contributions` and `affrescoui check --compose`: what an app offers and can install, and the
/// proof that a feature from another app fits it (CompositionProof.hpp), its scenarios and the host's played on the
/// composed document.

#include "Cli.hpp"
#include "CompositionProof.hpp"
#include "CompositionProofRevision.hpp"
#include "ContributionPoints.hpp"
#include "FeatureManifest.hpp"
#include "FeatureSignatures.hpp"

#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/core/Log.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <iostream>

namespace affresco::cli {

namespace {

/// @brief Silences the library's log for its lifetime: the checker's warnings and the scenarios' noise would bury
/// the proof.
class QuietLog {
public:
	/// @brief Silences the log.
	QuietLog() { setLogSink([](LogLevel, std::string_view) {}); }
	/// @brief Puts the default log back.
	~QuietLog() { setLogSink({}); }
	QuietLog(const QuietLog&) = delete;
	QuietLog& operator=(const QuietLog&) = delete;
	QuietLog(QuietLog&&) = delete;
	QuietLog& operator=(QuietLog&&) = delete;
};

/// @brief Loads a screen as the library does: its document with the "include"s in place.
/// @param path The screen file: its folder holds what it includes.
/// @param text Its text, or another document to load in its place (the live one, with --document).
/// @return The document.
/// @throws FileProblem If it doesn't load.
json loadDocument(const std::filesystem::path& path, const std::string& text) {
	const QuietLog quiet;
	try {
		return json::parse(Screen::fromString(text, path.parent_path()).documentJson());
	} catch(const DocumentError& error) {
		throw FileProblem(path.string() + ": " + error.what());
	}
}

/// @brief Reads an untrusted JSON file: a manifest or an adapter.
/// @param path The file.
/// @return Its value.
/// @throws FileProblem If it isn't JSON.
json readUntrusted(const std::filesystem::path& path) {
	try {
		return parseUntrusted(readFile(path));
	} catch(const std::exception& error) {
		throw FileProblem(path.string() + ": " + error.what());
	}
}

/// @brief A feature's manifest file: the path itself, or the feature.json in that folder.
/// @param path A feature.json or its folder.
/// @return The file.
std::filesystem::path manifestFile(const std::filesystem::path& path) {
	return std::filesystem::is_directory(path) ? path / "feature.json" : path;
}

/// @brief The installable parts an app ships: `<its folder>/features/*/feature.json`, by name.
/// @param screen The app's screen file.
/// @return partOf() each, or `{name, problem}` for one that doesn't check.
json partsOf(const std::filesystem::path& screen) {
	json parts = json::array();
	const std::filesystem::path folder = screen.parent_path() / "features";
	if(!std::filesystem::is_directory(folder)) return parts;
	std::vector<std::filesystem::path> files;
	for(const auto& entry : std::filesystem::directory_iterator(folder))
		if(std::filesystem::is_regular_file(entry.path() / "feature.json")) files.push_back(entry.path() / "feature.json");
	std::ranges::sort(files);
	for(const std::filesystem::path& file : files) try {
			const json manifest = readUntrusted(file);
			format::checkManifest(manifest, "feature");
			parts.push_back(format::partOf(manifest));
		} catch(const std::exception& error) {
			parts.push_back({{"name", file.parent_path().filename().string()}, {"problem", error.what()}});
		}
	return parts;
}

/// @brief Plays a scenarios file on a composed document, headless, and reports each scenario.
/// @param file The scenarios.
/// @param label Where they came from, for the proof.
/// @param document The composed document.
/// @param prepare Registers the host's natives on each screen.
/// @param outcomes Receives one outcome per scenario.
void play(const automation::ScenarioFile& file, const std::string& label, const json& document,
          const std::function<void(Screen&)>& prepare, std::vector<format::ScenarioOutcome>& outcomes) {
	automation::Options options;
	options.prepare = prepare;
	options.pictures = false; // a proof checks behaviour; pictures are the rendering check's
	options.document = document.dump();
	for(const automation::ScenarioResult& scenario : automation::run(file, options).scenarios) {
		std::string problem;
		if(!scenario.failures.empty())
			problem = std::format("step {}: {}", scenario.failures.front().step, scenario.failures.front().problem);
		outcomes.push_back({label, scenario.name, scenario.passed(), std::move(problem)});
	}
}

/// @brief The scenarios a feature ships in its manifest, as a scenarios file on the host's screen.
/// @param manifest The manifest.
/// @param screen The host's screen file.
/// @return The file's text, or "" if it ships none.
std::string featureScenarios(const json& manifest, const std::filesystem::path& screen) {
	json list = manifest.value("scenarios", json());
	if(list.is_object()) list = list.value("scenarios", json());
	if(!list.is_array() || list.empty()) return {};
	return json{{"format", "affrescoui-scenarios"},
	            {"version", 2},
	            {"screen", screen.filename().string()},
	            {"scenarios", list}}
	    .dump();
}

/// @brief The runner a proof plays scenarios with: the feature's, then the host's file (if any).
/// @param screen The host's screen file.
/// @param hostScenarios The host's scenarios file, or empty for none.
/// @param prepare Registers the host's natives.
/// @return The runner.
format::ScenarioRunner runnerFor(const std::filesystem::path& screen, const std::filesystem::path& hostScenarios,
                                 const std::function<void(Screen&)>& prepare) {
	return [=](const json& document, const json& manifest) {
		const QuietLog quiet;
		std::vector<format::ScenarioOutcome> outcomes;
		const auto attempt = [&](const std::string& label, const auto& load) {
			try {
				play(load(), label, document, prepare, outcomes);
			} catch(const DocumentError& error) {
				outcomes.push_back({label, "(file)", false, error.what()});
			}
		};
		if(const std::string own = featureScenarios(manifest, screen); !own.empty())
			attempt("feature", [&] {
				return automation::ScenarioFile::parse(own, screen.parent_path() / "feature.scenarios.json");
			});
		if(!hostScenarios.empty())
			attempt(hostScenarios.filename().string(), [&] { return automation::ScenarioFile::load(hostScenarios); });
		return outcomes;
	};
}

/// @brief The host's scenarios file: --scenarios, "none", or `<name>.scenarios.json` beside the screen.
/// @param arguments The command line.
/// @param screen The host's screen file.
/// @return The file, or empty for none.
std::filesystem::path hostScenariosOf(const Arguments& arguments, const std::filesystem::path& screen) {
	if(const std::optional<std::string> given = arguments.value("--scenarios"))
		return *given == "none" ? std::filesystem::path() : std::filesystem::path(*given);
	std::string name = screen.filename().string();
	if(const std::size_t at = name.find(".screen.json"); at != std::string::npos) name.replace(at, 12, ".scenarios.json");
	const std::filesystem::path beside = screen.parent_path() / name;
	return name != screen.filename().string() && std::filesystem::is_regular_file(beside) ? beside
	                                                                                      : std::filesystem::path();
}

/// @brief Where a feature comes from: "app:<the folder above its features folder>".
/// @param feature The feature's manifest file.
/// @return The source.
std::string sourceOf(const std::filesystem::path& feature) {
	const std::filesystem::path app = std::filesystem::absolute(feature).parent_path().parent_path().parent_path();
	return "app:" + app.filename().string();
}

} // namespace

Exit listContributions(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	const Arguments arguments(args, {}, flags);
	const std::filesystem::path file = arguments.file();
	const json contributions = format::contributionsOf(loadDocument(file, readFile(file)), partsOf(file));
	if(arguments.flag("--json")) std::cout << contributions.dump() << '\n';
	else std::cout << format::contributionsText(contributions);
	return Exit::Success;
}

Exit checkComposition(std::span<const std::string_view> args, const std::function<void(Screen&)>& prepare) {
	static constexpr std::array<std::string_view, 6> valued = {"--adapter",  "--base",     "--scenarios",
	                                                           "--document", "--revision", "--source"};
	static constexpr std::array<std::string_view, 2> flags = {"--compose", "--json"};
	const Arguments arguments(args, valued, flags);
	if(arguments.positional().size() != 2) throw UsageError("expected a screen and a feature");
	const std::filesystem::path screen = arguments.positional()[0];
	const std::filesystem::path feature = manifestFile(arguments.positional()[1]);
	const std::string manifestText = readFile(feature);

	format::ProofInputs inputs;
	inputs.host = loadDocument(screen, readFile(arguments.value("--document").value_or(screen.string())));
	inputs.manifest = readUntrusted(feature);
	if(const std::optional<std::string> adapter = arguments.value("--adapter")) inputs.adapter = readUntrusted(*adapter);
	inputs.source = arguments.value("--source").value_or(sourceOf(feature));
	inputs.hostName = "app:" + std::filesystem::absolute(screen).parent_path().filename().string();
	inputs.base = arguments.value("--base").value_or("");
	inputs.digest = format::digestOf(manifestText);
	const format::ProvenComposition proven =
	    format::proveComposition(inputs, runnerFor(screen, hostScenariosOf(arguments, screen), prepare));

	const bool passed = proven.proof.value("passed", false) == true;
	if(const std::optional<std::string> revision = arguments.value("--revision"); revision && passed)
		writeFile(*revision,
		          vcs::compositionRevision(proven.proof.value("feature", ""), proven.record, "features", inputs.host).dump(1, '\t'));
	if(arguments.flag("--json")) std::cout << proven.proof.dump() << '\n';
	else
		for(const std::string& line : format::proofReasons(proven.proof)) std::cout << line << '\n';
	return passed ? Exit::Success : Exit::Problems;
}

} // namespace affresco::cli
