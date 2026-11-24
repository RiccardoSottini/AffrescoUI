/// @file
/// @brief What the command-line providers share (CommandLine.hpp), run by the services module's runProgram(): an
/// argument vector, no shell, its own process group, only stdin, stdout and stderr inherited.

#include "affresco/intelligence/CommandLine.hpp"

#include <affresco/core/Portable.hpp>
#include <affresco/services/Grants.hpp>
#include <affresco/services/Program.hpp>

#include <algorithm>
#include <random>
#include <system_error>

namespace affresco::intelligence {

namespace {

/// @brief An empty folder of its own in the temporary folder, removed with it.
struct EmptyFolder {
	std::filesystem::path path; ///< The folder, or empty if it couldn't be made.

	/// @brief Makes it, with a random name.
	/// @param tag A short word in its name, e.g. "claude".
	explicit EmptyFolder(const std::string& tag) {
		std::random_device random;
		std::error_code error;
		for(int attempt = 0; attempt < 8 && path.empty(); ++attempt) {
			const std::filesystem::path candidate =
			    std::filesystem::temp_directory_path(error) /
			    ("affrescoui-" + tag + "-" + std::to_string(random()) + std::to_string(random()));
			if(!error && std::filesystem::create_directory(candidate, error) && !error) {
				std::filesystem::permissions(candidate, std::filesystem::perms::owner_all,
				                             std::filesystem::perm_options::replace, error); // this user's only
				path = candidate;
			}
		}
	}
	EmptyFolder(const EmptyFolder&) = delete;
	EmptyFolder& operator=(const EmptyFolder&) = delete;
	EmptyFolder(EmptyFolder&&) = delete;
	EmptyFolder& operator=(EmptyFolder&&) = delete;
	/// @brief Removes it.
	~EmptyFolder() {
		std::error_code error;
		if(!path.empty()) std::filesystem::remove_all(path, error);
	}
};

/// @brief An environment with a program's folder first on its PATH, unless PATH has it already: a program found where
/// its installer put it, outside the host's PATH (often short for an app opened from the desktop), finds what sits
/// beside it, such as the `node` an npm install's `#!/usr/bin/env node` asks for.
/// @param environment "NAME=value" entries.
/// @param program The program.
/// @return The entries.
std::vector<std::string> withProgramFolder(std::vector<std::string> environment, const std::filesystem::path& program) {
#ifdef _WIN32
	constexpr char separator = ';';
#else
	constexpr char separator = ':';
#endif
	const std::string folder = program.parent_path().string();
	if(folder.empty()) return environment;
	const auto path =
	    std::ranges::find_if(environment, [](const std::string& entry) { return entry.starts_with("PATH="); });
	if(path == environment.end()) {
		environment.push_back("PATH=" + folder);
		return environment;
	}
	const std::string folders = std::string(1, separator) + path->substr(5) + separator;
	if(!folders.contains(separator + folder + separator)) *path = "PATH=" + folder + separator + path->substr(5);
	return environment;
}

} // namespace

std::optional<std::filesystem::path> findCommandLine(std::string_view name, std::span<const char* const> homePlaces) {
	std::filesystem::path found = affresco::services::findProgram(name);
	if(!found.empty()) return found;
#ifdef _WIN32
	(void)homePlaces;
	return std::nullopt;
#else
	std::vector<std::filesystem::path> places;
	if(const std::string home = affresco::environmentVariable("HOME").value_or(""); !home.empty())
		for(const char* place : homePlaces) places.push_back(std::filesystem::path(home) / place / name);
	places.push_back(std::filesystem::path("/opt/homebrew/bin") / name);
	places.push_back(std::filesystem::path("/usr/local/bin") / name);
	std::error_code error;
	for(const std::filesystem::path& place : places)
		if(std::filesystem::is_regular_file(place, error)) return place;
	return std::nullopt;
#endif
}

std::vector<std::string> keptEnvironment(std::span<const char* const> names) {
	std::vector<std::string> entries;
	for(const char* name : names)
		if(const std::string value = affresco::environmentVariable(name).value_or(""); !value.empty())
			entries.push_back(std::string(name) + "=" + value);
	return entries;
}

CommandLineAnswer runCommandLine(const CommandLineRun& run, const std::string& input, std::chrono::milliseconds timeout,
                                 const std::function<bool()>& stopped) {
	const EmptyFolder folder(run.tag);
	if(folder.path.empty()) return {false, "Can't make a temporary folder for " + run.name + "."};
	affresco::services::Launch launch;
	launch.program = run.program;
	launch.arguments = run.arguments;
	launch.directory = folder.path;
	launch.environment = withProgramFolder(run.environment, run.program); // never the host's keys or settings
	const affresco::services::ProgramRun ran =
	    affresco::services::runProgram(launch, input, timeout, stopped, maxCommandLineOutput);
	using End = affresco::services::ProgramRun::End;
	switch(ran.end) {
	case End::NotStarted:
		return {false, "Can't start " + run.name + "."};
	case End::TimedOut:
		return {false, run.name + " didn't answer in time."};
	case End::Stopped:
		return {false, "Stopped."};
	case End::TooLarge:
		return {false, run.name + "'s answer was too large."};
	case End::Exited:
		break;
	}
	// 127: the shell's "not found", as `env` says when an npm install's `node` isn't on the PATH
	if(ran.status == 127 && ran.output.empty())
		return {false, run.name + " couldn't start (is Node.js on the PATH, for an npm install?)."};
	CommandLineAnswer answer = run.parse(ran.output);
	if(answer.ok && ran.status != 0)
		return {false, run.name + " stopped with status " + std::to_string(ran.status) + "."};
	return answer;
}

} // namespace affresco::intelligence
