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
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/notation/Notation.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <iostream>
#include <system_error>
#include <utility>

namespace affresco::cli {

namespace {

/// @brief Silences the library's log for its lifetime: the checker's warnings and the scenarios' noise would bury
/// the proof.
class QuietLog {
public:
	/// @brief Silences the log.
	QuietLog() {
		setLogSink([](LogLevel, std::string_view) {});
	}
	/// @brief Puts the default log back.
	~QuietLog() { setLogSink({}); }
	QuietLog(const QuietLog&) = delete;
	QuietLog& operator=(const QuietLog&) = delete;
	QuietLog(QuietLog&&) = delete;
	QuietLog& operator=(QuietLog&&) = delete;
};

/// @brief Loads a screen as the library does, and gives its document as version control keeps it: the "include"s in
/// place and, for a program, its source (VersionControl::sourceDocument()), which a proof and its scenarios play on.
/// @param path The screen file: its folder holds what it includes.
/// @param text Its text, or another document to load in its place (the live one, with --document).
/// @return The document.
/// @throws FileProblem If it doesn't load.
/// @throws UsageError If it is what a program built, read back as a document: an older client's --document.
json loadDocument(const std::filesystem::path& path, const std::string& text) {
	const QuietLog quiet;
	json source;
	try {
		source = Screen::fromString(text, path.parent_path()).versions().sourceDocument();
	} catch(const DocumentError& error) {
		throw FileProblem(path.string() + ": " + error.what());
	}
	if(format::isBuiltTree(source)) throw UsageError(std::string(format::builtTreeProblem));
	return source;
}

/// @brief A document as a screen: what a program's source builds, any other document as it is.
/// @param source The document as version control keeps it.
/// @param path Where it came from, for the message.
/// @return The screen.
/// @throws FileProblem If its program can't be built.
json screenOf(const json& source, const std::filesystem::path& path) {
	try {
		return format::Rules().built(source).value_or(source);
	} catch(const DocumentError& error) {
		throw FileProblem(path.string() + ": " + error.what());
	}
}

/// @brief The text of the document to load, as JSON: what `--document` names, or the screen's own file.
/// @param arguments The command's arguments.
/// @param screen The screen's file.
/// @return Its text.
/// @throws FileProblem If it can't be read.
std::string documentText(const Arguments& arguments, const std::filesystem::path& screen) {
	const std::filesystem::path source = arguments.value("--document").value_or(screen.string());
	return lisp::jsonText(readFile(source), source);
}

/// @brief Reads an untrusted file, in the notation its name says: a manifest or an adapter.
/// @param path The file.
/// @return Its value.
/// @throws FileProblem If it can't be read in that notation.
json readUntrusted(const std::filesystem::path& path) {
	try {
		return notation::readDocument(readFile(path), path);
	} catch(const std::exception& error) {
		throw FileProblem(path.string() + ": " + error.what());
	}
}

/// @brief A feature's manifest file: the path itself, or the feature.json or feature.lisp in that folder.
/// @param path A manifest or its folder.
/// @return The file.
std::filesystem::path manifestFile(const std::filesystem::path& path) {
	return std::filesystem::is_directory(path) ? notation::namedDocument(path, "feature") : path;
}

/// @brief How many program files a feature's manifest may list: far more than a feature needs, few enough that a
/// listing can't make the reader open files without end.
constexpr std::size_t maximumProgramFiles = 64;

/// @brief A program file a feature's manifest lists, checked to be a `.lisp` file inside the feature's folder: a
/// relative path that never climbs out of it, a link included.
/// @param folder The feature's folder.
/// @param name The file, as the manifest lists it.
/// @return The file.
/// @throws FileProblem If it isn't such a file.
std::filesystem::path programFile(const std::filesystem::path& folder, const json& name) {
	const std::string text = name.is_string() ? name.get<std::string>() : std::string();
	const std::filesystem::path relative = std::filesystem::path(text).lexically_normal();
	const auto climbs = [](const std::filesystem::path& path) {
		return !path.empty() && path.begin()->string() == "..";
	};
	if(text.empty() || relative.is_absolute() || relative.has_root_name() || climbs(relative) ||
	   relative.extension() != ".lisp")
		throw FileProblem(std::format("program: {} is not a .lisp file in the feature's folder", name.dump()));
	const std::filesystem::path file = folder / relative;
	std::error_code fileError;
	std::error_code folderError;
	const std::filesystem::path real = std::filesystem::weakly_canonical(file, fileError);
	const std::filesystem::path home = std::filesystem::weakly_canonical(folder, folderError);
	if(fileError || folderError || climbs(real.lexically_relative(home)) || !std::filesystem::is_regular_file(real))
		throw FileProblem(std::format("program: {} is not a .lisp file in the feature's folder", name.dump()));
	return file;
}

/// @brief Puts the program files a manifest lists (`"program": ["sorter.lisp", ...]`) into its
/// `provides.program`, each file's definitions by address, as a document's `program` section holds them.
/// @param manifest The manifest, which loses its list.
/// @param folder The feature's folder, which holds the files.
/// @throws FileProblem If the list or a file is wrong, or a file is also written inline.
void addProgramFiles(json& manifest, const std::filesystem::path& folder) {
	const json listed = manifest["program"];
	manifest.erase("program");
	if(!listed.is_array() || listed.empty() || listed.size() > maximumProgramFiles)
		throw FileProblem(
		    std::format("program: a list of 1 to {} .lisp files in the feature's folder", maximumProgramFiles));
	if(!manifest.contains("provides")) manifest["provides"] = json::object();
	json& provides = manifest["provides"];
	if(!provides.is_object()) throw FileProblem("provides: an object");
	json& program = provides["program"];
	if(program.is_null()) program = json::object();
	if(!program.is_object()) throw FileProblem("provides.program: an object");
	for(const json& name : listed) {
		const std::filesystem::path file = programFile(folder, name);
		const std::string key = std::filesystem::path(name.get<std::string>()).lexically_normal().generic_string();
		if(program.contains(key)) throw FileProblem("program: " + key + " is given twice");
		try {
			program[key] = lisp::sourceDocument(readFile(file), key).at("program").at(key);
		} catch(const DocumentError& problem) {
			throw FileProblem(file.string() + ": " + problem.what());
		}
	}
}

/// @brief A feature, read from its manifest: the manifest as it is, or, when it lists program files, with their
/// definitions in its `provides.program` (SCREEN_FORMAT.md, "A feature written as a program").
struct ReadFeature {
	json manifest;      ///< The manifest, its program files read in.
	std::string digest; ///< What pins it: its file's bytes, or, with program files, the manifest read as JSON text.
};

/// @brief Reads a feature from its manifest file: untrusted, in the notation its name says, its program files read in.
/// @param file The manifest file.
/// @return The feature.
/// @throws FileProblem If it, or a program file it lists, can't be read.
ReadFeature readFeature(const std::filesystem::path& file) {
	const std::string text = readFile(file);
	ReadFeature feature{.manifest = readUntrusted(file), .digest = format::digestOf(text)};
	if(!feature.manifest.is_object() || !feature.manifest.contains("program")) return feature;
	try {
		addProgramFiles(feature.manifest, file.parent_path());
	} catch(const FileProblem& problem) {
		throw FileProblem(file.string() + ": " + problem.what());
	}
	feature.digest = format::digestOf(feature.manifest.dump());
	return feature;
}

/// @brief The installable parts an app ships: `<its folder>/features/*/feature.json` (or `.lisp`, with the program
/// files it lists), by name.
/// @param screen The app's screen file.
/// @return partOf() each, or `{name, problem}` for one that doesn't check.
json partsOf(const std::filesystem::path& screen) {
	json parts = json::array();
	const std::filesystem::path folder = screen.parent_path() / "features";
	if(!std::filesystem::is_directory(folder)) return parts;
	std::vector<std::filesystem::path> files;
	for(const auto& entry : std::filesystem::directory_iterator(folder))
		if(const std::filesystem::path file = notation::namedDocument(entry.path(), "feature");
		   std::filesystem::is_regular_file(file))
			files.push_back(file);
	std::ranges::sort(files);
	for(const std::filesystem::path& file : files) try {
			const json manifest = readFeature(file).manifest;
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
/// @param document The composed document, a program's as its source.
/// @param prepare Registers the host's natives on each screen.
/// @param outcomes Receives one outcome per scenario.
void play(const automation::ScenarioFile& file, const std::string& label, const json& document,
          const std::function<void(Screen&)>& prepare, std::vector<format::ScenarioOutcome>& outcomes) {
	automation::Options options;
	options.prepare = prepare;
	options.pictures = false; // a proof checks behaviour; pictures are the rendering check's
	options.document = document.dump();
	const automation::Report report = automation::run(file, options); // named: the loop outlives a temporary
	for(const automation::ScenarioResult& scenario : report.scenarios) {
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
	return json{
	    {"format", "affrescoui-scenarios"}, {"version", 2}, {"screen", screen.filename().string()}, {"scenarios", list}}
	    .dump();
}

/// @brief The runner a proof plays scenarios with: the feature's, then the host's file (if any).
/// @param screen The host's screen file.
/// @param hostScenarios The host's scenarios file, or empty for none.
/// @param prepare Registers the host's natives.
/// @return The runner.
format::ScenarioRunner runnerFor(const std::filesystem::path& screen, const std::filesystem::path& hostScenarios,
                                 const std::function<void(Screen&)>& prepare) {
	return [screen = screen, hostScenarios = hostScenarios, prepare = prepare](const json& document,
	                                                                           const json& manifest) {
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
	if(const std::size_t at = name.find(".screen.json"); at != std::string::npos)
		name.replace(at, 12, ".scenarios.json");
	else if(const std::size_t lisp = name.find(".screen.lisp"); lisp != std::string::npos)
		name.replace(lisp, 12, ".scenarios.json");
	const std::filesystem::path beside = screen.parent_path() / name;
	return name != screen.filename().string() && std::filesystem::is_regular_file(beside) ? beside
	                                                                                      : std::filesystem::path();
}

/// @brief An app's name for a composition: "app:<its folder>", or the example's for a "json" or "lisp" folder, which is
/// one notation of an example.
/// @param folder The app's folder, absolute.
/// @return The name.
std::string appNameOf(const std::filesystem::path& folder) {
	return "app:" + (format::isNotationFolder(folder) ? folder.parent_path() : folder).filename().string();
}

/// @brief Where a feature comes from: the app of the folder above its features folder.
/// @param feature The feature's manifest file.
/// @return The source.
std::string sourceOf(const std::filesystem::path& feature) {
	return appNameOf(std::filesystem::absolute(feature).parent_path().parent_path().parent_path());
}

} // namespace

Exit listContributions(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 1> valued = {"--document"};
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	const Arguments arguments(args, valued, flags);
	const std::filesystem::path file = arguments.file();
	const json contributions =
	    format::contributionsOf(screenOf(loadDocument(file, documentText(arguments, file)), file), partsOf(file));
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
	ReadFeature read = readFeature(feature);

	format::ProofInputs inputs;
	inputs.host = loadDocument(screen, documentText(arguments, screen));
	inputs.manifest = std::move(read.manifest);
	if(const std::optional<std::string> adapter = arguments.value("--adapter"))
		inputs.adapter = readUntrusted(*adapter);
	inputs.source = arguments.value("--source").value_or(sourceOf(feature));
	inputs.hostName = appNameOf(std::filesystem::absolute(screen).parent_path());
	inputs.base = arguments.value("--base").value_or("");
	inputs.digest = std::move(read.digest);
	const format::ProvenComposition proven =
	    format::proveComposition(inputs, runnerFor(screen, hostScenariosOf(arguments, screen), prepare));

	const bool passed = proven.proof.value("passed", false);
	if(const std::optional<std::string> revision = arguments.value("--revision"); revision && passed)
		writeFile(*revision,
		          vcs::compositionRevision(proven.proof.value("feature", ""), proven.record, "features", inputs.host)
		              .dump(1, '\t'));
	if(arguments.flag("--json")) std::cout << proven.proof.dump() << '\n';
	else
		for(const std::string& line : format::proofReasons(proven.proof)) std::cout << line << '\n';
	return passed ? Exit::Success : Exit::Problems;
}

} // namespace affresco::cli
