#include "affresco/services/Syntax.hpp"

#include "syntax/SyntaxLanguages.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <map>

namespace affresco::services {

namespace {

using Clock = std::chrono::steady_clock;

/// @brief The most edited ranges remembered between parses (more merge into one).
constexpr std::size_t mostEditedRanges = 64;

/// @brief A tree-sitter point as ours.
/// @param point The point.
SyntaxPoint pointOf(TSPoint point) noexcept { return {point.row, point.column}; }
/// @brief Ours as a tree-sitter point.
/// @param point The point.
TSPoint tsPoint(SyntaxPoint point) noexcept { return {point.row, point.column}; }

/// @brief What the parser's read callback reaches: the reader.
struct Input {
	const SyntaxReader* read = nullptr; ///< The reader.
	std::size_t size = 0;               ///< The text's size: nothing is read past it.
};

/// @brief tree-sitter's read callback.
/// @param payload The Input.
/// @param byte Where to read.
/// @param at The same place as a point.
/// @param bytes Gets how many bytes the chunk has.
const char* readInput(void* payload, std::uint32_t byte, TSPoint at, std::uint32_t* bytes) {
	const auto& input = *static_cast<const Input*>(payload);
	if(byte >= input.size) {
		*bytes = 0;
		return "";
	}
	const std::string_view chunk = (*input.read)(byte, pointOf(at));
	*bytes = static_cast<std::uint32_t>(std::min(chunk.size(), input.size - byte));
	return chunk.data();
}

/// @brief tree-sitter's progress callback: true (stop) past the deadline.
/// @param state Its payload is the deadline.
bool pastDeadline(TSParseState* state) { return Clock::now() > *static_cast<const Clock::time_point*>(state->payload); }

/// @brief A capture found by highlight(), before it is cut into lines.
struct Found {
	TSPoint start;           ///< Where it starts.
	TSPoint end;             ///< Where it ends.
	std::uint32_t rank = 0;  ///< Lower paints later over the same range: (plain variable, pattern).
	std::uint16_t style = 0; ///< Its capture id.
};

/// @brief Orders points.
/// @param a One.
/// @param b The other.
bool before(TSPoint a, TSPoint b) noexcept { return a.row < b.row || (a.row == b.row && a.column < b.column); }

/// @brief Whether two points are the same.
/// @param a One.
/// @param b The other.
bool same(TSPoint a, TSPoint b) noexcept { return a.row == b.row && a.column == b.column; }

} // namespace

/// @brief A document's parser, tree and cursor.
struct SyntaxDocument::State {
	/// @brief Frees what tree-sitter allocated.
	~State() {
		if(tree) ts_tree_delete(tree);
		if(cursor) ts_query_cursor_delete(cursor);
		if(parser) ts_parser_delete(parser);
	}

	const syntax::Grammar* grammar = nullptr; ///< The language.
	SyntaxLimits limits;                      ///< The bounds.
	TSParser* parser = nullptr;               ///< The parser.
	TSTree* tree = nullptr;                   ///< The last tree (edited since), or nullptr.
	TSQueryCursor* cursor = nullptr;          ///< Runs the queries.
	bool stale = false;                       ///< Edits came since the last complete parse.
	std::vector<SyntaxRows> edited;           ///< Rows edited since the last complete parse.

