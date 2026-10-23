/// @file
/// @brief Fuzzes key names (format's Shortcuts.hpp): a revision or a peer can give a "press" step, a binding or a
/// menu item any chord, and the screen reads its presses, the character each types and the label people read.
///
/// The input is a chord. Whatever it is, normalising it twice gives what normalising it once does, every press it
/// names is found again from its own shortcut, and what a press types is printable.

#include "Fuzz.hpp"

#include "Shortcuts.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	using namespace affresco::format;
	const std::string_view text = affresco::fuzz::text(data, size);
	const std::string chord = normalizeChord(text);
	if(normalizeChord(chord) != chord) affresco::fuzz::broken("a normalised chord stays as it is");
	(void)keyLabel(chord);
	for(std::size_t at = 0; at < chord.size();) {
		const std::size_t end = std::min(chord.find(' ', at), chord.size());
		if(const std::optional<affresco::KeyEvent> press = keyPress(std::string_view(chord).substr(at, end - at))) {
			const std::optional<affresco::KeyEvent> again = keyPress(shortcut(*press));
			if(!again || again->key != press->key || again->mods.bits != press->mods.bits)
				affresco::fuzz::broken("a press is found again from its shortcut");
			const char32_t typed = typedBy(*press);
			if(typed != 0 && (typed < U' ' || typed > U'~'))
				affresco::fuzz::broken("a press types a printable character");
		}
		at = end + 1;
	}
	return 0;
}
