#pragma once

/// @file
/// @brief format::Strings: the "strings" section, the translations of a screen's text per locale, and the messages
/// `t("key", {params})` formats from them: `{name}`, `{n, plural, one {# file} other {# files}}`,
/// `{kind, select, a {...} other {...}}` (a subset of ICU MessageFormat).
///
///     "strings": { "default": "en", "locale": "{{state.locale}}",
///                  "en": { "files": "{n, plural, =0 {No files} one {# file} other {# files}}" },
///                  "it": { "files": "{n, plural, =0 {Nessun file} one {# file} other {# file}}" } }
///
/// A key is looked up along the locale's fallback chain: "fr-CA", then "fr", then "default"; a key found nowhere
/// is shown as itself.

#include "Locale.hpp"
#include "Value.hpp"

#include "affresco/core/Json.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

inline constexpr std::size_t stringsLocaleLimit = 64;    ///< Locales one "strings" section may have.
inline constexpr std::size_t stringsKeyLimit = 10000;    ///< Keys one locale may have.
inline constexpr std::size_t stringsMessageLimit = 4096; ///< Bytes of one message.
inline constexpr std::size_t stringsNestingLimit = 8;    ///< Plurals and selects one inside another.

/// @brief Checks the "strings" section: locales of keys to valid messages, "default" and "locale" text.
/// @param section The section.
/// @param name Its place, "strings".
/// @throws DocumentError If something is malformed, or past a limit.
void checkStrings(const json& section, const std::string& name);

/// @brief Checks a message and lists the parameters it reads.
/// @param message The message.
/// @param parameters Gets the names of its parameters, each once.
/// @return The problem, or "" if it is valid.
[[nodiscard]] std::string checkMessage(std::string_view message, std::vector<std::string>& parameters);

/// @brief Checks a call of `t()` against a "strings" section: its key is known, its parameters are those read.
/// @param section The section (null if the document has none: nothing is checked).
/// @param arguments The call's arguments as compiled: `[{"$value": key}, {"$object": {...}}]`.
/// @return The problem, or "" (also when the key or the parameters are computed).
[[nodiscard]] std::string checkTranslation(const json& section, const json& arguments);

/// @brief Formats a message.
/// @param message The message (valid: checkMessage()).
/// @param parameters An object of the parameters' values; null for none. A parameter missing is shown as "{name}".
/// @param locale How numbers are written and plurals chosen.
/// @return The text.
[[nodiscard]] std::string formatMessage(std::string_view message, const Value& parameters,
                                        const detail::LocaleData& locale);

/// @brief A document's translations, and the locale chosen among them.
///
/// Invariants: chain() holds locales of the section, most specific first, and data() the CLDR data of the chosen
/// locale's language. Read once per revision of the section; select() runs when the locale changes.
class Strings {
public:
	/// @brief Reads the section of a document (checked by checkStrings()); none leaves no translations.
	/// @param document The document.
	void read(const json& document);
	/// @brief Chooses the locale: the fallback chain, and the formatting data.
	/// @param tag "fr-CA", "fr_CA.UTF-8"; "" for the section's default, else "en".
	void select(std::string_view tag);
	/// @brief The chosen locale's tag, normalized: "fr-CA".
	[[nodiscard]] const std::string& locale() const noexcept { return locale_; }
	/// @brief The formatting data of the chosen locale.
	[[nodiscard]] const detail::LocaleData& data() const noexcept { return *data_; }
	/// @brief Whether the chosen locale writes right to left.
	[[nodiscard]] bool rightToLeft() const noexcept { return data_->rightToLeft; }
	/// @brief The section's "locale" (a template), or null.
	[[nodiscard]] const json& localeValue() const noexcept { return localeValue_; }
	/// @brief The section's locales, most specific first, that keys are looked up in.
	[[nodiscard]] const std::vector<std::string>& chain() const noexcept { return chain_; }
	/// @brief Finds a key's message along the chain.
	/// @param key The key.
	/// @return The message, or nullptr.
	[[nodiscard]] const std::string* find(std::string_view key) const;
	/// @brief Tells whether some locale of the section has a key.
	/// @param key The key.
	/// @return Whether.
	[[nodiscard]] bool known(std::string_view key) const;
	/// @brief Formats a key's message.
	/// @param key The key.
	/// @param parameters An object of the parameters' values, or null.
	/// @return The text; the key itself if no locale of the chain has it.
	[[nodiscard]] std::string translate(std::string_view key, const Value& parameters) const;

private:
	json tables_ = json::object();                               ///< Per locale of the section: key → message.
	json localeValue_;                                           ///< The section's "locale".
	std::string defaultLocale_;                                  ///< The section's "default".
	std::string locale_ = "en";                                  ///< See locale().
	std::vector<std::string> chain_;                             ///< See chain().
	const detail::LocaleData* data_ = &detail::findLocale("en"); ///< See data().
};

} // namespace affresco::format
