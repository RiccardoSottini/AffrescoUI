#pragma once

/// @file
/// @brief Features that hold Lisp: a feature's program, kept in its record and merged by the runtime, never written
/// into the screen's `/program`.
///
/// A feature's `provides.program` (`{ "<file>": { "<definition's address>": "<its text>" } }`) is built alone, as a
/// program with no `defscreen`, and what it builds is the feature's parts under their full names (`wordCount.toggle`,
/// `state.wordCount`, `wordCount-label`). Before composition merges them, the checks here keep a feature's code
/// strict: its names in its own namespace, its sections those a feature provides, and what its code reaches its own
/// state, its own commands, the language and the actions the app hands it. Every check is bounded: the build by the
/// Lisp build's budget, the walks by the manifest's size.

#include "Composition.hpp"
#include "affresco/core/Json.hpp"

#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Program.hpp>

#include <memory>
#include <string>
#include <vector>

namespace affresco::format {

/// @brief What a feature's program built, ready to merge.
struct BuiltFeature {
	json parts = json::object();                       ///< The built sections, `provides`' keys, under full names.
	std::shared_ptr<const lisp::BuiltProgram> program; ///< What the build gave, for the reach checks; null on failure.
	std::vector<lisp::Entry> entries;                  ///< Its definitions, as read.
	std::vector<std::string> problems;                 ///< Each with its place; the parts are empty when there is one.
};

/// @brief Checks the addresses of a feature's definitions (SCREEN_FORMAT.md, "A feature written as a program"):
/// `defstate` and `defcomputed` exactly `<feature>`, `defcommand` `<feature>.<word>`, the other definitions
/// `<feature>-<word>`, keys and extensions as written; `defscreen`, `defdefaults`, `defmenu`, `deftokens`, `ns` and
/// `include` refused.
/// @param feature The feature's name.
/// @param section Its program, whose shape is checked (lisp::checkProgramShape()).
/// @return The problems, each "features.<feature>.provides.program.<file>.<address>: ..."; empty if it reads well.
[[nodiscard]] std::vector<std::string> programNameProblems(const std::string& feature, const json& section);

/// @brief Builds a feature's program alone and checks what it built: only `provides`' sections, every member inside
/// the feature's namespace, no control step, and the same as a member the record's JSON parts give.
/// @param feature The feature's name.
/// @param provides Its record's `provides`, with a `program`.
/// @return The parts, with the JSON parts' members joined under their full names; or the problems.
[[nodiscard]] BuiltFeature builtFeature(const std::string& feature, const json& provides);

/// @brief Checks what a feature's code reaches: its own definitions, the language's and the expressions' functions,
/// and the steps `set`, `toggle`, `push`, `remove-state` (paths written out, under `state.<feature>`), `after`,
/// `debounce`, `run` (its own commands, or an action it needs), `copy` and `log`; a function's body reads none of the
/// app's values but the feature's own.
/// @param feature The feature's name.
/// @param built What its program built (builtFeature(), with no problem).
/// @param needs Its record's `needs`.
/// @return The problems, each naming the definition.
[[nodiscard]] std::vector<std::string> programReachProblems(const std::string& feature, const BuiltFeature& built,
                                                            const json& needs);

/// @brief Checks a feature's hooks and advice: a hook only on a hook point the app declares with `defhook` and lists
/// in a point the feature is placed in (`points.<name>.hooks`), advice only on an action export such a point receives,
/// and each one's function the feature's own.
/// @param feature The feature's name.
/// @param built What its program built (builtFeature(), with no problem).
/// @param document The app's document (its `points`, `exports` and `program`).
/// @param placed The points the feature is placed in: those it requires and those its views go to.
/// @param accepted Receives each extension that passes.
/// @return The problems, each naming the extension and its target.
[[nodiscard]] std::vector<std::string> programExtensionProblems(const std::string& feature, const BuiltFeature& built,
                                                                const json& document,
                                                                const std::vector<std::string>& placed,
                                                                std::vector<FeatureExtension>& accepted);

} // namespace affresco::format
