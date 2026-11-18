#pragma once

/// @file
/// @brief format::ScopeLayers: the layers of variables ("with", "let", a component's parameters) of the panels'
/// scopes, each a reader of what its variables read: a change re-evaluates them in place and marks only those whose
/// value changed, so a parameter's change reaches only what reads it.

#include "CompiledNode.hpp"
#include "Expression.hpp"
#include "Reads.hpp"
#include "Tracker.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

struct Engine;
class ScopeLayers;

/// @brief The place of a layer's variables: "/~s/<layer>" (a variable's is below it, by its name).
/// @param layer The layer's number.
/// @return The place, as a JSON Pointer.
[[nodiscard]] std::string layerPlace(std::uint32_t layer);

/// @brief What evaluating a layer's variables read, as the instance that built it needs it (Builder::instance()).
struct LayerReads {
	std::string prefix{}; ///< "/~s/<layer>/" (empty: no layer was made).
	/// Per variable (its name escaped as a key, its last definition): what it read, the layer's own places expanded.
	std::vector<std::pair<std::string, ReadSet>> byName{};
	ReadSet rows{}; ///< What they read that a repeat's row numbers (its item, its number) or that is read always.

	/// @brief Replaces the reads of this layer's variables in a set by what those variables read, so what reads them
	/// besides the panels below ("when", a dynamic "style") is built again as before when that changes.
	/// @param reads The set.
	void expand(ReadSet& reads) const;
};

/// @brief One layer: the variables of one scope, a reader of what they read. Made by ScopeLayers::make() while an
/// instance is built (Builder::instance()), it lives until the scope dies, then until ScopeLayers sweeps it.
///
/// Its update evaluates the variables again, in order, in the scope itself (each sees the ones before it, as when it
/// was built), writes them in place (Scope::vars is mutable) and marks "/~s/<layer>/<name>" for each whose value
/// changed: only the panels and layers that read it run. It comes before every panel (Reader::position()'s
/// default), after the layers made before it, which hold the variables it can read. Reads of a repeat's item or
/// number go to the row's reads too (LayerReads::rows): when such a row moves, the children list builds it again as
/// before. Main thread only.
class ScopeLayer final : public Reader {
public:
	/// @brief A layer, not added to the tracker yet.
	/// @param owner The layers.
	/// @param number Its number (not 0).
	/// @param sequence When it was made: its order among the layers.
	ScopeLayer(ScopeLayers& owner, std::uint32_t number, std::uint64_t sequence) noexcept
	    : owner_(owner), number_(number), sequence_(sequence) {}

	/// @brief Evaluates the variables again, writes them in place and marks those that changed.
	/// @param changes The update's changes.
	void update(const StateChanges& changes) override;
	/// @brief After the readers outside the panels that come first (computed values, queries), in the order the
	/// layers were made.
	/// @return The order.
	[[nodiscard]] std::uint64_t order() const noexcept override { return (std::uint64_t{1} << 32U) + sequence_; }
	/// @brief The node's place.
	/// @return "with and let of <place>".
	[[nodiscard]] std::string name() const override { return "with and let of " + at_; }
	/// @brief Whether a variable of the scope is tracked: a read of it records its place.
	/// @param variable Its name.
	/// @return true if it is one of the layer's dynamic variables.
	[[nodiscard]] bool tracks(std::string_view variable) const noexcept;

private:
	friend class ScopeLayers;

	/// @brief Evaluates the variables in order into the scope's vars.
	/// @param reads Gets what each read.
	/// @throws DocumentError If one can't be evaluated.
	void evaluate(LayerReads& reads) const;
	/// @brief Gives the tracker what the variables read, the places of the repeats' rows under the rows' keys.
	/// @param reads What they read.
	void read(const LayerReads& reads) const;
	/// @brief Whether a list one of its rows lies in may be numbered again by these changes.
	/// @param changes The changes.
	/// @return true if one of them is such a list or contains it.
	[[nodiscard]] bool rowsMayMove(const StateChanges& changes) const;
	/// @brief Marks the places of the tracked variables whose values differ from before.
	/// @param old The values before.
	void markChanged(const json& old) const;

