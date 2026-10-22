#pragma once

/// @file
/// @brief format::Events: input on the panels and the window, turned into a document's handlers.

#include "Record.hpp"

#include "affresco/render/Events.hpp"
#include "affresco/render/Window.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief Turns input into the document's handlers: pointer events with clicks, double clicks and drag and drop,
/// keys and shortcuts, focus, hover and scroll, bubbling to the nearest ancestor with a handler, closing overlays on a
/// click outside, and the values widgets report.
///
/// Invariants: it connects each panel's signals once, when the panel is adopted, and the handlers read the panel's
/// current record, so updates need no reconnecting; every connection it makes is kept (Record::Hooks and the window's)
/// so disconnectAll() can take them back. A press, a drag or the last click never point at a destroyed panel
/// (forget()). Callers: the reconciler (hookUp(), forget()), the properties subsystem's panels' signals, the window's
/// signals, and ~Runtime (disconnectAll()). Main thread only.
class Events {
public:
	/// @brief The events subsystem of an engine.
	/// @param engine The runtime; outlives this object.
	explicit Events(Engine& engine) : engine_(engine) {}

	/// @brief Connects the window's signals: its size for bindings, screen-wide shortcuts, overlays closed by a press.
	/// @param window The window built into.
	void connect(Window& window);
	/// @brief Connects a panel's signals once; handlers read its current record.
	/// @param panel The panel.
	/// @param adopted Its record, which keeps the connections.
	void hookUp(Panel& panel, Record& adopted);
	/// @brief Disconnects everything that can call into the runtime: the window's signals and those of every panel
	/// built from the document that is still in the window (~Runtime).
	void disconnectAll();
	/// @brief Drops what points at a panel about to be destroyed (the press, the drag, the last click).
	/// @param panel The panel.
	void forget(const Panel& panel) noexcept;
	/// @brief Runs a panel's handler for an event, if it has one (and isn't disabled).
	/// @param panel The panel.
	/// @param handler E.g. "onHover".
	/// @param event The event.
	void fire(Panel& panel, std::string_view handler, const json& event);
	/// @brief The chord in progress: the presses so far of a key with several ("ctrl+k"), or empty.
	/// @return It, normalised.
	[[nodiscard]] const std::string& chord() const noexcept { return chord_; }
	/// @brief A modal overlay opened or closed: it takes the focus (its first focusable node, after the layout) and
	/// keeps Tab inside, then gives the focus back to the panel that had it.
	/// @param overlay The overlay.
	/// @param open Whether it opened.
	void modal(Panel& overlay, bool open);
	/// @brief Closes every open overlay that closes on a click outside it (the "dismiss" step, Escape).
	void dismissAll() { dismissOverlays(nullptr); }

private:
	/// @brief A press that may become a click, a double click or a drag.
	struct Press {
		Panel* panel = nullptr;  ///< The panel pressed (it holds the mouse).
		Vec2 position;           ///< Where.
		Panel* source = nullptr; ///< The nearest panel with "drag", if any.
		bool dragging = false;   ///< It became a drag.
		json data;               ///< The drag's data.
		Panel* over = nullptr;   ///< The drop target under the pointer.
	};
	/// @brief The last click, for double clicks.
	struct Click {
		Panel* owner = nullptr; ///< Who handled it.
		double time = 0.0;      ///< When, on the window's clock.
	};

