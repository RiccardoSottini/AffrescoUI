/// @file
/// @brief Expression functions for forms: passwordStrength and isEmail.

#include "ExpressionFunctions.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace affresco::format::detail {
namespace {

/// The longest text the form functions read (longer texts are judged on this prefix).
constexpr std::size_t longestText = 4096;

/// @brief Tells which kinds of characters a text holds.
struct Kinds {
	bool upper = false;  ///< A capital ASCII letter.
	bool lower = false;  ///< A small ASCII letter.
	bool digit = false;  ///< A digit.
	bool symbol = false; ///< Anything else that isn't a space.
};

/// @brief Reads the kinds of characters of a text.
/// @param text The text.
/// @return Its kinds.
[[nodiscard]] Kinds kindsOf(std::string_view text) noexcept {
	Kinds kinds;
	for(const char c : text) {
		const auto byte = static_cast<unsigned char>(c);
		if(std::isupper(byte) != 0) kinds.upper = true;
		else if(std::islower(byte) != 0) kinds.lower = true;
		else if(std::isdigit(byte) != 0) kinds.digit = true;
		else if(std::isspace(byte) == 0) kinds.symbol = true;
	}
	return kinds;
}

/// @brief Counts the characters (not bytes) of a UTF-8 text.
/// @param text The text.
/// @return Its length.
[[nodiscard]] std::size_t characters(std::string_view text) noexcept {
	std::size_t count = 0;
	for(const char c : text)
		if((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) ++count;
	return count;
}

/// @brief `passwordStrength(text)`: how strong a password is.
/// @param call [text].
/// @return {score 0 to 4, label "empty"/"weak"/"fair"/"good"/"strong", missing: the words of what would help}.
Value passwordStrength(const Call& call) {
	std::string text = call[0].toText();
	if(text.size() > longestText) text.resize(longestText);
	const Kinds kinds = kindsOf(text);
	const std::size_t length = characters(text);
	std::vector<Value> missing;
	if(length < 12) missing.push_back(Value::fromText("length"));
	if(!kinds.upper) missing.push_back(Value::fromText("upper"));
	if(!kinds.lower) missing.push_back(Value::fromText("lower"));
	if(!kinds.digit) missing.push_back(Value::fromText("digit"));
	if(!kinds.symbol) missing.push_back(Value::fromText("symbol"));
	const int variety = int{kinds.upper} + int{kinds.lower} + int{kinds.digit} + int{kinds.symbol};
	int score = 0;
	if(length >= 8)
		score = 1 + (variety >= 3 ? 1 : 0) + (length >= 12 ? 1 : 0) + (variety == 4 && length >= 12 ? 1 : 0);
	else if(length > 0 && variety >= 3) score = 1;
	static constexpr std::array<const char*, 5> labels{"weak", "weak", "fair", "good", "strong"};
	const char* label = length == 0 ? "empty" : labels.at(static_cast<std::size_t>(score));
	std::vector<Value::Field> fields;
	fields.push_back({"score", Value::fromInteger(score)});
	fields.push_back({"label", Value::fromText(label)});
	fields.push_back({"missing", Value::fromList(std::move(missing))});
	return Value::fromObject(std::move(fields));
}

/// @brief Tells whether a part of an address uses only allowed characters.
/// @param part The part.
/// @param extra Characters allowed besides letters, digits and bytes beyond ASCII.
/// @return true when it is not empty and every character is allowed.
[[nodiscard]] bool madeOf(std::string_view part, std::string_view extra) noexcept {
	return !part.empty() && std::ranges::all_of(part, [extra](char c) {
		const auto byte = static_cast<unsigned char>(c);
		return std::isalnum(byte) != 0 || byte >= 0x80U || extra.contains(c);
	});
}

/// @brief `isEmail(text)`: whether the text has the shape of an email address.
/// @param call [text].
/// @return true for name@domain.tld without spaces.
Value isEmail(const Call& call) {
	const std::string text(trim(call[0].toText()));
	const std::size_t at = text.find('@');
	if(text.size() > 254 || at == std::string::npos || text.find('@', at + 1) != std::string::npos)
		return Value::fromBoolean(false);
	const std::string_view name = std::string_view(text).substr(0, at);
	const std::string_view domain = std::string_view(text).substr(at + 1);
	const std::size_t dot = domain.rfind('.');
	const bool shaped = madeOf(name, ".!#$%&'*+/=?^_`{|}~-") && madeOf(domain, ".-") && dot != std::string_view::npos &&
	                    dot > 0 && domain.size() - dot > 2 && !domain.contains("..") && name.front() != '.' &&
	                    name.back() != '.';
	return Value::fromBoolean(shaped);
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "passwordStrength",
	 .arguments = "text",
	 .about = "How strong a password is: {score 0 to 4, label, missing: what would help}",
	 .takes = Takes::One,
	 .apply = passwordStrength},
    {.name = "isEmail",
	 .arguments = "text",
	 .about = "Whether the text has the shape of an email address",
	 .takes = Takes::One,
	 .apply = isEmail},
});

} // namespace

std::span<const Function> formFunctions() { return table; }

} // namespace affresco::format::detail
