/// @file
/// @brief format::readTokens(): the "tokens" section, resolved once per document change for every theme.

#include "DesignTokens.hpp"
#include "PropertyValues.hpp"

#include <cmath>
#include <set>
#include <string>
#include <variant>

namespace affresco::format {

namespace {

constexpr int aliasDepthLimit = 16;         ///< "@a" -> "@b" -> ...: deeper (or circular) chains are errors.
constexpr std::size_t tokenLimit = 4096;    ///< Tokens of one document.
constexpr std::size_t themeLimit = 32;      ///< Themes the tokens and colours name, together.

/// @brief A token's value in one theme.
using TokenValue = std::variant<float, Color>;

/// @brief Resolves tokens, following aliases, in one theme at a time.
class TokenResolver {
public:
	/// @brief Prepares to resolve.
	/// @param tokens The "tokens" section.
	/// @param colors The colours of "colors" (aliases may name them).
	TokenResolver(const json& tokens, const ColorTable& colors) : tokens_(tokens), colors_(colors) {}

	/// @brief A token's value in a theme.
	/// @param name The token.
	/// @param theme The theme; empty for the base value.
	/// @param path Where it is used, for errors.
	/// @param depth The aliases followed so far.
	/// @return The value.
	/// @throws DocumentError If it is malformed or its aliases go too deep.
	TokenValue resolve(const std::string& name, std::string_view theme, const std::string& path, int depth) const {
		if(depth > aliasDepthLimit) fail(path, "aliases go deeper than 16 (or around in a circle)");
		const json& raw = tokens_[name];
		const json* value = &raw;
		if(raw.is_object()) {
			if(raw.empty()) fail(path, "expected at least one theme");
			const auto variant = raw.find(theme);
			value = variant != raw.end() ? &*variant : &raw.front();
		}
		if(value->is_number()) {
			const double number = value->get<double>();
			if(!std::isfinite(number)) fail(path, "expected a finite number");
			return static_cast<float>(number);
		}
		const std::string text = string(*value, path);
		if(text == "transparent") return colors::transparent;
		if(text.starts_with('@')) return alias(text.substr(1), theme, path, depth);
		const auto parsed = Color::parseHex(text);
		if(!parsed) fail(path, R"(expected a number, a color ("#rrggbb", "transparent") or "@another")");
		return *parsed;
	}

	/// @brief Every theme the tokens and the colours name.
	/// @return The theme names.
	[[nodiscard]] std::set<std::string, std::less<>> themes() const {
		std::set<std::string, std::less<>> names;
		for(const auto& [name, color] : colors_)
			for(const auto& [theme, value] : color.themes) names.insert(theme);
		for(const auto& [name, value] : tokens_.items())
			if(value.is_object())
				for(const auto& [theme, variant] : value.items()) names.insert(theme);
		return names;
	}

private:
	/// @brief Resolves "@name": a colour of "colors", or another token.
	/// @param name The name.
	/// @param theme The theme.
	/// @param path Where it is used, for errors.
	/// @param depth The aliases followed so far.
	/// @return The value.
	TokenValue alias(const std::string& name, std::string_view theme, const std::string& path, int depth) const {
		if(const auto color = colors_.find(name); color != colors_.end()) return color->second.in(theme);
		if(!tokens_.contains(name)) fail(path, "unknown token '@" + name + "'");
		return resolve(name, theme, path, depth + 1);
	}

	const json& tokens_;       ///< The section.
	const ColorTable& colors_; ///< From "colors".
};

/// @brief Collects one token's value per theme into a table.
/// @tparam Value float or Color.
/// @param resolver The resolver.
/// @param name The token.
/// @param base Its base value.
/// @param themes Every theme.
/// @param path Its place, for errors.
/// @return The themed value.
template <typename Value>
Themed<Value> collect(const TokenResolver& resolver, const std::string& name, Value base,
                      const std::set<std::string, std::less<>>& themes, const std::string& path) {
	Themed<Value> result{.base = base, .themes = {}};
	for(const std::string& theme : themes) {
		const TokenValue value = resolver.resolve(name, theme, path, 0);
		if(!std::holds_alternative<Value>(value)) fail(path, "a token is either a number or a color in every theme");
		result.themes.emplace(theme, std::get<Value>(value));
	}
	return result;
}

} // namespace

NumberTable readTokens(const json& document, ColorTable& colors) {
	NumberTable numbers;
	if(!document.contains("tokens")) return numbers;
	const json& tokens = object(document["tokens"], "tokens");

	if(tokens.size() > tokenLimit) fail("tokens", "at most " + std::to_string(tokenLimit) + " tokens");
	const TokenResolver resolver(tokens, colors);
	const auto themes = resolver.themes();
	if(themes.size() > themeLimit) fail("tokens", "at most " + std::to_string(themeLimit) + " themes");
	ColorTable newColors;
	for(const auto& [name, value] : tokens.items()) {
		const std::string path = "tokens." + name;
		if(colors.contains(name)) fail(path, "\"colors\" has a color of this name too");
		const TokenValue base = resolver.resolve(name, "", path, 0);
		if(const float* number = std::get_if<float>(&base))
			numbers.emplace(name, collect(resolver, name, *number, themes, path));
		else newColors.emplace(name, collect(resolver, name, std::get<Color>(base), themes, path));
	}
	colors.merge(newColors);
	return numbers;
}

} // namespace affresco::format
