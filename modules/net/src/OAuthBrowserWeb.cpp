/// @file
/// @brief openInBrowser (SignIn.hpp) in the browser build: a new tab, with no opener, for the same URLs as
/// OAuthBrowser.cpp. Under Node (the scenarios) there is no browser, and nothing opens.

#include "affresco/net/SignIn.hpp"

#include "affresco/core/Log.hpp"

#include <emscripten.h>

namespace affresco::net {

void openInBrowser(const std::string& url) {
	if(!url.starts_with("https://") && !url.starts_with("http://127.0.0.1") && !url.starts_with("http://localhost")) {
		log(LogLevel::Warning, "AffrescoUI sign-in: won't open a URL that isn't https in the browser");
		return;
	}
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
	if(!opened) log(LogLevel::Warning, "AffrescoUI sign-in: there is no browser window to open the page in");
}

} // namespace affresco::net
