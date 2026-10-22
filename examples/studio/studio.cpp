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

/// @brief Decodes base64 (RFC 4648), as the host sends pictures.
/// @param text The text.
/// @return The bytes.
/// @throws std::invalid_argument If it isn't base64.
std::vector<std::uint8_t> fromBase64(std::string_view text) {
	constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::vector<std::uint8_t> bytes;
	std::uint32_t bits = 0;
	int count = 0;
	for(const char c : text) {
		if(c == '=') break;
		const std::size_t value = alphabet.find(c);
		if(value == std::string_view::npos) throw std::invalid_argument("not base64");
		bits = (bits << 6) | static_cast<std::uint32_t>(value);
		count += 6;
		if(count >= 8) {
			count -= 8;
			bytes.push_back(static_cast<std::uint8_t>((bits >> count) & 0xFF));
		}
	}
	return bytes;
}

/// @brief Decodes one of the evidence's pictures.
/// @param picture `{"format": "png", "data": base64}`.
/// @return Its pixels, red, green, blue and alpha.
std::shared_ptr<const affresco::Bitmap> decodePicture(const affresco::json& picture) {
	const std::vector<std::uint8_t> png = fromBase64(picture.at("data").get<std::string>());
	return affresco::Bitmap::decode(png, affresco::Bitmap::Limits{});
}

/// @brief Draws the changed region's outline on a picture, so a reviewer sees at once what moved.
/// @param picture The picture, opaque.
/// @param region `[x, y, width, height]`, in its pixels; nothing is drawn when it's empty.
/// @return Its pixels with the outline, red, green, blue and alpha.
std::vector<std::uint8_t> outlined(const affresco::Bitmap& picture, const std::array<int, 4>& region) {
	std::vector<std::uint8_t> rgba(picture.pixels().begin(), picture.pixels().end());
	const auto [x, y, w, h] = region;
	if(w <= 0 || h <= 0) return rgba;
	constexpr int thickness = 3;
	const auto mark = [&](int px, int py) {
		if(px < 0 || py < 0 || px >= picture.width() || py >= picture.height()) return;
		const auto at = static_cast<std::size_t>(py * picture.width() + px) * 4;
		const std::array<std::uint8_t, 4> magenta = {236, 64, 122, 255};
		std::ranges::copy(magenta, rgba.begin() + static_cast<std::ptrdiff_t>(at));
	};
	for(int t = 1; t <= thickness; ++t) {
		for(int px = x - t; px < x + w + t; ++px) {
			mark(px, y - t);
			mark(px, y + h - 1 + t);
		}
		for(int py = y - t; py < y + h + t; ++py) {
			mark(x - t, py);
			mark(x + w - 1 + t, py);
		}
	}
	return rgba;
}

/// @brief Shows a proposal's evidence pictures as the images "evidenceBefore" and "evidenceAfter", the changed region
/// outlined on both.
/// @param studio The Studio's screen.
/// @param argumentsJson The evidence: `{"before", "after", "region"}`.
/// @return "true".
std::string showEvidence(affresco::Screen& studio, const std::string& argumentsJson) {
	const affresco::json evidence = affresco::json::parse(argumentsJson);
	const auto region = evidence.value("region", std::array<int, 4>{});
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
std::string connectTo(affresco::Screen& studio, const std::string& argumentsJson) {
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
		studio.onCall("showEvidence",
		              [&studio](const std::string& argumentsJson) { return showEvidence(studio, argumentsJson); });
		studio.onCall("connectTo",
		              [&studio](const std::string& argumentsJson) { return connectTo(studio, argumentsJson); });
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
