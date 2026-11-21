/// @file
/// @brief `affrescoui catalogue` (catalogue/README.md): `check` reads a catalogue's folder entry by entry and prints
/// one line per problem, `refresh` writes its index's digests and sizes from the files it lists.
///
/// The check is what a pull request that adds an entry must pass, and what a reviewer starts from: each entry's file
/// reads and passes a package's or a manifest's checks, its base app is in the examples' folder (its tree the one the
/// app has, when the entry names its current release), its proof's scenarios pass headless, it adds no capability,
/// its texts hold no path or key, it has a licence, and the index's digests and sizes are the files'. It checks; it
/// never signs, and passing it is no promise of safety.

#include "Cli.hpp"

#include "FeatureCatalog.hpp"
#include "FeatureManifest.hpp"
#include "FeatureSignatures.hpp"
#include "JsonText.hpp"
#include <affresco/affresco.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/core/Portable.hpp>
#include <affresco/format/Package.hpp>
#include <affresco/format/PackageFiles.hpp>

#include <array>
#include <cstdlib>
#include <format>
#include <iostream>
#include <optional>

namespace affresco::cli {

namespace {

namespace fs = std::filesystem;

/// @brief Keeps what the library logs out of the output while it lives.
class Quiet {
public:
	/// @brief Silences the log.
	Quiet() {
		setLogSink([](LogLevel, std::string_view) {});
	}
	/// @brief Puts the default log back.
	~Quiet() { setLogSink({}); }
	Quiet(const Quiet&) = delete;
	Quiet& operator=(const Quiet&) = delete;
	Quiet(Quiet&&) = delete;
	Quiet& operator=(Quiet&&) = delete;
};

/// @brief What the check found: one line per problem, each naming its entry.
struct Problems {
	std::vector<std::string> lines = {}; ///< "<entry>: <problem>".

