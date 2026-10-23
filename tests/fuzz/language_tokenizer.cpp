/// @file
/// @brief Fuzzes the tokenizer (format/src/languages/LanguageTokenizer.hpp) with the languages shipped in
/// examples/shared/languages: an editor's text comes from anywhere, so any text must tokenize within the limits, each
/// token inside its line and in its language's scopes, and a line tokenized again from the same start the same way.
///
/// The input's first byte picks the language; the rest is the text, tokenized line by line, carrying the open regions
/// from line to line.

#include "Fuzz.hpp"

#include "Language.hpp"
#include "LanguageTokenizer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace {

using affresco::format::Language;
using affresco::format::LanguageCatalog;
using affresco::format::LanguageState;
using affresco::format::LanguageTokenizer;
using affresco::format::TokenizedLine;

/// @brief The shipped languages, read once.
/// @return Them.
const std::vector<std::shared_ptr<const Language>>& shipped() {
	static const std::vector<std::shared_ptr<const Language>> languages = [] {
		const std::filesystem::path folder =
		    std::filesystem::path(AFFRESCOUI_FUZZ_TEMPLATE).parent_path().parent_path() / "shared" / "languages";
		std::vector<std::shared_ptr<const Language>> read;
		for(const char* file : {"cpp.json", "javascript.json", "typescript.json", "python.json", "json.json",
		                        "cmake.json", "markdown.json", "shell.json"}) {
			const auto catalog = LanguageCatalog::read(affresco::fuzz::readJsonFile(folder / file)["languages"]);
			for(const auto& [name, language] : catalog->languages()) read.push_back(language);
		}
		return read;
	}();
	return languages;
}

/// @brief Tells whether two tokenized lines are the same: their tokens, and the regions open at their ends (an end
/// that refers to its begin's groups is compiled again each time, so it is compared by its rule).
/// @param a One.
/// @param b The other.
bool same(const TokenizedLine& a, const TokenizedLine& b) {
	const bool sameEnd = std::ranges::equal(a.end, b.end, [](const auto& x, const auto& y) {
		return x.rule == y.rule && x.scope == y.scope && x.contentScope == y.contentScope;
	});
	return sameEnd && std::ranges::equal(a.tokens, b.tokens, [](const auto& x, const auto& y) {
		return x.from == y.from && x.to == y.to && x.scopes == y.scopes;
	});
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	static_cast<void>(shipped());
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const std::shared_ptr<const Language>& language = shipped()[data[0] % shipped().size()];
	std::string_view text = affresco::fuzz::text(data + 1, size - 1);
	LanguageTokenizer tokenizer(language), again(language);
	LanguageState state;
	while(true) {
		const std::size_t end = std::min(text.find('\n'), text.size());
		const std::string_view line = text.substr(0, end);
		const TokenizedLine tokenized = tokenizer.tokenize(state, line);
		for(const auto& token : tokenized.tokens) {
			if(token.from > token.to || token.to > line.size()) affresco::fuzz::broken("a token is inside its line");
			for(const auto scope : token.scopes)
				if(scope == 0 || scope > language->scopes.size())
					affresco::fuzz::broken("a token's scopes are its language's");
		}
		if(tokenized.end.size() > LanguageTokenizer::maxDepth)
			affresco::fuzz::broken("regions open stay under the limit");
		// Cut by the time limit, a line may stop elsewhere the second time
		if(!tokenized.cut) {
			const TokenizedLine second = again.tokenize(state, line);
			if(!second.cut && !same(tokenized, second)) affresco::fuzz::broken("a line tokenizes the same way again");
		}
		state = tokenized.end;
		if(end == text.size()) break;
		text.remove_prefix(end + 1);
	}
	return 0;
}
