/// @file
/// @brief Forge's accounts (Accounts.hpp).

#include "Accounts.hpp"

#include <affresco/affresco.hpp>

#include <array>
#include <iostream>
#include <string_view>

namespace forge {

void registerAccounts(affresco::Screen& screen, bool remember) {
	// placeholders, like accounts.json's: the provider to sign in with, and the server (apps/server)
	constexpr std::array<std::string_view, 2> origins{"https://id.example.com", "ws://127.0.0.1:7420"};
	for(const std::string_view origin : origins)
		if(const auto refused = screen.network().allow(origin))
			std::cerr << "affrescoui_forge: " << origin << ": " << *refused << '\n';
	if(remember) screen.setSignInOptions({affresco::net::systemSecretStore("forge"), {}, "forge"});
}

} // namespace forge
