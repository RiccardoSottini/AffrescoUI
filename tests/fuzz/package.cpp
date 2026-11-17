/// @file
/// @brief Fuzzes readPackage, packageSummary and installing (format/PackageFiles.hpp, format/PackageInstall.hpp,
/// Screen::proposePackage()): a package is a file a person shares, untrusted input (VERSION_CONTROL.md, "Sharing a
/// version: packages" and "Installing a package").
///
/// Any bytes are refused with json::parse_error or DocumentError, or are a package that writes back as itself, reads
/// back from what it writes, gives the base its tree names, and has a summary, against no copy and against a person's.
/// Installing it on an app "notes", a program (every input) and a JSON template (one input size in eight: checking a
/// proposal on it is the slow part), is refused or a proposal, never applied, and the same file twice is never a
/// second proposal.

#include "Fuzz.hpp"

#include "affresco/format/Package.hpp"
#include "affresco/format/PackageFiles.hpp"
#include "affresco/format/PackageInstall.hpp"

#include <affresco/affresco.hpp>

#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using affresco::json;
namespace fs = std::filesystem;

namespace {

/// The program the second app "notes" is: the corpus's packages are made for its base (corpus.py, package_seeds()).
constexpr const char* programScreen = R"json({"format": "affrescoui-screen", "version": 2,
	"state": {"title": "Notes", "columns": ["To do", "Done"]},
	"program": {"app.lisp": {"count": "(defn count [] 1)"}},
	"root": {"type": "label", "id": "title", "text": "{{state.title}}"}})json";

/// @brief The folder the two apps are written in, this process's own.
/// @return It.
fs::path appsFolder() { return fs::temp_directory_path() / ("affrescoui-fuzz-package-" + std::to_string(::getpid())); }

/// @brief Writes an app "notes" whose entry is a screen.
/// @param name The folder's name in appsFolder().
/// @param screen The entry screen's text.
/// @return The folder.
fs::path writeApp(const std::string& name, const std::string& screen) {
	const fs::path folder = appsFolder() / name;
	fs::create_directories(folder);
	std::ofstream(folder / "app.json") << R"({"format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes",
		"release": "1.0.0", "entry": "index.screen.json"})";
	std::ofstream(folder / "index.screen.json") << screen;
	return folder;
}

/// @brief The two apps "notes" to install on, opened once (LLVMFuzzerInitialize()): a program, then a JSON template.
/// @return Them.
std::vector<affresco::Screen>& apps() {
	static std::vector<affresco::Screen> opened;
	return opened;
}

/// @brief Installs the package on an app twice, as Screen::proposePackage() does: refused or a proposal, never
/// applied; the second time never a second proposal. A proposal is then rejected, so the next input meets the app
/// as it was.
/// @param screen The app.
/// @param text The package's file.
void install(affresco::Screen& screen, std::string_view text) {
	const json summary = screen.inspectPackage(text, json::object());
	if(!summary.contains("problems")) affresco::fuzz::broken("an inspected package's summary lists its problems");
	std::string error;
	const affresco::RevisionResult first = screen.proposePackage(text, &error);
	if(first.status == affresco::RevisionStatus::Applied) affresco::fuzz::broken("an install is never applied");
	if(first.status == affresco::RevisionStatus::Proposed && !summary["problems"].empty())
		affresco::fuzz::broken("a package with problems is never proposed");
	const affresco::RevisionResult again = screen.proposePackage(text, &error);
	if(again.status == affresco::RevisionStatus::Applied) affresco::fuzz::broken("an install is never applied");
	if(first.status == affresco::RevisionStatus::Proposed && again.status == affresco::RevisionStatus::Proposed)
		affresco::fuzz::broken("the same package twice is one proposal");
	if(first.status == affresco::RevisionStatus::Proposed)
		static_cast<void>(screen.versions().reject(first.proposal, "the next input"));
}

/// @brief The summary against a person's copy that isn't its base: one of each kind of part, never an exception.
/// @param package The package.
void summariseAgainstCopy(const affresco::format::Package& package) {
	affresco::format::PackageReceiver receiver;
	receiver.known = true;
	receiver.app = package.base.app;
	receiver.copy = affresco::fuzz::jsonTemplate();
	receiver.release = "1.0.0";
	receiver.capabilities = {"network:example.org"};
	const json summary = affresco::format::packageSummary(package, receiver, json::object());
	if(!summary["problems"].is_array()) affresco::fuzz::broken("a summary's problems are a list");
	if(summary["base"]["exact"] == true && affresco::format::baseOf(package) != receiver.copy)
		affresco::fuzz::broken("a base is exact only if it is the copy");
}

} // namespace

/// @brief libFuzzer's set-up: a quiet log, and the two apps "notes" written and opened.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	json screen = affresco::fuzz::jsonTemplate(); // its parts already merged in
	screen.erase("include");
	screen.erase("$schema");
	screen.erase("fonts"); // named from the repository's folder, not the app's
	for(const fs::path& folder : {writeApp("program", programScreen), writeApp("template", screen.dump())}) {
		try {
			apps().push_back(affresco::Screen::fromApp(folder, {}, affresco::Screen::History::InMemory));
		} catch(const affresco::DocumentError& error) {
			affresco::fuzz::broken((std::string("the app to install on opens: ") + error.what()).c_str());
		}
	}
	return 0;
}

/// @brief libFuzzer's entry point: reads the input as a package, writes it back, sums it up and installs it.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	affresco::format::Package package;
	try {
		package = affresco::format::readPackage(text);
	} catch(const json::parse_error&) {
		return 0;
	} catch(const affresco::DocumentError&) {
		return 0;
	}
	const json written = affresco::format::packageJson(package);
	const affresco::format::Package again = affresco::format::readPackage(written.dump());
	if(affresco::format::packageJson(again) != written) std::abort();
	const json base = affresco::format::baseOf(package);
	if(affresco::format::documentTree(base) != package.base.tree) std::abort();
	const json summary = affresco::format::packageSummary(package, base, json::object());
	if(summary["base"]["exact"] != true) std::abort();
	if(summary["kind"] != (package.kind == affresco::format::PackageKind::App ? "app" : "changes")) std::abort();
	summariseAgainstCopy(package);
	// Every input on the program; one size in eight on the template too, whose proposal checks a whole screen
	install(apps().front(), text);
	if(size % 8U == 0U) install(apps().back(), text);
	return 0;
}
