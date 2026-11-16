/// @file
/// @brief Rules::brokenLines(): the features a change would leave out, as lines a person reads. An undo's plan shows
/// them as warnings (VERSION_CONTROL.md, "History"): undoing an earlier change may take away what a feature needs.

#include "affresco/format/Rules.hpp"

#include "Composition.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

namespace {

/// Most lines given, the last one counting the rest when there are more.
constexpr std::size_t maxLines = 16;

/// @brief The composition problems of a document, or one line saying it can't be composed.
/// @param document The document.
/// @return The problems; empty for a document with nothing to compose.
std::vector<std::string> problemsOf(const json& document) {
	if(!document.is_object() || !needsComposition(document)) return {};
	try {
		return compose(document).problems;
	} catch(const std::exception& error) {
		return {std::string("the document can't be composed: ") + error.what()};
	}
}

/// @brief The feature a problem leaves out, when its place names one.
/// @param problem The problem, e.g. "features.wordCount.requires.points: ...".
/// @return E.g. "wordCount"; empty if its place names no feature (an import's problem).
std::string_view featureOf(std::string_view problem) {
	constexpr std::string_view prefix = "features.";
	if(!problem.starts_with(prefix)) return {};
	const std::string_view rest = problem.substr(prefix.size());
	return rest.substr(0, rest.find_first_of(".:"));
}

/// @brief The lines for the new problems: one for each feature left out (with its first problem, and how many more
/// it has), one for each other problem.
/// @param problems The new problems, in composition's order.
/// @return The lines, in the order of their first problem.
std::vector<std::string> linesOf(const std::vector<std::string>& problems) {
	std::vector<std::string> lines;
	std::vector<std::string_view> features; // the feature of each line; empty for another problem
	std::vector<std::size_t> extra;         // each line's further problems of the same feature
	for(const std::string& problem : problems) {
		const std::string_view feature = featureOf(problem);
		const auto seen = std::ranges::find(features, feature);
		if(!feature.empty() && seen != features.end()) {
			++extra[static_cast<std::size_t>(seen - features.begin())];
			continue;
		}
		lines.push_back(feature.empty() ? problem
		                                : "the feature '" + std::string(feature) + "' would be left out: " + problem);
		features.push_back(feature);
		extra.push_back(0U);
	}
	for(std::size_t i = 0; i < lines.size(); ++i)
		if(extra[i] > 0) lines[i] += " (and " + std::to_string(extra[i]) + " more)";
	return lines;
}

} // namespace

std::vector<std::string> Rules::brokenLines(const json& before, const json& after) const {
	try {
		const std::vector<std::string> then = problemsOf(before);
		std::vector<std::string> fresh;
		for(const std::string& problem : problemsOf(after))
			if(std::ranges::find(then, problem) == then.end()) fresh.push_back(problem);
		std::vector<std::string> lines = linesOf(fresh);
		if(lines.size() > maxLines) {
			const std::size_t more = lines.size() - (maxLines - 1U);
			lines.resize(maxLines - 1U);
			lines.push_back("and " + std::to_string(more) + " more");
		}
		return lines;
	} catch(const std::exception& error) {
		return {std::string("the features could not be checked: ") + error.what()};
	}
}

} // namespace affresco::format
