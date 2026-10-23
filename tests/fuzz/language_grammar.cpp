/// @file
/// @brief Fuzzes the "languages" section's reader (format/src/languages/Language.hpp): a document's languages come
/// from anyone who edits it, so any JSON must be read as languages or refused with a DocumentError, within the
/// limits; and a language it reads must tokenize any text within the tokenizer's.
///
/// The input up to the first zero byte is the section's JSON; the rest is text, tokenized line by line in each
/// language read, carrying the open regions from line to line.

#include "Fuzz.hpp"

#include "Language.hpp"
#include "LanguageTokenizer.hpp"

#include <affresco/core/Json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace {

using affresco::format::Language;
using affresco::format::LanguageCatalog;
using affresco::format::LanguageState;
using affresco::format::LanguageTokenizer;
using affresco::format::TokenizedLine;

/// @brief Checks a tokenized line against its text and its language.
/// @param line The line's tokens.
/// @param size The line's size in bytes.
/// @param language The language.
void checkLine(const TokenizedLine& line, std::size_t size, const Language& language) {
	for(const auto& token : line.tokens) {
		if(token.from > token.to || token.to > size) affresco::fuzz::broken("a token is inside its line");
		for(const auto scope : token.scopes)
			if(scope == 0 || scope > language.scopes.size()) affresco::fuzz::broken("a token's scopes are its language's");
	}
	if(line.end.size() > LanguageTokenizer::maxDepth) affresco::fuzz::broken("regions open stay under the limit");
}

/// @brief Tokenizes a text line by line in a language, as an editor does.
/// @param language The language.
/// @param text The text.
void tokenizeText(const std::shared_ptr<const Language>& language, std::string_view text) {
	LanguageTokenizer tokenizer(language);
	LanguageState state;
	while(true) {
		const std::size_t end = std::min(text.find('\n'), text.size());
		const TokenizedLine line = tokenizer.tokenize(state, text.substr(0, end));
		checkLine(line, end, *language);
		state = line.end;
		if(end == text.size()) break;
		text.remove_prefix(end + 1);
	}
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	std::string_view input = affresco::fuzz::text(data, size);
	const std::size_t zero = std::min(input.find('\0'), input.size());
	const affresco::json section = affresco::fuzz::untrusted(input.substr(0, zero));
	input.remove_prefix(std::min(zero + 1, input.size()));
	if(section.is_discarded()) return 0;
	std::shared_ptr<const LanguageCatalog> catalog;
	try {
		catalog = LanguageCatalog::read(section);
	} catch(const affresco::DocumentError&) {
		return 0; // refused, as it may be
	}
	if(!catalog) return 0;
	if(catalog->languages().size() > LanguageCatalog::maxLanguages) affresco::fuzz::broken("languages stay few");
	for(const auto& [name, language] : catalog->languages()) {
		if(language->rules.size() > Language::maxRules) affresco::fuzz::broken("rules stay under the limit");
		tokenizeText(language, input);
	}
	return 0;
}