	ScopeLayers& owner_;                 ///< The layers.
	std::uint32_t number_;               ///< Its number.
	std::uint64_t sequence_;             ///< When it was made.
	ReaderId id_ = 0;                    ///< Its handle in the tracker (0: none yet).
	const Scope* scope_ = nullptr;       ///< The scope; null once it died.
	CompiledRef node_{};                 ///< The node, whose "with" and "let" it evaluates.
	std::string at_{};                   ///< The node's place, for errors.
	std::vector<std::string> tracked_{}; ///< Its dynamic variables' names.
	ReadSet rows_{};                     ///< What the row's reads got of it (LayerReads::rows) when it was built.
	std::uint32_t nextReleased_ = 0;     ///< The layer given back before it, not swept yet (0: none).
};

/// @brief The layers of the panels' scopes, by number. Held by the builder, which makes them; the scopes hold them
/// (Scope::hold) and give them back when they die; a layer given back leaves the tracker at the next make() (sweep()),
/// and only then may its number be reused, oldest first: a reader that still reads its places would only run once
/// more than needed.
///
/// Invariants: a live layer's number is in no free list; a layer whose scope died updates nothing. Lifetime: it never
/// calls the tracker once it is destroyed; the engine destroys the tracker first, so the destructor frees the layers
/// without it. Main thread only.
class ScopeLayers {
public:
	/// @brief No layers yet.
	/// @param engine The runtime.
	explicit ScopeLayers(Engine& engine);
	/// @brief Frees the layers, without the tracker (the engine destroys it first).
	~ScopeLayers();
	ScopeLayers(const ScopeLayers&) = delete;
	ScopeLayers& operator=(const ScopeLayers&) = delete;
	ScopeLayers(ScopeLayers&&) = delete;
	ScopeLayers& operator=(ScopeLayers&&) = delete;

	/// @brief Whether a node's variables are tracked as a layer: some are dynamic, and it has no "check" (whose
	/// variables the children list builds again as before) and no malformed "with" or "let" (reported as before).
	/// @param node The node.
	/// @return true if they are.
	[[nodiscard]] static bool tracked(const CompiledNode& node);
	/// @brief Makes a scope's layer and evaluates its variables into it.
	/// @param scope The scope, not shared yet; gets its layer.
	/// @param node The node, tracked().
	/// @param at Its place, for errors.
	/// @param reads Gets what the variables read.
	/// @throws DocumentError If a variable can't be evaluated (the layer is given back with the scope).
	void make(Scope& scope, const CompiledRef& node, const std::string& at, LayerReads& reads);

	/// @brief The layers alive or not swept yet.
	/// @return Their number.
	[[nodiscard]] std::size_t size() const noexcept { return layers_.size() - free_.size() - 1; }
	/// @brief The runtime.
	/// @return It.
	[[nodiscard]] Engine& engine() const noexcept { return engine_; }

private:
	/// @brief A layer's scope died: it updates nothing and leaves the tracker at the next sweep().
	/// @param number Its number.
	void release(std::uint32_t number) noexcept;
	/// @brief Removes the layers given back from the tracker and frees their numbers.
	void sweep();

	Engine& engine_;                                  ///< The runtime.
	std::vector<std::unique_ptr<ScopeLayer>> layers_; ///< By number; 0 is never one.
	std::deque<std::uint32_t> free_;                  ///< Numbers free for reuse, oldest first.
	std::uint32_t released_ = 0;                      ///< The last layer given back, not swept yet (0: none).
	std::uint64_t sequence_ = 0;                      ///< The layers made.
	std::shared_ptr<ScopeLayers*> alive_;             ///< Scopes that outlive the layers see it expired.
};

} // namespace affresco::format
