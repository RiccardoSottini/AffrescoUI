/// @file
/// @brief Fuzzes the stream parsers (modules/net/src/http/Streams.hpp): server-sent events and line streams, the bodies a
/// server streams to a screen, cut into chunks at any place.
///
/// The first byte picks where chunks are cut; the rest is the body. Every event delivered is within the limit, a line
/// has no line break, and once a parser refuses (over its limit) the stream ends there.

#include "../../modules/net/src/http/Streams.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

/// @brief The parsers' limit, small so inputs reach it.
constexpr std::size_t limit = 64;

/// @brief Feeds a body to a parser in chunks of a size, stopping where it refuses.
/// @param parser An EventStreamParser or a LineStreamParser.
/// @param body The body.
/// @param chunk The size of each chunk (at least 1).
/// @return Whether the whole body was taken.
template <typename Parser> bool feedInChunks(Parser& parser, std::string_view body, std::size_t chunk) {
	for(std::size_t offset = 0; offset < body.size(); offset += chunk)
		if(!parser.feed(body.substr(offset, chunk))) return false;
	return true;
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace net = affresco::net;
	if(size == 0) return 0;
	const std::size_t chunk = std::max<std::size_t>(1, data[0] % 17);
	const auto body = affresco::fuzz::text(data + 1, size - 1);

	net::EventStreamParser events(limit, [](affresco::network::StreamEvent event) {
		if(event.data.size() + event.type.size() + event.id.size() > limit) affresco::fuzz::broken("an event is within the limit");
		if(event.type.empty()) affresco::fuzz::broken("an event has a type");
	});
	static_cast<void>(feedInChunks(events, body, chunk));

	net::LineStreamParser lines(limit, [](affresco::network::StreamEvent event) {
		if(event.data.empty() || event.data.size() > limit) affresco::fuzz::broken("a line is not blank, within the limit");
		if(event.data.find('\n') != std::string_view::npos) affresco::fuzz::broken("a line has no line feed");
	});
	if(feedInChunks(lines, body, chunk)) lines.finish();
	return 0;
}
