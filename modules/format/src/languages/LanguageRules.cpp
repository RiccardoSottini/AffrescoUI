/// @file
/// @brief Compiling a language's "patterns" and "repository" (Language.hpp): each rule checked and bounded, the
/// includes resolved by name, and the matches and regions each place tries listed once, includes opened.

#include "Language.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <unordered_map>

namespace affresco::format {

namespace {

using expect::fail;
using expect::object;
using expect::string;

/// @brief Compiles one language's rules: the state of the walk over its declaration.
class RuleCompiler {
public:
	/// @brief Starts on a language.
	/// @param language The language, which gets the rules and scopes.
	explicit RuleCompiler(Language& language) : language_(language) {}

	/// @brief Compiles the repository and the top level, then lists each place's candidates.
	/// @param value The declaration.
	/// @param at Its place.
	void compile(const json& value, const std::string& at) {
		language_.rules.assign(1, LanguageRule{}); // [0]: the top level, a group
		if(const auto found = value.find("repository"); found != value.end()) reserveRepository(*found, at);
		if(const auto found = value.find("repository"); found != value.end())
			for(const auto& [name, item] : found->items())
				fill(repository_.at(name), item, at + ".repository." + name, 1);
		if(const auto found = value.find("patterns"); found != value.end())
			language_.rules[0].patterns = patternList(*found, at + ".patterns", 1);
		for(std::uint32_t index = 0; index < language_.rules.size(); ++index) {
			LanguageRule& rule = language_.rules[index];
			if(rule.kind == LanguageRule::Kind::Group || rule.kind == LanguageRule::Kind::Region)
				listCandidates(index, at);
		}
	}

private:
	/// @brief Gives each repository entry its rule, so includes can name entries declared after them.
	/// @param repository The "repository" object.
	/// @param at The language's place.
	void reserveRepository(const json& repository, const std::string& at) {
		object(repository, at + ".repository");
		for(const auto& [name, item] : repository.items()) {
			if(name.empty() || name.size() > LanguageCatalog::maxNameBytes)
				fail(at + ".repository", std::format("expected names of 1 to {} bytes", LanguageCatalog::maxNameBytes));
			repository_.emplace(name, newRule(at + ".repository." + name));
		}
	}

	/// @brief Adds an empty rule.
	/// @param at Its place, for the error over the limit.
	/// @return Its index.
	std::uint32_t newRule(const std::string& at) {
		if(language_.rules.size() >= Language::maxRules)
			fail(at, std::format("a language has at most {} rules", Language::maxRules));
		language_.rules.emplace_back();
		return static_cast<std::uint32_t>(language_.rules.size() - 1);
	}

	/// @brief Reads a list of rules ("patterns").
	/// @param list The list.
	/// @param at Its place.
	/// @param depth How deep it is.
	/// @return The rules' indices.
	std::vector<std::uint32_t> patternList(const json& list, const std::string& at, std::size_t depth) {
		if(!list.is_array()) fail(at, "expected a list of rules");
		if(depth > Language::maxNesting) fail(at, std::format("rules nest at most {} deep", Language::maxNesting));
		std::vector<std::uint32_t> out;
		for(std::size_t index = 0; index < list.size(); ++index) {
			const std::string place = std::format("{}[{}]", at, index);
			const std::uint32_t rule = newRule(place);
			fill(rule, list[index], place, depth);
			out.push_back(rule);
		}
		return out;
	}

	/// @brief Interns a scope name: dotted words of letters, digits, '-', '_' and '+'.
	/// @param value The name.
	/// @param at Its place.
	/// @return Its id.
	ScopeId scope(const json& value, const std::string& at) {
		const std::string name = string(value, at);
		const bool valid =
		    !name.empty() && name.size() <= Language::maxScopeBytes && name.front() != '.' && name.back() != '.' &&
		    name.find("..") == std::string::npos && std::ranges::all_of(name, [](char c) {
			    return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_' || c == '+';
		    });
		if(!valid)
			fail(at,
			     std::format("expected a scope of at most {} bytes, e.g. \"string.escape\"", Language::maxScopeBytes));
		const auto known = std::ranges::find(language_.scopes, name);
		if(known != language_.scopes.end()) return static_cast<ScopeId>(known - language_.scopes.begin() + 1);
		if(language_.scopes.size() >= Language::maxScopes)
			fail(at, std::format("a language has at most {} scopes", Language::maxScopes));
		language_.scopes.push_back(name);
		return static_cast<ScopeId>(language_.scopes.size());
	}

