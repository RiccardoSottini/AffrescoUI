/// @file
/// @brief openInBrowser (SignIn.hpp): the system's default browser, started without a shell.

#include "affresco/net/SignIn.hpp"
#include "affresco/net/Url.hpp"

#include "affresco/core/Log.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ; // NOLINT: POSIX declares it for posix_spawn's callers
#endif

#include <array>
#include <cstdlib>
#include <string>
#include <string_view>

namespace affresco::net {

#if !defined(_WIN32) && !defined(__APPLE__)
namespace {

/// @brief Finds xdg-open in the PATH's absolute folders only: a relative entry ("." or "bin") would run whatever
/// the working folder holds under that name.
/// @return Its absolute path, or "" if there is none.
std::string browserOpener() {
	const char* path = std::getenv("PATH");
	std::string_view rest = path ? path : "/usr/bin:/bin";
	while(!rest.empty()) {
		const std::size_t colon = rest.find(':');
		const std::string_view folder = rest.substr(0, colon);
		rest = colon == std::string_view::npos ? std::string_view{} : rest.substr(colon + 1);
		if(!folder.starts_with('/')) continue;
		std::string candidate = std::string(folder) + "/xdg-open";
		if(::access(candidate.c_str(), X_OK) == 0) return candidate;
	}
	return {};
}

} // namespace
#endif

void openInBrowser(const std::string& url) {
	if(!isSignInPage(url)) {
		log(LogLevel::Warning, "AffrescoUI sign-in: won't open a URL that isn't https in the browser");
		return;
	}
#ifdef _WIN32
	const int length = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
	std::wstring wide(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wide.data(), length);
	ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
#ifdef __APPLE__
	const std::string program = "/usr/bin/open"; // the system's, never one found on the PATH
#else
	const std::string program = browserOpener();
	if(program.empty()) {
		log(LogLevel::Warning, "AffrescoUI sign-in: no xdg-open on the PATH's absolute folders to open the browser");
		return;
	}
#endif
	std::string first = program;
	std::string second = url;
	std::array<char*, 3> arguments{first.data(), second.data(), nullptr};
	pid_t child = 0;
	if(posix_spawn(&child, program.c_str(), nullptr, nullptr, arguments.data(), environ) != 0) {
		log(LogLevel::Warning, "AffrescoUI sign-in: couldn't start " + program + " to open the browser");
		return;
	}
	int status = 0;
	waitpid(child, &status, 0); // "open" and "xdg-open" return once the browser has the URL
#endif
}

} // namespace affresco::net
