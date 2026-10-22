/// @file
/// @brief openInBrowser (SignIn.hpp): the system's default browser, started without a shell.

#include "affresco/net/SignIn.hpp"
#include "affresco/net/Url.hpp"

#include "affresco/core/Log.hpp"

#if defined(_WIN32)
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

namespace affresco::net {

void openInBrowser(const std::string& url) {
	if(!isSignInPage(url)) {
		log(LogLevel::Warning, "AffrescoUI sign-in: won't open a URL that isn't https in the browser");
		return;
	}
#if defined(_WIN32)
	const int length = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
	std::wstring wide(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wide.data(), length);
	ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
#if defined(__APPLE__)
	const char* program = "open";
#else
	const char* program = "xdg-open";
#endif
	std::string first = program;
	std::string second = url;
	std::array<char*, 3> arguments{first.data(), second.data(), nullptr};
	pid_t child = 0;
	if(posix_spawnp(&child, program, nullptr, nullptr, arguments.data(), environ) != 0) {
		log(LogLevel::Warning, std::string("AffrescoUI sign-in: couldn't start ") + program + " to open the browser");
		return;
	}
	int status = 0;
	waitpid(child, &status, 0); // "open" and "xdg-open" return once the browser has the URL
#endif
}

} // namespace affresco::net
