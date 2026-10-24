/// @file
/// @brief The app-programs grant: the host's sibling programs, each opening an app from a granted folder, detached,
/// with the host's data folder and a socket folder per app (Grants.hpp). For a launcher.

#include "Providers.hpp"
#include "affresco/services/Grants.hpp"
#include "affresco/services/ServiceRegistry.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace affresco::services {

namespace {

namespace fs = std::filesystem;

constexpr std::size_t longestAsk = 4096;              ///< `--ask TEXT`, at most 4 KiB (a `process` argument's bound).
constexpr std::size_t longestAppName = 48;            ///< An app's socket folder's name, at most.
constexpr std::string_view ownPrefix = "affrescoui_"; ///< What the host's sibling programs' names start with.

/// @brief A path as UTF-8 text, as arguments are.
/// @param path The path.
/// @return Its text.
std::string utf8(const fs::path& path) {
	const std::u8string text = path.u8string();
	return {text.begin(), text.end()};
}

/// @brief Resolves `ROOT:PATH` inside one of the grant's roots (links resolved, kept inside it).
/// @param programs The grant.
/// @param reference The reference.
/// @param rootName Receives the root's name.
/// @return The path, or std::nullopt if the root isn't one of the grant's or the path leads outside it.
std::optional<fs::path> inRoot(const AppPrograms& programs, std::string_view reference, std::string& rootName) {
	const std::size_t colon = reference.find(':');
	if(colon == std::string_view::npos) return std::nullopt;
	rootName = std::string(reference.substr(0, colon));
	const auto root = programs.roots.find(rootName);
	if(root == programs.roots.end()) return std::nullopt;
	const std::string_view relative = reference.substr(colon + 1);
	if(relative.empty()) return root->second;
	if(!isFolderPath(relative)) return std::nullopt;
	return underRoot(root->second, relative);
}

/// @brief An app's socket folder's name: the text's ASCII letters and digits in lower case, other runs as one `-`,
/// at most longestAppName long.
/// @param text E.g. "apps-My budget".
/// @return E.g. "apps-my-budget", or "app" if nothing is left.
std::string socketName(std::string_view text) {
	std::string name;
	for(const char c : text) {
		const bool letter = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
		const bool upper = c >= 'A' && c <= 'Z';
		if(letter || upper) name += upper ? static_cast<char>(c - 'A' + 'a') : c;
		else if(!name.empty() && name.back() != '-') name += '-';
		if(name.size() >= longestAppName) break;
	}
	while(!name.empty() && name.back() == '-') name.pop_back();
	return name.empty() ? "app" : name;
}

/// @brief The desktop's file manager: open on macOS, Explorer on Windows, xdg-open elsewhere.
/// @return Its file, or empty if there is none.
fs::path fileManager() {
#ifdef _WIN32
	const char* windows = std::getenv("SystemRoot"); // NOLINT(concurrency-mt-unsafe): read on the main thread
	return (windows && fs::path(windows).is_absolute() ? fs::path(windows) : fs::path("C:\\Windows")) / "explorer.exe";
#elif defined(__APPLE__)
	return "/usr/bin/open";
#else
	return findProgram("xdg-open");
#endif
}

/// @brief Shapes `reveal ROOT:FOLDER`.
/// @param programs The grant.
/// @param run The run.
/// @return Why it is refused, or std::nullopt.
std::optional<std::string> shapeReveal(const AppPrograms& programs, ProcessRun& run) {
	if(!programs.reveal) return "this grant doesn't show folders";
	if(run.arguments.size() != 2) return "reveal takes one folder, ROOT:FOLDER";
	std::string rootName;
	const std::optional<fs::path> folder = inRoot(programs, run.arguments[1], rootName);
	std::error_code error;
	if(!folder || !fs::is_directory(*folder, error)) return "reveal: not a folder inside a granted root";
	fs::path manager = fileManager();
	if(manager.empty()) return "no file manager to show the folder with (xdg-open)";
	run.program = std::move(manager);
	run.arguments = {utf8(*folder)};
	run.directory.clear();
	return std::nullopt;
}

/// @brief Shapes `PROGRAM [ROOT:SCREEN] [--ask TEXT]`.
/// @param programs The grant.
/// @param run The run.
/// @return Why it is refused, or std::nullopt.
std::optional<std::string> shapeApp(const AppPrograms& programs, ProcessRun& run) {
	const std::string& name = run.arguments.front();
	const auto program = std::ranges::find(programs.programs, name, &AppProgram::name);
	if(program == programs.programs.end()) return "'" + name.substr(0, 64) + "' isn't a program this grant starts";
	std::vector<std::string> arguments;
	std::string app = name.starts_with(ownPrefix) ? name.substr(ownPrefix.size()) : name;
	std::size_t at = 1;
	if(program->screen) {
		std::string rootName;
		const std::optional<fs::path> screen =
		    run.arguments.size() > 1 ? inRoot(programs, run.arguments[1], rootName) : std::nullopt;
		std::error_code error;
		if(!screen || !utf8(screen->filename()).ends_with(".screen.json") || !fs::is_regular_file(*screen, error))
			return name + " needs a screen file, ROOT:PATH, a .screen.json inside a granted root";
		arguments.push_back(utf8(*screen));
		app = rootName + "-" + utf8(screen->parent_path().filename());
		at = 2;
	}
	bool asked = false;
	for(; at < run.arguments.size(); at += 2) {
		if(run.arguments[at] != "--ask" || !program->ask || asked || at + 1 >= run.arguments.size())
			return "'" + run.arguments[at].substr(0, 64) + "' isn't an argument " + name + " takes here";
		const std::string& text = run.arguments[at + 1];
		if(text.size() > longestAsk || text.contains('\0')) return "--ask takes at most 4 KiB of text";
		arguments.insert(arguments.end(), {"--ask", text});
		asked = true;
	}
	fs::path file = findAppProgram(programs.folder, name);
	if(file.empty()) return name + " isn't there next to the host";
	arguments.insert(arguments.end(), {"--data", utf8(programs.dataDirectory), "--socket",
	                                   utf8(programs.socketDirectory / socketName(app))});
	run.program = std::move(file);
	run.arguments = std::move(arguments);
	run.directory.clear();
	return std::nullopt;
}

} // namespace

