/// @file
/// @brief The measure of a value's size the limits use.

#include "Limits.hpp"

#include <array>
#include <charconv>
#include <vector>

namespace affresco::format {

namespace {

/// @brief The size of a value that holds no others.
/// @param value A number, a text, a boolean or null.
/// @return Its size as JSON.
std::size_t scalarBytes(const json& value) {
	if(value.is_string()) return value.get_ref<const std::string&>().size() + 2;
	if(value.is_number()) {
		std::array<char, 32> digits{};
		char* const end = digits.data() + digits.size();
		const auto written = value.is_number_float() ? std::to_chars(digits.data(), end, value.get<double>())
		                                             : std::to_chars(digits.data(), end, value.get<std::int64_t>());
		return static_cast<std::size_t>(written.ptr - digits.data());
	}
	return value.is_boolean() && !value.get<bool>() ? 5 : 4; // false, true, null
}

} // namespace

// An explicit stack, not recursion: what is measured may come from a peer
std::size_t approximateBytes(const json& value, std::size_t enough) {
	std::size_t total = 0;
	std::vector<const json*> pending{&value};
	while(!pending.empty() && total <= enough) {
		const json* item = pending.back();
		pending.pop_back();
		if(!item->is_structured()) {
			total += scalarBytes(*item);
			continue;
		}
		total += 2 + (item->empty() ? 0 : item->size() - 1); // brackets and commas
		if(item->is_object())
			for(const auto& [key, inside] : item->items()) {
				total += key.size() + 3; // quotes and colon
				pending.push_back(&inside);
			}
		else
			for(const json& inside : *item) pending.push_back(&inside);
	}
	return total;
}

} // namespace affresco::format
