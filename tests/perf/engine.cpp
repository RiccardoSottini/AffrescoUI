/// @file
/// @brief The workloads that need no window: expressions, commits and text.
///
/// Expressions are evaluated through the format module's own header (src/Expression.hpp), as the unit tests do: the
/// library offers no public call that evaluates one expression on its own.

#include "Bench.hpp"
#include "Expression.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

/// @brief The expressions of tests/unit/expressions.cpp that evaluate without an error, except the two that test the
/// 10,000-item limits (pad and range), which would outweigh all the others.
/// @return The values, each a `{{...}}` string, a template or a `$` operator.
std::vector<json> expressionTable() {
	const std::array texts{"{{1 + 2 * 3}}",
	                       "{{(1 + 2) * 3}}",
	                       "{{7 / 2}}",
	                       "{{8 / 2}}",
	                       "{{7 % 3}}",
	                       "{{-state.count}}",
	                       "{{1.5 + 1}}",
	                       "{{state.count > 2 ? 'big' : 'small'}}",
	                       "{{'abc' < 'abd'}}",
	                       "{{state.count == 3 && !false}}",
	                       "{{null || 0 || 'x'}}",
	                       "{{state.missing ?? 'none'}}",
	                       "{{coalesce(null, 2)}}",
	                       "{{state.items[1]}}",
	                       "{{state.items[state.count - 2]}}",
	                       "{{state.items.length}}",
	                       "{{state.items[5]}}",
	                       "{{state['my-list']}}",
	                       "{{state[\"my-list\"][1]}}",
	                       "{{len(state['my-list'])}}",
	                       "{{window.width / 2}}",
	                       "{{mode}}",
	                       "Clicked {{state.count}} times",
	                       "{{state.count + ' cards'}}",
	                       "{{@accent}}",
	                       "{{upper('abc') + lower('DEF')}}",
	                       "{{fixed(3.14159, 2)}}",
	                       "{{plural(2, 'card')}}",
	                       "{{plural(1, 'card')}}",
	                       "{{number('42')}}",
	                       "{{number('4x')}}",
	                       "{{slice('hello', -3)}}",
	                       "{{split('a-b', '-')}}",
	                       "{{join(state.items, ', ')}}",
	                       "{{pad(7, 3)}}",
	                       "{{pad('x', 4, '-')}}",
	                       "{{range(3)}}",
	                       "{{max(3, 9, 4)}}",
	                       "{{min(state.scores)}}",
	                       "{{clamp(15, 0, 10)}}",
	                       "{{round(2.5)}}",
	                       "{{filter(state.items, it != 'b')}}",
	                       "{{map([1, 2, 3], it * 2)}}",
	                       "{{sum([1, 2, 3.5])}}",
	                       "{{count(state.scores, it > 3)}}",
	                       "{{sort(state.scores, it, 'desc')}}",
	                       "{{contains(state.items, 'c')}}",
	                       "{{type([])}}",
	                       "{{{a: 1, 'b c': [2]}}}"};
	std::vector<json> table(std::begin(texts), std::end(texts));
	table.emplace_back("{{" + std::string(30, '(') + "1" + std::string(30, ')') + "}}");
	table.push_back(json::parse(R"({"$clamp": ["{{state.count}}", 0, 2]})"));
	table.push_back(json::parse(R"({"$length": [1, 2, 3]})"));
	return table;
}

/// @brief What the expressions see: the unit tests' fixed state.
struct ExpressionInputs {
	json state = {{"count", 3}, {"items", {"a", "b", "c"}}, {"scores", {4, 9, 2}}, {"my-list", {1, 2}}}; ///< state
	json window = {{"width", 800}, {"height", 600}};                                                     ///< window
	json revision = 0;                                                                                   ///< revision
	json event = json::object();                                                                         ///< event
	json globals = {{"mode", "default"}}; ///< mode and the other globals
	format::Scope locals;                 ///< No local variables.
};

