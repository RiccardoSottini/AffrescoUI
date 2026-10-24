#pragma once

/// @file
/// @brief data::placeOf: a place in a document or a store for messages, "models.songs.fields.title", built in one
/// string (errors are built in loops over records and fields).

#include <initializer_list>
#include <string>
#include <string_view>

namespace affresco::data {

/// @brief A place inside another, joined with dots.
/// @param where The outer place.
/// @param keys The keys inside it, in order.
/// @return "where.key1.key2".
[[nodiscard]] inline std::string placeOf(std::string_view where, std::initializer_list<std::string_view> keys) {
	std::size_t size = where.size();
	for(const std::string_view key : keys) size += key.size() + 1;
	std::string place;
	place.reserve(size);
	place.append(where);
	for(const std::string_view key : keys) place.append(".").append(key);
	return place;
}

/// @brief A place inside another: "where.key".
/// @param where The outer place.
/// @param key The key.
/// @return The place.
[[nodiscard]] inline std::string placeOf(std::string_view where, std::string_view key) {
	return placeOf(where, {key});
}

} // namespace affresco::data
