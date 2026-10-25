/// @file
/// @brief The grants a host makes from its command line (Grants.hpp).

#include "affresco/services/Grants.hpp"
#include "affresco/services/ServiceRegistry.hpp"

#include "affresco/core/Portable.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

namespace {

constexpr std::size_t longestSpec = 4096; ///< A spec, at most 4 KiB.
constexpr std::size_t mostArguments = 32; ///< A spec's arguments, at most.

/// @brief Whether a name is one a grant can have: 1 to 64 letters, digits, `_` and `-`.
/// @param name The name.
/// @return Whether it is.
bool isGrantName(std::string_view name) noexcept {
	return !name.empty() && name.size() <= 64 && std::ranges::all_of(name, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
	});
}

/// @brief Splits a text at spaces.
/// @param text The text.
/// @return Its words, none empty.
std::vector<std::string> words(std::string_view text) {
	std::vector<std::string> out;
	for(std::size_t start = 0; start < text.size();) {
		const std::size_t end = std::min(text.find(' ', start), text.size());
		if(end > start) out.emplace_back(text.substr(start, end - start));
		start = end + 1;
	}
	return out;
}

/// @brief A program by name or path: an absolute path as it is, else found on the PATH.
/// @param name The name or path.
/// @return Its absolute path, or empty.
std::filesystem::path programOf(std::string_view name) {
	const std::filesystem::path path{std::u8string(name.begin(), name.end())};
	if(path.is_absolute()) {
		std::error_code error;
		return std::filesystem::is_regular_file(path, error) ? path : std::filesystem::path();
	}
	return name.contains('/') || name.contains('\\') ? std::filesystem::path() : findProgram(name);
}

} // namespace

std::filesystem::path findProgram(std::string_view name) {
#ifdef _WIN32
	constexpr char separator = ';';
	const std::string file = std::string(name) + ".exe";
#else
	constexpr char separator = ':';
	const std::string file(name);
#endif
	if(file.empty() || file.contains('/') || file.contains('\\')) return {};
	const std::string path = environmentVariable("PATH").value_or("");
	for(std::string_view folders = path; !folders.empty();) {
		const std::size_t end = std::min(folders.find(separator), folders.size());
		const std::filesystem::path folder(folders.substr(0, end));
		folders.remove_prefix(std::min(end + 1, folders.size()));
		std::error_code error;
		if(folder.is_absolute() && std::filesystem::is_regular_file(folder / file, error)) return folder / file;
	}
	return {};
}

std::filesystem::path loginShell() {
#ifdef _WIN32
	const std::string shell = environmentVariable("COMSPEC").value_or("");
	if(std::filesystem::path(shell).is_absolute()) return shell;
	return "C:\\Windows\\System32\\cmd.exe";
#else
	const char* shell = std::getenv("SHELL"); // NOLINT(concurrency-mt-unsafe): read at launch
	std::error_code error;
	if(shell && std::filesystem::path(shell).is_absolute() && std::filesystem::is_regular_file(shell, error))
		return shell;
	return "/bin/sh";
#endif
}

std::optional<std::string> grantProgram(ServiceRegistry& services, std::string_view spec) {
	if(spec.size() > longestSpec) return "a program grant is at most 4 KiB";
	const std::size_t equals = spec.find('=');
	const std::string name(spec.substr(0, equals));
	if(!isGrantName(name)) return "a program grant's name is 1 to 64 letters, digits, _ and -: " + name;
	if(equals == std::string_view::npos) {
		if(name == "shell") {
#ifdef _WIN32
			services.allowProcess(name, loginShell());
#else
			services.allowProcess(name, loginShell(), {"-l"});
#endif
			return std::nullopt;
		}
		const std::filesystem::path program = findProgram(name);
		if(program.empty()) return "no program '" + name + "' on the PATH";
		if(name == "git") services.allowProcess(name, program, gitOptions(), std::nullopt, gitShaper());
		else services.allowProcess(name, program);
		return std::nullopt;
	}
	std::vector<std::string> arguments = words(spec.substr(equals + 1));
	if(arguments.empty()) return "the grant '" + name + "' names no program";
	if(arguments.size() > mostArguments + 1) return "a program grant has at most 32 arguments";
	const std::filesystem::path program = programOf(arguments.front());
	if(program.empty()) return "no program '" + arguments.front() + "' (an absolute path, or a name on the PATH)";
	arguments.erase(arguments.begin());
	services.allowProcess(name, program, std::move(arguments));
	return std::nullopt;
}

} // namespace affresco::services