	/// @brief Disconnects what hookUp() connected, so the panel no longer calls into the runtime.
	/// @param panel The panel.
	/// @param record Its record.
	static void unhook(Panel& panel, const Record& record);
	/// @brief Connects a text editor's caret, which it writes to the state path of its "caret".
	/// @param panel The panel, a TextEditor.
	/// @return The connection, or 0 if it isn't one.
	Connection hookCaret(Panel& panel);
	/// @brief Connects a label's links to its "onLink".
	/// @param panel The panel, a Label.
	/// @return The connection, or 0 if it isn't one.
	Connection hookLink(Panel& panel);
	/// @brief Writes a value to a bound state path and runs the panel's "onChange".
	/// @param panel The panel.
	/// @param value The new value.
	void valueChanged(Panel& panel, const json& value);
	/// @brief Resolves a key press in one walk: the chord in progress, the "keys" of the focused panel and its
	/// ancestors, then the document's; a focused text input keeps plain keys from the document's (rfcs/0001).
	/// @param from The focused panel, or nullptr.
	/// @param event The event.
	/// @return true if a binding ran, a chord started or went on, or a press ended one.
	bool resolve(Panel* from, const KeyEvent& event);
	/// @brief Runs the "keys" binding of a press on the focused panel or its nearest ancestor that has one.
	/// @param from The focused panel, or nullptr.
	/// @param press The press, with the chord in progress before it.
	/// @param waits Set when one of their keys starts with the press, so it may start or go on with a chord.
	/// @return true if a binding ran.
	bool runPanelKeys(Panel* from, const std::string& press, bool& waits);
	/// @brief Runs the document's "keys" binding of a press, on the root.
	/// @param press The press, with the chord in progress before it.
	/// @param waits Set when one of its keys starts with the press, so it may start or go on with a chord.
	/// @return true if a binding ran.
	bool runDocumentKeys(const std::string& press, bool& waits);
	/// @brief Runs a key's handler if one of its bindings applies: plain steps, a binding `{when, do, with}`
	/// (skipped if its "when" is false or its command is disabled) or a list of them (the first that applies).
	/// @param handler The handler, as written.
	/// @param owner The panel whose "keys" it is (the root for the document's).
	/// @param locals Their locals.
	/// @param press The press, normalised.
	/// @param where Its place, for errors.
	/// @return true if it ran.
	bool runBinding(const json& handler, Panel& owner, const Scope& locals, const std::string& press,
	                const std::string& where);
	/// @brief The panel that handles an event: this one or its nearest ancestor with the handler (bubbling).
	/// @param from The panel the event happened on.
	/// @param handler E.g. "onClick".
	/// @return The panel, or nullptr if none handles it.
	Panel* handlerOwner(Panel* from, std::string_view handler);
	/// @brief Runs an event's handler on the panel or its nearest ancestor with one.
	/// @param from The panel the event happened on.
	/// @param handler E.g. "onClick".
	/// @param event The event (pointer events get left/top/width/height relative to the handling panel).
	/// @param position The pointer, for pointer events (else nullptr).
	/// @return true if someone handled it.
	bool bubble(Panel& from, std::string_view handler, json event, const Vec2* position = nullptr);
	/// @brief A pointer event's fields: where, in the window and in a panel.
	/// @param type The event type.
	/// @param panel The handling panel.
	/// @param position The pointer.
	/// @return The event.
	static json pointerEvent(std::string_view type, const Panel& panel, Vec2 position);
	/// @brief Handles a mouse button on a built panel: press, release, click, double click, drop.
	/// @param panel The panel that holds the mouse.
	/// @param event The event.
	void mouseButton(Panel& panel, const MouseButtonEvent& event);
	/// @brief Handles a press of the left button: it may become a click or a drag.
	/// @param panel The panel pressed.
	/// @param at Where.
	void pressed(Panel& panel, Vec2 at);
	/// @brief Handles a release of the left button: a drop, or a release and a click (or a double click).
	/// @param panel The panel that holds the mouse.
	/// @param at Where.
	void released(Panel& panel, Vec2 at);
	/// @brief Ends a drag: the source and the target lose their states, and the target gets "onDrop".
	/// @param ended The press that became a drag.
	/// @param at Where it ended.
	void dropped(const Press& ended, Vec2 at);
	/// @brief Handles pointer moves while a panel holds the mouse: onDrag, drag and drop.
	/// @param panel The panel.
	/// @param event The event.
	void mouseMove(Panel& panel, const MouseMoveEvent& event);
	/// @brief Updates the drop target under the pointer during a drag, and a widget target's drop indicator.
	/// @param at The pointer.
	void dragOver(Vec2 at);
	/// @brief The panels the focus can move to inside a scope, in tab order: shown, with a record and not disabled,
	/// focusable or a text input, with a "tabIndex" of 0 or more (positive ones first).
	/// @param scope The scope.
	/// @param list true for an "arrows" scope: its nested "arrows" nodes are skipped; false for Tab: nested modal
	/// overlays are skipped.
	/// @return Them.
	std::vector<Panel*> focusables(Panel& scope, bool list);
	/// @brief Where Tab moves: the last open modal overlay, else the root.
	/// @return It.
	Panel& modalScope();
	/// @brief Moves the focus with the keyboard when no binding took the key: Tab and Shift+Tab, the arrows, Home
	/// and End inside an "arrows" node, Enter and Space on a focusable node, and mnemonics.
	/// @param from The focused panel, or nullptr.
	/// @param event The event.
	/// @return true if it did something.
	bool navigate(Panel* from, const KeyEvent& event);
	/// @brief Moves the focus to the next or the previous tab stop, in the open modal overlay if any, wrapping round.
	/// @param from The focused panel, or nullptr.
	/// @param back true for Shift+Tab.
	/// @return true if there was a tab stop to move to.
	bool tabFocus(Panel* from, bool back);
	/// @brief Clicks the node of a mnemonic letter (Alt+letter), anywhere in the open modal overlay if any, lists
	/// too.
	/// @param letter The letter.
	/// @return true if its "onClick" ran.
	bool pressMnemonic(char letter);
	/// @brief Handles a key pressed without modifiers on the focused panel: Enter and Space click a focusable one, the
	/// other keys move in its list.
	/// @param from The focused panel.
	/// @param key The key.
	/// @param letter The key's letter, for a mnemonic, or '\0'.
	/// @return true if it did something.
	bool plainKey(Panel& from, Key key, char letter);
	/// @brief Moves the focus inside the nearest "arrows" node above a panel with an arrow, Home or End, or clicks
	/// its item with a mnemonic.
	/// @param from The focused panel.
	/// @param key The key.
	/// @param letter The key's letter, for a mnemonic, or '\0'.
	/// @return true if the focus moved or a mnemonic's item was clicked.
	bool moveInList(Panel& from, Key key, char letter);
	/// @brief Clicks the item of an "arrows" node whose mnemonic is a letter.
	/// @param list The "arrows" node.
	/// @param letter The letter.
	/// @return true if an item's "onClick" ran.
	bool clickMnemonic(Panel& list, char letter);
	/// @brief Clicks a panel from the keyboard: its (or its nearest ancestor's) "onClick".
	/// @param panel The panel.
	/// @return true if one ran.
	bool keyClick(Panel& panel);
	/// @brief Focuses a panel after the update and its layout, if it still exists then.
	/// @param panel The panel, or nullptr for the first focusable node inside `scope`.
	/// @param scope The overlay to look in, when `panel` is nullptr.
	/// @param frames How many more frames to wait for `scope` to be laid out and shown.
	void focusLater(Panel* panel, Panel* scope, int frames = 2);
	/// @brief Runs shortcuts ("keys") and "onKey" for a key pressed on a focused panel.
	/// @param panel The panel.
	/// @param event The event.
	void key(Panel& panel, const KeyEvent& event);
	/// @brief Sets a visual flag on a panel and its ancestors (like CSS :active), refreshing them.
	/// @param from The innermost panel.
	/// @param flag Which flag.
	/// @param on Its value.
	void setFlagUpwards(Panel* from, bool Record::* flag, bool on);
	/// @brief Sets a visual flag on one panel, refreshing it.
	/// @param panel The panel, or nullptr.
	/// @param flag Which flag.
	/// @param on Its value.
	void setFlag(Panel* panel, bool Record::* flag, bool on);
	/// @brief Closes the open overlays that close on a click outside them.
	/// @param position The click, or nullptr for Escape (closes them all).
	void dismissOverlays(const Vec2* position);
	/// @brief Tells whether a click is on an overlay or on its anchor, which toggles it itself, so it stays open.
	/// @param panel The overlay.
	/// @param record Its record.
	/// @param position The click.
	/// @return true if the click leaves it as it is.
	[[nodiscard]] bool clickKeepsOpen(const Panel& panel, const Record& record, Vec2 position) const;

