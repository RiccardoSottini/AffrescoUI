/// @file
/// @brief Strings: messages, a subset of ICU MessageFormat, checked and formatted: `{name}`, `{n, number}`,
/// `{t, date, short}`, `{n, plural, offset:1 =0 {...} one {# item} other {# items}}`, `{x, select, a {...} other
/// {...}}`, and quotes ('' is a quote, '{' starts quoted text).

#include "Strings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <utility>

namespace affresco::format {
namespace {

/// @brief Walks a message once, writing its text and checking it.
///
/// Every branch of a plural or select is walked (each into its own text, the chosen one kept), so a check sees
/// every part and the work stays linear in the message's length.
class MessageWalker {
public:
	/// @brief Prepares a walk.
	/// @param message The message.
	/// @param parameters An object of the parameters' values, or null.
	/// @param locale How numbers are written and plurals chosen.
	/// @param names Gets the parameters' names, or nullptr.
	MessageWalker(std::string_view message, const Value& parameters, const detail::LocaleData& locale,
	              std::vector<std::string>* names) noexcept
	    : text_(message), parameters_(parameters), locale_(locale), names_(names) {}

	/// @brief Walks the whole message.
	/// @param out Gets its text.
	/// @return The problem, or "".
	[[nodiscard]] std::string run(std::string& out) {
		if(text_.size() > stringsMessageLimit) return std::format("a message is at most {} bytes", stringsMessageLimit);
		message(out, 0, nullptr);
		if(error_.empty() && at_ < text_.size()) error_ = "a '}' closes nothing";
		return error_;
	}

private:
	/// @brief Records the first problem and stops the walk.
	/// @param problem What is wrong.
	void fail(std::string problem) {
		if(error_.empty()) error_ = std::move(problem) + std::format(" (at character {})", at_ + 1);
		at_ = text_.size();
	}

