/// @file
/// @brief openInBrowser and openWebAddress (SignIn.hpp) in the browser build: a new tab, with no opener, for the same URLs as
/// OAuthBrowser.cpp. Under Node (the scenarios) there is no browser, and nothing opens.

#include "affresco/net/SignIn.hpp"
#include "affresco/net/Url.hpp"

#include "affresco/core/Log.hpp"

#include <emscripten.h>

#include <optional>

namespace affresco::net {

namespace {

/// @brief Opens a URL already checked in a new tab, with no opener.
/// @param url The URL.
/// @return Whether there was a browser window to open it from.
bool launchBrowser(const std::string& url) {
	// clang-format off: JavaScript
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension" // EM_ASM names its arguments $0, $1...
	const int opened = EM_ASM_INT(
	    {
		    if(typeof window === "undefined" || !window.open) return 0;
		    window.open(UTF8ToString($0), "_blank", "noopener,noreferrer");
		    return 1;
	    },
	    url.c_str());
#pragma clang diagnostic pop
	// clang-format on
	if(!opened) log(LogLevel::Warning, "AffrescoUI: there is no browser window to open the page in");
	return opened != 0;
}

} // namespace

void openInBrowser(const std::string& url) {
	if(!isSignInPage(url)) {
		log(LogLevel::Warning, "AffrescoUI sign-in: won't open a URL that isn't https in the browser");
		return;
	}
	launchBrowser(url);
}

bool openWebAddress(const std::string& url) {
	const std::optional<Url> parsed = parseUrl(url);
	if(!parsed || (parsed->scheme != "http" && parsed->scheme != "https")) {
		log(LogLevel::Warning, "AffrescoUI: won't open a web address that isn't a well-formed http or https URL");
		return false;
	}
	return launchBrowser(url);
}

} // namespace affresco::net