/// @brief How many times "expressions" evaluates the table in one step.
constexpr int expressionPasses = 100;

/// @brief Evaluates every expression of the table expressionPasses times.
/// @param table The expressions.
/// @param inputs What they see.
/// @throws DocumentError If one is invalid.
void evaluateTable(const std::vector<json>& table, const ExpressionInputs& inputs) {
	const format::Context context{inputs.state, inputs.window, inputs.revision,
	                              inputs.event, inputs.locals, inputs.globals};
	std::size_t size = 0; // used, so the evaluations can't be left out
	for(int pass = 0; pass < expressionPasses; ++pass)
		for(const json& value : table) size += format::evaluate(value, context, "bench").size();
	if(size == 0) throw std::logic_error("expressions: nothing evaluated");
}

/// @brief "expressions": every expression of the table, evaluated many times, as a screen does on each update (they
/// are compiled once, then cached).
/// @return The workload.
std::vector<Workload> expressionWorkloads(Context& /*context*/) {
	const auto table = std::make_shared<const std::vector<json>>(expressionTable());
	const auto inputs = std::make_shared<const ExpressionInputs>();
	Workload workload{.name = "expressions",
	                  .unit = "ns",
	                  .runs = 9,
	                  .operations = expressionPasses * static_cast<int>(table->size())};
	workload.step = [table, inputs] { evaluateTable(*table, *inputs); };
	return single(std::move(workload));
}

/// @brief How many paths "fuzzy" filters in one step.
constexpr int fuzzyCount = 100000;

/// @brief fuzzyCount path-like texts, as a large project's file list.
/// @return The state, with the texts at `files`.
json fuzzyFiles() {
	static constexpr std::array<const char*, 6> folders{"src", "include/app", "modules/core", "tests/unit", "docs",
	                                                    "examples/assets"};
	static constexpr std::array<const char*, 6> names{"main", "window", "panel", "layout", "render", "screen"};
	static constexpr std::array<const char*, 4> extensions{".cpp", ".hpp", ".json", ".md"};
	json files = json::array();
	for(std::size_t i = 0; i < static_cast<std::size_t>(fuzzyCount); ++i)
		files.push_back(std::string(folders[i % folders.size()]) + "/" + names[(i / 7) % names.size()] +
		                std::to_string(i) + extensions[(i / 3) % extensions.size()]);
	return {{"files", std::move(files)}};
}

/// @brief "fuzzy/100000": a quick pick's filter, fuzzySort over fuzzyCount paths, keeping the best 50.
/// @return The workload.
std::vector<Workload> fuzzyWorkloads(Context& /*context*/) {
	auto inputs = std::make_shared<ExpressionInputs>();
	inputs->state = fuzzyFiles();
	const auto query = std::make_shared<const json>("{{fuzzySort(state.files, 'srcwin12', null, 50)}}");
	Workload workload{.name = "fuzzy/100000", .unit = "ms", .runs = 9};
	workload.step = [inputs, query] {
		const format::Context context{inputs->state, inputs->window, inputs->revision,
		                              inputs->event, inputs->locals, inputs->globals};
		if(format::evaluate(*query, context, "bench").empty()) throw std::logic_error("fuzzy: nothing matched");
	};
	return single(std::move(workload));
}

/// @brief How many commits "commits" makes in one step.
constexpr int commitCount = 1000;

/// @brief Makes commitCount small commits: the logo's text, a different one each time.
/// @param repository The history.
/// @throws std::runtime_error If one is refused.
void commitRenames(vcs::Repository& repository) {
	for(int i = 0; i < commitCount; ++i) {
		const json revision = {{"author", "bench"},
		                       {"message", "Rename the logo"},
		                       {"set", {{"#logo/text", "Flow " + std::to_string(i)}}}};
		if(const auto error = repository.commit(revision)) throw std::runtime_error("commits: " + *error);
	}
}

