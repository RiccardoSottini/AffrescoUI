/// @file
/// @brief TextPattern over PCRE2's 8-bit interpreter (no JIT): compile with limits, find, expand a replacement.

#include "affresco/core/TextPattern.hpp"

#include <pcre2.h>

#include <array>

namespace affresco {

/// @brief PCRE2's compiled pattern and the match context with the limits, freed together.
struct TextPattern::Code {
	pcre2_code* code = nullptr;             ///< The compiled pattern.
	pcre2_match_context* context = nullptr; ///< The limits of each match.
	Code() = default;
	Code(const Code&) = delete;
	Code& operator=(const Code&) = delete;
	/// @brief Frees both.
	~Code() {
		pcre2_match_context_free(context);
		pcre2_code_free(code);
	}
};

namespace {

/// @brief PCRE2's message for an error code.
/// @param error The code.
/// @return The message.
[[nodiscard]] std::string errorMessage(int error) {
	std::array<PCRE2_UCHAR, 256> buffer{};
	const int length = pcre2_get_error_message(error, buffer.data(), buffer.size());
	if(length < 0) return "error " + std::to_string(error);
	return {reinterpret_cast<const char*>(buffer.data()), static_cast<std::size_t>(length)};
}

/// @brief Frees a match data block when it goes out of scope.
struct MatchData {
	pcre2_match_data* data; ///< The block.
	/// @brief A block for a pattern's groups.
	/// @param code The pattern.
	explicit MatchData(const pcre2_code* code) : data(pcre2_match_data_create_from_pattern(code, nullptr)) {}
	MatchData(const MatchData&) = delete;
	MatchData& operator=(const MatchData&) = delete;
	/// @brief Frees it.
	~MatchData() { pcre2_match_data_free(data); }
};

/// @brief The compile options for some pattern options.
/// @param options The pattern options.
/// @return PCRE2's options.
[[nodiscard]] std::uint32_t compileOptions(PatternOptions options) noexcept {
	// PCRE2_LITERAL refuses PCRE2_UCP: plain text has no classes for it to change
	std::uint32_t flags = PCRE2_UTF | PCRE2_MATCH_INVALID_UTF;
	if(!options.regex) flags |= PCRE2_LITERAL;
	else flags |= PCRE2_UCP | PCRE2_MULTILINE;
	if(!options.caseSensitive) flags |= PCRE2_CASELESS;
	return flags;
}

} // namespace

TextPattern::TextPattern() = default;
TextPattern::TextPattern(TextPattern&& other) noexcept = default;
TextPattern& TextPattern::operator=(TextPattern&& other) noexcept = default;
TextPattern::~TextPattern() = default;

std::optional<std::string> TextPattern::compile(std::string_view pattern, PatternOptions options) {
	code_.reset();
	regex_ = options.regex;
	if(pattern.empty()) return std::nullopt;
	if(pattern.size() > maxPatternBytes) return "the pattern is longer than " + std::to_string(maxPatternBytes) + " bytes";
	pcre2_compile_context* compileContext = pcre2_compile_context_create(nullptr);
	if(compileContext == nullptr) return "out of memory";
	pcre2_set_parens_nest_limit(compileContext, 64);
	if(options.wholeWord) pcre2_set_compile_extra_options(compileContext, PCRE2_EXTRA_MATCH_WORD);
	int error = 0;
	PCRE2_SIZE errorOffset = 0;
	pcre2_code* code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(),
	                                 compileOptions(options), &error, &errorOffset, compileContext);
	pcre2_compile_context_free(compileContext);
	if(code == nullptr) return errorMessage(error) + " at " + std::to_string(errorOffset);
	auto compiled = std::make_unique<Code>();
	compiled->code = code;
	compiled->context = pcre2_match_context_create(nullptr);
	if(compiled->context == nullptr) return "out of memory";
	pcre2_set_match_limit(compiled->context, maxSteps);
	pcre2_set_depth_limit(compiled->context, maxDepth);
	pcre2_set_heap_limit(compiled->context, maxHeapKibibytes);
	code_ = std::move(compiled);
	return std::nullopt;
}

PatternFind TextPattern::find(std::string_view subject, std::size_t start) const {
	PatternFind result;
	if(code_ == nullptr || start > subject.size()) return result;
	const MatchData match(code_->code);
	if(match.data == nullptr) return result;
	const int found = pcre2_match(code_->code, reinterpret_cast<PCRE2_SPTR>(subject.data()), subject.size(), start, 0,
	                              match.data, code_->context);
	if(found == PCRE2_ERROR_MATCHLIMIT || found == PCRE2_ERROR_DEPTHLIMIT || found == PCRE2_ERROR_HEAPLIMIT)
		result.exceeded = true;
	if(found < 0) return result;
	const PCRE2_SIZE* vector = pcre2_get_ovector_pointer(match.data);
	if(vector[1] < vector[0]) return result; // \K in a lookahead can end a match before its start
	result.match = PatternMatch{vector[0], vector[1] - vector[0]};
	return result;
}

std::optional<std::string> TextPattern::expand(std::string_view subject, PatternMatch match,
                                               std::string_view replacement) const {
	if(!regex_ || code_ == nullptr) return std::string(replacement);
	const std::uint32_t options = PCRE2_ANCHORED | PCRE2_SUBSTITUTE_REPLACEMENT_ONLY | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;
	std::string out(replacement.size() + match.length + 64, '\0');
	for(int attempt = 0; attempt < 2; ++attempt) {
		PCRE2_SIZE length = out.size();
		const int done = pcre2_substitute(code_->code, reinterpret_cast<PCRE2_SPTR>(subject.data()), subject.size(),
		                                  match.offset, options, nullptr, code_->context,
		                                  reinterpret_cast<PCRE2_SPTR>(replacement.data()), replacement.size(),
		                                  reinterpret_cast<PCRE2_UCHAR*>(out.data()), &length);
		if(done == PCRE2_ERROR_NOMEMORY) {
			if(length > maxExpandedBytes) return std::nullopt;
			out.resize(length); // the length it needs
			continue;
		}
		if(done < 1) return std::nullopt;
		out.resize(length);
		return out;
	}
	return std::nullopt;
}

} // namespace affresco
