#pragma once

/// @file
/// @brief format::Events: input on the panels and the window, turned into a document's handlers.

#include "Record.hpp"

#include "affresco/render/Events.hpp"
#include "affresco/render/Window.hpp"

#include <string_view>

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
	/// @brief Writes a value to a bound state path and runs the panel's "onChange".
	/// @param panel The panel.
	/// @param value The new value.
	void valueChanged(Panel& panel, const json& value);
	/// @brief Runs a screen-wide shortcut ("keys"), and closes the overlays on Escape.
	/// @param event The key event, from the window.
	void screenKey(const KeyEvent& event);
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
	/// @brief Updates the drop target under the pointer during a drag.
	void dragOver();
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

	Engine& engine_;  ///< The runtime.
	Press press_;     ///< The press in progress.
	Click lastClick_; ///< For double clicks.
	/// @brief The connections connect() made to the window's signals (0: none).
	struct {
		Connection resized = 0, key = 0, button = 0; ///< onResize, onKey, onMouseButton.
	} windowHooks_;
};

} // namespace affresco::format
