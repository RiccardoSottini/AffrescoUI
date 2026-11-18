#pragma once

/// @file
/// @brief What the runtime keeps about each panel it built (Record), the children it is about to build (Item), and
/// the limits its subsystems share.

#include "ChildList.hpp"
#include "CompiledNode.hpp"
#include "NodeKeys.hpp"
#include "Reads.hpp"
#include "Tracker.hpp"
#include "TypeRegistry.hpp"

#include "affresco/core/Signal.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Input.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::format {

/// @brief A colour animating towards a target (a "transition").
struct Tween {
	Color from;              ///< Where it started.
	Color to;                ///< Where it goes.
	double start = 0.0;      ///< The clock's seconds when it started.
	double ms = 0.0;         ///< How long it takes.
	std::uint64_t timer = 0; ///< The next frame's timer, if running.
};

/// @brief One of the three readers of a panel's record (Tracker): the copies of the items its variables hold, its
/// values, or its children's list. They run in the order of the panels, depth first: a panel's own before its
/// children's, and among its own the items, the values, then the list. Defined with the reconciler, which runs them.
class PanelReader final : public Reader {
public:
	/// @brief What it brings up to date.
	enum class Kind : std::uint8_t {
		Items,  ///< The copies of the items its variables hold (Reconciler::refreshItems()).
		Values, ///< Its values.
		List    ///< Its children's list.
	};
	/// @brief A reader of a kind, not added yet.
	/// @param of Its kind.
	explicit PanelReader(Kind of) noexcept : kind(of) {}

	/// @brief Brings its part of the panel up to date (Reconciler::runReader()).
	/// @param changes The update's changes.
	void update(const StateChanges& changes) override;
	/// @brief Unused: position() places it.
	/// @return The last order.
	[[nodiscard]] std::uint64_t order() const noexcept override { return ~std::uint64_t{0}; }
	/// @brief Its kind and the panel's type, never a place of the state.
	/// @return The name.
	[[nodiscard]] std::string name() const override;
	/// @brief Where it comes: 1, then the panel's place among its parent's children at each level from the root
	/// (each plus 3), then its kind; so after every reader outside the panels, depth first.
	/// @param out Gets the numbers.
	void position(std::vector<std::uint32_t>& out) const override;
	/// @brief A panel's: a full pass brings it up to date.
	/// @return true.
	[[nodiscard]] bool panelReader() const noexcept override { return true; }

	Kind kind;                        ///< What it brings up to date.
	ReaderId id = 0;                  ///< Its handle in the tracker (0: not added).
	Reconciler* reconciler = nullptr; ///< Who runs it.
	Panel* panel = nullptr;           ///< The panel.
};

/// @brief What the runtime knows about each panel it built. Kept by the Reconciler, from the panel's creation until
/// it is destroyed (forgetSubtree()); the other subsystems read and update it through Reconciler::recordOf().
struct Record {
	std::string type;                     ///< The node's "type".
	PropertySets sets;                    ///< The property sets of its type that apply or show something.
	std::size_t firstManaged = 0;         ///< Children a factory created come first and are left alone.
	std::size_t managed = 0;              ///< Children built from the document.
	Color defaultColor;                   ///< Restored when "color" goes away.
	Cursor defaultCursor = Cursor::Arrow; ///< Restored when "cursor" goes away.
	SharedScope locals = noVariables();   ///< Variables visible to its bindings and handlers (repeat, let, with).
	std::string path;                     ///< Place in the document, for errors.
	CompiledRef node;                     ///< The compiled node last applied (what refresh() re-applies).
	std::vector<json> values;             ///< Its values last applied: CompiledProperty::evaluated places them.
	std::uint64_t appearance = 0;         ///< DocumentContext::appearance() when it was last applied.
	PanelReader itemsReader{PanelReader::Kind::Items};   ///< Reads the items its own variables hold, if any.
	PanelReader valuesReader{PanelReader::Kind::Values}; ///< Reads what its values read when last applied.
	PanelReader listReader{PanelReader::Kind::List};     ///< Reads what building its children's list read.
	const Scope* itemsOf = nullptr;                      ///< The variables whose items itemsReader reads.
	std::uint32_t rank = 0;           ///< Where it was last found among its parent's children (a hint).
	ListState list;                   ///< What its children's list keeps between updates (ChildList).
	json handlers = json::object();   ///< "onClick" and the others: an action name or steps each.
	json keys;                        ///< Shortcuts: "cmd+k" -> handler (null: none).
	std::size_t keysHash = 0;         ///< keys' hash, set with them: what keysFingerprint() reads.
	json drag;                        ///< "drag", unevaluated (data is read when a drag starts).
	json context;                     ///< "context", unevaluated (read with the `context` variable).
	json key;                         ///< A keyed repeat's key for this item (null: none).
	std::optional<std::string> bind;  ///< State path the value mirrors.
	std::optional<std::string> caret; ///< State path a text editor writes its caret to.
	std::optional<std::string> view;  ///< State path a text editor writes what it shows to.
	/// The values last given to the apply function of its property set that runs only when they change (an
	/// application's type); there is at most one such set, its type's own.
	std::optional<json> ownProperties;
	std::uint64_t ownLooks = 0;          ///< DocumentContext::looks() when ownProperties were applied.
	std::optional<std::string> lastText; ///< "text" applied last time.
	bool hovered = false;                ///< The pointer is over it ("hover" styles).
	bool down = false;                ///< A mouse button went down on it or inside it, and is still down ("pressed").
	bool focused = false;             ///< It has the keyboard focus ("focused").
	bool dragOver = false;            ///< Something dragged is over it ("dragOver").
	bool dragging = false;            ///< It is being dragged ("dragging").
	bool disabled = false;            ///< No events but hover.
	bool shown = false;               ///< Applied at least once (transitions start only after that).
	json transition = json::object(); ///< "transition": property -> milliseconds.
	Tween colorTween;                 ///< The running transition of "color".
	Tween textTween;                  ///< The running transition of "textColor".
	Tween borderTween;                ///< The running transition of "borderColor".

