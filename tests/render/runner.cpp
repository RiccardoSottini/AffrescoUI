/// @file
/// @brief The rendering check's runner: plays scenario files with the automation module (affresco::automation) and
/// prints one line per picture, then the totals, as check.py has always printed them.
///
/// Usage: affrescoui_render_runner [--goldens DIR] [--out DIR] [--update] [--only NAME] FILE...
///
/// Each file is one area's (scenarios/kanban.json, ...), or a template's own scenarios (examples/<folder>/
/// *.scenarios.json). A picture with no golden image yet is recorded from this run; --update records them all.
/// Frames that differ are written to --out with a diff (changed pixels in red). Exits with 1 if anything failed.
///
/// With AFFRESCOUI_NO_OPENGL=1 in the environment (CI's macOS and Windows runners, which have no OpenGL 4.1), the
/// scenarios that draw (capture, expect image) are left out, each one named; the others play headless.

#include "surfaces/Surfaces.hpp"

#include <glad/glad.h>

#include <affresco/Screen.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/services/Grants.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace automation = affresco::automation;

/// The public key of Forge's example catalogue's publisher (examples/forge/catalog, its README). Its private half was
/// discarded once the catalogue was signed, so nothing more can be signed with it.
constexpr std::string_view examplePublisher = "ed25519:wvzomDMyk9aPCGQCvTzNPugq4saaDmJdO_8LaeLFYQc";

/// @brief A surface that fails: it raises a GL error (surfaces.json shows that nothing else changes).
class BrokenSurface : public affresco::GpuSurface {
public:
	/// @brief Clears to red, then enables a capability that doesn't exist (GL_INVALID_ENUM).
	void render(affresco::SurfaceFrame&) override {
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_RGBA);
	}
};

/// @brief A fresh copy of forge/folder for each scenario, so that one which creates, moves or deletes files changes
/// neither the next one's folder nor the source tree. The copies are removed when the runner ends.
class FolderCopies {
public:
	/// @brief Makes the folder that holds the copies.
	FolderCopies()
	    : base_(std::filesystem::temp_directory_path() /
		        ("affrescoui-runner-" + std::to_string(std::random_device{}()))) {
		std::filesystem::create_directories(base_);
	}
	/// @brief Removes the copies.
	~FolderCopies() {
		std::error_code ignored;
		std::filesystem::remove_all(base_, ignored);
	}
	FolderCopies(const FolderCopies&) = delete;
	FolderCopies& operator=(const FolderCopies&) = delete;
	FolderCopies(FolderCopies&&) = delete;
	FolderCopies& operator=(FolderCopies&&) = delete;
	/// @brief Copies the folder once more.
	/// @return The copy, named "folder" like the original.
	[[nodiscard]] std::filesystem::path next() {
		const std::filesystem::path copy = base_ / std::to_string(++count_) / "folder";
		std::filesystem::create_directories(copy);
		std::filesystem::copy(AFFRESCOUI_FORGE_FOLDER, copy, std::filesystem::copy_options::recursive);
		return copy;
	}

private:
	std::filesystem::path base_; ///< Where the copies are.
	std::size_t count_ = 0;      ///< How many there are.
};

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
			else if(picture.outcome == automation::Picture::Outcome::Recorded)
				std::cout << "golden  " << picture.name << '\n';
		}
		for(const automation::Failure& failure : scenario.failures) {
			++failed;
			std::cout << "FAILED  " << scenario.name << (failure.step ? ", step " + std::to_string(failure.step) : "")
			          << ": " << failure.problem << '\n';
		}
	}
	return failed;
}

/// @brief Reads the command line.
/// @param args It, without the program's name.
/// @param options Receives the options.
/// @param files Receives the scenario files.
/// @return Whether it is right (else the usage was printed).
bool readArguments(const std::vector<std::string_view>& args, automation::Options& options,
                   std::vector<std::string>& files) {
	for(std::size_t i = 0; i < args.size(); ++i) {
		const bool valued = args[i] == "--goldens" || args[i] == "--out" || args[i] == "--only";
		if(valued && i + 1 == args.size()) break;
		if(args[i] == "--goldens") options.goldens = args[++i];
		else if(args[i] == "--out") options.output = args[++i];
		else if(args[i] == "--only") options.only.emplace_back(args[++i]);
		else if(args[i] == "--update") options.policy = automation::Goldens::Update;
		else files.emplace_back(args[i]);
	}
	if(!files.empty()) return true;
	std::cerr << "usage: affrescoui_render_runner [--goldens DIR] [--out DIR] [--update] [--only NAME] FILE...\n";
	return false;
}