	/// @brief Starts the cursor on a query over some rows.
	/// @param query The query.
	/// @param first The first row.
	/// @param last The last row (included).
	void start(const TSQuery* query, std::uint32_t first, std::uint32_t last) const {
		ts_query_cursor_set_match_limit(cursor, limits.maxMatches);
		ts_query_cursor_set_max_start_depth(cursor, limits.maxDepth);
		const std::uint32_t end = last == std::numeric_limits<std::uint32_t>::max() ? last : last + 1;
		ts_query_cursor_set_point_range(cursor, {first, 0}, {end, 0});
		ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));
	}

	/// @brief Reads a node's text (at most limits.maxPredicateBytes).
	/// @param read The reader.
	/// @param node The node.
	std::string text(const SyntaxReader& read, TSNode node) const {
		std::uint32_t byte = ts_node_start_byte(node);
		const std::uint32_t end = ts_node_end_byte(node);
		TSPoint at = ts_node_start_point(node);
		std::string out;
		while(byte < end && out.size() < limits.maxPredicateBytes) {
			std::string_view chunk = read(byte, pointOf(at));
			if(chunk.empty()) break;
			chunk = chunk.substr(
			    0, std::min<std::size_t>({chunk.size(), end - byte, limits.maxPredicateBytes - out.size()}));
			for(const char c : chunk) at = c == '\n' ? TSPoint{at.row + 1, 0} : TSPoint{at.row, at.column + 1};
			out += chunk;
			byte += static_cast<std::uint32_t>(chunk.size());
		}
		return out;
	}

	/// @brief Whether a match satisfies its pattern's predicates.
	/// @param query The query.
	/// @param match The match.
	/// @param read Reads the text.
	bool satisfies(const syntax::Query& query, const TSQueryMatch& match, const SyntaxReader& read) const {
		const auto textOf = [&](std::uint32_t capture) -> std::optional<std::string> {
			for(std::uint16_t i = 0; i < match.capture_count; ++i)
				if(match.captures[i].index == capture) return text(read, match.captures[i].node);
			return std::nullopt;
		};
		for(const syntax::Predicate& check : query.checks[match.pattern_index]) {
			const std::optional<std::string> subject = textOf(check.capture);
			if(!subject) continue;
			bool holds = false;
			if(check.kind == syntax::Predicate::Kind::Match) holds = check.pattern.find(*subject).match.has_value();
			else if(check.kind == syntax::Predicate::Kind::Eq && check.otherCapture >= 0)
				holds = subject == textOf(static_cast<std::uint32_t>(check.otherCapture));
			else holds = std::ranges::find(check.words, *subject) != check.words.end();
			if(holds == check.negated) return false;
		}
		return true;
	}
};

SyntaxDocument::SyntaxDocument(std::unique_ptr<State> state) : state_(std::move(state)) {}

SyntaxDocument::~SyntaxDocument() = default;

std::unique_ptr<SyntaxDocument> SyntaxDocument::create(std::string_view language, SyntaxLimits limits) {
	const syntax::Grammar* grammar = syntax::grammar(syntax::canonical(language));
	if(!grammar) return nullptr;
	auto state = std::make_unique<State>();
	state->grammar = grammar;
	state->limits = limits;
	state->parser = ts_parser_new();
	state->cursor = ts_query_cursor_new();
	if(!ts_parser_set_language(state->parser, grammar->language)) return nullptr;
	return std::unique_ptr<SyntaxDocument>(new SyntaxDocument(std::move(state)));
}

std::vector<std::string> SyntaxDocument::languages() { return syntax::names(); }

std::string SyntaxDocument::languageFor(std::string_view fileName) { return syntax::forFile(fileName); }

std::string SyntaxDocument::canonical(std::string_view name) { return syntax::canonical(name); }

const std::string& SyntaxDocument::language() const noexcept { return state_->grammar->name; }

const std::vector<std::string>& SyntaxDocument::styles() const noexcept { return state_->grammar->styles; }

bool SyntaxDocument::parsed() const noexcept { return state_->tree != nullptr; }

bool SyntaxDocument::stale() const noexcept { return state_->stale; }

void SyntaxDocument::edit(const SyntaxEdit& edit) {
	State& state = *state_;
	if(!state.tree) return;
	const TSInputEdit input{edit.startByte,      edit.oldEndByte,      edit.newEndByte,
	                        tsPoint(edit.start), tsPoint(edit.oldEnd), tsPoint(edit.newEnd)};
	ts_tree_edit(state.tree, &input);
	state.stale = true;
	// Rows remembered before shift with the lines this edit added or removed
	const auto shift = static_cast<std::int64_t>(edit.newEnd.row) - static_cast<std::int64_t>(edit.oldEnd.row);
	for(SyntaxRows& rows : state.edited) {
		if(rows.first > edit.oldEnd.row)
			rows.first = static_cast<std::uint32_t>(std::max<std::int64_t>(0, rows.first + shift));
		if(rows.last > edit.oldEnd.row)
			rows.last = static_cast<std::uint32_t>(std::max<std::int64_t>(0, rows.last + shift));
	}
	state.edited.push_back({edit.start.row, edit.newEnd.row});
	if(state.edited.size() > mostEditedRanges) {
		SyntaxRows all = state.edited.front();
		for(const SyntaxRows& rows : state.edited)
			all = {std::min(all.first, rows.first), std::max(all.last, rows.last)};
		state.edited = {all};
	}
}

