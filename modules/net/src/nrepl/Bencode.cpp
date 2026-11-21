/// @file
/// @brief Bencode's Reader, decode() and encode() (Bencode.hpp). Reading keeps its own stack of the lists and
/// dictionaries it is inside, so an editor's nesting never reaches the native stack, and it allocates a string only
/// once all its bytes are there.

#include "Bencode.hpp"

#include <cstdint>
#include <limits>

namespace affresco::net::bencode {

namespace {

/// @brief What readDigits() read.
struct Digits {
	Read status = Read::NeedMore; ///< Value (read up to the terminator), NeedMore or Malformed.
	std::uint64_t value = 0;      ///< The number, when status is Read::Value.
	std::size_t end = 0;          ///< Just past the terminator, when status is Read::Value.
	const char* why = "";         ///< What is wrong, when status is Read::Malformed.
};

/// @brief Reads a decimal number up to a terminator: digits only, no leading zero, at most a limit. It reads at most
/// twenty digits before it stops, so reading it again as more bytes come costs little.
/// @param bytes The bytes.
/// @param at Where the digits start.
/// @param terminator The byte that ends them (`e` or `:`).
/// @param limit The largest number allowed.
/// @param integer Whether it is an integer (or a length), for the reason.
/// @return What it read.
Digits readDigits(std::string_view bytes, std::size_t at, char terminator, std::uint64_t limit, bool integer) {
	std::uint64_t value = 0;
	std::size_t index = at;
	for(; index < bytes.size() && bytes[index] != terminator; ++index) {
		const char character = bytes[index];
		if(character < '0' || character > '9')
			return {.status = Read::Malformed,
			        .why = integer ? "an integer has a byte that isn't a digit"
			                       : "a length has a byte that isn't a digit"};
		if(index > at && bytes[at] == '0')
			return {.status = Read::Malformed,
			        .why = integer ? "an integer has a leading zero" : "a length has a leading zero"};
		const auto digit = static_cast<std::uint64_t>(character - '0');
		if(value > (limit - digit) / 10u)
			return {.status = Read::Malformed,
			        .why = integer ? "an integer is outside 64 bits" : "a length is outside 64 bits"};
		value = value * 10u + digit;
	}
	if(index == bytes.size()) return {};
	if(index == at)
		return {.status = Read::Malformed, .why = integer ? "an integer has no digits" : "a length has no digits"};
	return {.status = Read::Value, .value = value, .end = index + 1};
}

/// @brief Appends a value's bytes.
/// @param value The value (a few levels deep: values written are the protocol's own).
/// @param into Receives them.
void write(const Value& value, std::string& into) {
	if(value.isInteger()) {
		into += 'i';
		into += std::to_string(value.integer());
		into += 'e';
	} else if(value.isString()) {
		into += std::to_string(value.string().size());
		into += ':';
		into += value.string();
	} else if(value.isList()) {
		into += 'l';
		for(const Value& item : value.list()) write(item, into);
		into += 'e';
	} else {
		into += 'd';
		for(const auto& [key, entry] : value.dictionary()) { // std::map: sorted by key, as bencode wants
			into += std::to_string(key.size());
			into += ':';
			into += key;
			write(entry, into);
		}
		into += 'e';
	}
}

} // namespace

Decoded Reader::read(std::string_view bytes) {
	if(failed_) return {.status = Read::Malformed, .value = {}, .used = 0, .why = why_};
	if(bytes.size() < wanted_) return {.status = Read::NeedMore, .value = {}, .used = 0, .why = {}};
	wanted_ = 0;
	while(position_ < bytes.size()) {
		std::optional<Read> stepped = step(bytes);
		if(position_ > limits_.maxBytes)
			stepped = fail("a message is longer than " + std::to_string(limits_.maxBytes) + " bytes");
		if(!stepped) continue;
		if(*stepped == Read::Malformed) return {.status = Read::Malformed, .value = {}, .used = 0, .why = why_};
		if(*stepped == Read::NeedMore) break;
		Decoded whole{.status = Read::Value, .value = std::move(result_), .used = position_, .why = {}};
		reset();
		return whole;
	}
	return {.status = Read::NeedMore, .value = {}, .used = 0, .why = {}};
}

void Reader::reset() noexcept {
	open_.clear();
	position_ = 0;
	wanted_ = 0;
	values_ = 0;
	result_ = Value();
	why_.clear();
	failed_ = false;
}

std::optional<Read> Reader::step(std::string_view bytes) {
	const char first = bytes[position_];
	const bool awaitingKey = !open_.empty() && open_.back().value.isDictionary() && !open_.back().hasKey;
	if(first == 'e' && !open_.empty()) return close();
	if(awaitingKey && (first < '0' || first > '9')) return fail("a dictionary key isn't a string");
	if(first >= '0' && first <= '9') return readString(bytes);
	if(first == 'i') return readInteger(bytes);
	if(first == 'l' || first == 'd') return open(first == 'd');
	return fail("a byte starts no value (bencode values start with i, l, d or a digit)");
}

std::optional<Read> Reader::readString(std::string_view bytes) {
	const Digits length = readDigits(bytes, position_, ':', std::numeric_limits<std::uint64_t>::max(), false);
	if(length.status == Read::Malformed) return fail(length.why);
	if(length.status == Read::NeedMore) return Read::NeedMore;
	if(length.value > limits_.maxBytes)
		return fail("a length is over the limit of " + std::to_string(limits_.maxBytes) + " bytes");
	const std::size_t end = length.end + static_cast<std::size_t>(length.value);
	if(end > limits_.maxBytes) return fail("a message is longer than " + std::to_string(limits_.maxBytes) + " bytes");
	if(bytes.size() < end) { // wait for the whole string, without reading its prefix again until it is there
		wanted_ = end;
		return Read::NeedMore;
	}
	if(!count()) return Read::Malformed;
	std::string text(bytes.substr(length.end, static_cast<std::size_t>(length.value)));
	position_ = end;
	return place(Value(std::move(text)));
}

std::optional<Read> Reader::readInteger(std::string_view bytes) {
	const std::size_t start = position_ + 1;
	if(start == bytes.size()) return Read::NeedMore;
	const bool negative = bytes[start] == '-';
	const std::size_t digits = negative ? start + 1 : start;
	if(negative && digits < bytes.size() && bytes[digits] == '0') return fail("an integer is -0 or has a leading zero");
	constexpr auto largest = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
	const Digits number = readDigits(bytes, digits, 'e', negative ? largest + 1u : largest, true);
	if(number.status == Read::Malformed) return fail(number.why);
	if(number.status == Read::NeedMore) return Read::NeedMore;
	if(!count()) return Read::Malformed;
	position_ = number.end;
	// Negating in unsigned arithmetic keeps -2^63, which has no positive int64_t
	const std::int64_t value =
	    negative ? static_cast<std::int64_t>(0u - number.value) : static_cast<std::int64_t>(number.value);
	return place(Value(value));
}

std::optional<Read> Reader::open(bool dictionary) {
	if(open_.size() >= limits_.maxDepth)
		return fail("lists and dictionaries are nested deeper than " + std::to_string(limits_.maxDepth));
	if(!count()) return Read::Malformed;
	open_.push_back(
	    {.value = dictionary ? Value(Value::Dictionary{}) : Value(Value::List{}), .key = {}, .hasKey = false});
	++position_;
	return std::nullopt;
}

std::optional<Read> Reader::close() {
	if(open_.back().hasKey) return fail("a dictionary key has no value");
	Value value = std::move(open_.back().value);
	open_.pop_back();
	++position_;
	return place(std::move(value));
}

std::optional<Read> Reader::place(Value value) {
	if(open_.empty()) {
		result_ = std::move(value);
		return Read::Value;
	}
	Open& inside = open_.back();
	if(inside.value.isList()) {
		inside.value.list().push_back(std::move(value));
		return std::nullopt;
	}
	auto& entries = inside.value.dictionary();
	if(inside.hasKey) {
		entries.emplace(std::move(inside.key), std::move(value));
		inside.key.clear();
		inside.hasKey = false;
		return std::nullopt;
	}
	const std::string& key = value.string(); // step() let only a string through as a key
	if(entries.contains(key)) return fail("a dictionary key comes twice");
	if(entries.size() >= limits_.maxKeys)
		return fail("a dictionary has more than " + std::to_string(limits_.maxKeys) + " keys");
	inside.key = key;
	inside.hasKey = true;
	return std::nullopt;
}

bool Reader::count() {
	if(values_ < limits_.maxValues) {
		++values_;
		return true;
	}
	failed_ = true;
	why_ = "a message has more than " + std::to_string(limits_.maxValues) + " values";
	return false;
}

Read Reader::fail(std::string why) {
	failed_ = true;
	why_ = std::move(why);
	return Read::Malformed;
}

Decoded decode(std::string_view bytes, const Limits& limits) {
	Reader reader(limits);
	return reader.read(bytes);
}

std::string encode(const Value& value) {
	std::string bytes;
	write(value, bytes);
	return bytes;
}

} // namespace affresco::net::bencode