	Engine& engine_;           ///< The runtime.
	Press press_;              ///< The press in progress.
	Click lastClick_;          ///< For double clicks.
	std::string chord_;        ///< The chord in progress (chord()).
	bool keyResolved_ = false; ///< The focused panel resolved the press the window is about to get.
	/// @brief An open modal overlay, and who had the focus before it.
	struct Modal {
		Panel* overlay = nullptr;  ///< The overlay.
		Panel* previous = nullptr; ///< The focus before it opened (nullptr: none, or destroyed since).
	};
	std::vector<Modal> modals_; ///< Open modal overlays, the last opened last.
	/// @brief What focusLater() will focus (nullptr: forgotten, destroyed meanwhile).
	struct FocusLater {
		Panel* panel = nullptr; ///< The panel, or nullptr to look inside scope.
		Panel* scope = nullptr; ///< The overlay.
		bool pending = false;   ///< Scheduled.
		bool cancelled = false; ///< A target was destroyed meanwhile.
	};
	FocusLater focusLater_; ///< The focus change scheduled by focusLater().
	/// @brief The connections connect() made to the window's signals (0: none).
	struct {
		Connection resized = 0, key = 0, button = 0, display = 0, drop = 0; ///< onResize, onKey, onMouseButton, ...
	} windowHooks_;
};

} // namespace affresco::format
