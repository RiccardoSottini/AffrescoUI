/// @file
/// @brief Fuzzes the syntax engine (affresco/services/Syntax.hpp): an editor's text and its edits come from anywhere,
/// so any text, in any language, edited any way, must parse within its limits and read back sane runs, folds and
/// bracket pairs.
///
/// The input's first byte picks the language, the rest up to the first zero byte is the text; after it, each five
/// bytes are an edit: where (two bytes), how many bytes it removes, how many it inserts, and the byte inserted. After
/// each edit the document is parsed again, and once at the end a fresh document is parsed: both must give the same
/// highlights. Runs stay on their lines, folds go downwards, and bracket pairs are read back as pairs.

#include "Fuzz.hpp"

#include <affresco/services/Syntax.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

using affresco::services::SyntaxDocument;
using affresco::services::SyntaxPoint;
using affresco::services::SyntaxSpan;

constexpr std::size_t maxEdits = 32; ///< Edits applied from one input, at most.

/// @brief Reads a text kept whole, a few bytes at a time (as an editor's chunks are).
/// @param text The text.
affresco::services::SyntaxReader readerOf(const std::string& text) {
	return [&text](std::size_t byte, SyntaxPoint) { return std::string_view(text).substr(byte, 7); };
}

/// @brief A byte offset as a point.
/// @param text The text.
/// @param byte The offset, at most its size.
SyntaxPoint pointOf(std::string_view text, std::size_t byte) {
	const std::string_view before = text.substr(0, byte);
	const std::size_t lineStart = before.rfind('\n') == std::string_view::npos ? 0 : before.rfind('\n') + 1;
	return {static_cast<std::uint32_t>(std::ranges::count(before, '\n')),
	        static_cast<std::uint32_t>(byte - lineStart)};
}

/// @brief A document's highlights, checked against its text.
/// @param document The document, parsed.
/// @param text Its text.
/// @param complete Gets whether no limit cut them short.
std::vector<SyntaxSpan> highlights(SyntaxDocument& document, const std::string& text, bool& complete) {
	std::vector<SyntaxSpan> spans;
	const auto lines = static_cast<std::uint32_t>(std::ranges::count(text, '\n'));
	complete = document.highlight(readerOf(text), 0, lines, spans);
	for(const SyntaxSpan& span : spans) {
		if(span.row > lines) affresco::fuzz::broken("a run is on a line of the text");
		if(span.start > span.end) affresco::fuzz::broken("a run ends after it starts");
		if(span.style >= document.styles().size()) affresco::fuzz::broken("a run's style is one of the document's");
	}
	for(std::uint32_t row = 0; row <= lines; ++row) {
		if(const auto fold = document.fold(row); fold && (fold->first != row || fold->last < row))
			affresco::fuzz::broken("a fold starts at its line and goes downwards");
	}
	return spans;
}

/// @brief Checks the bracket pairs at every place of the text.
/// @param document The document, parsed.
/// @param text Its text.
void checkBrackets(const SyntaxDocument& document, std::string_view text) {
	for(std::size_t byte = 0; byte < text.size(); ++byte) {
		const SyntaxPoint at = pointOf(text, byte);
		const auto pair = document.brackets(at);
		if(!pair) continue;
		if(!(pair->open < pair->close)) affresco::fuzz::broken("a pair opens before it closes");
		if(pair->open != at && pair->close != at) affresco::fuzz::broken("a pair is found from one of its brackets");
	}
}

/// @brief Tells whether two highlights are the same.
/// @param a One.
/// @param b The other.
bool same(const std::vector<SyntaxSpan>& a, const std::vector<SyntaxSpan>& b) {
	return std::ranges::equal(a, b, [](const SyntaxSpan& x, const SyntaxSpan& y) {
		return x.row == y.row && x.start == y.start && x.end == y.end && x.style == y.style;
	});
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const std::vector<std::string> languages = SyntaxDocument::languages();
	const std::string& language = languages[data[0] % languages.size()];
	std::string_view input = affresco::fuzz::text(data + 1, size - 1);
	const std::size_t zero = std::min(input.find('\0'), input.size());
	std::string text(input.substr(0, zero));
	input.remove_prefix(std::min(zero + 1, input.size()));

	const auto document = SyntaxDocument::create(language);
	if(!document) affresco::fuzz::broken("every listed language has a grammar");
	document->parse(readerOf(text), text.size());
	for(std::size_t count = 0; count < maxEdits && input.size() >= 5; ++count, input.remove_prefix(5)) {
		const auto byte = [&input](std::size_t i) { return static_cast<std::uint8_t>(input[i]); };
		const std::size_t at = text.empty() ? 0 : (std::size_t{byte(0)} << 8 | byte(1)) % (text.size() + 1);
		const std::size_t removed = std::min<std::size_t>(byte(2) % 16, text.size() - at);
		const std::string inserted(byte(3) % 16, static_cast<char>(byte(4)));
		const SyntaxPoint start = pointOf(text, at), oldEnd = pointOf(text, at + removed);
		text.replace(at, removed, inserted);
		document->edit({.startByte = static_cast<std::uint32_t>(at),
		                .oldEndByte = static_cast<std::uint32_t>(at + removed),
		                .newEndByte = static_cast<std::uint32_t>(at + inserted.size()),
		                .start = start,
		                .oldEnd = oldEnd,
		                .newEnd = pointOf(text, at + inserted.size())});
		document->parse(readerOf(text), text.size());
	}
	if(!document->parsed() || document->stale()) return 0; // over a limit: nothing to compare
	bool complete = false, freshComplete = false;
	const std::vector<SyntaxSpan> edited = highlights(*document, text, complete);
	checkBrackets(*document, text);
	const auto fresh = SyntaxDocument::create(language);
	if(fresh->parse(readerOf(text), text.size()) != affresco::services::SyntaxParse::Parsed) return 0;
	const std::vector<SyntaxSpan> again = highlights(*fresh, text, freshComplete);
	if(complete && freshComplete && !same(edited, again))
		affresco::fuzz::broken("an edited document highlights as a fresh one");
	return 0;
}
