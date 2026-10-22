#include "SyntaxLanguages.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <mutex>
#include <string_view>

extern "C" {
const TSLanguage* tree_sitter_c();
const TSLanguage* tree_sitter_cpp();
const TSLanguage* tree_sitter_json();
const TSLanguage* tree_sitter_python();
const TSLanguage* tree_sitter_javascript();
const TSLanguage* tree_sitter_typescript();
const TSLanguage* tree_sitter_markdown();
}

namespace affresco::services::syntax {

namespace {

#include "SyntaxQueries.inc"

/// @brief A language as built in: names, extensions, grammar and query texts.
struct Entry {
	std::string_view name;                    ///< Its name.
	std::vector<std::string_view> aliases;    ///< Other names for it.
	std::vector<std::string_view> extensions; ///< File extensions, with the dot.
	const TSLanguage* (*grammar)();           ///< Its grammar.
	std::string_view highlights;              ///< Its highlight query.
	std::string_view folds;                   ///< Its fold query.
	std::string_view brackets;                ///< Its bracket query.
};

/// @brief The languages.
const std::vector<Entry>& entries() {
	static const std::vector<Entry> all = {
	    {"c", {}, {".c", ".h"}, tree_sitter_c, c_highlights, c_folds, c_brackets},
	    {"cpp",
		 {"c++", "cc", "cxx", "hpp"},
		 {".cpp", ".cc", ".cxx", ".c++", ".hpp", ".hh", ".hxx", ".h++", ".ipp", ".inl", ".tpp"},
		 tree_sitter_cpp,
		 cpp_highlights,
		 cpp_folds,
		 cpp_brackets},
	    {"json", {"jsonc"}, {".json", ".jsonc"}, tree_sitter_json, json_highlights, json_folds, json_brackets},
	    {"python",
		 {"py", "python3"},
		 {".py", ".pyi", ".pyw"},
		 tree_sitter_python,
		 python_highlights,
		 python_folds,
		 python_brackets},
	    {"javascript",
		 {"js", "jsx", "mjs", "cjs"},
		 {".js", ".jsx", ".mjs", ".cjs"},
		 tree_sitter_javascript,
		 javascript_highlights,
		 javascript_folds,
		 javascript_brackets},
	    {"typescript",
		 {"ts", "mts", "cts"},
		 {".ts", ".mts", ".cts"},
		 tree_sitter_typescript,
		 typescript_highlights,
		 typescript_folds,
		 typescript_brackets},
	    {"markdown",
		 {"md"},
		 {".md", ".markdown"},
		 tree_sitter_markdown,
		 markdown_highlights,
		 markdown_folds,
		 markdown_brackets}};
	return all;
}

/// @brief Text in lower case (ASCII).
/// @param text The text.
std::string lower(std::string_view text) {
	std::string out(text);
	std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return out;
}

/// @brief Reads one predicate (its steps up to a Done step) into a check.
/// @param query The query.
/// @param steps Its steps.
/// @param count How many.
/// @param check Gets it.
/// @return false if the predicate isn't one this engine checks (it is then ignored: always satisfied).
bool readPredicate(const TSQuery* query, const TSQueryPredicateStep* steps, std::uint32_t count, Predicate& check) {
	if(count < 2 || steps[0].type != TSQueryPredicateStepTypeString || steps[1].type != TSQueryPredicateStepTypeCapture)
		return false;
	std::uint32_t length = 0;
	std::string_view name = ts_query_string_value_for_id(query, steps[0].value_id, &length);
	name = name.substr(0, length);
	check.negated = name.starts_with("not-");
	if(check.negated) name.remove_prefix(4);
	check.capture = steps[1].value_id;
	for(std::uint32_t i = 2; i < count; ++i) {
		if(steps[i].type == TSQueryPredicateStepTypeCapture) {
			check.otherCapture = steps[i].value_id;
			continue;
		}
		const char* word = ts_query_string_value_for_id(query, steps[i].value_id, &length);
		check.words.emplace_back(word, length);
	}
	if(name == "match?" && check.words.size() == 1) {
		check.kind = Predicate::Kind::Match;
		return !check.pattern.compile(check.words[0], {.regex = true, .caseSensitive = true});
	}
	if(name == "eq?" && check.words.size() + (check.otherCapture >= 0 ? 1 : 0) == 1) {
		check.kind = Predicate::Kind::Eq;
		return true;
	}
	check.kind = Predicate::Kind::AnyOf;
	return name == "any-of?" && check.otherCapture < 0;
}

/// @brief Compiles a query and reads its predicates.
/// @param language The grammar.
/// @param text The query (none if blank or only comments).
/// @param query Gets it.
void compile(const TSLanguage* language, std::string_view text, Query& query) {
	std::uint32_t errorOffset = 0;
	TSQueryError error = TSQueryErrorNone;
	query.query = ts_query_new(language, text.data(), static_cast<std::uint32_t>(text.size()), &errorOffset, &error);
	if(!query.query) {
		query.error = "query error " + std::to_string(error) + " at byte " + std::to_string(errorOffset);
		return;
	}
	const std::uint32_t patterns = ts_query_pattern_count(query.query);
	query.checks.resize(patterns);
	for(std::uint32_t pattern = 0; pattern < patterns; ++pattern) {
		std::uint32_t count = 0;
		const TSQueryPredicateStep* steps = ts_query_predicates_for_pattern(query.query, pattern, &count);
		for(std::uint32_t start = 0, i = 0; i < count; ++i) {
			if(steps[i].type != TSQueryPredicateStepTypeDone) continue;
			Predicate check;
			if(readPredicate(query.query, steps + start, i - start, check))
				query.checks[pattern].push_back(std::move(check));
			start = i + 1;
		}
	}
}

/// @brief Builds a language's grammar and queries.
/// @param entry The language.
std::unique_ptr<Grammar> build(const Entry& entry) {
	auto made = std::make_unique<Grammar>();
	made->name = entry.name;
	made->language = entry.grammar();
	compile(made->language, entry.highlights, made->highlights);
	compile(made->language, entry.folds, made->folds);
	compile(made->language, entry.brackets, made->brackets);
	if(made->highlights.query) {
		const std::uint32_t captures = ts_query_capture_count(made->highlights.query);
		for(std::uint32_t id = 0; id < captures; ++id) {
			std::uint32_t length = 0;
			const char* name = ts_query_capture_name_for_id(made->highlights.query, id, &length);
			made->styles.emplace_back(name, length);
			made->plain.push_back(made->styles.back() == "variable");
		}
	}
	return made;
}

} // namespace

Query::~Query() {
	if(query) ts_query_delete(query);
}

const std::vector<std::string>& names() {
	static const std::vector<std::string> all = [] {
		std::vector<std::string> out;
		for(const Entry& entry : entries()) out.emplace_back(entry.name);
		return out;
	}();
	return all;
}

std::string canonical(std::string_view name) {
	const std::string wanted = lower(name);
	for(const Entry& entry : entries())
		if(entry.name == wanted || std::ranges::find(entry.aliases, wanted) != entry.aliases.end())
			return std::string(entry.name);
	return {};
}

std::string forFile(std::string_view fileName) {
	const std::size_t slash = fileName.find_last_of("/\\");
	if(slash != std::string_view::npos) fileName.remove_prefix(slash + 1);
	const std::size_t dot = fileName.rfind('.');
	if(dot == std::string_view::npos || dot == 0) return {};
	const std::string extension = lower(fileName.substr(dot));
	for(const Entry& entry : entries())
		if(std::ranges::find(entry.extensions, extension) != entry.extensions.end()) return std::string(entry.name);
	return {};
}

const Grammar* grammar(std::string_view name) {
	static std::mutex mutex;
	static std::vector<std::unique_ptr<Grammar>> built(entries().size());
	const auto& all = entries();
	const auto entry = std::ranges::find(all, name, &Entry::name);
	if(entry == all.end()) return nullptr;
	const std::lock_guard lock(mutex);
	std::unique_ptr<Grammar>& slot = built[static_cast<std::size_t>(entry - all.begin())];
	if(!slot) slot = build(*entry);
	return slot.get();
}

} // namespace affresco::services::syntax
