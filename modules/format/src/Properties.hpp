#pragma once

/// @file
/// @brief format::Properties: a node's evaluated values on its panel: behaviour keys, look, layout, the properties
/// node types registered, and the value a widget shows.

#include "Record.hpp"

#include "affresco/render/Label.hpp"
#include "affresco/render/Stack.hpp"
#include "affresco/render/TextArea.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>

namespace affresco::format {

struct Engine;

/// @brief Puts a node's evaluated values on its panel.
///
/// Every update starts from the panel's defaults (reset()) and applies the node's properties in key order: those of
/// every panel, then the layout, then those of the core types (labels, text areas, rows, columns, grids), then those
/// the node's type registered (TypeRegistry), whose apply functions run last, the built-in widgets' first. A property
/// nobody takes is an error that names its place. Colours that have a "transition" animate (Scheduler).
/// Invariants: a record's properties are what was last applied; state blocks ("hover", ...) are applied on top of
/// them, in the order of stateBlocks. Callers: the reconciler (apply()), the events subsystem (refresh() when a visual
/// state changes) and the apply functions of node types (as their PropertyReader). Main thread only.
class Properties final : public PropertyReader {
public:
	/// @brief The properties subsystem of an engine.
	/// @param engine The runtime; outlives this object.
	explicit Properties(Engine& engine) : engine_(engine) {}

	/// @brief Applies an evaluated node to a panel: handlers, bindings, timers, then its look, then its value.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties The evaluated node.
	/// @param path Its place, for errors.
	/// @param isRoot Whether it is the window's root.
	/// @param created Whether it was just created.
	/// @throws DocumentError If a property is invalid.
	void apply(Panel& panel, Record& record, const json& properties, const std::string& path, bool isRoot,
	           bool created);
	/// @brief Shows a panel's state blocks again after hover, press, focus or drag changed; errors are reported.
	/// @param panel The panel.
	/// @param record Its record.
	void refresh(Panel& panel, Record& record);

	/* PropertyReader, for the apply functions of node types */
	[[nodiscard]] Color color(const json& value, const std::string& at) const override;
	[[nodiscard]] std::shared_ptr<const Font> font(const json& value, const std::string& at) const override;

private:
	/// @brief What an update reads from the behaviour keys and applies after the look.
	struct Behaviour {
		bool autofocus = false; ///< "autofocus".
		double everyMs = 0.0;   ///< The "every" period, or 0.
	};
	/// @brief One applyVisual() pass: the panel as each core widget type it may be, and the values set at the end
	/// (they depend on each other, animate, or go to the apply functions of node types).
	struct Look {
		Label* label = nullptr;                           ///< The panel, if it is a label (or a text area).
		TextArea* textArea = nullptr;                     ///< The panel, if it is a text area.
		Stack* stack = nullptr;                           ///< The panel, if it is a row or a column.
		Grid* grid = nullptr;                             ///< The panel, if it is a grid.
		std::optional<Color> background = {};             ///< "color".
		std::optional<Color> foreground = {};             ///< "textColor".
		std::optional<Color> border = {};                 ///< "borderColor".
		std::optional<std::pair<bool, bool>> scroll = {}; ///< "scroll": horizontal, vertical.
		std::optional<bool> overlayOpen = {};             ///< An overlay's "open", evaluated.
		std::array<json, std::tuple_size_v<PropertySets>> values = {}; ///< For each of the record's property sets.
	};

	/// @brief One key of a node, with its place.
	struct KeyValue {
		const std::string& key; ///< The key.
		const json& value;      ///< Its value.
		const std::string& at;  ///< Its place, for errors.
		bool hasId;             ///< Whether the node has an "id".
	};

