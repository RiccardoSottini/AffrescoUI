/// @file
/// @brief Fuzzes a remote peer's opening on the server side (modules/net/src/peers/PeerHandshake.hpp): the upgrade request
/// and the first "auth" message.
///
/// The first byte picks what is read (bit 0: the upgrade request, else a first message); the rest is the bytes. A
/// refusal carries a known status and never the token; an accepted request has a key; a token is never empty or over
/// the limit.

#include "../../modules/net/src/peers/PeerHandshake.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

/// @brief Checks what the upgrade request reader made of a head.
/// @param head The bytes, up to the limit.
void checkUpgrade(std::string_view head) {
	namespace peer = affresco::net::peer;
	const auto end = peer::headEnd(head);
	if(end && *end > head.size()) affresco::fuzz::broken("the head ends within the bytes");
	const peer::Upgrade upgrade = peer::parseUpgrade(end ? head.substr(0, *end) : head);
	constexpr std::array statuses{0, 400, 401, 426, 431}; // 401: a malformed bearer token
	if(std::ranges::find(statuses, upgrade.status) == statuses.end()) affresco::fuzz::broken("a known status");
	if(upgrade.status != 0) {
		if(upgrade.reason.empty()) affresco::fuzz::broken("a refusal says why");
		if(!upgrade.token.empty() && upgrade.reason.find(upgrade.token) != std::string::npos)
			affresco::fuzz::broken("a refusal never holds the token");
		static_cast<void>(peer::refusalResponse(upgrade.status, upgrade.reason));
		return;
	}
	if(upgrade.key.empty()) affresco::fuzz::broken("an accepted request has a key");
	if(upgrade.token.size() > peer::maxTokenBytes) affresco::fuzz::broken("a token is within the limit");
	static_cast<void>(peer::acceptResponse(upgrade.key));
	if(upgrade.hasOrigin) static_cast<void>(peer::originAllowed(upgrade.origin, {"https://studio.example.com"}));
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace peer = affresco::net::peer;
	if(size == 0) return 0;
	const auto bytes = affresco::fuzz::text(data + 1, size - 1);
	if((data[0] & 1) != 0) {
		checkUpgrade(bytes.substr(0, peer::maxHandshakeBytes));
		return 0;
	}
	const auto token = peer::authToken(bytes);
	if(token && (token->empty() || token->size() > peer::maxTokenBytes))
		affresco::fuzz::broken("a token is never empty or over the limit");
	return 0;
}
