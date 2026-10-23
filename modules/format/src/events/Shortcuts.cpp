/// @file
/// @brief Key names and shortcuts.

#include "Shortcuts.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// The names of the keys that aren't letters, digits or function keys.
constexpr std::array<std::pair<Key, std::string_view>, 41> namedKeys = {{{Key::Enter, "enter"},
                                                                         {Key::KeypadEnter, "enter"},
                                                                         {Key::Escape, "escape"},
                                                                         {Key::Tab, "tab"},
                                                                         {Key::Backspace, "backspace"},
                                                                         {Key::Delete, "delete"},
                                                                         {Key::Space, "space"},
                                                                         {Key::Up, "up"},
                                                                         {Key::Down, "down"},
                                                                         {Key::Left, "left"},
                                                                         {Key::Right, "right"},
                                                                         {Key::Home, "home"},
                                                                         {Key::End, "end"},
                                                                         {Key::PageUp, "pageup"},
                                                                         {Key::PageDown, "pagedown"},
                                                                         {Key::Minus, "minus"},
                                                                         {Key::Equal, "equal"},
                                                                         {Key::Comma, "comma"},
                                                                         {Key::Period, "period"},
                                                                         {Key::Slash, "slash"},
                                                                         {Key::Backslash, "backslash"},
                                                                         {Key::Semicolon, "semicolon"},
                                                                         {Key::Apostrophe, "apostrophe"},
                                                                         {Key::LeftBracket, "leftbracket"},
                                                                         {Key::RightBracket, "rightbracket"},
                                                                         {Key::GraveAccent, "grave"},
                                                                         {Key::Keypad0, "numpad0"},
                                                                         {Key::Keypad1, "numpad1"},
                                                                         {Key::Keypad2, "numpad2"},
                                                                         {Key::Keypad3, "numpad3"},
                                                                         {Key::Keypad4, "numpad4"},
                                                                         {Key::Keypad5, "numpad5"},
                                                                         {Key::Keypad6, "numpad6"},
                                                                         {Key::Keypad7, "numpad7"},
                                                                         {Key::Keypad8, "numpad8"},
                                                                         {Key::Keypad9, "numpad9"},
                                                                         {Key::KeypadDecimal, "numpadperiod"},
                                                                         {Key::KeypadAdd, "numpadplus"},
                                                                         {Key::KeypadSubtract, "numpadminus"},
                                                                         {Key::KeypadMultiply, "numpadmultiply"},
                                                                         {Key::GraveAccent, "backquote"}}};

/// The modifier keys, in the order a shortcut writes them.
enum Modifier { Command, Control, Alt, Shift }; // NOLINT(cppcoreguidelines-use-enum-class): it indexes held[]

/// "mod" is the platform's own: cmd on macOS, ctrl elsewhere.
#ifdef __APPLE__
constexpr Modifier platformModifier = Command;
#else
constexpr Modifier platformModifier = Control;
#endif

/// The names documents give the modifiers.
constexpr std::array<std::pair<std::string_view, Modifier>, 10> modifierNames = {{{"mod", platformModifier},
                                                                                  {"cmd", Command},
                                                                                  {"command", Command},
                                                                                  {"super", Command},
                                                                                  {"meta", Command},
                                                                                  {"ctrl", Control},
                                                                                  {"control", Control},
                                                                                  {"alt", Alt},
                                                                                  {"option", Alt},
                                                                                  {"shift", Shift}}};

/// Other names documents give keys.
constexpr std::array<std::pair<std::string_view, std::string_view>, 2> keyAliases = {
    {{"return", "enter"}, {"esc", "escape"}}};

/// @brief The parts of a shortcut as written, in lower case and without spaces: "Shift + K" is "shift", "k".
/// @param text The shortcut.
/// @return Its parts.
std::vector<std::string> shortcutParts(std::string_view text) {
	std::vector<std::string> parts(1);
	for(const char c : text) {
		if(c == '+') parts.emplace_back();
		else if(c != ' ') parts.back() += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return parts;
}

/// @brief The modifier a document names.
/// @param name The name, in lower case.
/// @return The modifier, or std::nullopt if it isn't one.
std::optional<Modifier> modifierNamed(std::string_view name) {
	for(const auto& [named, modifier] : modifierNames)
		if(name == named) return modifier;
	return std::nullopt;
}

/// The labels of the keys whose names are words, for keyLabel().
constexpr std::array<std::pair<std::string_view, std::string_view>, 16> keyLabels = {
    {{"minus", "-"},          {"equal", "="},          {"comma", ","},          {"period", "."},
     {"slash", "/"},          {"backslash", "\\"},     {"semicolon", ";"},      {"apostrophe", "'"},
     {"leftbracket", "["},    {"rightbracket", "]"},   {"grave", "`"},          {"numpadperiod", "Numpad ."},
     {"numpadplus", "Numpad +"}, {"numpadminus", "Numpad -"}, {"numpadmultiply", "Numpad *"}, {"backquote", "`"}}};

/// @brief The label of one normalised press: "⌘⇧K" on macOS, "Ctrl+Shift+K" elsewhere.
/// @param press The press, "cmd+shift+k".
/// @return Its label.
std::string pressLabel(std::string_view press) {
	const std::size_t last = press.rfind('+');
	const std::string_view mods = last == std::string_view::npos ? "" : press.substr(0, last + 1);
	std::string key(last == std::string_view::npos ? press : press.substr(last + 1));
	for(const auto& [name, label] : keyLabels)
		if(key == name) key = label;
	if(key.size() == 7 && key.starts_with("numpad")) key = "Numpad " + key.substr(6); // "numpad7" is "Numpad 7"
	if(!key.empty()) key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
	const auto has = [&](std::string_view name) { return mods.contains(name); };
#ifdef __APPLE__
	return std::string(has("ctrl+") ? "⌃" : "") + (has("alt+") ? "⌥" : "") + (has("shift+") ? "⇧" : "") +
	       (has("cmd+") ? "⌘" : "") + key;
#else
	return std::string(has("ctrl+") ? "Ctrl+" : "") + (has("alt+") ? "Alt+" : "") + (has("shift+") ? "Shift+" : "") +
	       (has("cmd+") ? "Super+" : "") + key;
#endif
}

} // namespace