	/// @brief Reads the behaviour keys of a node into its record: handlers, "bind", "caret", "keys", "drag".
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties The evaluated node.
	/// @param path Its place, for errors.
	/// @return What is applied after the look.
	Behaviour readBehaviour(Panel& panel, Record& record, const json& properties, const std::string& path) const;
	/// @brief Reads an event handler ("onClick", ...) into the record.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param key The handler.
	/// @param value An action name or steps.
	/// @param at Its place, for errors.
	static void readHandler(const Panel& panel, Record& record, const std::string& key, const json& value,
	                        const std::string& at);
	/// @brief Reads one of the other behaviour keys: bind, caret, keys, drag, every, autofocus, locked.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param property The key.
	/// @param behaviour Gets what is applied after the look.
	void readBehaviourKey(const Panel& panel, Record& record, const KeyValue& property, Behaviour& behaviour) const;
	/// @brief Reads a state path: a path, or "{{...}}" giving one (e.g. a component's parameter).
	/// @param record The node's record, for its variables.
	/// @param value The value.
	/// @param at Its place, for errors.
	/// @return The path.
	[[nodiscard]] std::string statePath(const Record& record, const json& value, const std::string& at) const;
	/// @brief Reads "every": its period, at least 16 milliseconds.
	/// @param record The node's record, for its variables.
	/// @param value {"ms": milliseconds, "do": steps}.
	/// @param at Its place, for errors.
	/// @return The period.
	[[nodiscard]] double everyPeriod(const Record& record, const json& value, const std::string& at) const;
	/// @brief Tells whether apply() takes a key with the values rather than the look ("text", "checked", ...).
	/// @param key The key.
	/// @return true if it does.
	[[nodiscard]] static bool appliedWithValues(const std::string& key);
	/// @brief Sets what depends on the whole look: scrolling, and a closed overlay hidden.
	/// @param panel The panel.
	/// @param look The look.
	static void finish(Panel& panel, const Look& look);
	/// @brief Reads "transition": property -> milliseconds, for the properties that can transition.
	/// @param value The value.
	/// @param at Its place, for errors.
	/// @return The value.
	const json& transition(const json& value, const std::string& at) const;
	/// @brief Reads "scroll": "vertical", "horizontal", "both", "none", or a boolean (vertical or none).
	/// @param value The value.
	/// @param at Its place, for errors.
	/// @return Horizontal, vertical.
	[[nodiscard]] static std::pair<bool, bool> scroll(const json& value, const std::string& at);
	/// @brief Shows a widget's value: its bound state value, else its own value property, or its "text".
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties The evaluated node.
	/// @param path Its place, for errors.
	void showValue(Panel& panel, Record& record, const json& properties, const std::string& path) const;
	/// @brief Tells whether a panel can "bind": a text area, or a widget with a value.
	/// @param panel The panel.
	/// @param record Its record.
	/// @return true if it can.
	[[nodiscard]] static bool bindable(const Panel& panel, const Record& record);
	/// @brief The properties to show now: the node with the active state blocks (hover, pressed, ...) on top.
	/// @param record The record.
	/// @return The properties.
	[[nodiscard]] json visual(const Record& record) const;
	/// @brief Sets a panel's look and layout from properties.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties From visual().
	/// @param isRoot Whether it is the window's root.
	void applyVisual(Panel& panel, Record& record, const json& properties, bool isRoot);
	/// @brief Restores a panel's defaults, so properties no longer in the document disappear.
	/// @param panel The panel.
	/// @param record Its record.
	static void reset(Panel& panel, Record& record);
	/// @brief Applies a property every panel has (id, color, visible, cursor, tooltip, disabled, transition, scroll,
	/// overlay).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @param look Collects values set at the end.
	/// @return false if it isn't one of these.
	bool commonProperty(Panel& panel, Record& record, const std::string& key, const json& value, const std::string& at,
	                    Look& look) const;
	/// @brief Applies a property of every panel's shape: radius, borderWidth, borderColor or shadow.
	/// @param panel The panel.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @param look Collects values set at the end (the border colour, which may transition).
	/// @return false if it isn't one of these.
	/// @throws DocumentError If the value is malformed.
	bool shapeProperty(Panel& panel, const std::string& key, const json& value, const std::string& at,
	                   Look& look) const;
	/// @brief Reads "overlay": where the panel floats, and whether it is open.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param value The "overlay" object.
	/// @param at Its place, for errors.
	/// @param look Gets whether it is open.
	void overlay(Panel& panel, Record& record, const json& value, const std::string& at, Look& look) const;
	/// @brief Applies a property of labels, text areas, rows, columns or grids.
	/// @param look The panel as each type, and the values set at the end.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if the panel's type has no such property.
	bool widgetProperty(Look& look, const std::string& key, const json& value, const std::string& at) const;
	/// @brief Applies a property of labels and text areas.
	/// @param look The panel as a label (and maybe a text area), and the values set at the end.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if it isn't one of these.
	bool textProperty(Look& look, const std::string& key, const json& value, const std::string& at) const;
	/// @brief Applies a property of rows and columns.
	/// @param stack The panel.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if it isn't one of these.
	static bool stackProperty(Stack& stack, const std::string& key, const json& value, const std::string& at);
	/// @brief Applies a property of grids.
	/// @param grid The panel.
	/// @param key The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if it isn't one of these.
	static bool gridProperty(Grid& grid, const std::string& key, const json& value, const std::string& at);
	/// @brief Keeps a property for one of the record's property sets, if one has it (the built-in widgets' first).
	/// @param record The record.
	/// @param key The property.
	/// @param value Its value.
	/// @param look Collects it.
	/// @return false if none has it.
	static bool registeredProperty(const Record& record, const std::string& key, const json& value, Look& look);
	/// @brief Runs the apply functions of the record's property sets: the built-in widgets' first, then its type's.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param look The values collected for each set.
	void applySets(Panel& panel, Record& record, const Look& look) const;
	/// @brief Sets the colours an update gave a panel, animating those the record has a transition for.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param look The colours the update set.
	/// @param shown The background, text and border colours shown before the update.
	void transitionLook(Panel& panel, Record& record, const Look& look, const std::array<Color, 3>& shown);

	Engine& engine_; ///< The runtime.
};

} // namespace affresco::format
