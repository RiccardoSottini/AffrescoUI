/// @file
/// @brief The names scenarios use: keys, state paths, node properties and file-safe names.

#include "Internal.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace affresco::automation::detail {

namespace {

/// @brief The keys with a name of their own (letters, digits and f1-f12 are counted out).
constexpr std::array<std::pair<std::string_view, Key>, 25> namedKeys = {{
    {"enter", Key::Enter},         {"escape", Key::Escape},       {"tab", Key::Tab},
    {"backspace", Key::Backspace}, {"delete", Key::Delete},       {"insert", Key::Insert},
    {"space", Key::Space},         {"left", Key::Left},           {"right", Key::Right},
    {"up", Key::Up},               {"down", Key::Down},           {"home", Key::Home},
    {"end", Key::End},             {"pageup", Key::PageUp},       {"pagedown", Key::PageDown},
    {"slash", Key::Slash},         {"backslash", Key::Backslash}, {"minus", Key::Minus},
    {"equal", Key::Equal},         {"comma", Key::Comma},         {"period", Key::Period},
    {"semicolon", Key::Semicolon}, {"apostrophe", Key::Apostrophe}, {"leftbracket", Key::LeftBracket},
    {"rightbracket", Key::RightBracket},
}};

#ifdef __APPLE__
constexpr int platformModifier = 0x8; ///< "mod": Cmd on macOS, as the format's shortcuts read it.
#else
constexpr int platformModifier = 0x2; ///< "mod": Ctrl elsewhere, as the format's shortcuts read it.
#endif

/// @brief The modifiers and their bits (Mods, GLFW's values). "mod" plays a screen's "mod+k" on every platform.
constexpr std::array<std::pair<std::string_view, int>, 5> modifiers = {{
    {"shift", 0x1}, {"ctrl", 0x2}, {"alt", 0x4}, {"cmd", 0x8}, {"mod", platformModifier},
}};

/// @brief The properties `expect` reads from a node.
constexpr std::array<std::string_view, 21> properties = {
    "text",  "textColor", "color",   "borderColor", "borderWidth", "shown", "visible", "focused", "hovered",
    "tooltip", "checked", "value", "x",           "y",           "width", "height", "scrollX", "scrollY",
    "role",  "name",      "description"};

/// @brief Reads a function key.
/// @param name "f1" to "f12".
/// @return The key, or std::nullopt.
std::optional<Key> functionKey(std::string_view name) {
	if(name.size() < 2 || name[0] != 'f') return std::nullopt;
	int number = 0;
	const auto [end, error] = std::from_chars(name.data() + 1, name.data() + name.size(), number);
	if(error != std::errc() || end != name.data() + name.size() || number < 1 || number > 12) return std::nullopt;
	return static_cast<Key>(static_cast<int>(Key::F1) + number - 1);
}

/// @brief Reads a key without modifiers.
/// @param name "enter", "a", "7", "f5".
/// @return The key, or std::nullopt.
std::optional<Key> keyOf(std::string_view name) {
	for(const auto& [named, key] : namedKeys)
		if(named == name) return key;
	if(name.size() == 1 && name[0] >= 'a' && name[0] <= 'z')
		return static_cast<Key>(static_cast<int>(Key::A) + (name[0] - 'a'));
	if(name.size() == 1 && name[0] >= '0' && name[0] <= '9')
		return static_cast<Key>(static_cast<int>(Key::Num0) + (name[0] - '0'));
	return functionKey(name);
}

/// @brief Reads a modifier.
/// @param name "shift", "ctrl", "alt", "cmd" or "mod".
/// @return Its bit in Mods, or std::nullopt.
std::optional<int> modifierBit(std::string_view name) {
	for(const auto& [modifier, bit] : modifiers)
		if(modifier == name) return bit;
	return std::nullopt;
}

/// @brief Tells whether a character can start a name in a state path.
/// @param c The character.
/// @return true for letters, '_' and '$'.
bool startsName(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

/// @brief Tells whether a character can continue a name in a state path.
/// @param c The character.
/// @return true for what starts a name, and digits.
bool continuesName(char c) noexcept { return startsName(c) || (c >= '0' && c <= '9'); }

/// @brief Reads a bracketed segment of a state path: [3], ['name'] or ["name"].
/// @param text The path.
/// @param at Where the '[' is; moved past the ']'.
/// @return The segment, or std::nullopt if it is malformed.
std::optional<std::string> bracketed(std::string_view text, std::size_t& at) {
	const std::size_t close = text.find(']', at);
	if(close == std::string_view::npos) return std::nullopt;
	const std::string_view inside = text.substr(at + 1, close - at - 1);
	at = close + 1;
	if(inside.size() >= 2 && (inside.front() == '\'' || inside.front() == '"') && inside.back() == inside.front())
		return std::string(inside.substr(1, inside.size() - 2));
	if(inside.empty() || !std::ranges::all_of(inside, [](char c) { return c >= '0' && c <= '9'; })) return std::nullopt;
	return std::string(inside);
}

} // namespace

void fail(const std::string& place, const std::string& message) {
	throw DocumentError("AffrescoUI scenarios: " + (place.empty() ? message : place + ": " + message));
}

std::optional<KeyPress> parseKey(std::string_view text) {
	KeyPress press;
	std::string_view rest = text;
	for(std::size_t plus = rest.find('+'); plus != std::string_view::npos && plus + 1 < rest.size();
	    plus = rest.find('+')) {
		const std::optional<int> bit = modifierBit(rest.substr(0, plus));
		if(!bit) return std::nullopt;
		press.mods.bits |= *bit;
		rest.remove_prefix(plus + 1);
	}
	const std::optional<Key> key = keyOf(rest);
	if(!key) return std::nullopt;
	press.key = *key;
	return press;
}

std::string keyNames() {
	std::string names;
	for(const auto& [name, key] : namedKeys) names += std::string(name) + ", ";
	return names + "a-z, 0-9, f1-f12, after any of shift+, ctrl+, alt+, cmd+, mod+ (cmd on macOS, ctrl elsewhere)";
}

std::optional<std::vector<std::string>> parseStatePath(std::string_view text) {
	std::vector<std::string> segments;
	std::size_t at = 0;
	while(at < text.size()) {
		if(text[at] == '[') {
			std::optional<std::string> segment = bracketed(text, at);
			if(!segment) return std::nullopt;
			segments.push_back(*std::move(segment));
			continue;
		}
		if(!segments.empty() && text[at++] != '.') return std::nullopt; // names after the first follow a '.'
		const std::size_t start = at;
		if(at >= text.size() || !startsName(text[at])) return std::nullopt;
		while(at < text.size() && continuesName(text[at])) ++at;
		segments.emplace_back(text.substr(start, at - start));
	}
	if(segments.empty()) return std::nullopt;
	return segments;
}

bool isReadableProperty(std::string_view name) { return std::ranges::find(properties, name) != properties.end(); }

std::string readableProperties() {
	std::string names;
	for(const std::string_view name : properties) names += (names.empty() ? "" : ", ") + std::string(name);
	return names;
}

bool isSafeFileName(std::string_view name) {
	return !name.empty() && name.size() <= 128 && name.front() != '.' && std::ranges::all_of(name, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
		       c == '.';
	});
}

} // namespace affresco::automation::detail
