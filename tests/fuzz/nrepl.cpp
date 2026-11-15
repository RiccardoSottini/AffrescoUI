/// @file
/// @brief Fuzzes a connection's nREPL protocol (modules/net/src/nrepl/EditorLink.hpp) with an editor's messages. A
/// stub until the protocol's package (f2c) fills it, with the rules its answers keep.

#include "../../modules/net/src/nrepl/EditorLink.hpp" // by its path: clang-tidy reads this file without its include folders

#include <cstddef>
#include <cstdint>

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	static_cast<void>(data);
	static_cast<void>(size);
	return 0;
}