	/// @brief The transitions, so their timers can be cancelled together.
	/// @return Each one.
	std::array<Tween*, 3> tweens() noexcept { return {&colorTween, &textTween, &borderTween}; }
	double everyMs = 0.0;                   ///< "every" period, if any.
	std::uint64_t everyTimer = 0;           ///< Its timer.
	std::optional<std::string> overlayOpen; ///< An overlay's "open" state path.
	bool dismissOutside = false;            ///< The overlay closes on a click outside it (and on Escape).
	std::string overlayAnchor;              ///< The overlay's anchor id (empty: the parent).
	bool modal = false;                     ///< The overlay traps the focus while it is open ("modal").
	bool modalShown = false;                ///< Events::modal() was told it opened (kept across updates).
	bool focusable = false;                 ///< Tab, Enter and Space reach it ("focusable").
	int tabIndex = 0;                       ///< "tabIndex": positive ones first, -1 leaves the tab order.
	std::string arrows;                     ///< "arrows": "vertical", "horizontal" or "both" move the focus inside.
	char mnemonic = 0;                      ///< "mnemonic": its letter, lowercase (0: none).
	std::string snapshotImage;              ///< "snapshot": the image its panel is drawn into (empty: none).
	float snapshotScale = 1.0f;             ///< That image's pixels per layout pixel.

	/// @brief The connections the events subsystem made to the panel's signals, which ~Runtime takes back (0: none).
	struct Hooks {
		Connection hover = 0;      ///< Every panel's onHover.
		Connection focus = 0;      ///< Every panel's onFocusChanged.
		Connection button = 0;     ///< Every panel's onMouseButton.
		Connection move = 0;       ///< Every panel's onMouseMove.
		Connection scroll = 0;     ///< Every panel's onScroll.
		Connection key = 0;        ///< Every panel's onKey.
		Connection value = 0;      ///< A widget's value (a text area's text, or ValueHooks::listen()).
		Connection caret = 0;      ///< A text editor's caret.
		Connection view = 0;       ///< A text editor's view.
		EventConnections events{}; ///< A widget's own events (WidgetEvents::listen()).
		Connection link = 0;       ///< A label's links.
	} hooks;                       ///< See Hooks.
};

/// @brief A child to build.
struct Item {
	CompiledRef node;   ///< Compiled once per version of the document, shared by a repeat's items.
	SharedScope locals; ///< Its variables.
	std::string path;   ///< Its place in the document.
	json key;           ///< A keyed repeat's key (null: none).
};

inline constexpr int maxActionDepth = 32;               ///< Nested action calls before giving up (recursion guard).
inline constexpr std::size_t maxLoopIterations = 10000; ///< "while" iterations before giving up.
inline constexpr int maxComponentDepth = 32;            ///< Components inside components before giving up.
/// Computed values that read computed values, before giving up: a chain of them recurses once per link.
inline constexpr std::size_t maxComputedDepth = 32;
/// Panels built inside each other before giving up. A document alone stays far below it (maxNesting levels of JSON
/// hold half as many nodes); a component or a screen that holds itself (an outlet) would recurse without end.
inline constexpr int maxNodeDepth = maxNesting;
/// Panels one screen builds before giving up, about 900 MB: components and repeats inside each other multiply (a
/// component that uses another twice, 25 levels down, would build 2^25), and the depth limits don't bound that.
inline constexpr std::size_t maxPanels = 250000;
inline constexpr double frameMs = 16.0;           ///< Animation frame period.
inline constexpr double doubleClickSeconds = 0.4; ///< Two clicks closer than this are a double click.
inline constexpr float dragThreshold = 4.0f;      ///< Pixels the pointer moves before a press becomes a drag.

} // namespace affresco::format
