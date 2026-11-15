/// @file
/// @brief Unit tests of the editor server's bencode (modules/net/src/nrepl/Bencode.hpp): every type, round trips,
/// each malformed input, and values that arrive a byte at a time.

#include "nrepl/Bencode.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <string_view>

using namespace affresco::net::bencode;

namespace {

/// @brief Decodes under the default limits.
/// @param bytes The bytes.
/// @return What decode() read.
Decoded read(std::string_view bytes) { return decode(bytes, Limits{}); }

/// @brief Tells whether two values are the same, in type and content.
/// @param left One.
/// @param right The other.
/// @return true if they are.
bool same(const Value& left, const Value& right) { return encode(left) == encode(right); }

/// @brief Checks that some bytes are malformed.
/// @param bytes The bytes.
/// @param limits The limits.
/// @return Why, as decode() said.
std::string malformed(std::string_view bytes, const Limits& limits = {}) {
	const Decoded decoded = decode(bytes, limits);
	CHECK(decoded.status == Read::Malformed);
	CHECK_FALSE(decoded.why.empty());
	return decoded.why;
}

/// @brief Makes a value from a generator: scalars, and lists and dictionaries a few levels deep.
/// @param random The generator.
/// @param depth How deep it may still go.
/// @return The value.
Value generate(std::mt19937& random, int depth) {
	const auto kind = std::uniform_int_distribution<int>(0, depth > 0 ? 3 : 1)(random);
	const auto size = std::uniform_int_distribution<int>(0, 5)(random);
	if(kind == 0) {
		const std::int64_t extremes[] = {0, -1, std::numeric_limits<std::int64_t>::min(),
		                                 std::numeric_limits<std::int64_t>::max()};
		if(size == 0) return Value(extremes[std::uniform_int_distribution<int>(0, 3)(random)]);
		return Value(std::uniform_int_distribution<std::int64_t>(-1000000, 1000000)(random));
	}
	if(kind == 1) {
		std::string bytes;
		for(int index = 0; index < size * 3; ++index)
			bytes += static_cast<char>(std::uniform_int_distribution<int>(0, 255)(random));
		return Value(std::move(bytes));
	}
	if(kind == 2) {
		Value::List items;
		for(int index = 0; index < size; ++index) items.push_back(generate(random, depth - 1));
		return Value(std::move(items));
	}
	Value::Dictionary entries;
	for(int index = 0; index < size; ++index)
		entries.insert_or_assign("key" + std::to_string(std::uniform_int_distribution<int>(0, 9)(random)),
		                         generate(random, depth - 1));
	return Value(std::move(entries));
}

} // namespace

TEST_CASE("nrepl_bencode: every type decodes, empty and nested") {
	const Decoded integer = read("i42e");
	REQUIRE(integer.status == Read::Value);
	CHECK(integer.used == 4);
	CHECK(integer.value.integer() == 42);
	CHECK(read("i0e").value.integer() == 0);
	CHECK(read("i-7e").value.integer() == -7);
	CHECK(read("i9223372036854775807e").value.integer() == std::numeric_limits<std::int64_t>::max());
	CHECK(read("i-9223372036854775808e").value.integer() == std::numeric_limits<std::int64_t>::min());

	CHECK(read("5:proof").value.string() == "proof");
	CHECK(read("0:").value.string().empty());
	CHECK(read("3:a\0b").status == Read::NeedMore); // a string_view of a literal stops at the NUL: 2 of 3 bytes
	CHECK(read(std::string_view("3:a\0b", 5)).value.string() == std::string("a\0b", 3));

	CHECK(read("le").value.list().empty());
	CHECK(read("de").value.dictionary().empty());
	const Decoded nested = read("d2:opl4:evalli1eee4:code5:(+ 1)e");
	REQUIRE(nested.status == Read::Value);
	CHECK(nested.value.find("code")->string() == "(+ 1)");
	const auto& list = nested.value.find("op")->list();
	REQUIRE(list.size() == 2);
	CHECK(list[0].string() == "eval");
	CHECK(list[1].list()[0].integer() == 1);
	CHECK(nested.value.find("absent") == nullptr);
}

TEST_CASE("nrepl_bencode: keys in any order are accepted, and encoding sorts them") {
	const Decoded decoded = read("d2:op4:eval2:id1:1e");
	REQUIRE(decoded.status == Read::Value);
	CHECK(encode(decoded.value) == "d2:id1:12:op4:evale");
}

TEST_CASE("nrepl_bencode: a value is followed by what comes next, left for the next call") {
	const Decoded decoded = read("i1ed2:op5:closee");
	REQUIRE(decoded.status == Read::Value);
	CHECK(decoded.used == 3);
}

TEST_CASE("nrepl_bencode: 1,000 generated values round-trip") {
	std::mt19937 random(20261115u);
	for(int index = 0; index < 1000; ++index) {
		const Value value = generate(random, 4);
		const std::string bytes = encode(value);
		const Decoded decoded = read(bytes);
		REQUIRE(decoded.status == Read::Value);
		CHECK(decoded.used == bytes.size());
		CHECK(same(decoded.value, value));
	}
}

