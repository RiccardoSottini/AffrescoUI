#pragma once

/// @file
/// @brief format::HostVariables: the globals of expressions (`window`, `history`, `theme`...) and the variables the
/// host builds (`context`, `commands`, `menus`...), each built on its first read and kept until a place it read is
/// marked changed, so an update that reads none of them builds none.

#include "Reads.hpp"
#include "affresco/core/Json.hpp"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

struct Engine;

/// The globals of expressions, each built on its first read and kept until its place, "/~g/<name>", is marked.
inline constexpr std::array<std::string_view, 9> globalNames = {"window", "revision", "proposals", "history", "preview",
                                                                "mode",   "theme",    "locale",    "screen"};

/// @brief The globals of expressions and the variables the host builds, as values built when read: each keeps what
/// it read while it was built, and goes stale the moment one of those places is marked changed (placeMarked()),
/// between updates as well as during one, so a value read after a change is never the one before it. Whoever reads
/// one records its own place: "/~g/<name>" for a global, "/~v/<name>" for a variable (places::globals,
/// places::variables).
///
/// Invariants: a value is rebuilt only when it is stale and read; one going stale marks its own place once, so what
/// read it (panels, other variables) is updated; a value read in an evaluation stays valid until the evaluation ends
/// (nothing marks a place while an expression is evaluated). A variable built from what no change announces (a
/// panel's geometry, the clock) is built again on every update, and its readers read always. Callers: DocumentContext
/// (reads, marks), UpdateFrame (updates). Main thread only.
class HostVariables {
public:
	/// @brief The values of an engine's expressions.
	/// @param engine The runtime; outlives this object.
	explicit HostVariables(Engine& engine) noexcept : engine_(engine) {}

	/// @brief A global, built if it is stale; records "/~g/<name>" in the running read set.
	/// @param name Its name.
	/// @return Its value, valid until it is stale; null if globalNames hasn't the name.
	[[nodiscard]] const json* global(std::string_view name) const;
	/// @brief Adds a variable built when first read (or replaces its builder), stale until then.
	/// @param name Its name.
	/// @param build Builds its value; it may evaluate the document's values and read other variables, not itself.
	void add(const std::string& name, std::function<json()> build);
	/// @brief A variable add() added, built if it is stale; records "/~v/<name>" in the running read set.
	/// @param name Its name.
	/// @return Its value, valid until it is stale; null if there is no such variable, or it is being built.
	[[nodiscard]] const json* variable(std::string_view name) const;

	/// @brief Makes stale what read a place changed: a global or a variable named by it, and the variables whose
	/// last build read it, then, the same way, what read those.
	/// @param place The place, as a JSON Pointer.
	void placeMarked(std::string_view place) const;
	/// @brief Makes every value stale, without marking their places: for a change of everything.
	void everythingChanged() const noexcept;
	/// @brief Before an update: makes stale the variables built from what no change announces.
	void updateStarted() const noexcept;
	/// @brief Whether a variable is built and not stale.
	/// @param name Its name.
	/// @return true if reading it now builds nothing.
	[[nodiscard]] bool built(std::string_view name) const;

private:
	/// @brief A variable: its builder, its last value, and what that build read.
	struct Variable {
		std::string place;           ///< "/~v/<name>".
		std::function<json()> build; ///< Builds it.
		json value;                  ///< Its last build.
		ReadSet reads;               ///< What its last build read.
		bool stale = true;           ///< Whether it must be built again before it is read.
	};

	/// @brief Makes a variable stale and marks its place, if it wasn't stale already.
	/// @param variable The variable.
	void makeStale(Variable& variable) const;
	/// @brief Builds a global.
	/// @param index Its index in globalNames.
	/// @return Its value.
	[[nodiscard]] json buildGlobal(std::size_t index) const;

	Engine& engine_;                                                        ///< The runtime.
	mutable std::array<std::optional<json>, globalNames.size()> globals_{}; ///< Built globals, by globalNames index.
	mutable std::map<std::string, Variable, std::less<>> variables_;        ///< The variables, by name.
	mutable std::vector<std::string> building_;                             ///< Variables being built (finds circles).
};

} // namespace affresco::format
