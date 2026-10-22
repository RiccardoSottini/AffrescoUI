/// @file
/// @brief TokenFile: hashed tokens in a file.

#include "TokenFile.hpp"

#include <affresco/core/Sha256.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <stdexcept>

namespace affresco::server {

namespace {

/// @brief Reads 64 hex digits.
/// @param text The digits.
/// @return The 32 bytes, or std::nullopt if they aren't 64 hex digits.
std::optional<std::array<std::uint8_t, 32>> fromHex(std::string_view text) {
	if(text.size() != 64) return std::nullopt;
	std::array<std::uint8_t, 32> bytes{};
	const auto digit = [](char c) -> int {
		if(c >= '0' && c <= '9') return c - '0';
		if(c >= 'a' && c <= 'f') return c - 'a' + 10;
		return -1;
	};
	for(std::size_t i = 0; i < bytes.size(); ++i) {
		const int high = digit(text[2 * i]), low = digit(text[2 * i + 1]);
		if(high < 0 || low < 0) return std::nullopt;
		bytes[i] = static_cast<std::uint8_t>(high * 16 + low);
	}
	return bytes;
}

} // namespace

TokenFile::TokenFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if(!file) throw std::runtime_error("can't read the token file " + path.string());
	std::string line;
	for(int number = 1; std::getline(file, line); ++number) {
		if(line.ends_with('\r')) line.pop_back();
		if(line.empty() || line.starts_with('#')) continue;
		const std::size_t space = line.find(' ');
		const auto hash = space == std::string::npos ? std::nullopt : fromHex(std::string_view(line).substr(0, space));
		const std::string name = space == std::string::npos ? std::string() : line.substr(space + 1);
		if(!hash || !validName(name) || entries_.size() >= maxEntries)
			throw std::runtime_error("the token file's line " + std::to_string(number) +
			                         " isn't \"<sha256 hex> <name>\" (or there are too many lines)");
		entries_.push_back({*hash, name});
	}
}

// Every entry is compared in full, so the time doesn't say which entry, or how much of one, matched
std::optional<std::string> TokenFile::verify(std::string_view token) {
	const std::array<std::uint8_t, 32> hash = sha256::digest(token);
	const Entry* found = nullptr;
	for(const Entry& entry : entries_) {
		std::uint8_t difference = 0;
		for(std::size_t i = 0; i < hash.size(); ++i) difference |= static_cast<std::uint8_t>(hash[i] ^ entry.hash[i]);
		if(difference == 0 && !found) found = &entry;
	}
	if(!found) return std::nullopt;
	return found->name;
}

std::vector<std::string> TokenFile::people() const {
	std::vector<std::string> names;
	for(const Entry& entry : entries_)
		if(std::ranges::find(names, entry.name) == names.end()) names.push_back(entry.name);
	return names;
}

bool TokenFile::validName(std::string_view name) noexcept {
	return !name.empty() && name.size() <= 64 && std::ranges::all_of(name, [](char c) {
		       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
	       });
}

std::string TokenFile::add(const std::filesystem::path& path, std::string_view name) {
	if(!validName(name)) throw std::runtime_error("a name is 1 to 64 of A-Z, a-z, 0-9, '_' and '-'");
	std::random_device random; // the system's source (e.g. /dev/urandom), not a seeded generator
	std::string token;
	for(int i = 0; i < 8; ++i) {
		char part[9];
		std::snprintf(part, sizeof part, "%08x", static_cast<unsigned>(random()));
		token += part;
	}
	{
		std::ofstream file(path, std::ios::app);
		if(!file) throw std::runtime_error("can't write the token file " + path.string());
		file << sha256::hex(token) << ' ' << name << '\n';
		if(!file) throw std::runtime_error("can't write the token file " + path.string());
	}
	std::error_code ignored; // not every file system has POSIX permissions
	std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
	                             std::filesystem::perm_options::replace, ignored);
	return token;
}

} // namespace affresco::server
