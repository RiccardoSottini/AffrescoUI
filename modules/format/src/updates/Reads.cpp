/// @file
/// @brief format::ReadSet and format::StateChanges: places read and places changed, and when they meet.

#include "Reads.hpp"

#include <algorithm>

namespace affresco::format {

namespace {

/// @brief Takes the next key off a JSON Pointer.
/// @param pointer The pointer, "/key..."; loses its first key.
/// @return The key.
std::string_view nextKey(std::string_view& pointer) noexcept {
	pointer.remove_prefix(1);
	const std::size_t end = std::min(pointer.find('/'), pointer.size());
	const std::string_view key = pointer.substr(0, end);
	pointer.remove_prefix(end);
	return key;
}

/// @brief The number of keys of a place.
/// @param pointer The place, as a JSON Pointer.
/// @return Its keys.
std::size_t keysOf(std::string_view pointer) noexcept {
	return static_cast<std::size_t>(std::ranges::count(pointer, '/'));
}

/// @brief Two places with as many keys that differ in one, merged with anyKey there.
/// @param a A place.
/// @param b Another.
/// @return The merged place, or empty if they differ in more or fewer keys than one.
std::string merged(std::string_view a, std::string_view b) {
	if(keysOf(a) != keysOf(b)) return {};
	std::string result;
	int differ = 0;
	while(!a.empty()) {
		const std::string_view x = nextKey(a);
		const std::string_view y = nextKey(b);
		result += '/';
		if(x == y) {
			result += x;
		} else {
			if(++differ > 1) return {};
			result += anyKey;
		}
	}
	return differ == 1 ? result : std::string();
}

/// @brief The common place of two places, cut after a whole key.
/// @param a A place.
/// @param b Another.
/// @return The longest place that contains both ("" when they share no key).
std::string_view common(std::string_view a, std::string_view b) noexcept {
	const auto [end, ignored] = std::ranges::mismatch(a, b);
	const std::string_view same = a.substr(0, static_cast<std::size_t>(end - a.begin()));
	if(contains(same, a) && contains(same, b)) return same;
	const std::size_t slash = same.rfind('/');
	return slash == std::string_view::npos ? std::string_view() : a.substr(0, slash);
}

} // namespace

bool contains(std::string_view outer, std::string_view inner, bool both) noexcept {
	if(outer.find('~') == std::string_view::npos && inner.find('~') == std::string_view::npos)
		return inner.starts_with(outer) && (inner.size() == outer.size() || inner[outer.size()] == '/');
	while(!outer.empty()) {
		if(inner.empty()) return false;
		const std::string_view a = nextKey(outer);
		const std::string_view b = nextKey(inner);
		if(a != b && a != anyKey && !(both && b == anyKey)) return false;
	}
	return true;
}

bool overlaps(std::string_view read, std::string_view written) noexcept {
	return contains(read, written, true) || contains(written, read, true);
}

void StateChanges::add(std::string pointer) {
	if(all || std::ranges::find(paths, pointer) != paths.end()) return;
	if(paths.size() >= maxChangedPaths) {
		addAll();
		return;
	}
	paths.push_back(std::move(pointer));
}

void ReadSet::addPath(std::string_view pointer) {
	if(std::ranges::any_of(paths, [pointer](const std::string& path) { return contains(path, pointer); })) return;
	std::erase_if(paths, [pointer](const std::string& path) { return contains(pointer, path); });
	paths.emplace_back(pointer);
	if(paths.size() > maxReadPaths) collapse();
}

void ReadSet::add(const ReadSet& other) {
	always = always || other.always;
	for(const std::string& path : other.paths) addPath(path);
}

bool ReadSet::generaliseOnce() {
	for(std::size_t i = 0; i < paths.size(); ++i) {
		for(std::size_t j = i + 1; j < paths.size(); ++j) {
			std::string place = merged(paths[i], paths[j]);
			if(place.empty()) continue;
			std::erase_if(paths, [&place](const std::string& path) { return contains(place, path); });
			paths.push_back(std::move(place));
			return true;
		}
	}
	return false;
}

void ReadSet::collapse() {
	while(paths.size() > maxReadPaths && generaliseOnce()) {}
	if(paths.size() <= maxReadPaths) return;
	std::string_view place = paths.front();
	for(const std::string& path : paths) place = common(place, path);
	const std::string kept(place); // a view into paths: copied before they go
	paths.assign(1, kept);
}

bool ReadSet::touchedBy(const StateChanges& changes) const noexcept {
	if(always || changes.all) return true;
	return std::ranges::any_of(changes.paths, [this](const std::string& written) {
		return std::ranges::any_of(paths, [&written](const std::string& read) { return overlaps(read, written); });
	});
}

void appendKey(std::string& pointer, std::string_view key) {
	pointer += '/';
	for(const char c : key) {
		if(c == '~') pointer += "~0";
		else if(c == '/') pointer += "~1";
		else pointer += c;
	}
}

} // namespace affresco::format
