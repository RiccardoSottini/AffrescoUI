#pragma once

/// @file
/// @brief What the fuzz targets share: the input as text, the template they load, and how a broken rule is reported.

#include <affresco/core/Json.hpp>
#include <affresco/core/Log.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
/// @brief Reads a JSON file of the repository.
/// @param path The file.
/// @return Its value.
inline json readJsonFile(const std::filesystem::path& path) {
	const std::ifstream file(path);
	if(!file) broken("a template's file can be read");
	std::stringstream content;
	content << file.rdbuf();
	return json::parse(content.str());
}

/// @brief The Kanban template (examples/flow), read once: the document version control is fuzzed with.
/// Its "include" files are merged as a screen file's are when it is read (a Repository takes a merged document): what
/// the template lacks comes from them, and its own entries win.
/// @return The document.
inline const json& kanbanTemplate() {
	static const json document = [] {
		const std::filesystem::path path(AFFRESCOUI_FUZZ_TEMPLATE);
		json screen = readJsonFile(path);
		for(const json& name : screen.value("include", json::array())) {
			for(const auto& [key, value] : readJsonFile(path.parent_path() / name.get<std::string>()).items()) {
				if(key == "$schema") continue;
				if(!screen.contains(key)) screen[key] = value;
				else if(value.is_object() && screen[key].is_object())
					for(const auto& [entry, definition] : value.items())
						if(!screen[key].contains(entry)) screen[key][entry] = definition;
			}
		}
		screen.erase("include");
		return screen;
	}();
	return document;
}
#endif

} // namespace affresco::fuzz
