#pragma once

/// @file
/// @brief format::Builder: what a node's children are once "use", "repeat", "with", "let", "check", styles and "when"
/// are applied, from nodes compiled once per version of the document.

#include "CompiledCache.hpp"
#include "Record.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief Turns the nodes of the document into the nodes to build: components instantiated, styles merged, repeats
/// expanded, variables scoped and "when" decided. It creates no panels.
///
/// Each node comes compiled from its CompiledCache (compiler()), once per version of the document. Invariants: it
/// never changes the document. Callers: the reconciler and the children lists (expand(), scoped(), values(),
/// compiler()) and the properties subsystem (compiler().withStyles() for state blocks). Main thread only.
class Builder {
public:
	/// @brief A builder for an engine's document.
	/// @param engine The runtime; outlives this object.
	explicit Builder(Engine& engine) : engine_(engine), compiler_(engine) {}

	/// @brief The nodes of the document compiled for its current version.
	/// @return The cache, owned by the builder.
	[[nodiscard]] const CompiledCache& compiler() const noexcept { return compiler_; }

	/// @brief The values of a node an update evaluates, placed as CompiledProperty::evaluated says.
	/// @param node The compiled node.
	/// @param locals Its variables.
	/// @param path Its place, for errors.
	/// @return The values.
	/// @throws DocumentError If one is invalid.
	[[nodiscard]] std::vector<json> values(const CompiledNode& node, const Scope& locals,
	                                       const std::string& path) const;
	/// @brief Expands "use", "repeat", "with", "let", "check", dynamic styles and "when" into the children to build.
	/// @param children The "children" array.
	/// @param locals The parent's variables.
	/// @param path Its place, for errors.
	/// @return The children.
	[[nodiscard]] std::vector<Item> expand(const json& children, const SharedScope& locals,
	                                       const std::string& path) const;
	/// @brief The variables and the final node of one built instance: "with", "let", "check", dynamic style.
	/// @param node The compiled node.
	/// @param scope Its variables so far; gets the new ones.
	/// @param at Its place, for errors.
	/// @return The node, with a dynamic style merged.
	[[nodiscard]] CompiledRef scoped(const CompiledRef& node, SharedScope& scope, const std::string& at) const;

private:
	/// @brief The list a "repeat" goes over.
	/// @param repeat The node's "repeat", compiled.
	/// @param locals The variables.
	/// @param at The node's place, for errors.
	/// @return The list, or null to build nothing.
	/// @throws DocumentError If it isn't a list.
	[[nodiscard]] json repeated(const CompiledRepeat& repeat, const Scope& locals, const std::string& at) const;
	/// @brief The list a "repeat" goes over, a data model's rows or repeated(), and records what it read.
	/// @param repeat The node's "repeat", compiled.
	/// @param locals The variables.
	/// @param at The node's place, for errors.
	/// @param place Gets where the list lies, if its items lie in the state or in a model.
	/// @return The list, or null to build nothing.
	/// @throws DocumentError If it isn't a list, or names no registered model.
	[[nodiscard]] json listOf(const CompiledRepeat& repeat, const Scope& locals, const std::string& at,
	                          std::optional<std::string>& place) const;
	/// @brief Adds the instances of one child: one, or one per item of its "repeat".
	/// @param node The child, compiled.
	/// @param locals The parent's variables.
	/// @param at The child's place, for errors.
	/// @param items Gets the instances whose "when" holds.
	void instances(const CompiledRef& node, const SharedScope& locals, const std::string& at,
	               std::vector<Item>& items) const;
	/// @brief The "check" variables of an instance: value, valid and error.
	/// @param node The node.
	/// @param layer The instance's variables so far; gets the new ones.
	/// @param at Its place, for errors.
	void check(const json& node, Scope& layer, const std::string& at) const;

	Engine& engine_;         ///< The runtime.
	CompiledCache compiler_; ///< The nodes compiled for the current version.
};

} // namespace affresco::format
