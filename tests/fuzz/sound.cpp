/// @file
/// @brief Fuzzes sound descriptions (audio/Sound.hpp): an agent writes them, so their text can hold anything. A
/// description that reads is rendered for a second, at the start and past its end, and every sample must be finite
/// and within -1 to 1.

#include "Fuzz.hpp"

#include <affresco/audio/Sound.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace audio = affresco::audio;
	std::string error;
	audio::SoundLimits limits;
	limits.seconds = 30; // small, so that inputs reach it
	const auto score = audio::readSound(affresco::fuzz::text(data, size), error, limits);
	if(!score) return 0;
	const audio::Format format{8000, 2};
	std::vector<float> block(800 * 2);
	for(const std::uint64_t first : {std::uint64_t{0}, std::uint64_t{4000}, std::uint64_t{8000 * 31}})
		for(int i = 0; i < 2; ++i) {
			const std::size_t frames =
			    audio::renderSound(*score, block, first + static_cast<std::uint64_t>(i) * 800, format);
			for(std::size_t k = 0; k < frames * 2; ++k)
				if(!std::isfinite(block[k]) || std::abs(block[k]) > 1.0F) std::abort();
		}
	return 0;
}