/// @brief "commits": 1,000 small commits (a label's text) on the Kanban template's history; the value and the
/// memory are per commit (every commit keeps a copy of the document today).
/// @param context The context.
/// @return The workload.
std::vector<Workload> commitWorkloads(const Context& context) {
	const auto rules = std::make_shared<const format::Rules>();
	const auto document = std::make_shared<const json>(json::parse(readFile(context.screens() / "kanban.screen.json")));
	const auto repository = std::make_shared<std::unique_ptr<vcs::Repository>>();
	Workload workload{.name = "commits", .runs = 3, .operations = commitCount};
	workload.prepare = [rules, document, repository] {
		*repository = nullptr; // the previous run's history goes first, unmeasured
		*repository = std::make_unique<vcs::Repository>(*rules, *document);
	};
	workload.step = [repository] { commitRenames(**repository); };
	return single(std::move(workload));
}

/// @brief How many lines "text" measures and wraps in one step.
constexpr int lineCount = 10'000;

/// @brief Lines of text, the same every time: words of the Kanban template's cards in a fixed pseudo-random order.
/// @return lineCount lines of 4 to 28 words.
std::vector<std::u32string> sampleLines() {
	const std::array words{"Onboarding", "checklist", "for",    "new",       "teams",   "Empty",  "states",
	                       "every",      "page",      "Crash",  "when",      "a",       "board",  "has",
	                       "no",         "columns",   "Invite", "teammates", "by",      "email",  "Keyboard",
	                       "shortcuts",  "moving",    "cards",  "colour",    "palette", "labels", "Fix"};
	std::vector<std::u32string> lines;
	lines.reserve(lineCount);
	std::uint32_t random = 2017; // a linear congruential generator: the same lines on every machine
	const auto next = [&random](std::uint32_t range) {
		random = random * 1664525u + 1013904223u;
		return (random >> 8) % range;
	};
	for(int i = 0; i < lineCount; ++i) {
		std::u32string line;
		for(std::uint32_t word = 0, count = 4 + next(25); word < count; ++word) {
			if(word) line += U' ';
			for(const char* c = words[next(std::size(words))]; *c; ++c) line += static_cast<char32_t>(*c);
		}
		lines.push_back(std::move(line));
	}
	return lines;
}

/// @brief Measures every line, and wraps it to 240 px.
/// @param font The font.
/// @param lines The lines.
/// @throws std::logic_error If nothing was measured.
void measureAndWrap(const Font& font, const std::vector<std::u32string>& lines) {
	float width = 0.0f; // used, so the work can't be left out
	std::size_t wrapped = 0;
	for(const std::u32string& line : lines) {
		width += font.measure(line);
		wrapped += font.wrap(line, 240.0f).size();
	}
	if(width <= 0.0f || wrapped < lines.size()) throw std::logic_error("text: nothing measured");
}

/// @brief "text": measuring and wrapping 10,000 lines in the templates' default font (Roboto, 15 px) to 240 px.
/// @param context The context.
/// @return The workload.
std::vector<Workload> textWorkloads(const Context& context) {
	const auto font = Font::load(context.screens().parent_path() / "fonts" / "Roboto-Regular.ttf", 15.0f);
	const auto lines = std::make_shared<const std::vector<std::u32string>>(sampleLines());
	Workload workload{.name = "text", .unit = "ms", .runs = 9};
	workload.step = [font, lines] { measureAndWrap(*font, *lines); };
	return single(std::move(workload));
}

} // namespace

std::vector<Group> engineGroups() {
	return {{"expressions", expressionWorkloads},
	        {"fuzzy", fuzzyWorkloads},
	        {"commits", commitWorkloads},
	        {"text", textWorkloads},
	        {"history", historyWorkloads},
	        {"views", viewWorkloads},
	        {"buffers", bufferWorkloads}};
}

} // namespace affresco::bench