/// @brief Leaves out the scenarios that draw, for a run with no OpenGL, naming each one.
/// @param scenarios The file's scenarios.
/// @return How many it left out.
std::size_t leaveOutPictures(automation::ScenarioFile& scenarios) {
	return std::erase_if(scenarios.scenarios, [](const automation::Scenario& scenario) {
		if(scenario.needsPictures) std::cout << "skipped " << scenario.name << ": it draws; no OpenGL here\n";
		return scenario.needsPictures;
	});
}

/// @brief Plays every file, printing as it goes.
/// @param files The scenario files.
/// @param options How.
/// @return How many failures there were.
/// @throws affresco::DocumentError If a file is malformed.
std::size_t playAll(const std::vector<std::string>& files, const automation::Options& options) {
	std::size_t frames = 0, failed = 0, skipped = 0;
	std::set<std::string, std::less<>> names; // a name is the prefix of its golden images: unique across the files
	for(const std::string& file : files) {
		automation::ScenarioFile scenarios = automation::ScenarioFile::load(file);
		// A second scenario of a name is one failure, and isn't played: the others still are
		std::erase_if(scenarios.scenarios, [&](const automation::Scenario& scenario) {
			if(names.insert(scenario.name).second) return false;
			std::cout << "FAILED  scenario '" << scenario.name << "' is in two files; the second is " << file << '\n';
			++failed;
			return true;
		});
		if(!options.pictures) skipped += leaveOutPictures(scenarios);
		failed += print(automation::run(scenarios, options), frames);
	}
	std::cout << frames << " frames, " << failed << " failed\n";
	if(skipped) std::cout << skipped << " scenarios that draw left out: this run has no OpenGL\n";
	return failed;
}

} // namespace

int main(int argc, char** argv) {
	try {
		automation::Options options;
		options.policy = automation::Goldens::RecordMissing;
		// As the host grants Forge (examples/forge/app.json): its scenarios see forge/folder open as the workspace, a
		// shell, sh and git, its example catalogue, and no file dialog. No language server or debug adapter is granted:
		// the scenarios mirror their answers.
		const auto copies = std::make_shared<FolderCopies>();
		options.prepare = [copies](affresco::Screen& screen) {
			const std::filesystem::path folder = copies->next();
			screen.services().allowChosenRoot("workspace");
			screen.services().addRoot("workspace", folder);
			for(const char* program : {"shell", "sh", "git"}) // a machine without one runs its scenarios without it
				static_cast<void>(affresco::services::grantProgram(screen.services(), program));
			screen.features().trustPublisher(examplePublisher);
			screen.features().addCatalog(std::filesystem::path(AFFRESCOUI_FORGE_DIRECTORY) / "catalog" / "index.json");
			// The example surfaces at a fixed time, and one that fails (surfaces.json)
			examples::registerSurfaces(screen, 1.25);
			screen.registerSurface("broken", [] { return std::make_unique<BrokenSurface>(); });
			// The inspector's overlay (inspector.json), without its times so that each picture is the same
			screen.onAction("inspector:on", [&screen](affresco::Panel&) { screen.setInspector(true, false); });
		};
		const char* noOpenGL = std::getenv("AFFRESCOUI_NO_OPENGL");
		options.pictures = !noOpenGL || std::string_view(noOpenGL) != "1";
		std::vector<std::string> files;
		if(!readArguments(std::vector<std::string_view>(argv + 1, argv + argc), options, files)) return 2;
		return playAll(files, options) ? EXIT_FAILURE : EXIT_SUCCESS;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_render_runner: " << error.what() << '\n';
	} catch(...) { // nothing else is thrown, but nothing may leave main
		std::cerr << "affrescoui_render_runner: failed\n";
	}
	return EXIT_FAILURE;
}