SyntaxParse SyntaxDocument::parse(const SyntaxReader& read, std::size_t size, std::vector<SyntaxRows>* changed) {
	State& state = *state_;
	if(changed) changed->clear();
	if(size > state.limits.maxBytes || size > std::numeric_limits<std::uint32_t>::max()) {
		if(state.tree) ts_tree_delete(state.tree);
		state.tree = nullptr;
		state.stale = false;
		state.edited.clear();
		return SyntaxParse::TooLarge;
	}
	Input input{&read, size};
	const std::uint32_t budget = state.tree ? state.limits.editMilliseconds : state.limits.parseMilliseconds;
	Clock::time_point deadline = Clock::now() + std::chrono::milliseconds(budget);
	const TSParseOptions options{&deadline, pastDeadline};
	TSTree* next = ts_parser_parse_with_options(state.parser, state.tree,
	                                            {&input, readInput, TSInputEncodingUTF8, nullptr}, options);
	if(!next) {
		ts_parser_reset(state.parser); // the next parse starts afresh, from the edited tree
		return SyntaxParse::OverBudget;
	}
	if(changed && state.tree) {
		std::uint32_t count = 0;
		TSRange* ranges = ts_tree_get_changed_ranges(state.tree, next, &count);
		for(std::uint32_t i = 0; i < count; ++i)
			changed->push_back({ranges[i].start_point.row, ranges[i].end_point.row});
		std::free(ranges);
		changed->insert(changed->end(), state.edited.begin(), state.edited.end());
	} else if(changed) {
		changed->push_back({0, ts_node_end_point(ts_tree_root_node(next)).row});
	}
	if(state.tree) ts_tree_delete(state.tree);
	state.tree = next;
	state.stale = false;
	state.edited.clear();
	return SyntaxParse::Parsed;
}

bool SyntaxDocument::highlight(const SyntaxReader& read, std::uint32_t first, std::uint32_t last,
                               std::vector<SyntaxSpan>& out) const {
	out.clear();
	const State& state = *state_;
	const syntax::Query& query = state.grammar->highlights;
	if(!state.tree || !query.query || last < first) return true;
	state.start(query.query, first, last);
	std::vector<Found> found;
	TSQueryMatch match;
	std::uint32_t index = 0;
	bool whole = true;
	while(ts_query_cursor_next_capture(state.cursor, &match, &index)) {
		if(found.size() >= state.limits.maxCaptures) {
			whole = false;
			break;
		}
		if(!state.satisfies(query, match, read)) {
			ts_query_cursor_remove_match(state.cursor, match.id);
			continue;
		}
		const TSQueryCapture& capture = match.captures[index];
		const bool plain = state.grammar->plain[capture.index];
		found.push_back({ts_node_start_point(capture.node), ts_node_end_point(capture.node),
		                 (plain ? 0x10000u : 0u) + match.pattern_index, static_cast<std::uint16_t>(capture.index)});
	}
	if(ts_query_cursor_did_exceed_match_limit(state.cursor)) whole = false;
	// Outer first; on one range the first pattern wins (a plain variable loses to any other)
	std::ranges::stable_sort(found, [](const Found& a, const Found& b) {
		if(!same(a.start, b.start)) return before(a.start, b.start);
		if(!same(a.end, b.end)) return before(b.end, a.end);
		return a.rank < b.rank;
	});
	const auto repeat = std::ranges::unique(
	    found, [](const Found& a, const Found& b) { return same(a.start, b.start) && same(a.end, b.end); });
	found.erase(repeat.begin(), repeat.end());
	for(const Found& capture : found) {
		const std::uint32_t from = std::max(capture.start.row, first);
		const std::uint32_t to = std::min(capture.end.row, last);
		for(std::uint32_t row = from; row <= to; ++row) {
			const std::uint32_t start = row == capture.start.row ? capture.start.column : 0;
			const std::uint32_t end =
			    row == capture.end.row ? capture.end.column : std::numeric_limits<std::uint32_t>::max();
			if(start < end) out.push_back({row, start, end, capture.style});
		}
	}
	std::ranges::stable_sort(out, {}, &SyntaxSpan::row);
	return whole;
}

