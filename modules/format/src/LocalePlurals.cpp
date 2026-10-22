/// @file
/// @brief Locales: finding one by its tag, and CLDR's plural rules ("i = 1 and v = 0") evaluated for a number.

#include "Locale.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>

namespace affresco::format::detail {
namespace {

/// @brief The operands CLDR's plural rules read of a number.
struct Operands {
	double n = 0;        ///< The absolute value.
	std::uint64_t i = 0; ///< Its integer digits.
	std::uint64_t v = 0; ///< How many visible fraction digits.
	std::uint64_t w = 0; ///< How many, without trailing zeros.
	std::uint64_t f = 0; ///< The visible fraction digits.
	std::uint64_t t = 0; ///< Them, without trailing zeros.
	std::uint64_t e = 0; ///< The compact exponent (always 0 here).
};

/// @brief Reads the operands of a number shown with some decimals.
/// @param number The number.
/// @param decimals Its visible decimals (0 to 9).
/// @return Them.
Operands operandsOf(double number, int decimals) noexcept {
	Operands operands;
	operands.n = std::fabs(number);
	if(!std::isfinite(operands.n) || operands.n >= 1e18) return operands;
	const int visible = decimals < 0 ? 0 : decimals > 9 ? 9 : decimals;
	const double scale = std::pow(10.0, visible);
	const auto scaled = static_cast<std::uint64_t>(std::llround(operands.n * scale));
	const auto unit = static_cast<std::uint64_t>(scale);
	operands.i = scaled / unit;
	operands.v = static_cast<std::uint64_t>(visible);
	operands.f = scaled % unit;
	operands.t = operands.f;
	operands.w = operands.v;
	while(operands.w > 0 && operands.t % 10 == 0) {
		operands.t /= 10;
		--operands.w;
	}
	return operands;
}

/// @brief Reads a rule's tokens one by one.
class RuleReader {
public:
	/// @brief Reads a condition.
	/// @param text "i = 1 and v = 0 or ...".
	explicit RuleReader(std::string_view text) noexcept : text_(text) {}

	/// @brief Skips spaces, then tells whether the next text is a word or symbol, and consumes it if so.
	/// @param word "and", "=", "..".
	/// @return Whether it was.
	[[nodiscard]] bool take(std::string_view word) noexcept {
		skipSpaces();
		if(!text_.substr(at_).starts_with(word)) return false;
		at_ += word.size();
		return true;
	}

	/// @brief Reads a whole number.
	/// @return It (0 if none).
	[[nodiscard]] std::uint64_t number() noexcept {
		skipSpaces();
		std::uint64_t value = 0;
		while(at_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[at_])))
			value = value * 10 + static_cast<std::uint64_t>(text_[at_++] - '0');
		return value;
	}

	/// @brief Reads an operand's letter.
	/// @return It, or 0 at the end.
	[[nodiscard]] char letter() noexcept {
		skipSpaces();
		return at_ < text_.size() ? text_[at_++] : '\0';
	}

	/// @brief Tells whether the whole rule was read.
	[[nodiscard]] bool done() noexcept {
		skipSpaces();
		return at_ >= text_.size();
	}

private:
	/// @brief Skips spaces.
	void skipSpaces() noexcept {
		while(at_ < text_.size() && text_[at_] == ' ') ++at_;
	}

	std::string_view text_; ///< The rule.
	std::size_t at_ = 0;    ///< Where reading is.
};

/// @brief The value of an operand.
/// @param operands The number's.
/// @param letter n, i, v, w, f, t, e or c.
/// @return It; n as a double, the others whole.
double operandValue(const Operands& operands, char letter) noexcept {
	switch(letter) {
	case 'n':
		return operands.n;
	case 'i':
		return static_cast<double>(operands.i);
	case 'v':
		return static_cast<double>(operands.v);
	case 'w':
		return static_cast<double>(operands.w);
	case 'f':
		return static_cast<double>(operands.f);
	case 't':
		return static_cast<double>(operands.t);
	default:
		return static_cast<double>(operands.e);
	}
}

/// @brief Evaluates one relation: "n % 100 = 3..10,12" or "v != 0".
/// @param reader The rule, at the relation.
/// @param operands The number's.
/// @return Whether it holds.
bool relation(RuleReader& reader, const Operands& operands) noexcept {
	double value = operandValue(operands, reader.letter());
	if(reader.take("%")) {
		const auto modulus = static_cast<double>(reader.number());
		value = modulus > 0 ? std::fmod(value, modulus) : value;
	}
	const bool negated = reader.take("!=");
	if(!negated && !reader.take("=")) return false;
	bool matched = false;
	do {
		const auto low = static_cast<double>(reader.number());
		const double high = reader.take("..") ? static_cast<double>(reader.number()) : low;
		// A range matches whole values only: n = 1..3 doesn't hold for 1.5
		if(value >= low && value <= high && std::trunc(value) == value) matched = true;
	} while(reader.take(","));
	return matched != negated;
}

/// @brief Evaluates a condition: relations joined by "and", those joined by "or".
/// @param condition The condition.
/// @param operands The number's.
/// @return Whether it holds ("" never does).
bool holds(std::string_view condition, const Operands& operands) noexcept {
	if(condition.empty()) return false;
	RuleReader reader(condition);
	bool any = false;
	do {
		bool all = true;
		do all = relation(reader, operands) && all;
		while(reader.take("and"));
		any = any || all;
	} while(reader.take("or"));
	return any && reader.done();
}

} // namespace

std::string normalizeTag(std::string_view tag) {
	const std::size_t end = tag.find_first_of(".@");
	if(end != std::string_view::npos) tag = tag.substr(0, end);
	if(tag == "C" || tag == "POSIX") return "en";
	std::string result;
	for(const char character : tag.substr(0, 35)) {
		if(character == '_') result += '-';
		else if(std::isalnum(static_cast<unsigned char>(character)) || character == '-') result += character;
	}
	return result;
}

const LocaleData& findLocale(std::string_view tag) noexcept {
	const std::span<const LocaleData> all = localeData();
	const std::size_t end = tag.find_first_of("-_.@");
	const std::string_view language = tag.substr(0, end);
	for(const LocaleData& locale : all) {
		if(locale.tag.size() != language.size()) continue;
		bool same = true;
		for(std::size_t i = 0; i < language.size() && same; ++i)
			same = std::tolower(static_cast<unsigned char>(language[i])) == locale.tag[i];
		if(same) return locale;
	}
	return all.front();
}

Plural pluralCategory(const LocaleData& locale, double number, int decimals) noexcept {
	const Operands operands = operandsOf(number, decimals);
	for(std::size_t category = 0; category < locale.plurals.size(); ++category)
		if(holds(locale.plurals[category], operands)) return static_cast<Plural>(category);
	return Plural::Other;
}

bool pluralFromName(std::string_view name, Plural& category) noexcept {
	static constexpr std::array<std::string_view, 6> names{"zero", "one", "two", "few", "many", "other"};
	for(std::size_t i = 0; i < names.size(); ++i)
		if(names[i] == name) {
			category = static_cast<Plural>(i);
			return true;
		}
	return false;
}

} // namespace affresco::format::detail
