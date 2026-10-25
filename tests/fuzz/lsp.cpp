/// @file
/// @brief Fuzzes what a language server sends (modules/services/src/lsp/LspProtocol.hpp): a result or a notification's
/// params, normalised as the `lsp` service gives it to a screen, the first byte picking what it answers and whether
/// its columns are UTF-16 units; and the same bytes as a line whose columns are mapped both ways.
///
/// A result is refused (a `std::runtime_error`) or within its limits: lists cut, texts UTF-8 and cut, every path a
/// file of the folder; a line's columns map back to where they started.

#include "../../modules/services/src/lsp/LspProtocol.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

/// @brief Small limits, so that an input of a few bytes can reach them.
/// @return The limits.
affresco::services::lsp::Limits smallLimits() {
	affresco::services::lsp::Limits limits;
	limits.diagnostics = limits.completions = limits.edits = limits.locations = limits.tokens = 8;
	limits.text = limits.documentation = 32;
	limits.editBytes = 64;
	limits.depth = 4;
	return limits;
}

/// @brief Checks what a screen gets, at any depth: lists within the limit, texts within theirs, paths relative and
/// inside the folder.
/// @param value The value.
/// @param depth How deep it is.
void checkBounded(const affresco::json& value, int depth) {
	if(depth > 64) affresco::fuzz::broken("a result is shallow");
	if(value.is_array()) {
		if(value.size() > 8) affresco::fuzz::broken("a list is within its limit");
		for(const affresco::json& item : value) checkBounded(item, depth + 1);
		return;
	}
	if(!value.is_object()) return;
	for(const auto& [key, item] : value.items()) {
		if(item.is_string() && key != "path" && key != "insert" && key != "filter" && key != "text" &&
		   item.get_ref<const std::string&>().size() > 64 + 3)
			affresco::fuzz::broken("a text is cut");
		if(key == "path" && item.is_string()) {
			const std::string& path = item.get_ref<const std::string&>();
			if(path.empty() || path.front() == '/' || path.starts_with("../") || path == "..")
				affresco::fuzz::broken("a path is a file of the folder");
		}
		checkBounded(item, depth + 1);
	}
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace lsp = affresco::services::lsp;
	static constexpr std::array<const char*, 11> kinds = {"hover",   "completion", "signature",        "definition",
	                                                      "references", "rename",  "format",           "symbols",
	                                                      "workspaceSymbols", "tokens", "diagnostics"};
	if(size == 0) return 0;
	const std::filesystem::path root("/folder");
	const std::string_view body = affresco::fuzz::text(data + 1, size - 1);
	// The bytes as a line: its columns map to UTF-16 units and back
	if(affresco::fuzz::isUtf8(affresco::json(std::string(body))))
		for(std::size_t points = 0; points <= body.size() + 2; ++points)
			if(lsp::codePointsOf(body, lsp::utf16Units(body, points)) != points)
				affresco::fuzz::broken("a column maps back to where it started");
	const affresco::json result = affresco::fuzz::untrusted(body);
	if(result.is_discarded()) return 0;
	const char* kind = kinds[data[0] % kinds.size()];
	const lsp::ColumnMapper halve = [](const std::string&, std::size_t, std::size_t column) { return column / 2; };
	affresco::json given;
	try {
		given = lsp::normalize(kind, result, root, "src/a.cpp", smallLimits(), data[0] & 0x80 ? halve : lsp::ColumnMapper());
	} catch(const std::runtime_error&) {
		return 0; // refused: an edit that leaves the folder, or diagnostics of no file of it
	}
	if(!affresco::fuzz::isUtf8(given)) affresco::fuzz::broken("what a screen gets is UTF-8");
	checkBounded(given, 0);
	return 0;
}