std::string keyName(Key key) {
	const int code = static_cast<int>(key);
	if(key >= Key::A && key <= Key::Z) return std::string(1, static_cast<char>('a' + code - static_cast<int>(Key::A)));
	if(key >= Key::Num0 && key <= Key::Num9)
		return std::string(1, static_cast<char>('0' + code - static_cast<int>(Key::Num0)));
	if(key >= Key::F1 && key <= Key::F25) return "f" + std::to_string(code - static_cast<int>(Key::F1) + 1);
	for(const auto& [named, name] : namedKeys)
		if(named == key) return std::string(name);
	return "";
}

std::string shortcut(const KeyEvent& event) {
	const std::string name = keyName(event.key);
	if(name.empty()) return "";
	std::string result;
	if(event.mods.super()) result += "cmd+";
	if(event.mods.control()) result += "ctrl+";
	if(event.mods.alt()) result += "alt+";
	if(event.mods.shift()) result += "shift+";
	return result + name;
}

std::string normalizeShortcut(std::string_view text) {
	std::array<bool, 4> held{};
	std::string key;
	for(const std::string& part : shortcutParts(text)) {
		if(const std::optional<Modifier> modifier = modifierNamed(part)) {
			held[*modifier] = true;
			continue;
		}
		key = part;
		for(const auto& [alias, name] : keyAliases)
			if(part == alias) key = name;
	}
	return std::string(held[Command] ? "cmd+" : "") + (held[Control] ? "ctrl+" : "") + (held[Alt] ? "alt+" : "") +
	       (held[Shift] ? "shift+" : "") + key;
}

std::optional<KeyEvent> keyPress(std::string_view text) {
	const std::string press = normalizeShortcut(text);
	const std::size_t last = press.rfind('+');
	const std::string_view name =
	    last == std::string::npos ? std::string_view(press) : std::string_view(press).substr(last + 1);
	if(name.empty()) return std::nullopt;
	const std::string_view mods = std::string_view(press).substr(0, press.size() - name.size());
	for(int code = static_cast<int>(Key::Space); code <= static_cast<int>(Key::Menu); ++code) {
		if(keyName(static_cast<Key>(code)) != name) continue;
		KeyEvent event;
		event.key = static_cast<Key>(code);
		event.mods.bits = (mods.contains("shift+") ? 0x1 : 0) | (mods.contains("ctrl+") ? 0x2 : 0) |
		                  (mods.contains("alt+") ? 0x4 : 0) | (mods.contains("cmd+") ? 0x8 : 0);
		return event;
	}
	return std::nullopt;
}

char32_t typedBy(const KeyEvent& event) noexcept {
	if(event.mods.control() || event.mods.alt() || event.mods.super()) return 0;
	const int code = static_cast<int>(event.key);
	if(code < static_cast<int>(Key::Space) || code > static_cast<int>(Key::GraveAccent)) return 0; // not printable
	const bool shift = event.mods.shift();
	if(event.key >= Key::A && event.key <= Key::Z) return static_cast<char32_t>(shift ? code : code - 'A' + 'a');
	constexpr std::string_view plain = "`1234567890-=[]\\;',./", shifted = "~!@#$%^&*()_+{}|:\"<>?";
	const std::size_t at = plain.find(static_cast<char>(code));
	if(at == std::string_view::npos) return code == ' ' ? U' ' : 0;
	return static_cast<char32_t>(shift ? shifted[at] : plain[at]);
}

std::string normalizeChord(std::string_view text) {
	std::vector<std::string> presses;
	bool join = false; // the next word belongs to the last press: one is next to a '+', or the press has no key yet
	for(std::size_t at = 0; at < text.size();) {
		if(text[at] == ' ') {
			++at;
			continue;
		}
		const std::size_t end = std::min(text.find(' ', at), text.size());
		const std::string_view word = text.substr(at, end - at);
		if(join || (word.front() == '+' && !presses.empty())) {
			if(!presses.back().ends_with('+') && word.front() != '+') presses.back() += '+';
			presses.back() += word;
		} else {
			presses.emplace_back(word);
		}
		join = word.back() == '+' || normalizeShortcut(presses.back()).ends_with('+'); // "ctrl s" is "ctrl+s"
		at = end;
	}
	std::string chord;
	for(const std::string& press : presses) {
		const std::string normal = normalizeShortcut(press);
		if(!normal.empty()) chord += (chord.empty() ? "" : " ") + normal; // "x+" names no key: nothing to press
	}
	return chord;
}

std::string keyLabel(std::string_view chord) {
	std::string label;
	for(std::size_t at = 0; at <= chord.size();) {
		const std::size_t end = std::min(chord.find(' ', at), chord.size());
		label += (label.empty() ? "" : " ") + pressLabel(chord.substr(at, end - at));
		at = end + 1;
	}
	return label;
}

} // namespace affresco::format
