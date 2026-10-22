/// @file
/// @brief Strings: the "strings" section checked and read, the locale's fallback chain, and keys translated.

#include "Strings.hpp"

#include <algorithm>
#include <cctype>
#include <format>

namespace affresco::format {
namespace {

/// @brief A tag lowercased, for comparing tags: "fr-CA" and "fr-ca" are one.
/// @param tag The tag.
/// @return It, lowercase.
std::string lowerTag(std::string_view tag) {
	std::string lower(tag);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

/// @brief Tells whether a key of the section names a locale: letters, digits and '-', at most 35.
/// @param tag The key.
/// @return Whether.
bool validTag(std::string_view tag) noexcept {
	return !tag.empty() && tag.size() <= 35 && std::isalpha(static_cast<unsigned char>(tag.front())) &&
	       std::ranges::all_of(tag, [](unsigned char c) { return std::isalnum(c) != 0 || c == '-'; });
}

/// @brief Checks one locale's table.
/// @param table Key → message.
/// @param at Its place.
void checkTable(const json& table, const std::string& at) {
	expect::object(table, at);
	if(table.size() > stringsKeyLimit) expect::fail(at, std::format("a locale has at most {} keys", stringsKeyLimit));
	std::vector<std::string> parameters;
	for(const auto& [key, message] : table.items()) {
		const std::string place = std::format("{}.{}", at, key);
		if(key.empty() || key.size() > 128) expect::fail(place, "a key has 1 to 128 characters");
		const std::string text = expect::string(message, place);
		parameters.clear();
		if(const std::string problem = checkMessage(text, parameters); !problem.empty()) expect::fail(place, problem);
	}
}

} // namespace

void checkStrings(const json& section, const std::string& name) {
	expect::object(section, name);
	std::size_t locales = 0;
	for(const auto& [key, value] : section.items()) {
		const std::string at = std::format("{}.{}", name, key);
		if(key == "locale") {
			static_cast<void>(expect::string(value, at));
		} else if(key == "default") {
			if(!validTag(expect::string(value, at))) expect::fail(at, "expected a locale's tag, e.g. \"en\"");
		} else {
			if(!validTag(key)) expect::fail(at, "a locale's tag has letters, digits and '-', e.g. \"fr-CA\"");
			if(++locales > stringsLocaleLimit)
				expect::fail(name, std::format("\"strings\" has at most {} locales", stringsLocaleLimit));
			checkTable(value, at);
		}
	}
}

std::string checkTranslation(const json& section, const json& arguments) {
	if(!section.is_object() || !arguments.is_array() || arguments.empty()) return "";
	const json& key = arguments[0];
	if(!key.is_object() || !key.contains("$value") || !key["$value"].is_string())
		return ""; // a key an expression gives
	const std::string& name = key["$value"].get_ref<const std::string&>();
	bool found = false;
	std::vector<std::string> read;
	for(const auto& [locale, table] : section.items()) {
		if(!table.is_object() || !table.contains(name) || !table[name].is_string()) continue;
		found = true;
		static_cast<void>(checkMessage(table[name].get_ref<const std::string&>(), read));
	}
	if(!found) return std::format("unknown key '{}': no locale of \"strings\" has it", name);
	const json* given = arguments.size() > 1 ? &arguments[1] : nullptr;
	if(given != nullptr && !(given->is_object() && given->contains("$object") && (*given)["$object"].is_object()))
		return ""; // parameters an expression gives
	const json none = json::object();
	const json& fields = given != nullptr ? (*given)["$object"] : none;
	for(const std::string& parameter : read)
		if(!fields.contains(parameter))
			return std::format("'{}' reads the parameter '{}', which isn't given", name, parameter);
	for(const auto& [parameter, value] : fields.items())
		if(std::ranges::find(read, parameter) == read.end())
			return std::format("'{}' has no parameter '{}'", name, parameter);
	return "";
}

void Strings::read(const json& document) {
	tables_ = json::object();
	localeValue_ = nullptr;
	defaultLocale_.clear();
	const auto section = document.find("strings");
	if(section != document.end() && section->is_object())
		for(const auto& [key, value] : section->items()) {
			if(key == "locale") localeValue_ = value;
			else if(key == "default" && value.is_string()) defaultLocale_ = lowerTag(value.get<std::string>());
			else if(value.is_object()) tables_[lowerTag(key)] = value;
		}
	select(locale_);
}

void Strings::select(std::string_view tag) {
	locale_ = detail::normalizeTag(tag);
	if(locale_.empty()) locale_ = defaultLocale_.empty() ? "en" : defaultLocale_;
	data_ = &detail::findLocale(locale_);
	chain_.clear();
	const auto add = [this](std::string candidate) {
		if(tables_.contains(candidate) && std::ranges::find(chain_, candidate) == chain_.end())
			chain_.push_back(std::move(candidate));
	};
	// "zh-Hant-TW", then "zh-Hant", then "zh"
	for(std::string prefix = lowerTag(locale_); !prefix.empty();) {
		add(prefix);
		const std::size_t dash = prefix.rfind('-');
		prefix.resize(dash == std::string::npos ? 0 : dash);
	}
	if(!defaultLocale_.empty()) add(defaultLocale_);
	add("en");
}

const std::string* Strings::find(std::string_view key) const {
	for(const std::string& locale : chain_) {
		const json& table = tables_[locale];
		if(const auto message = table.find(key); message != table.end() && message->is_string())
			return message->get_ptr<const std::string*>();
	}
	return nullptr;
}

bool Strings::known(std::string_view key) const {
	return std::ranges::any_of(tables_.items(), [key](const auto& entry) { return entry.value().contains(key); });
}

std::string Strings::translate(std::string_view key, const Value& parameters) const {
	const std::string* message = find(key);
	return message != nullptr ? formatMessage(*message, parameters, *data_) : std::string(key);
}

} // namespace affresco::format
