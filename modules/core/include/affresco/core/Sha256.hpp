#pragma once

/// @file
/// @brief SHA-256 (FIPS 180-4): small, with no dependency. Commits and documents are named by it, PKCE (RFC 7636)
/// hashes its verifier with it, and a server keeps its peers' tokens hashed with it.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace affresco::sha256 {

/// @brief The parts of the hash; not for callers.
namespace detail {

/// The first 32 bits of the fractional parts of the cube roots of the first 64 primes.
inline constexpr std::array<std::uint32_t, 64> roundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/// @brief Rotates right.
/// @param x The word.
/// @param n The number of bits.
/// @return The rotated word.
inline constexpr std::uint32_t rotr(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

/// @brief Pads a message to a multiple of 64 bytes: a 1 bit, zeros, then the length in bits (big-endian).
/// @param data The message.
/// @return The padded message.
inline std::string padded(std::string_view data) {
	std::string message(data);
	const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8u;
	message.push_back(static_cast<char>(0x80));
	while(message.size() % 64 != 56) message.push_back('\0');
	for(unsigned shift = 56;; shift -= 8) {
		message.push_back(static_cast<char>((bits >> shift) & 0xffu));
		if(shift == 0) break;
	}
	return message;
}

/// @brief Adds one 64-byte chunk to the hash: its message schedule, then the 64 rounds of the compression function.
/// @param hash The hash so far, updated.
/// @param chunk The chunk's first byte.
inline void compress(std::array<std::uint32_t, 8>& hash, const char* chunk) {
	std::array<std::uint32_t, 64> w{};
	for(std::size_t i = 0; i < 16; ++i)
		for(std::size_t b = 0; b < 4; ++b)
			w[i] = (w[i] << 8) | static_cast<std::uint32_t>(static_cast<unsigned char>(chunk[i * 4 + b]));
	for(std::size_t i = 16; i < 64; ++i) {
		const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
		const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	auto [a, b, c, d, e, f, g, h] = hash;
	for(std::size_t i = 0; i < 64; ++i) {
		const std::uint32_t t1 =
		    h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + roundConstants[i] + w[i];
		const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	const std::array<std::uint32_t, 8> add = {a, b, c, d, e, f, g, h};
	for(std::size_t i = 0; i < 8; ++i) hash[i] += add[i];
}

} // namespace detail

/// @brief Hashes bytes.
/// @param data The bytes.
/// @return The 32 bytes of the digest.
[[nodiscard]] inline std::array<std::uint8_t, 32> digest(std::string_view data) {
	std::array<std::uint32_t, 8> hash = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	const std::string message = detail::padded(data);
	for(std::size_t chunk = 0; chunk < message.size(); chunk += 64) detail::compress(hash, message.data() + chunk);
	std::array<std::uint8_t, 32> bytes{};
	for(std::size_t i = 0; i < 32; ++i) bytes[i] = static_cast<std::uint8_t>(hash[i / 4] >> (24 - 8 * (i % 4)));
	return bytes;
}

/// @brief Hashes bytes, as hex.
/// @param data The bytes.
/// @return 64 lowercase hex digits.
[[nodiscard]] inline std::string hex(std::string_view data) {
	static constexpr std::string_view digits = "0123456789abcdef";
	std::string text;
	text.reserve(64);
	for(const std::uint8_t byte : digest(data)) {
		text.push_back(digits[byte >> 4]);
		text.push_back(digits[byte & 0xfu]);
	}
	return text;
}

} // namespace affresco::sha256
