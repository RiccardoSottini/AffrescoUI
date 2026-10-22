/// @file
/// @brief Fuzzes the WebSocket frame reader (modules/net/src/websocket/WebSocketFrames.hpp): the bytes a server sends a client,
/// and the bytes a remote peer sends the host, cut into pieces at any place.
///
/// The first byte picks the side (bit 0: the server side, which expects masked frames) and where the bytes are cut;
/// the rest is the bytes. A message is valid UTF-8 within the limit, control payloads are at most 125 bytes, an error
/// carries a close code, and after an error nothing more is read.

#include "../../modules/net/src/websocket/WebSocketFrames.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

/// @brief The reader's limit, small so inputs reach it.
constexpr std::size_t limit = 256;

/// @brief Checks one event against the rules.
/// @param event The event.
void checkEvent(const affresco::net::FrameEvent& event) {
	using Kind = affresco::net::FrameEvent::Kind;
	switch(event.kind) {
	case Kind::Message:
		if(event.data.size() > limit || !affresco::net::isUtf8(event.data))
			affresco::fuzz::broken("a message is UTF-8, within the limit");
		break;
	case Kind::Ping:
	case Kind::Pong:
	case Kind::Close:
		if(event.data.size() > 125) affresco::fuzz::broken("a control payload is at most 125 bytes");
		break;
	case Kind::Error:
		if(event.code < 1000) affresco::fuzz::broken("an error carries a close code");
		break;
	}
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace net = affresco::net;
	if(size == 0) return 0;
	const bool serverSide = (data[0] & 1) != 0;
	const std::size_t piece = std::max<std::size_t>(1, data[0] >> 1);
	const auto bytes = affresco::fuzz::text(data + 1, size - 1);
	net::FrameReader reader(serverSide, limit);
	bool ended = false;
	for(std::size_t offset = 0; offset < bytes.size(); offset += piece) {
		reader.feed(bytes.substr(offset, piece));
		while(const auto event = reader.next()) {
			if(ended) affresco::fuzz::broken("nothing is read after an error");
			checkEvent(*event);
			ended = event->kind == net::FrameEvent::Kind::Error;
		}
	}
	return 0;
}