	/// @brief Skips spaces.
	void skipSpaces() noexcept {
		while(at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_]))) ++at_;
	}

	/// @brief Reads a name, keyword or selector: letters, digits, '_', '-', '.', '=' and ':'.
	/// @return It ("" if none).
	// NOLINTNEXTLINE(bugprone-exception-escape): the word starts inside the text, so substr doesn't throw
	std::string_view word() noexcept {
		skipSpaces();
		const std::size_t start = at_;
		while(at_ < text_.size()) {
			const auto character = static_cast<unsigned char>(text_[at_]);
			if(!std::isalnum(character) && character != '_' && character != '-' && character != '.' &&
			   character != '=' && character != ':')
				break;
			++at_;
		}
		return text_.substr(start, at_ - start);
	}

	/// @brief Consumes a character after spaces.
	/// @param character It.
	/// @return Whether it was there.
	bool take(char character) noexcept {
		skipSpaces();
		if(at_ >= text_.size() || text_[at_] != character) return false;
		++at_;
		return true;
	}

	/// @brief A parameter's value, recording its name.
	/// @param name The name.
	/// @return Its value, or nothing if it is missing.
	std::optional<Value> parameter(std::string_view name) {
		if(names_ != nullptr && std::ranges::find(*names_, name) == names_->end()) names_->emplace_back(name);
		if(!parameters_.isObject()) return std::nullopt;
		return parameters_.member(name);
	}

	/// @brief Writes text up to the end, or up to a '}' closing the current branch (not consumed).
	/// @param out Gets it.
	/// @param depth How many plurals and selects hold it.
	/// @param pound What '#' writes (a plural's number), or nullptr outside plurals.
	void message(std::string& out, std::size_t depth, const std::string* pound) {
		while(at_ < text_.size()) {
			const char character = text_[at_];
			if(character == '}') {
				if(depth == 0) fail("a '}' closes nothing");
				return;
			}
			if(character == '{') argument(out, depth, pound);
			else if(character == '#' && pound != nullptr) out += *pound, ++at_;
			else if(character == '\'') quote(out);
			else out += character, ++at_;
			// Each argument can write a long parameter: a short message can repeat it into a huge text
			if(out.size() > textLimit) fail(std::format("the text is longer than {} MiB", textLimit >> 20));
		}
		if(depth > 0) fail("a '{' is never closed");
	}

	/// @brief Writes a quote: '' is one, '{...' is quoted text up to the next one, any other is itself.
	/// @param out Gets it.
	void quote(std::string& out) {
		++at_;
		if(at_ < text_.size() && text_[at_] == '\'') {
			out += '\'', ++at_;
			return;
		}
		if(at_ >= text_.size() || (text_[at_] != '{' && text_[at_] != '}' && text_[at_] != '#')) {
			out += '\'';
			return;
		}
		const std::size_t end = text_.find('\'', at_);
		out += text_.substr(at_, end == std::string_view::npos ? end : end - at_);
		at_ = end == std::string_view::npos ? text_.size() : end + 1;
	}

	/// @brief Writes an argument: from its '{' to its '}'.
	/// @param out Gets it.
	/// @param depth How many plurals and selects hold it.
	/// @param pound What '#' writes in the branch holding it.
	void argument(std::string& out, std::size_t depth, const std::string* pound) {
		++at_;
		const std::string_view name = word();
		if(name.empty()) return fail("an argument needs a name");
		const std::optional<Value> value = parameter(name);
		const std::string missing = std::format("{{{}}}", name);
		if(take('}')) {
			out += value ? plain(*value) : missing;
			return;
		}
		if(!take(',')) return fail(std::format("'{}' is followed by '}}' or ','", name));
		const std::string_view kind = word();
		if(kind == "plural" || kind == "select") {
			if(depth + 1 > stringsNestingLimit)
				return fail(std::format("plurals and selects nest at most {} deep", stringsNestingLimit));
			if(!take(',')) return fail(std::format("'{}' is followed by ','", kind));
			choice(out, depth + 1, kind == "plural", value, kind == "plural" ? nullptr : pound, missing);
		} else if(kind == "number" || kind == "date" || kind == "time") {
			formatted(out, kind, value, missing);
		} else fail(std::format("'{}' isn't number, date, time, plural or select", kind));
	}

	/// @brief Writes a number, date or time argument: from after its kind to its '}'.
	/// @param out Gets it.
	/// @param kind "number", "date" or "time".
	/// @param value The parameter's value.
	/// @param missing What a missing parameter writes.
	void formatted(std::string& out, std::string_view kind, const std::optional<Value>& value,
	               const std::string& missing) {
		const std::string_view style = take(',') ? word() : std::string_view();
		if(!take('}')) return fail(std::format("'{}' is followed by a style and '}}'", kind));
		if(!styled(out, value, kind, style)) return fail(std::format("'{}' isn't a style of {}", style, kind));
		if(!value) out += missing;
	}

	/// @brief Writes a value as a plain argument: numbers in the locale's way, the rest as text.
	/// @param value The value.
	/// @return The text.
	[[nodiscard]] std::string plain(const Value& value) const {
		return value.isNumber() ? detail::formatNumber(locale_, value.number()) : value.toText();
	}

	/// @brief Writes a number, date or time argument.
	/// @param out Gets it (if the parameter isn't missing).
	/// @param value The parameter's value.
	/// @param kind "number", "date" or "time".
	/// @param style "", "integer", "percent"; "short", "medium", "long", "full".
	/// @return Whether the style is one of the kind's.
	bool styled(std::string& out, const std::optional<Value>& value, std::string_view kind, std::string_view style) {
		if(!knownStyle(kind, style)) return false;
		if(!value) return true;
		if(!value->isNumber()) {
			out += value->toText();
			return true;
		}
		const double number = value->number();
		const std::string_view chosen = style.empty() ? std::string_view("medium") : style;
		if(kind == "date") out += detail::formatDate(locale_, number, chosen, false);
		else if(kind == "time") out += detail::formatTime(locale_, number, chosen, false);
		else if(style == "percent") out += detail::formatPercent(locale_, number);
		else out += detail::formatNumber(locale_, number, 0, style == "integer" ? 0 : 3);
		return true;
	}

	/// @brief Tells whether a style is one of a kind's.
	/// @param kind "number", "date" or "time".
	/// @param style The style ("" is every kind's).
	/// @return Whether.
	[[nodiscard]] static bool knownStyle(std::string_view kind, std::string_view style) noexcept {
		if(style.empty()) return true;
		if(kind == "number") return style == "integer" || style == "percent";
		return style == "short" || style == "medium" || style == "long" || style == "full";
	}

	/// @brief Reads a plural's "offset:N", if it has one.
	/// @param offset Gets N (it stays 0 without one).
	/// @return Whether it is well formed (or absent).
	bool readOffset(double& offset) {
		skipSpaces();
		if(!text_.substr(at_).starts_with("offset:")) return true;
		at_ += 7;
		const std::string_view digits = word();
		// At most 9 digits: a longer one is out of a double's range, or no offset a count needs
		if(digits.empty() || digits.size() > 9 ||
		   !std::ranges::all_of(digits, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
			fail("offset: is followed by a whole number of at most 9 digits");
			return false;
		}
		offset = std::stod(std::string(digits));
		return true;
	}

	/// @brief Reads a plural's or select's branches, each into its own text, up to its '}'.
	/// @param branches Gets each selector and its text.
	/// @param depth How many plurals and selects hold its branches.
	/// @param plural A plural, not a select.
	/// @param pound What '#' writes in the branches.
	/// @return Whether they are well formed.
	bool readBranches(std::vector<std::pair<std::string, std::string>>& branches, std::size_t depth, bool plural,
	                  const std::string* pound) {
		while(!take('}')) {
			const std::string_view selector = word();
			if(selector.empty() || !validSelector(selector, plural)) {
				fail(at_ < text_.size()
				         ? std::format("'{}' isn't a {} selector", selector, plural ? "plural" : "select")
						 : "a '{' is never closed");
				return false;
			}
			if(!take('{')) {
				fail(std::format("the selector '{}' is followed by '{{'", selector));
				return false;
			}
			std::string branch;
			message(branch, depth, pound);
			if(!take('}')) return false;
			branches.emplace_back(selector, std::move(branch));
		}
		return error_.empty();
	}

	/// @brief Writes a plural's or select's branches, keeping the chosen one: from after its ',' to its '}'.
	/// @param out Gets the chosen branch.
	/// @param depth How many plurals and selects hold its branches.
	/// @param plural A plural, not a select.
	/// @param value The parameter's value.
	/// @param pound What '#' writes, for a select (a plural writes its own number).
	/// @param missing What a missing parameter writes.
	void choice(std::string& out, std::size_t depth, bool plural, const std::optional<Value>& value,
	            const std::string* pound, const std::string& missing) {
		double offset = 0;
		if(plural && !readOffset(offset)) return;
		const bool numeric = plural && value && value->isNumber();
		const double number = numeric ? value->number() - offset : 0;
		const std::string own = numeric ? detail::formatNumber(locale_, number) : missing;
		std::vector<std::pair<std::string, std::string>> branches; // selector, text
		if(!readBranches(branches, depth, plural, plural ? &own : pound)) return;
		const auto other = std::ranges::find(branches, "other", &std::pair<std::string, std::string>::first);
		if(other == branches.end())
			return fail(std::format("a {} needs an 'other' branch", plural ? "plural" : "select"));
		out += chosen(branches, plural, value, number, numeric)->second;
	}

	/// @brief Tells whether a selector fits: a plural's "=3" or category, a select's word.
	/// @param selector The selector.
	/// @param plural A plural's.
	/// @return Whether.
	[[nodiscard]] static bool validSelector(std::string_view selector, bool plural) noexcept {
		if(!plural) return selector.find_first_of("=:") == std::string_view::npos;
		detail::Plural unused{};
		if(detail::pluralFromName(selector, unused)) return true;
		return selector.size() > 1 && selector.front() == '=' &&
		       std::ranges::all_of(selector | std::views::drop(1),
		                           [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
	}

	/// @brief The branch a value chooses: an exact "=N", then its plural category, else "other".
	/// @param branches The branches (one is "other").
	/// @param plural A plural's.
	/// @param value The parameter's value.
	/// @param number The plural's number, offset taken away.
	/// @param numeric Whether the plural has a number.
	/// @return The branch.
	[[nodiscard]] std::vector<std::pair<std::string, std::string>>::const_iterator
	chosen(const std::vector<std::pair<std::string, std::string>>& branches, bool plural,
	       const std::optional<Value>& value, double number, bool numeric) const {
		const auto find = [&](std::string_view selector) {
			return std::ranges::find(branches, selector, &std::pair<std::string, std::string>::first);
		};
		if(!plural) {
			const auto match =
			    value ? find(value->isText() ? std::string(value->text()) : value->toText()) : branches.end();
			return match != branches.end() ? match : find("other");
		}
		if(!numeric) return find("other");
		const double whole = value->number(); // NOLINT(bugprone-unchecked-optional-access): numeric means a value
		if(std::trunc(whole) == whole && std::fabs(whole) < 1e15)
			if(const auto exact = find(std::format("={}", static_cast<long long>(std::fabs(whole))));
			   exact != branches.end() && whole >= 0)
				return exact;
		static constexpr std::array<std::string_view, 6> names{"zero", "one", "two", "few", "many", "other"};
		const std::string shown = detail::formatNumber(detail::findLocale("en"), std::fabs(number));
		const std::size_t point = shown.find('.');
		const int decimals = point == std::string::npos ? 0 : static_cast<int>(shown.size() - point - 1);
		const auto category = find(names[static_cast<std::size_t>(detail::pluralCategory(locale_, number, decimals))]);
		return category != branches.end() ? category : find("other");
	}

	std::string_view text_;            ///< The message.
	const Value& parameters_;          ///< Its parameters' values.
	const detail::LocaleData& locale_; ///< The locale.
	std::vector<std::string>* names_;  ///< Gets the parameters' names, or nullptr.
	std::size_t at_ = 0;               ///< Where the walk is.
	std::string error_;                ///< The first problem.
};

} // namespace

std::string checkMessage(std::string_view message, std::vector<std::string>& parameters) {
	std::string unused;
	const Value none;
	return MessageWalker(message, none, detail::findLocale("en"), &parameters).run(unused);
}

std::string formatMessage(std::string_view message, const Value& parameters, const detail::LocaleData& locale) {
	std::string out;
	if(!MessageWalker(message, parameters, locale, nullptr).run(out).empty()) return std::string(message);
	return out;
}

} // namespace affresco::format