	/// @brief Reads captures: `{ "1": "scope", ... }`, groups 0 to 99.
	/// @param value The object.
	/// @param at Its place.
	/// @return The captures.
	std::vector<LanguageRule::Capture> captures(const json& value, const std::string& at) {
		object(value, at);
		std::vector<LanguageRule::Capture> out;
		for(const auto& [key, item] : value.items()) {
			const bool number = !key.empty() && key.size() <= 2 &&
			                    std::ranges::all_of(key, [](char c) { return c >= '0' && c <= '9'; });
			if(!number) fail(at + "." + key, "expected a group's number, 0 to 99");
			out.push_back({static_cast<std::uint8_t>(std::stoi(key)), scope(item, at + "." + key)});
		}
		std::ranges::sort(out, {}, &LanguageRule::Capture::group);
		return out;
	}

	/// @brief Fills a rule from its declaration.
	/// @param index The rule.
	/// @param value Its declaration.
	/// @param at Its place.
	/// @param depth How deep it is.
	void fill(std::uint32_t index, const json& value, const std::string& at, std::size_t depth) {
		object(value, at);
		static constexpr std::array<std::string_view, 10> known = {
		    "match",    "begin",         "end",         "scope",    "contentScope",
		    "captures", "beginCaptures", "endCaptures", "patterns", "include"};
		for(const auto& [key, item] : value.items())
			if(std::ranges::find(known, key) == known.end())
				fail(at + "." + key, "expected match, begin, end, scope, contentScope, captures, beginCaptures, "
				                     "endCaptures, patterns or include");
		if(value.contains("include")) return fillInclude(index, value, at);
		if(value.contains("match") && (value.contains("begin") || value.contains("end")))
			fail(at, "expected match, or begin and end, not both");
		if(value.contains("begin") != value.contains("end")) fail(at, "expected both begin and end");
		LanguageRule rule;
		if(value.contains("scope")) rule.scope = scope(value["scope"], at + ".scope");
		if(value.contains("captures")) rule.captures = captures(value["captures"], at + ".captures");
		if(value.contains("match")) {
			rule.kind = LanguageRule::Kind::Match;
			rule.pattern = expression(value["match"], at + ".match", rule.anchored);
		} else if(value.contains("begin")) fillRegion(rule, value, at);
		if(value.contains("patterns")) {
			if(rule.kind == LanguageRule::Kind::Match) fail(at + ".patterns", "a match has no patterns inside");
			rule.patterns = patternList(value["patterns"], at + ".patterns", depth + 1);
		}
		language_.rules[index] = std::move(rule);
	}

	/// @brief Reads a region's own keys: begin, end, contentScope and the ends' captures.
	/// @param rule The rule.
	/// @param value Its declaration.
	/// @param at Its place.
	void fillRegion(LanguageRule& rule, const json& value, const std::string& at) {
		rule.kind = LanguageRule::Kind::Region;
		bool anchored = false;
		rule.pattern = expression(value["begin"], at + ".begin", anchored);
		const std::string end = string(value["end"], at + ".end");
		if(refersToGroups(end)) {
			// Compiled for each region it ends, with begin's groups put in: checked now with stand-ins
			rule.endSource = end;
			static_cast<void>(compileLanguagePattern(withGroups(end, {}), at + ".end"));
		} else rule.end = compileLanguagePattern(end, at + ".end");
		if(value.contains("contentScope")) rule.contentScope = scope(value["contentScope"], at + ".contentScope");
		if(value.contains("beginCaptures"))
			rule.beginCaptures = captures(value["beginCaptures"], at + ".beginCaptures");
		if(value.contains("endCaptures")) rule.endCaptures = captures(value["endCaptures"], at + ".endCaptures");
	}

