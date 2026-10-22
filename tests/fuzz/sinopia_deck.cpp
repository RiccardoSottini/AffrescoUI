/// @file
/// @brief Fuzzes Sinopia's deck reader (examples/sinopia/Deck.hpp): a deck file someone sent, or a peer's change to
/// one, is untrusted, and what it gives is drawn, presented and exported.
///
/// A deck read holds within its limits (slides, objects, notes, sections, theme tokens), and writing it and reading it
/// again gives the same deck; anything else is a std::runtime_error.

#include "Fuzz.hpp"

#include "deck/Deck.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

/// @brief Tells whether a read deck holds within its limits.
/// @param deck The deck.
/// @param limits The limits it was read with.
/// @return Whether every count and size is at most its limit.
bool withinLimits(const sinopia::Deck& deck, const sinopia::DeckLimits& limits) {
	if(deck.slides.size() > limits.slides || deck.masters.size() > limits.masters ||
	   deck.layouts.size() > limits.masters || deck.sections.size() > limits.sections ||
	   deck.theme.size() > limits.tokens)
		return false;
	for(const sinopia::Slide& slide : deck.slides)
		if(slide.objects.size() > limits.objects || slide.notes.size() > limits.notesBytes) return false;
	return true;
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const affresco::json value = affresco::fuzz::untrusted(affresco::fuzz::text(data, size));
	if(value.is_discarded()) return 0;
	const sinopia::DeckLimits limits;
	try {
		const sinopia::Deck deck = sinopia::readDeck(value, limits);
		if(!withinLimits(deck, limits)) affresco::fuzz::broken("a deck read holds within its limits");
		if(sinopia::readDeck(sinopia::writeDeck(deck), limits) != deck)
			affresco::fuzz::broken("a deck written and read again is the same deck");
	} catch(const std::runtime_error&) {
		return 0; // not a deck: refused, as it should be
	}
	return 0;
}