namespace {

/// @brief The last line a fold node hides: its last unless that line closes it (a closing token, or ends at column 0).
/// @param node The node.
std::uint32_t foldEnd(TSNode node) {
	const TSPoint end = ts_node_end_point(node);
	if(end.column == 0) return end.row == 0 ? 0 : end.row - 1;
	const std::uint32_t children = ts_node_child_count(node);
	if(children > 0 && !ts_node_is_named(ts_node_child(node, children - 1))) return end.row == 0 ? 0 : end.row - 1;
	return end.row;
}

} // namespace

void SyntaxDocument::folds(std::uint32_t first, std::uint32_t last, std::vector<SyntaxFold>& out) const {
	out.clear();
	const State& state = *state_;
	const syntax::Query& query = state.grammar->folds;
	if(!state.tree || !query.query || last < first) return;
	state.start(query.query, first, last);
	std::map<std::uint32_t, std::uint32_t> largest;
	TSQueryMatch match;
	std::uint32_t index = 0;
	std::uint32_t seen = 0;
	while(ts_query_cursor_next_capture(state.cursor, &match, &index) && seen++ < state.limits.maxCaptures) {
		const TSNode node = match.captures[index].node;
		const std::uint32_t row = ts_node_start_point(node).row;
		const std::uint32_t end = foldEnd(node);
		if(row < first || row > last || end <= row) continue;
		std::uint32_t& kept = largest[row];
		kept = std::max(kept, end);
	}
	for(const auto& [row, end] : largest) out.push_back({row, end});
}

std::optional<SyntaxFold> SyntaxDocument::fold(std::uint32_t row) const {
	std::vector<SyntaxFold> found;
	folds(row, row, found);
	if(found.empty()) return std::nullopt;
	return found.front();
}

std::optional<SyntaxBrackets> SyntaxDocument::brackets(SyntaxPoint at) const {
	const State& state = *state_;
	const syntax::Query& query = state.grammar->brackets;
	if(!state.tree || !query.query) return std::nullopt;
	ts_query_cursor_set_match_limit(state.cursor, state.limits.maxMatches);
	ts_query_cursor_set_max_start_depth(state.cursor, state.limits.maxDepth);
	ts_query_cursor_set_point_range(state.cursor, tsPoint(at), {at.row, at.column + 1});
	ts_query_cursor_exec(state.cursor, query.query, ts_tree_root_node(state.tree));
	std::optional<SyntaxBrackets> best;
	std::uint64_t nearest = std::numeric_limits<std::uint64_t>::max();
	const auto distance = [](SyntaxPoint a, SyntaxPoint b) {
		return (std::uint64_t{b.row - a.row} << 32) + (b.row == a.row ? b.column - a.column : b.column);
	};
	TSQueryMatch match;
	for(std::uint32_t seen = 0; ts_query_cursor_next_match(state.cursor, &match) && seen < state.limits.maxCaptures;
	    ++seen) {
		if(match.capture_count != 2) continue;
		SyntaxBrackets pair{pointOf(ts_node_start_point(match.captures[0].node)),
		                    pointOf(ts_node_start_point(match.captures[1].node))};
		if(pair.close < pair.open) std::swap(pair.open, pair.close);
		if(pair.open != at && pair.close != at) continue;
		const std::uint64_t apart = distance(pair.open, pair.close);
		if(apart < nearest) nearest = apart, best = pair;
	}
	return best;
}

std::string SyntaxDocument::tree() const {
	if(!state_->tree) return {};
	char* text = ts_node_string(ts_tree_root_node(state_->tree));
	std::string out(text);
	std::free(text);
	return out;
}

} // namespace affresco::services
