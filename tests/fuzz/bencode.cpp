/// @file
/// @brief Fuzzes the editor server's bencode reader (modules/net/src/nrepl/Bencode.hpp) with an editor's bytes. A stub
/// until the wire's package (f2b) fills it, with the rules a decoded value keeps.

#include "../../modules/net/src/nrepl/Bencode.hpp" // by its path: clang-tidy reads this file without its include folders

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