	/// @brief Adds one.
	/// @param entry The entry ("flowFocus 1.0.0"), or "index".
	/// @param problem What is wrong.
	void add(const std::string& entry, const std::string& problem) { lines.push_back(entry + ": " + problem); }
};

/// @brief An app of the examples' folder, in one notation.
struct ExampleApp {
	fs::path folder = {}; ///< Its folder ("examples/flow/lisp").
	std::string release;  ///< Its manifest's release, or "".
};

/// @brief Finds the app of an id in the examples' folder, in a notation.
/// @param examples The examples' folder.
/// @param id The app's id.
/// @param notation "json" or "lisp".
/// @return It, or std::nullopt if no app there has that id.
std::optional<ExampleApp> exampleApp(const fs::path& examples, const std::string& id, const std::string& notation) {
	std::error_code error;
	for(const fs::directory_entry& entry : fs::directory_iterator(examples, error)) {
		const fs::path manifest = entry.path() / "json" / "app.json";
		if(!fs::is_regular_file(manifest, error)) continue;
		try {
			const json app = parseUntrusted(readFile(manifest));
			if(!app.is_object() || app.value("id", json()) != id) continue;
			const fs::path folder = entry.path() / (notation.empty() ? "json" : notation);
			if(!fs::is_directory(folder, error)) return std::nullopt;
			return ExampleApp{folder, app.value("release", std::string())};
		} catch(const std::exception&) {
			continue; // not an app's manifest; the examples' own check reports it
		}
	}
	return std::nullopt;
}

/// @brief The texts a listed file must never hold: the home folder, and the folders the check reads.
/// @param folders The folders.
/// @return Them, absolute.
std::vector<std::string> privateTexts(std::initializer_list<fs::path> folders) {
	std::vector<std::string> texts;
	const auto add = [&texts](const fs::path& path) {
		std::error_code error;
		const fs::path real = fs::weakly_canonical(path, error);
		if(!error) texts.push_back(real.string());
	};
	for(const char* name : {"HOME", "USERPROFILE"})
		if(const std::optional<std::string> home = affresco::environmentVariable(name); home && !home->empty())
			add(*home);
	for(const fs::path& folder : folders) add(folder);
	return texts;
}

/// @brief Plays scenarios headless on a document, the screen file they name being an app's entry.
/// @param scenarios The scenarios (a proof's list).
/// @param document The document, a program's as its source.
/// @param app The app's folder: its entry screen's files are found there.
/// @param entry The entry, for the lines.
/// @param problems Gets one line per scenario that fails.
void playProof(const json& scenarios, const json& document, const fs::path& app, const std::string& entry,
               Problems& problems) {
	const fs::path screen =
	    app / (fs::is_regular_file(app / "index.screen.lisp") ? "index.screen.lisp" : "index.screen.json");
	const json file = {{"format", "affrescoui-scenarios"},
	                   {"version", 2},
	                   {"screen", screen.filename().string()},
	                   {"scenarios", scenarios}};
	automation::Options options;
	options.pictures = false; // a proof checks behaviour; pictures are the rendering check's
	options.document = document.dump();
	try {
		const Quiet quiet;
		const automation::ScenarioFile parsed =
		    automation::ScenarioFile::parse(file.dump(), app / "proof.scenarios.json");
		const automation::Report report = automation::run(parsed, options); // named: the loop outlives a temporary
		for(const automation::ScenarioResult& scenario : report.scenarios)
			if(!scenario.passed())
				problems.add(entry, std::format("its proof's scenario {} fails at step {}: {}", scenario.name,
				                                scenario.failures.front().step, scenario.failures.front().problem));
	} catch(const std::exception& failure) {
		problems.add(entry, std::string("its proof can't be played: ") + failure.what());
	}
}

/// @brief Adds a problem for each section only a host grants (format::capabilitySections: the permissions, channels,
/// services...) that a whole app sets otherwise than its base app does. A package of changes can't change them, nor
/// can a revision, so neither may a version the catalogue lists as a whole app.
/// @param package The package, of kind "app".
/// @param base The base app's document, as the examples' folder holds it.
/// @param entry The entry, for the lines.
/// @param problems Gets them.
void checkGrantedSections(const format::Package& package, const json& base, const std::string& entry,
                          Problems& problems) {
	for(const std::string_view section : format::capabilitySections) {
		const std::string key(section);
		if(package.document.value(key, json()) != base.value(key, json()))
			problems.add(entry, "it changes " + key + ", which a version keeps as its base app has it");
	}
}

/// @brief Checks a package's entry: its file, its base app, its proof, what it adds, its texts and its licence.
/// @param index The index.
/// @param id The entry's id.
/// @param release The release.
/// @param examples The examples' folder.
/// @param problems Gets what is wrong.
void checkPackage(const format::CatalogIndex& index, const std::string& id,
                  const format::CatalogPackageRelease& release, const fs::path& examples, Problems& problems) {
	const std::string entry = id + " " + release.release;
	format::Package package;
	std::string text;
	try {
		text = format::fetchPackage(index, id, release);
		package = format::readPackage(text);
	} catch(const std::exception& failure) {
		problems.add(entry, failure.what());
		return;
	}
	if(package.license.empty()) problems.add(entry, "it has no license");
	for(const std::string& problem : format::scrubProblems(package, privateTexts({examples, index.folder})))
		problems.add(entry, problem);
	const std::optional<ExampleApp> app = exampleApp(examples, package.base.app, package.base.notation);
	if(!app) {
		problems.add(entry, "its base app " + package.base.app + " isn't in the examples' folder, in the notation " +
		                        package.base.notation);
		return;
	}
	json summary, base;
	try {
		const Quiet quiet;
		// in memory: nothing is kept in the data folder, which only has to be one
		Screen screen = Screen::fromApp(app->folder, fs::temp_directory_path(), Screen::History::InMemory);
		summary = screen.inspectPackage(text, json::object());
		base = screen.versions().sourceDocument();
	} catch(const std::exception& failure) {
		problems.add(entry, std::string("it can't be inspected against its base app: ") + failure.what());
		return;
	}
	for(const json& problem : summary.value("problems", json::array())) problems.add(entry, problem.get<std::string>());
	if(release.baseRelease == app->release && summary["base"].value("exact", json()) != true)
		problems.add(entry, "it names " + package.base.app + "'s current release (" +
		                        (app->release.empty() ? std::string("none") : app->release) +
		                        "), but its base isn't that app's tree: pack it again from the app as it is");
	for(const json& capability : summary.value("capabilities", json::array()))
		if(capability.value("added", false))
			problems.add(entry, "it adds the capability " + capability.value("capability", std::string()));
	if(package.kind == format::PackageKind::App) checkGrantedSections(package, base, entry, problems);
	if(package.kind == format::PackageKind::Changes)
		for(const json& use : summary.value("uses", json::array())) // each {step, use, in}
			problems.add(entry, "it uses " + use.value("use", std::string()) + " (in " +
			                        use.value("in", std::string()) + "), which its base app doesn't");
	const json scenarios = package.proof.is_object() ? package.proof.value("scenarios", json()) : json();
	if(!scenarios.is_array() || scenarios.empty()) problems.add(entry, "it has no proof: list its scenarios in proof");
	else playProof(scenarios, package.document, app->folder, entry, problems);
}

/// @brief Checks a feature's entry: its manifest, its scenarios, its texts and its licence. Its scenarios play on an
/// app that installs it, by the composition proof, since they need what an app binds.
/// @param index The index.
/// @param name The feature's name.
/// @param release The release.
/// @param examples The examples' folder.
/// @param problems Gets what is wrong.
void checkFeature(const format::CatalogIndex& index, const std::string& name, const format::CatalogRelease& release,
                  const fs::path& examples, Problems& problems) {
	const std::string entry = name + " " + release.release;
	json manifest;
	try {
		const std::string text =
		    format::readListedFile(index, release.url, release.size, release.digest, format::maxManifestBytes, entry);
		manifest = parseUntrusted(text);
		expect::object(manifest, "feature");
		if(manifest.contains("signature")) static_cast<void>(format::verifySignature(manifest, "feature"));
		format::checkManifest(manifest, "feature"); // which refuses a capability
	} catch(const std::exception& failure) {
		problems.add(entry, failure.what());
		return;
	}
	if(manifest.value("name", json()) != name) problems.add(entry, "its manifest's name isn't the index's");
	if(manifest.value("release", json()) != release.release)
		problems.add(entry, "its manifest's release isn't the index's");
	if(manifest.value("license", std::string()).empty()) problems.add(entry, "it has no license");
	format::Package wrapped; // scanned as a package's document is
	wrapped.document = manifest;
	for(const std::string& problem : format::scrubProblems(wrapped, privateTexts({examples, index.folder})))
		problems.add(entry, problem);
	const json scenarios = manifest.value("scenarios", json());
	if(!scenarios.is_array() || scenarios.empty()) problems.add(entry, "it has no scenarios");
	else try {
			const json file = {{"format", "affrescoui-scenarios"},
			                   {"version", 2},
			                   {"screen", "index.screen.json"},
			                   {"scenarios", scenarios}};
			static_cast<void>(automation::ScenarioFile::parse(file.dump(), index.folder / "feature.scenarios.json"));
		} catch(const std::exception& failure) {
			problems.add(entry, std::string("its scenarios: ") + failure.what());
		}
}

/// @brief `catalogue check`.
/// @param folder The catalogue's folder.
/// @param examples The examples' folder.
/// @return How it ended.
Exit checkCatalogue(const fs::path& folder, const fs::path& examples) {
	Problems problems;
	format::CatalogIndex index;
	try {
		index = format::readUnsignedCatalog(
		    format::readBounded(folder / "index.json", format::maxCatalogBytes, "catalog"), fs::canonical(folder));
	} catch(const std::exception& failure) {
		std::cout << "index: " << failure.what() << '\n';
		return Exit::Problems;
	}
	std::size_t entries = 0;
	for(const auto& [id, package] : index.packages)
		for(const format::CatalogPackageRelease& release : package.releases) {
			++entries;
			checkPackage(index, id, release, examples, problems);
		}
	for(const auto& [name, feature] : index.features)
		for(const format::CatalogRelease& release : feature.releases) {
			++entries;
			checkFeature(index, name, release, examples, problems);
		}
	for(const std::string& line : problems.lines) std::cout << line << '\n';
	std::cout << folder.string() << ": " << entries << (entries == 1 ? " entry, " : " entries, ")
	          << problems.lines.size() << (problems.lines.size() == 1 ? " problem\n" : " problems\n");
	return problems.lines.empty() ? Exit::Success : Exit::Problems;
}

} // namespace

Exit catalogueCommand(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 1> valued = {"--examples"};
	const Arguments arguments(args, valued, {});
	const std::vector<std::string>& positional = arguments.positional();
	if(positional.size() != 2 || (positional[0] != "check" && positional[0] != "refresh"))
		throw UsageError("catalogue check <folder> [--examples DIR] | catalogue refresh <folder>");
	const fs::path folder = positional[1];
	if(positional[0] == "refresh") {
		json index;
		try {
			index = json::parse(readFile(folder / "index.json"));
		} catch(const json::parse_error& error) {
			throw FileProblem((folder / "index.json").string() + ": isn't JSON (" + error.what() + ")");
		}
		if(index.contains("signature"))
			throw FileProblem((folder / "index.json").string() + ": is signed: sign it again to refresh it");
		refreshReleases(index, folder);
		writeFile(folder / "index.json", printText(parseText(index.dump())));
		std::cerr << (folder / "index.json").string() << ": digests and sizes written\n";
		return Exit::Success;
	}
	std::error_code error;
	const fs::path real = fs::weakly_canonical(fs::absolute(folder), error);
	const fs::path examples = arguments.value("--examples").value_or((real.parent_path() / "examples").string());
	return checkCatalogue(folder, examples);
}

} // namespace affresco::cli
