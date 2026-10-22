/// @file
/// @brief Fuzzes the frame reader (modules/services/src/rpc/Framing.hpp): what a service process writes, framed LSP's way
/// (Content-Length) or one message per line, cut into pieces at any place.
///
/// The first byte picks the framing (its lowest bit) and where pieces are cut; the rest is the stream. Every body is
/// within the limit, a line has no line break, and once the reader refuses it gives the same reason and no body after.

#include "../../modules/services/src/rpc/Framing.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace services = affresco::services;
	if(size == 0) return 0;
	const bool lines = (data[0] & 1) != 0;
	const std::size_t piece = std::max<std::size_t>(1, (data[0] >> 1) % 17);
	const std::string_view stream = affresco::fuzz::text(data + 1, size - 1);
	constexpr services::FrameLimits limits{64, 64}; // small, so inputs reach them

	services::FrameReader reader(lines ? services::Framing::Lines : services::Framing::ContentLength, limits);
	std::optional<std::string> refused;
	for(std::size_t offset = 0; offset < stream.size(); offset += piece) {
		std::vector<std::string> bodies;
		const std::optional<std::string> problem = reader.feed(stream.substr(offset, piece), bodies);
		if(refused && (problem != refused || !bodies.empty())) affresco::fuzz::broken("a refused stream stays refused");
		for(const std::string& body : bodies) {
			if(body.size() > limits.messageBytes) affresco::fuzz::broken("a body is within the limit");
			if(lines && body.find('\n') != std::string::npos) affresco::fuzz::broken("a line has no line feed");
		}
		if(problem) refused = problem;
	}
	return 0;
}
