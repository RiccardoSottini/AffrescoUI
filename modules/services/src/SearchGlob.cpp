/// @file
/// @brief The search's globs and .gitignore rules (SearchGlob.hpp).

#include "SearchGlob.hpp"

#include <algorithm>
#include <utility>

namespace affresco::services {

namespace {

using Element = Glob::Element;

/// @brief Decodes the code point at a byte; an invalid sequence reads as its first byte alone.
/// @param text UTF-8 text.
/// @param at The byte (inside the text).
/// @return The code point and its bytes.
std::pair<char32_t, std::size_t> decode(std::string_view text, std::size_t at) noexcept {
	const auto byte = [&](std::size_t index) { return static_cast<unsigned char>(text[index]); };
	const unsigned char lead = byte(at);
	const std::size_t length = lead < 0x80           ? 1
	                           : (lead >> 5) == 0x6  ? 2
	                           : (lead >> 4) == 0xE  ? 3
	                           : (lead >> 3) == 0x1E ? 4
	                                                 : 0;
	if(length <= 1 || at + length > text.size()) return {lead, 1};
	char32_t point = lead & (0x7F >> length);
	for(std::size_t index = 1; index < length; ++index) {
		if((byte(at + index) & 0xC0) != 0x80) return {lead, 1};
		point = (point << 6) | (byte(at + index) & 0x3F);
	}
	return {point, length};
}

/// @brief Reads a `[...]` class.
/// @param text The segment.
/// @param at Its '['; moved past the ']' when it is a class.
/// @param element Gets the class.
/// @return Whether it is one (else the '[' is a literal).
bool readClass(std::string_view text, std::size_t& at, Element& element) {
	std::size_t index = at + 1;
	element.kind = Element::Kind::Class;
	element.negated = index < text.size() && (text[index] == '!' || text[index] == '^');
	if(element.negated) ++index;
	for(bool first = true; index < text.size(); first = false) {
		if(text[index] == ']' && !first) {
			at = index + 1;
			return true;
		}
		if(text[index] == '\\' && index + 1 < text.size()) ++index;
		const auto [low, lowBytes] = decode(text, index);
		index += lowBytes;
		char32_t high = low;
		if(index + 1 < text.size() && text[index] == '-' && text[index + 1] != ']') {
			const auto [upper, upperBytes] = decode(text, index + 1);
			high = upper;
			index += 1 + upperBytes;
		}
		element.ranges.emplace_back(std::min(low, high), std::max(low, high));
	}
	return false;
}

/// @brief Compiles one name's glob.
/// @param text The name's glob (no '/').
/// @return Its elements; consecutive stars are one.
std::vector<Element> readElements(std::string_view text) {
	std::vector<Element> elements;
	const auto literal = [&](char byte) {
		if(elements.empty() || elements.back().kind != Element::Kind::Literal) elements.push_back({});
		elements.back().literal += byte;
	};
	for(std::size_t at = 0; at < text.size();) {
		const char byte = text[at];
		if(byte == '\\' && at + 1 < text.size()) {
			literal(text[at + 1]);
			at += 2;
		} else if(byte == '*') {
			if(elements.empty() || elements.back().kind != Element::Kind::Star)
				elements.push_back({.kind = Element::Kind::Star});
			++at;
		} else if(byte == '?') {
			elements.push_back({.kind = Element::Kind::Any});
			++at;
		} else if(Element range; byte == '[' && readClass(text, at, range)) {
			elements.push_back(std::move(range));
		} else {
			literal(byte);
			++at;
		}
	}
	return elements;
}

/// @brief How many bytes of a name an element matches at a byte.
/// @param element The element (not a star).
/// @param name The name.
/// @param at The byte.
/// @return The bytes, or 0 if it doesn't match.
std::size_t consume(const Element& element, std::string_view name, std::size_t at) noexcept {
	if(element.kind == Element::Kind::Literal)
		return name.substr(at).starts_with(element.literal) ? element.literal.size() : 0;
	if(at >= name.size()) return 0;
	const auto [point, bytes] = decode(name, at);
	if(element.kind == Element::Kind::Any) return bytes;
	const bool inside = std::ranges::any_of(
	    element.ranges, [point](const auto& range) { return point >= range.first && point <= range.second; });
	return inside != element.negated ? bytes : 0;
}

/// @brief Matches a name against a segment's elements, going back to the last star on a mismatch (no recursion).
/// @param elements The elements.
/// @param name The name.
/// @return Whether it matches.
bool matchName(const std::vector<Element>& elements, std::string_view name) noexcept {
	constexpr std::size_t none = std::string_view::npos;
	std::size_t element = 0, at = 0, star = none, starAt = 0;
	while(at < name.size()) {
		if(element < elements.size() && elements[element].kind == Element::Kind::Star) {
			star = element++;
			starAt = at;
			continue;
		}
		if(element < elements.size())
			if(const std::size_t bytes = consume(elements[element], name, at)) {
				at += bytes;
				++element;
				continue;
			}
		if(star == none) return false;
		element = star + 1;
		starAt += decode(name, starAt).second;
		at = starAt;
	}
	while(element < elements.size() && elements[element].kind == Element::Kind::Star) ++element;
	return element == elements.size();
}

/// @brief Drops the spaces around a text.
/// @param text The text.
/// @return It, trimmed.
std::string_view trim(std::string_view text) noexcept {
	const std::size_t first = text.find_first_not_of(" \t\r");
	if(first == std::string_view::npos) return {};
	return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

} // namespace

std::optional<Glob> Glob::compile(std::string_view text) {
	if(text.empty() || text.size() > maxBytes) return std::nullopt;
	Glob glob;
	glob.folderOnly_ = text.ends_with('/');
	while(text.ends_with('/')) text.remove_suffix(1);
	const bool anchored = text.find('/') != std::string_view::npos;
	while(text.starts_with('/')) text.remove_prefix(1);
	if(text.empty()) return std::nullopt;
	if(!anchored) glob.segments_.push_back({.globstar = true});
	for(const std::string_view name : splitPath(text)) {
		const bool globstar = name == "**";
		if(globstar && !glob.segments_.empty() && glob.segments_.back().globstar) continue;
		glob.segments_.push_back(
		    {.globstar = globstar, .elements = globstar ? std::vector<Element>{} : readElements(name)});
	}
	return glob;
}

bool Glob::matches(std::span<const std::string_view> segments, bool folder) const {
	return (folder || !folderOnly_) && matchesNames(segments);
}

bool Glob::matchesOrParent(std::span<const std::string_view> segments, bool folder) const {
	for(std::size_t count = 1; count <= segments.size(); ++count)
		if(matches(segments.first(count), count < segments.size() || folder)) return true;
	return false;
}

bool Glob::matchesNames(std::span<const std::string_view> names) const {
	// As matchName(), a `**` for a star and a name for a character
	constexpr std::size_t none = std::string_view::npos;
	std::size_t segment = 0, name = 0, star = none, starName = 0;
	while(name < names.size()) {
		if(segment < segments_.size() && segments_[segment].globstar) {
			star = segment++;
			starName = name;
			continue;
		}
		if(segment < segments_.size() && matchName(segments_[segment].elements, names[name])) {
			++segment;
			++name;
			continue;
		}
		if(star == none) return false;
		segment = star + 1;
		name = ++starName;
	}
	while(segment < segments_.size() && segments_[segment].globstar) ++segment;
	return segment == segments_.size();
}

std::optional<std::string> GlobList::compile(std::string_view text) {
	globs_.clear();
	while(!text.empty()) {
		const std::size_t comma = std::min(text.find(','), text.size());
		const std::string_view entry = trim(text.substr(0, comma));
		text.remove_prefix(std::min(comma + 1, text.size()));
		if(entry.empty()) continue;
		if(globs_.size() == maxGlobs) return "at most 64 globs";
		std::optional<Glob> glob = Glob::compile(entry);
		if(!glob) return "a glob is longer than 256 bytes, or only slashes";
		globs_.push_back(std::move(*glob));
	}
	return std::nullopt;
}

bool GlobList::matches(std::span<const std::string_view> segments, bool folder) const {
	return std::ranges::any_of(globs_, [&](const Glob& glob) { return glob.matchesOrParent(segments, folder); });
}

void IgnoreRules::add(std::string_view base, std::string_view text) {
	text = text.substr(0, maxFileBytes);
	std::vector<std::string> folder;
	for(const std::string_view name : splitPath(base)) folder.emplace_back(name);
	while(!text.empty() && rules_.size() < maxRules) {
		const std::size_t end = std::min(text.find('\n'), text.size());
		std::string_view line = trim(text.substr(0, end));
		text.remove_prefix(std::min(end + 1, text.size()));
		if(line.empty() || line.starts_with('#')) continue;
		const bool negated = line.starts_with('!');
		if(negated) line.remove_prefix(1);
		if(line.starts_with("\\#") || line.starts_with("\\!")) line.remove_prefix(1);
		if(std::optional<Glob> glob = Glob::compile(line))
			rules_.push_back({.glob = std::move(*glob), .base = folder, .negated = negated});
	}
}

bool IgnoreRules::ignored(std::span<const std::string_view> segments, bool folder) const {
	for(auto rule = rules_.rbegin(); rule != rules_.rend(); ++rule) {
		if(rule->base.size() >= segments.size() || !std::equal(rule->base.begin(), rule->base.end(), segments.begin()))
			continue;
		if(rule->glob.matches(segments.subspan(rule->base.size()), folder)) return !rule->negated;
	}
	return false;
}

std::vector<std::string_view> splitPath(std::string_view path) {
	std::vector<std::string_view> names;
	while(!path.empty()) {
		const std::size_t slash = std::min(path.find('/'), path.size());
		if(slash) names.push_back(path.substr(0, slash));
		path.remove_prefix(std::min(slash + 1, path.size()));
	}
	return names;
}

} // namespace affresco::services
