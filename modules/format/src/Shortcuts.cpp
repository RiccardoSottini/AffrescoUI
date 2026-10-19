/// @file
/// @brief Key names and shortcuts.

#include "Shortcuts.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// The names of the keys that aren't letters, digits or function keys.
constexpr std::array<std::pair<Key, std::string_view>, 20> namedKeys = {{{Key::Enter, "enter"},
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
                                                                         {Key::Slash, "slash"}}};

/// The modifier keys, in the order a shortcut writes them.
enum Modifier { Command, Control, Alt, Shift };

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
		const auto modifier = std::ranges::find(modifierNames, part, &std::pair<std::string_view, Modifier>::first);
		if(modifier != modifierNames.end()) {
			held[modifier->second] = true;
			continue;
		}
		const auto alias = std::ranges::find(keyAliases, part, &std::pair<std::string_view, std::string_view>::first);
		key = alias != keyAliases.end() ? std::string(alias->second) : part;
	}
	return std::string(held[Command] ? "cmd+" : "") + (held[Control] ? "ctrl+" : "") + (held[Alt] ? "alt+" : "") +
	       (held[Shift] ? "shift+" : "") + key;
}

} // namespace affresco::format
