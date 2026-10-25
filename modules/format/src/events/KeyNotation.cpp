/// @file
/// @brief Key notations: the platform's, macOS's, a PC's and Emacs's.

#include "KeyNotation.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace affresco::format {

namespace {

/// The labels of the keys whose names are words, for the mac and pc notations.
constexpr std::array<std::pair<std::string_view, std::string_view>, 15> wordLabels = {
    {{"minus", "-"},          {"equal", "="},          {"comma", ","},          {"period", "."},
     {"slash", "/"},          {"backslash", "\\"},     {"semicolon", ";"},      {"apostrophe", "'"},
     {"leftbracket", "["},    {"rightbracket", "]"},   {"grave", "`"},          {"numpadperiod", "Numpad ."},
     {"numpadplus", "Numpad +"}, {"numpadminus", "Numpad -"}, {"numpadmultiply", "Numpad *"}}};

/// Emacs's names of the keys whose names are words; other words are written "<name>".
constexpr std::array<std::pair<std::string_view, std::string_view>, 18> emacsLabels = {
    {{"enter", "RET"},
     {"tab", "TAB"},
     {"escape", "ESC"},
     {"backspace", "DEL"},
     {"space", "SPC"},
     {"pageup", "<prior>"},
     {"pagedown", "<next>"},
     {"minus", "-"},
     {"equal", "="},
     {"comma", ","},
     {"period", "."},
     {"slash", "/"},
     {"grave", "`"},
     {"leftbracket", "["},
     {"rightbracket", "]"},
     {"semicolon", ";"},
     {"apostrophe", "'"},
     {"backslash", "\\"}}};

/// The characters Shift makes of the keys that type one, for the Emacs notation: "M-<" is Alt+Shift+Comma.
constexpr std::string_view plainKeys = "`1234567890-=[]\\;',./", shiftedKeys = "~!@#$%^&*()_+{}|:\"<>?";

/// One normalised press taken apart: "cmd+ctrl+alt+shift+" and the key's name.
struct Press {
	bool cmd = false, ctrl = false, alt = false, shift = false; ///< The modifiers held.
	std::string key;                                            ///< The key's name: "k", "enter", "f3".
};

/// @brief Takes a normalised press apart.
/// @param press "cmd+shift+k".
/// @return Its modifiers and key.
Press parse(std::string_view press) {
	const std::size_t last = press.rfind('+');
	const std::string_view mods = last == std::string_view::npos ? "" : press.substr(0, last + 1);
	return {mods.contains("cmd+"), mods.contains("ctrl+"), mods.contains("alt+"), mods.contains("shift+"),
	        std::string(last == std::string_view::npos ? press : press.substr(last + 1))};
}

/// @brief A press in the mac or pc notation: "⌃⌥⇧⌘K" or "Ctrl+Alt+Shift+Super+K".
/// @param press The press.
/// @param mac Whether in the mac notation.
/// @return Its label.
std::string platformLabel(const Press& press, bool mac) {
	std::string key = press.key;
	for(const auto& [name, label] : wordLabels)
		if(key == name) key = label;
	if(key.size() == 7 && key.starts_with("numpad")) key = "Numpad " + key.substr(6); // "numpad7" is "Numpad 7"
	if(!key.empty()) key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
	if(mac)
		return std::string(press.ctrl ? "⌃" : "") + (press.alt ? "⌥" : "") + (press.shift ? "⇧" : "") +
		       (press.cmd ? "⌘" : "") + key;
	return std::string(press.ctrl ? "Ctrl+" : "") + (press.alt ? "Alt+" : "") + (press.shift ? "Shift+" : "") +
	       (press.cmd ? "Super+" : "") + key;
}

/// @brief A press in Emacs's notation: "C-x", "M-<", "C-M-S-<f3>", "s-s"; Shift on a key that types a character is
/// that character ("A", "%"), and "S-" otherwise.
/// @param press The press.
/// @return Its label.
std::string emacsLabel(const Press& press) {
	std::string key = press.key;
	bool shift = press.shift;
	const std::size_t typed = key.size() == 1 ? plainKeys.find(key[0]) : std::string_view::npos;
	if(key.size() == 1 && std::isalpha(static_cast<unsigned char>(key[0])) && shift && !press.ctrl && !press.alt &&
	   !press.cmd) {
		key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
		shift = false;
	} else if(typed != std::string_view::npos && shift) {
		key = std::string(1, shiftedKeys[typed]);
		shift = false;
	} else if(key.size() > 1) {
		bool named = false;
		for(const auto& [name, label] : emacsLabels)
			if(key == name) {
				key = label;
				named = true;
			}
		if(!named) key = "<" + key + ">";
	}
	if(key.size() == 1) // the punctuation of the named keys, Shift turning them too: "M-<"
		if(const std::size_t at = plainKeys.find(key[0]); at != std::string_view::npos && shift) {
			key = std::string(1, shiftedKeys[at]);
			shift = false;
		}
	return std::string(press.ctrl ? "C-" : "") + (press.alt ? "M-" : "") + (shift ? "S-" : "") +
	       (press.cmd ? "s-" : "") + key;
}

} // namespace

bool knownKeyNotation(std::string_view notation) noexcept {
	return notation == "platform" || notation == "mac" || notation == "pc" || notation == "emacs";
}

std::string keyLabel(std::string_view chord, std::string_view notation) {
#ifdef __APPLE__
	const bool mac = notation != "pc";
#else
	const bool mac = notation == "mac";
#endif
	std::string label;
	for(std::size_t at = 0; at <= chord.size();) {
		const std::size_t end = std::min(chord.find(' ', at), chord.size());
		const Press press = parse(chord.substr(at, end - at));
		label += (label.empty() ? "" : " ") + (notation == "emacs" ? emacsLabel(press) : platformLabel(press, mac));
		at = end + 1;
	}
	return label;
}

} // namespace affresco::format
