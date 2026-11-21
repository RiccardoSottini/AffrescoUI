/// @file
/// @brief Fuzzes the editor server's bencode reader (modules/net/src/nrepl/Bencode.hpp) with an editor's bytes: what
/// decodes encodes back to bytes that decode to the same value, and a reader given the bytes in two pieces reads
/// what decode() reads from them whole.

#include "../../modules/net/src/nrepl/Bencode.hpp" // by its path: clang-tidy reads this file without its include folders

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

/// @brief Stops the run because a rule was broken: libFuzzer saves the input as a crash.
/// @param rule What should have held.
[[noreturn]] void broken(const char* rule) noexcept {
	std::fputs("affrescoui fuzz: broken rule: ", stderr); // fputs: std::println may throw, and this is noexcept
	std::fputs(rule, stderr);
	std::fputs("\n", stderr);
	std::abort();
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	using namespace affresco::net::bencode;
	const std::string_view bytes(reinterpret_cast<const char*>(data), size);
	const Limits limits;
	const Decoded decoded = decode(bytes, limits);
	if(decoded.status == Read::Malformed && decoded.why.empty()) broken("a malformed value says why");

	// The same bytes in two pieces, the way a socket delivers them, read the same
	Reader reader(limits);
	Decoded pieces = reader.read(bytes.substr(0, size / 2));
	if(pieces.status == Read::NeedMore) pieces = reader.read(bytes);
	if(pieces.status != decoded.status || pieces.used != decoded.used)
		broken("a value read in two pieces reads as it does whole");

	if(decoded.status != Read::Value) return 0;
	if(decoded.used == 0 || decoded.used > size) broken("a value takes some of the bytes, and no more");
	const std::string written = encode(decoded.value);
	const Decoded again = decode(written, limits);
	if(again.status != Read::Value || again.used != written.size()) broken("what decodes, encoded, decodes whole");
	if(encode(again.value) != written) broken("what decodes, encoded and decoded, is the same value");
	return 0;
}
