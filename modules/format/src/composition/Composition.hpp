#pragma once

/// @file
/// @brief Composition (rfcs/0004-features.md): the screen the runtime builds from a document and its enabled
/// features, and what keeps a feature from fitting it.
///
/// A document with features keeps each one whole in its record (`/features/<name>`), so history, diffs and blame
/// need nothing else; the runtime, the rules and the checker work on the composed screen, where every declaration
/// has its full name (`wordCount.toggle`, `state.wordCount`), `app.<export>` is the export's expression, the views
/// are children of their points, and `imports` are resolved.

#include "affresco/core/Json.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief A hook or an advice a feature's program adds to the app, accepted for a review to show.
struct FeatureExtension {
	std::string feature; ///< The feature.
	std::string kind;    ///< `add-hook`, `remove-hook` or `advice`.
	std::string name;    ///< Its address in the feature's program.
	std::string target;  ///< The hook point, or the action it advises.
};

/// @brief A composed screen and what kept features out of it.
struct Composition {
	json screen;                              ///< The screen: the document with its enabled features merged in.
	std::vector<std::string> problems;        ///< Each with its place, "features.wordCount.requires.points: ...".
	std::vector<FeatureExtension> extensions; ///< The hooks and advice the merged features' programs add.
	std::vector<std::string> merged;          ///< The features merged, by name: those whose program runs.
};

/// @brief Tells whether a document has anything to compose: features or imports. Without them, the document is the
/// screen as it is (its "exports" and "point"s are only read by composition), so it costs nothing.
/// @param document The document.
/// @return true if it has a non-empty "features" or "imports" section.
[[nodiscard]] bool needsComposition(const json& document);

/// @brief Composes a document: merges its enabled features in order (then by name, after their dependencies),
/// resolves its imports and drops the sections only composition reads. A feature with a problem is left out.
/// @param document A document whose sections are valid (Rules::validate() checks them first).
/// @return The screen and the problems.
[[nodiscard]] Composition compose(const json& document);

/// @brief Composes a document whose features must all fit.
/// @param document The document.
/// @return The screen.
/// @throws DocumentError With the first problem.
[[nodiscard]] json composeOrFail(const json& document);

/// @brief Finds a node of a feature's views by id: what "#id" reaches when the app has no node with it.
/// @param document The document.
/// @param id The id, without '#'.
/// @return The node's JSON Pointer in its record ("/features/wordCount/provides/views/statusBar.right/wordCount-label"),
/// or std::nullopt.
[[nodiscard]] std::optional<std::string> findFeatureNode(const json& document, std::string_view id);

/// @brief The state an enabled feature declares, by feature: what the live state must have at `state.<name>`.
/// @param document The document.
/// @return `{ "<name>": <its initial state> }` for each enabled feature that provides state.
[[nodiscard]] json featureStates(const json& document);

} // namespace affresco::format