TEST_CASE("nrepl_bencode: a first byte that starts no value is malformed") {
	malformed("x");
	malformed("e");
	malformed("-1:a");
	malformed("li1ex");
}

TEST_CASE("nrepl_bencode: an integer with a leading zero is malformed") { malformed("i03e"); }

TEST_CASE("nrepl_bencode: the integer -0 is malformed") {
	malformed("i-0e");
	malformed("i-0"); // as soon as it shows
}

TEST_CASE("nrepl_bencode: an integer with no digits is malformed") {
	malformed("ie");
	malformed("i-e");
	malformed("i1x2e");
}

TEST_CASE("nrepl_bencode: an integer outside 64 bits is malformed") {
	malformed("i9223372036854775808e");
	malformed("i-9223372036854775809e");
	malformed("i99999999999999999999999"); // before its end comes
}

TEST_CASE("nrepl_bencode: a length with a leading zero is malformed") { malformed("05:proof"); }

TEST_CASE("nrepl_bencode: a length over maxBytes is malformed, before its bytes come") {
	malformed("100:", Limits{.maxBytes = 64});
	malformed("9:", Limits{.maxBytes = 4});
}

TEST_CASE("nrepl_bencode: a length outside 64 bits is malformed") { malformed("99999999999999999999999:"); }

TEST_CASE("nrepl_bencode: nesting over maxDepth is malformed") {
	const Limits limits{.maxDepth = 3};
	CHECK(decode("llleee", limits).status == Read::Value);
	malformed("lllle", limits);
	malformed(std::string(100000, 'l')); // under the default limits: no native recursion
}

TEST_CASE("nrepl_bencode: a dictionary key that isn't a string is malformed") {
	malformed("di1e1:ae");
	malformed("dle1:ae");
}

TEST_CASE("nrepl_bencode: a dictionary key seen twice is malformed") { malformed("d1:ai1e1:ai2ee"); }

TEST_CASE("nrepl_bencode: a dictionary with more than maxKeys keys is malformed") {
	const Limits limits{.maxKeys = 2};
	CHECK(decode("d1:ai1e1:bi2ee", limits).status == Read::Value);
	malformed("d1:ai1e1:bi2e1:ci3ee", limits);
}

TEST_CASE("nrepl_bencode: a key with no value is malformed") { malformed("d1:ae"); }

TEST_CASE("nrepl_bencode: a message over maxBytes in all is malformed") {
	const Limits limits{.maxBytes = 16};
	CHECK(decode("l5:proof5:proofe", limits).status == Read::Value);   // 16 bytes
	CHECK(decode("l5:proof5:proof", limits).status == Read::NeedMore); // the 16th byte may still end it
	malformed("l5:proof5:proofi1ee", limits);
}

TEST_CASE("nrepl_bencode: more than maxValues values is malformed") {
	const Limits limits{.maxValues = 3};
	CHECK(decode("li1ei2ee", limits).status == Read::Value);
	malformed("li1ei2ei3ee", limits);
}

TEST_CASE("nrepl_bencode: every prefix of a valid message needs more, then the whole is a value") {
	const std::string message = "d4:code13:(println \"a\")2:id1:72:op4:eval7:session36:"
	                            "0123456789abcdef0123456789abcdef01234:argsli-12ei0ed1:xleeee";
	for(std::size_t length = 0; length < message.size(); ++length)
		CHECK_MESSAGE(read(std::string_view(message).substr(0, length)).status == Read::NeedMore, length);
	const Decoded whole = read(message);
	REQUIRE(whole.status == Read::Value);
	CHECK(whole.used == message.size());
}

TEST_CASE("nrepl_bencode: a reader given the bytes as they arrive reads the same value") {
	const std::string message = "d4:code4096:" + std::string(4096, 'x') + "2:op4:evale" + "i5e";
	Reader reader{Limits{}};
	std::size_t length = 0;
	Decoded decoded;
	while((decoded = reader.read(std::string_view(message).substr(0, ++length))).status == Read::NeedMore) {
	}
	REQUIRE(decoded.status == Read::Value);
	CHECK(decoded.used == message.size() - 3);
	CHECK(decoded.value.find("code")->string().size() == 4096);
	const Decoded next = reader.read(std::string_view(message).substr(decoded.used));
	REQUIRE(next.status == Read::Value);
	CHECK(next.value.integer() == 5);
}

TEST_CASE("nrepl_bencode: short and cut inputs neither crash nor hang") {
	CHECK(read("le").status == Read::Value);
	CHECK(read("5:proof").status == Read::Value);
	CHECK(read("i42e").status == Read::Value);
	CHECK(read("proof").status == Read::Malformed);
	CHECK(read("d2:op4:ev").status == Read::NeedMore);
	CHECK(read("d2:op").status == Read::NeedMore);
	CHECK(read("").status == Read::NeedMore);
}

TEST_CASE("nrepl_bencode: a reader that failed stays failed until reset") {
	Reader reader{Limits{}};
	CHECK(reader.read("x").status == Read::Malformed);
	CHECK(reader.read("i1e").status == Read::Malformed);
	reader.reset();
	CHECK(reader.read("i1e").status == Read::Value);
}
