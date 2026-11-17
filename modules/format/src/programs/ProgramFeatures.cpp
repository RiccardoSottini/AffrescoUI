/// @file
/// @brief format::Programs: what a feature's code may reach as it runs, the second line after composition's
/// (FeaturePrograms.hpp), and the actions the app hands it.

#include "Programs.hpp"

#include "ComposedVersions.hpp"
#include "Engine.hpp"

#include <affresco/lisp/Document.hpp>

#include <algorithm>
#include <array>
#include <format>

namespace affresco::format {

namespace {

/// The prefix of the author a feature's definitions run as.
constexpr std::string_view featurePrefix = "feature:";

/// The steps a feature's code may run that reach nothing but what it is given: the others write its state, or run its
/// own commands and the actions the app hands it.
constexpr auto featureFlow = std::to_array<std::string_view>(
    {"if", "forEach", "while", "stop", "note", "log", "copy", "after", "debounce"});

/// The steps that write state, whose path must be written out under `state.<feature>`.
constexpr auto featureWrites = std::to_array<std::string_view>({"set", "toggle", "push", "remove", "increment"});

/// @brief Tells whether a list of names holds one.
/// @param names The names.
/// @param name The name.
/// @return Whether it does.
template <std::size_t Size> bool among(const std::array<std::string_view, Size>& names, std::string_view name) {
	return std::ranges::find(names, name) != names.end();
}

/// @brief Tells whether a path is written out, and is the feature's state or under it.
/// @param value The step's path.
/// @param feature The feature.
/// @return Whether it is.
bool ownState(const json& value, const std::string& feature) {
	if(!value.is_string()) return false;
	const std::string& path = value.get_ref<const std::string&>();
	const std::string root = "state." + feature;
	if(path.contains("{{") || !path.starts_with(root)) return false;
	return path.size() == root.size() || path[root.size()] == '.' || path[root.size()] == '[';
}

/// @brief Tells whether a name is written out and starts with a prefix.
/// @param value The step's name.
/// @param prefix The prefix.
/// @return Whether it does.
bool namedUnder(const json& value, const std::string& prefix) {
	if(!value.is_string()) return false;
	const std::string& name = value.get_ref<const std::string&>();
	return !name.contains("{{") && name.size() > prefix.size() && name.starts_with(prefix);
}

/// @brief The definition a step's place names, as Programs::runStepPrimitive() writes it ("lisp <definition>.<step>").
/// @param where The place.
/// @return The definition; the place itself when it names none.
std::string definitionAt(const std::string& where) {
	constexpr std::string_view lisp = "lisp ";
	if(!where.starts_with(lisp)) return where;
	const std::string rest = where.substr(lisp.size());
	const std::size_t dot = rest.rfind('.');
	return dot == std::string::npos ? rest : rest.substr(0, dot);
}

} // namespace

std::string featureAuthor(std::string_view feature) { return std::string(featurePrefix) + std::string(feature); }

bool featureMayRun(const std::string& key, const json& value, const std::string& feature) {
	if(among(featureFlow, key)) return true;
	if(among(featureWrites, key)) return ownState(value, feature);
	if(key == "run") return namedUnder(value, feature + ".") || namedUnder(value, "app.");
	// A callback of its own (after, debounce): one of its definitions
	if(key == "lisp") return namedUnder(value, feature + ".") || namedUnder(value, feature + "-");
	return false;
}

std::string_view featureOfAuthor(std::string_view author) noexcept {
	if(!author.starts_with(featurePrefix)) return {};
	author.remove_prefix(featurePrefix.size());
	return author.contains('/') ? std::string_view() : author;
}

void creditFeatures(const json& section, json& authors) {
	for(const auto& [file, entries] : section.items()) {
		const std::string feature = lisp::featureOfFile(file);
		if(feature.empty() || !entries.is_object()) continue;
		json& ofFile = authors[file];
		for(const auto& [name, text] : entries.items()) ofFile[name] = featureAuthor(feature);
	}
}

json Programs::runningProgram(const json& source) const {
	const auto* composed = dynamic_cast<const ComposedVersions*>(&engine_.versions);
	if(!composed || composed->mergedFeatures().empty()) return source.at("program");
	return lisp::effectiveProgram(source, composed->mergedFeatures());
}

std::string Programs::featureRunning() const {
	if(!engine_.authority) return {};
	for(const std::string& author : engine_.authority->authors)
		if(const std::string_view feature = featureOfAuthor(author); !feature.empty()) return std::string(feature);
	return {};
}

void Programs::checkFeatureStep(const json& step, const std::string& where) const {
	if(!engine_.authority) return;
	for(const std::string& author : engine_.authority->authors) {
		const std::string feature(featureOfAuthor(author));
		if(feature.empty()) continue;
		for(const auto& [key, value] : step.items())
			if(stepKind(key) != StepKind::None && !featureMayRun(key, value, feature))
				fail(where,
				     std::format("the feature \"{}\" led here, from {}, and a feature's code may not run the step "
				                 "\"{}\" so: it writes only its own state and runs its own commands and what the app "
				                 "hands it",
				                 feature, definitionAt(where), key));
	}
}

void Programs::checkFeatureCall(std::string_view primitive, std::span<const std::string> ran) {
	for(const std::string& author : ran)
		if(const std::string_view feature = featureOfAuthor(author); !feature.empty())
			throw lisp::Refusal("the feature \"" + std::string(feature) + "\" led here, and a feature's code may not run "
			                    "the step \"" + std::string(primitive) +
			                    "\" so: it writes only its own state and runs its own commands and what the app hands it");
}

// The app's action runs as the feature's code asked, with none of its authors' reach: a stranger's, so it runs no
// control step either
void Programs::runForFeature(const std::string& feature, const std::string& need,
                             const std::function<void(const std::string&)>& run) {
	StepAuthority apart{{featureAuthor(feature) + "/app"}, engine_.authority ? engine_.authority->run : nullptr};
	if(engine_.authority)
		for(const std::string& author : engine_.authority->authors)
			if(featureOfAuthor(author).empty() && author != apart.authors.front()) apart.authors.push_back(author);
	const AuthorityScope holder(engine_, &apart);
	std::optional<lisp::Interpreter::Apart> fresh;
	if(live_) fresh.emplace(*live_);
	run(feature + ".adapted." + need);
}

} // namespace affresco::format
