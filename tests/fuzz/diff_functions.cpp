/// @file
/// @brief Fuzzes the diff and JSON expression functions (format's ExpressionFunctionsDiff): a peer's documents, patches
/// and texts reach diffLines(), atPointer() and applyPatch() through a screen's state.
///
/// The input's first byte picks diffLines()'s options; the rest is cut at its first zero byte into an original and a
/// modified text, diffed, and, when the two parse as JSON, the first is patched with the second and read at a pointer
/// made of the second text. Each call gives a value or throws DocumentError, as the runtime expects, and every line
/// number diffLines() gives lies inside its text.

#include "Expression.hpp"
#include "Fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

using affresco::json;

namespace {

/// @brief Evaluates an expression with a state.
/// @param expression The expression, without its braces.
/// @param state What it sees as `state`.
/// @return Its value.
/// @throws DocumentError If it is invalid.
json evaluate(const std::string& expression, const json& state) {
	static const json window = {{"width", 800}, {"height", 600}}, revision = 1;
	static const json event = json::object(), globals = json::object();
	static const affresco::format::Scope locals{};
	const affresco::format::Context context{state, window, revision, event, locals, globals};
	return affresco::format::evaluate("{{" + expression + "}}", context, "fuzz");
}

/// @brief Checks that every line number of a diff lies inside its texts.
/// @param diff diffLines()'s result.
/// @param originalLines The original's line count, at least.
/// @param modifiedLines The modified text's.
void checkNumbers(const json& diff, std::size_t originalLines, std::size_t modifiedLines) {
	const auto outside = [](const json& number, std::size_t lines) {
		return number.is_number() && (number.get<std::size_t>() < 1 || number.get<std::size_t>() > lines);
	};
	for(const json& hunk : diff.at("hunks"))
		for(const json& line : hunk.at("lines"))
			if(outside(line.at("old"), originalLines) || outside(line.at("new"), modifiedLines)) std::abort();
}

/// @brief Parses a text as JSON, or gives a text value.
/// @param text The text.
/// @return The value.
json valueOf(std::string_view text) {
	try {
		return affresco::parseUntrusted(text);
	} catch(const std::exception&) {
		return std::string(text);
	}
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const std::uint8_t pick = data[0];
	const std::string_view text = affresco::fuzz::text(data + 1, size - 1);
	const std::size_t zero = text.find('\0');
	const std::string original(text.substr(0, zero));
	const std::string modified(zero == std::string_view::npos ? std::string_view() : text.substr(zero + 1));
	if(!affresco::fuzz::isUtf8(json(original)) || !affresco::fuzz::isUtf8(json(modified))) return 0; // states hold JSON's UTF-8
	const json state = {{"a", original},
	                    {"b", modified},
	                    {"context", static_cast<int>(pick % 8) - 1},
	                    {"ignore", (pick & 8) != 0},
	                    {"split", (pick & 16) != 0},
	                    {"limit", (pick & 32) != 0 ? 7 : 5000},
	                    {"doc", valueOf(original)},
	                    {"patch", valueOf(modified)}};
	try {
		const json diff = evaluate("diffLines(state.a, state.b, { context: state.context, ignoreWhitespace: "
		                           "state.ignore, split: state.split, limit: state.limit })",
		                           state);
		checkNumbers(diff, original.size() + 1, modified.size() + 1);
		static_cast<void>(evaluate("applyPatch(state.doc, state.patch)", state));
		static_cast<void>(evaluate("atPointer(state.doc, state.b)", state));
		static_cast<void>(evaluate("formatJson(state.doc, state.context)", state));
	} catch(const affresco::DocumentError&) {
	}
	return 0;
}