fs::path findAppProgram(const fs::path& folder, std::string_view name) {
	if(name.empty() || name.contains('/') || name.contains('\\') || name.contains(':') || name.starts_with('.'))
		return {};
#ifdef _WIN32
	const std::string file = std::string(name) + ".exe";
#else
	const std::string file(name);
#endif
	std::vector<fs::path> candidates{folder / file};
	if(name.starts_with(ownPrefix) && name.size() > ownPrefix.size())
		candidates.push_back(folder / "examples" / std::string(name.substr(ownPrefix.size())) / file);
#ifdef __APPLE__
	const fs::path besideBundle = folder.parent_path().parent_path().parent_path(); // Name.app/Contents/MacOS
	candidates.push_back(besideBundle / file);
	candidates.push_back(besideBundle / (file + ".app") / "Contents" / "MacOS" / file);
#endif
	for(const fs::path& candidate : candidates) {
		std::error_code error;
		if(fs::is_regular_file(candidate, error)) return candidate;
	}
	return {};
}

ProcessShaper appProgramsShaper(AppPrograms programs) {
	return [programs = std::move(programs)](ProcessRun& run) -> std::optional<std::string> {
		if(run.arguments.empty()) return "no program named";
		if(run.arguments.front() == "reveal") return shapeReveal(programs, run);
		return shapeApp(programs, run);
	};
}

std::optional<std::string> grantAppPrograms(ServiceRegistry& services, std::string name, AppPrograms programs) {
	if(name.empty() || name.size() > 64) return "a program grant's name is 1 to 64 characters";
	for(auto& [root, folder] : programs.roots) {
		std::error_code error;
		fs::path canonical = fs::canonical(folder, error);
		if(error || !fs::is_directory(canonical, error)) return "the folder '" + root + "' isn't a folder";
		folder = std::move(canonical);
	}
	if(!programs.folder.is_absolute() || !programs.dataDirectory.is_absolute() ||
	   !programs.socketDirectory.is_absolute())
		return "the programs', data and socket folders must be absolute paths";
	ProcessGrant grant;
	// Every run's program is the one the rule picks; the grant names the first, or else the folder
	grant.program = programs.programs.empty() ? programs.folder : programs.folder / programs.programs.front().name;
	grant.detach = true;
	grant.shape = appProgramsShaper(std::move(programs));
	services.allowProcess(std::move(name), std::move(grant));
	return std::nullopt;
}

} // namespace affresco::services
