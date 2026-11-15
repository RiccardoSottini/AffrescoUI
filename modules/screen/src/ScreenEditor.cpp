/// @file
/// @brief Screen::listenEditor(): the nREPL server for the screen's program (net::EditorServer). A stub until the
/// host's package (f2e) fills it: it never listens, and nothing calls it yet.

#include "ScreenImpl.hpp"

#include <stdexcept>

namespace affresco {

// NOLINTNEXTLINE(readability-convert-member-functions-to-static,performance-unnecessary-value-param): f2e fills it
std::uint16_t Screen::listenEditor(Executor& executor, net::EditorServerOptions options) {
	static_cast<void>(executor);
	static_cast<void>(options);
	throw std::runtime_error("AffrescoUI screen: the editor server isn't built yet");
}

} // namespace affresco
