#pragma once

/// @file
/// @brief What the fuzz targets share: the input as text, the template they load, and how a broken rule is reported.

#include <affresco/core/Json.hpp>
#include <affresco/core/Log.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string_view>

namespace affresco::fuzz {

/// @brief Reads libFuzzer's input as text.
/// @param data The bytes.
/// @param size How many.
/// @return A view of them, valid for the call.
inline std::string_view text(const std::uint8_t* data, std::size_t size) noexcept {
	return {reinterpret_cast<const char*>(data), size};
}

/// @brief Stops the run because a rule the app relies on was broken: libFuzzer saves the input as a crash.
/// @param rule What should have held.
[[noreturn]] inline void broken(const char* rule) noexcept {
	std::fprintf(stderr, "affrescoui fuzz: broken rule: %s\n", rule);
	std::abort();
}

/// @brief Tells whether a string value is valid UTF-8, as every string parsed from JSON is.
/// @param value A string value.
/// @return true if it can be written out as JSON.
inline bool isUtf8(const json& value) {
	try {
		static_cast<void>(value.dump());
		return true;
	} catch(const json::type_error&) {
		return false;
	}
}

/// @brief Keeps the library's log quiet, from LLVMFuzzerInitialize: a refused revision logs a warning, and fuzzing
/// refuses millions.
inline void silenceLog() {
	setLogSink([](LogLevel, std::string_view) {});
}

#ifdef AFFRESCOUI_FUZZ_TEMPLATE // set for every target (CMakeLists.txt); clang-tidy reads this file without it
/// @brief The Kanban template (examples/assets/screens), read once: the document version control is fuzzed with.
/// @return The document.
inline const json& kanbanTemplate() {
	static const json document = [] {
		std::ifstream file(AFFRESCOUI_FUZZ_TEMPLATE);
		if(!file) broken("the template " AFFRESCOUI_FUZZ_TEMPLATE " can be read");
		std::stringstream content;
		content << file.rdbuf();
		return json::parse(content.str());
	}();
	return document;
}
#endif

} // namespace affresco::fuzz