	/// @brief Reads an include: "#name" (the repository), "$self" or "$base" (the top level).
	/// @param index The rule.
	/// @param value Its declaration.
	/// @param at Its place.
	void fillInclude(std::uint32_t index, const json& value, const std::string& at) {
		if(value.size() != 1) fail(at, "an include has no other keys");
		const std::string name = string(value["include"], at + ".include");
		LanguageRule rule;
		rule.kind = LanguageRule::Kind::Include;
		if(name == "$self" || name == "$base") rule.target = 0;
		else if(name.starts_with('#') && repository_.contains(name.substr(1)))
			rule.target = repository_.at(name.substr(1));
		else fail(at + ".include", "expected \"#name\" of the repository, or \"$self\"");
		language_.rules[index] = std::move(rule);
	}

	/// @brief Compiles a match's or a begin's expression.
	/// @param value The expression.
	/// @param at Its place.
	/// @param anchored Gets whether it uses `\G`.
	/// @return It.
	static std::shared_ptr<const TextPattern> expression(const json& value, const std::string& at, bool& anchored) {
		const std::string source = string(value, at);
		anchored = source.find("\\G") != std::string::npos;
		return compileLanguagePattern(source, at);
	}

	/// @brief Lists the matches and regions a group or region tries, in order, includes and groups opened.
	/// @param index The group or region.
	/// @param at The language's place, for the error over the limit.
	void listCandidates(std::uint32_t index, const std::string& at) {
		std::vector<std::uint32_t> found;
		std::vector<bool> opened(language_.rules.size(), false);
		const auto open = [&](auto& self, const std::vector<std::uint32_t>& rules) -> void {
			for(std::uint32_t rule : rules) {
				while(language_.rules[rule].kind == LanguageRule::Kind::Include) {
					const std::uint32_t target = language_.rules[rule].target;
					if(opened[rule]) break;
					opened[rule] = true;
					rule = target;
				}
				const LanguageRule& item = language_.rules[rule];
				if(item.kind == LanguageRule::Kind::Include) continue; // a cycle of includes
				if(item.kind != LanguageRule::Kind::Group) {
					if(std::ranges::find(found, rule) == found.end()) found.push_back(rule);
				} else if(!opened[rule]) {
					opened[rule] = true;
					self(self, item.patterns);
				}
				if(found.size() > Language::maxCandidates)
					fail(at, std::format("a place tries at most {} rules", Language::maxCandidates));
			}
		};
		open(open, language_.rules[index].patterns);
		language_.rules[index].candidates = std::move(found);
	}

	Language& language_;                                        ///< The language compiled.
	std::unordered_map<std::string, std::uint32_t> repository_; ///< Repository names → their rules.
};

} // namespace

bool refersToGroups(std::string_view source) noexcept {
	for(std::size_t i = 0; i + 1 < source.size(); ++i)
		if(source[i] == '\\') {
			if(source[i + 1] >= '1' && source[i + 1] <= '9') return true;
			++i; // an escaped character, e.g. "\\"
		}
	return false;
}

std::string withGroups(std::string_view source, const std::vector<std::string>& groups) {
	std::string out;
	for(std::size_t i = 0; i < source.size(); ++i) {
		if(source[i] != '\\' || i + 1 == source.size()) {
			out += source[i];
			continue;
		}
		const char next = source[++i];
		if(next < '1' || next > '9') {
			out += '\\';
			out += next;
			continue;
		}
		const auto group = static_cast<std::size_t>(next - '0');
		// The group's text, its punctuation escaped, so it matches only itself
		if(group < groups.size())
			for(const char c : groups[group]) {
				if(std::ispunct(static_cast<unsigned char>(c)) != 0) out += '\\';
				out += c;
			}
	}
	return out;
}

void compileLanguageRules(Language& language, const json& value, const std::string& at) {
	RuleCompiler(language).compile(value, at);
}

} // namespace affresco::format
