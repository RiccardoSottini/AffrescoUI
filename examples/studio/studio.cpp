/// @file
/// @brief affrescoui_studio: a client for a running affrescoui_host.
//
// Ask an AI agent for changes in plain words, apply JSON Patches yourself, review the agent's proposals (preview them
// live in the host, accept, "always allow", reject), pick the mode, revoke grants, browse, revert and restore the
// history.
//
//   affrescoui_studio [--socket DIR]      (default: where affrescoui_host listens; see host.cpp)
//
// The Studio is itself an AffrescoUI screen (examples/studio/studio.screen.json): layout, behaviour and the connection
// to the host. Its "channels" section connects to studio.sock (and reconnects); every message from the host runs the
// screen's "host:<type>" action, and its "send" steps go to the host. This program only opens it, and shows the
// pictures of a proposal's evidence (VERSION_CONTROL.md, "Evidence"), which a screen can't decode itself, and moves
// the channel to the connection chosen in the Connections list (a host's socket or the document server).

#include "Assets.hpp"
#include "EvidencePicture.hpp"
#include "SocketDir.hpp"

#include <affresco/affresco.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

using examples::studio::fromBase64;
using examples::studio::outlined;

/// @brief Decodes one of the evidence's pictures.
/// @param picture `{"format": "png", "data": base64}`.
/// @return Its pixels, red, green, blue and alpha.
std::shared_ptr<const affresco::Bitmap> decodePicture(const affresco::json& picture) {
	const std::vector<std::uint8_t> png = fromBase64(picture.at("data").get<std::string>());
	return affresco::Bitmap::decode(png, affresco::Bitmap::Limits{});
}

/// @brief Shows a proposal's evidence pictures as the images "evidenceBefore" and "evidenceAfter", the changed region
/// outlined on both.
/// @param studio The Studio's screen.
/// @param argumentsJson The evidence: `{"before", "after", "region"}`.
/// @return "true".
std::string_view showEvidence(affresco::Screen& studio, std::string_view argumentsJson) {
	const affresco::json evidence = affresco::json::parse(argumentsJson);
	const std::array<int, 4> region = examples::studio::regionOf(evidence);
	for(const auto& [key, name] : {std::pair{"before", "evidenceBefore"}, std::pair{"after", "evidenceAfter"}}) {
		const std::shared_ptr<const affresco::Bitmap> picture = decodePicture(evidence.at(key));
		studio.setImage(name, picture->width(), picture->height(), outlined(*picture, region));
	}
	return "true";
}

/// @brief Moves the Studio's "host" channel to another connection from its Connections list: a host's socket (a
/// name in the socket folder, or a path) or a remote peer's `ws://` or `wss://` address, such as the document server.
/// The token stays in this run's memory and goes to the server once, as the connection's credentials; the screen
/// never keeps it.
/// @param studio The Studio's screen.
/// @param argumentsJson `{"address", "token"}` (the token may be empty for a local socket).
/// @return "true"; the connection then reports on the "host:" actions.
/// @throws std::runtime_error Without an address, with one longer than 2,048 bytes or a token longer than 4,096, or
/// when the screen has no "host" channel.
std::string_view connectTo(affresco::Screen& studio, std::string_view argumentsJson) {
	const affresco::json arguments = affresco::json::parse(argumentsJson);
	std::string address = arguments.value("address", std::string{});
	std::string token = arguments.value("token", std::string{});
	if(address.empty() || address.size() > 2048 || token.size() > 4096)
		throw std::runtime_error("no address, or an address or token too long");
	if(!studio.connectChannel("host", std::move(address), std::move(token)))
		throw std::runtime_error("the Studio has no host channel");
	return "true";
}

#ifdef __EMSCRIPTEN__
affresco::Screen* connectable = nullptr; ///< The Studio, for the page's connect form (main() keeps it to the end).
#endif

} // namespace

#ifdef __EMSCRIPTEN__
/// @brief Connects the Studio to a host's remote-peer server, from the page's connect form (web/studio.html). The
/// token is the server's to verify; it stays in this run's memory and is sent once, as the connection's credentials.
/// @param address The server's `wss://` address (`ws://` on the loopback).
/// @param token The token.
/// @return 1 if the Studio is running and took the address (the connection reports on the "host:" actions), else 0.
extern "C" EMSCRIPTEN_KEEPALIVE int affresco_studio_connect(const char* address, const char* token) {
	if(!connectable || !address || !token) return 0;
	return connectable->connectChannel("host", address, token) ? 1 : 0;
}
#endif

int main(int argc, char** argv) { // NOLINT(misc-const-correctness): main's own signature
	try {
		std::filesystem::path socketDir = affresco::Screen::defaultSocketDirectory();
		std::filesystem::path dataDir = affresco::Screen::defaultDataDirectory();
		for(int i = 1; i < argc; ++i) {
			if(std::string_view(argv[i]) == "--socket" && i + 1 < argc) socketDir = argv[++i];
			else if(std::string_view(argv[i]) == "--data" && i + 1 < argc) dataDir = argv[++i];
			else {
				std::cerr << "usage: affrescoui_studio [--socket DIR] [--data DIR]\n";
				return 2;
			}
		}

#ifndef __EMSCRIPTEN__ // a browser tab has no socket folder: the page's connect form gives the host's address
		checkSocketDir(socketDir);
#endif
		affresco::Application app;
		affresco::Screen studio =
		    affresco::Screen::fromFile(assetsDirectory() / "studio" / "studio.screen.json", dataDir);
		studio.setSocketDirectory(socketDir);
		studio.onCall("showEvidence", [&studio](const std::string& argumentsJson) {
			return std::string(showEvidence(studio, argumentsJson));
		});
		studio.onCall("connectTo", [&studio](const std::string& argumentsJson) {
			return std::string(connectTo(studio, argumentsJson));
		});
		studio.createWindow(app); // connects the file's channels
#ifdef __EMSCRIPTEN__
		connectable = &studio;
#endif
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_studio: " << error.what() << '\n';
		return 1;
	}
}
