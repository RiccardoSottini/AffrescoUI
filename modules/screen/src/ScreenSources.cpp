/// @file
/// @brief detail::SourceMap: which file each entry of a merged screen came from.

#include "ScreenSources.hpp"

#include <array>
#include <utility>

namespace affresco::detail {

namespace {

/// @brief Whether a section merges one level deeper (mergeSection() in Screen.cpp).
/// @param key The section's name.
/// @return Whether it does.
[[nodiscard]] bool mergesDeeper(std::string_view key) { return key == "menus" || key == "strings"; }

} // namespace

std::string SourceMap::child(std::string_view pointer, std::string_view key) {
	std::string out(pointer);
	out += '/';
	for(const char c : key) {
		if(c == '~') out += "~0";
		else if(c == '/') out += "~1";
		else out += c;
	}
	return out;
}

void SourceMap::claimDocument(const json& document, const std::string& file) {
	if(!document.is_object()) return;
	const auto claim = [&](const json& value, std::string pointer, int deeper, const auto& self) -> void {
		if(deeper == 0 || !value.is_object() || value.empty()) {
			entries_.try_emplace(std::move(pointer), file);
			return;
		}
		for(const auto& [key, member] : value.items()) self(member, child(pointer, key), deeper - 1, self);
	};
	for(const auto& [key, value] : document.items()) {
		if(key == "include" || key == "$schema") continue;
		claim(value, child("", key), mergesDeeper(key) ? 2 : 1, claim);
	}
}

void SourceMap::absorb(const SourceMap& part) {
	for(const auto& [pointer, file] : part.entries_) entries_.try_emplace(pointer, file);
}

void SourceMap::assign(std::string pointer, std::string file) { entries_[std::move(pointer)] = std::move(file); }

std::string SourceMap::ownerOf(std::string_view pointer) const {
	for(std::string_view at = pointer; !at.empty(); at = at.substr(0, at.rfind('/'))) {
		if(const auto found = entries_.find(at); found != entries_.end()) return found->second;
		if(at.rfind('/') == std::string_view::npos) break;
	}
	return {};
}

bool SourceMap::hasEntriesBelow(std::string_view pointer) const {
	const std::string prefix = std::string(pointer) + '/';
	const auto found = entries_.lower_bound(prefix);
	return found != entries_.end() && found->first.starts_with(prefix);
}

} // namespace affresco::detail
