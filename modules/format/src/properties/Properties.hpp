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
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief Puts a node's evaluated values on its panel.
///
/// The node comes compiled (CompiledNode): each property is a slot, so applying it dispatches on the slot. An update
/// applies a node from the panel's defaults (reset()), in key order: the properties of every panel, then the layout,
/// then those of the core types (labels, text areas, rows, columns, grids), then those the node's type registered
/// (TypeRegistry), whose apply functions run last, the built-in widgets' first. A property nobody takes is an error
/// that names its place. Colours that have a "transition" animate (Scheduler). Cost follows the change: when the
/// compiled node, its evaluated values, the theme and the colours are what was applied last, only the widget's value
/// is shown again (it may be bound to the state).
/// Invariants: a record's node and values are what was last applied, and its appearance is non-zero only once they
/// were applied without an error; state blocks ("hover", ...) are applied on top of them, in the order of stateBlocks.
/// Callers: the reconciler (apply()), the events subsystem (refresh() when a visual state changes) and the apply
/// functions of node types (as their PropertyReader). Main thread only.
class Properties final : public PropertyReader {
public:
	/// @brief The properties subsystem of an engine.
	/// @param engine The runtime; outlives this object.
	explicit Properties(Engine& engine) : engine_(engine) {}

	/// @brief Applies a compiled node to a panel: handlers, bindings, timers, then its look, then its value.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param node The compiled node.
	/// @param values Its evaluated values (Builder::values()).
	/// @param path Its place, for errors.
	/// @param isRoot Whether it is the window's root.
	/// @param created Whether it was just created.
	/// @throws DocumentError If a property is invalid.
	void apply(Panel& panel, Record& record, CompiledRef node, std::vector<json> values, const std::string& path,
	           bool isRoot, bool created);
	/// @brief Shows a panel's state blocks again after hover, press, focus or drag changed; errors are reported.
	/// @param panel The panel.
	/// @param record Its record.
	void refresh(Panel& panel, Record& record);

	/* PropertyReader, for the apply functions of node types */
	[[nodiscard]] Color color(const json& value, const std::string& at) const override;
	[[nodiscard]] std::shared_ptr<const Font> font(const json& value, const std::string& at) const override;
	[[nodiscard]] std::shared_ptr<const Font> fontVariant(const Font* base, float pixelHeight, FontStyle style,
	                                                      const std::string& at) const override;
	[[nodiscard]] ImageCache& images() const override;
	[[nodiscard]] std::filesystem::path imageFile(const json& value, const std::string& at) const override;
	[[nodiscard]] std::string assetImage(const json& value, const std::string& at) const override;
	[[nodiscard]] const LanguageCatalog* languages() const override;

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
		std::string snapshot = {};                        ///< "snapshot": the image's name (empty: none).
		float snapshotScale = 1.0f;                       ///< Its pixels per layout pixel.
		std::array<json, std::tuple_size_v<PropertySets>> values = {}; ///< For each of the record's property sets.
	};
	/// @brief The properties shown now: the node at rest, with the active state blocks on top.
	struct Visual {
		/// @brief One property.
		struct Entry {
			std::string_view key; ///< Its key (in the compiled node, or in blocks).
			Slot slot;            ///< What it sets.
			const json* value;    ///< Its value (in the record, the compiled node, or blocks).
		};
		std::vector<Entry> entries;                        ///< In the order they are applied.
		std::array<json, std::size(stateBlocks)> blocks{}; ///< The active state blocks, their styles merged.
	};

	/// @brief One key of a node, with its place.
	struct KeyValue {
		Slot slot;             ///< What it sets.
		const json& value;     ///< Its value.
		const std::string& at; ///< Its place, for errors.
	};

	/// @brief Reads the behaviour keys of the record's node into it: handlers, "bind", "caret", "keys", "drag".
	/// @param panel The panel.
	/// @param record Its record.
	/// @return What is applied after the look.
	Behaviour readBehaviour(Panel& panel, Record& record) const;
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
	/// @param key The key.
	/// @param property Its slot, value and place.
	/// @param behaviour Gets what is applied after the look.
	void readBehaviourKey(const Panel& panel, Record& record, std::string_view key, const KeyValue& property,
	                      Behaviour& behaviour) const;
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
	/// @brief Sets what depends on the whole look: scrolling, and a closed overlay hidden.
	/// @param panel The panel.
	/// @param look The look.
	static void finish(Panel& panel, const Look& look);
	/// @brief Reads "gradient": a list of colours spread evenly from top to bottom, or {"kind": "linear" or "radial",
	/// "angle": degrees (0 up, 90 right; 180 by default), "stops": colours, or {"at": 0..1, "color"}}.
	/// @param value The value.
	/// @param at Its place, for errors.
	/// @return The gradient, normalised (see normalizeGradient()).
	[[nodiscard]] std::shared_ptr<const Gradient> gradient(const json& value, const std::string& at) const;
	/// @brief Checks "transition": property -> milliseconds, for the properties that can transition.
	/// @param value The value.
	/// @param at Its place, for errors.
	void transition(const json& value, const std::string& at) const;
	/// @brief Reads "scroll": "vertical", "horizontal", "both", "none", or a boolean (vertical or none).
	/// @param value The value.
	/// @param at Its place, for errors.
	/// @return Horizontal, vertical.
	[[nodiscard]] static std::pair<bool, bool> scroll(const json& value, const std::string& at);
	/// @brief Shows a widget's value: its bound state value, else its own value property, or its "text".
	/// @param panel The panel.
	/// @param record Its record, with its node and values.
	void showValue(Panel& panel, Record& record) const;
	/// @brief Shows the value of a widget whose type has value hooks (ValueHooks::show).
	/// @param panel The panel.
	/// @param record Its record, with its node and values.
	/// @param bound Its bound state value, or nullptr if it isn't bound.
	static void showWidgetValue(Panel& panel, Record& record, const json* bound);
	/// @brief Tells whether a panel can "bind": a text area, or a widget with a value.
	/// @param panel The panel.
	/// @param record Its record.
	/// @return true if it can.
	[[nodiscard]] static bool bindable(const Panel& panel, const Record& record);
	/// @brief The properties to show now: the node with the active state blocks (hover, pressed, ...) on top.
	/// @param record The record.
	/// @return The properties.
	[[nodiscard]] Visual visual(const Record& record) const;
	/// @brief Puts an active state block's properties on top of the others.
	/// @param record The record.
	/// @param name The block, e.g. "hover".
	/// @param block Where its value, styles merged, is kept.
	/// @param result Gets its properties.
	void addStateBlock(const Record& record, std::string_view name, json& block, Visual& result) const;
	/// @brief Sets a panel's look and layout from properties.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param properties From visual().
	/// @param isRoot Whether it is the window's root.
	void applyVisual(Panel& panel, Record& record, const Visual& properties, bool isRoot);
	/// @brief Applies a property every panel has: common, shape or accessible state.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param property The property.
	/// @param look Collects values set at the end.
	/// @return false if it isn't one of these.
	/// @throws DocumentError If the value is malformed.
	bool panelProperty(Panel& panel, Record& record, const KeyValue& property, Look& look) const;
	/// @brief Applies a property only some panels have: layout, a core widget type's, or a property set's.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param key The property's key.
	/// @param property The property.
	/// @param look Collects values set at the end.
	/// @return false if the panel's type has no such property.
	/// @throws DocumentError If the value is malformed.
	bool typeProperty(Panel& panel, const Record& record, std::string_view key, const KeyValue& property,
	                  Look& look) const;
	/// @brief Restores a panel's defaults, so properties no longer in the document disappear.
	/// @param panel The panel.
	/// @param record Its record.
	static void reset(Panel& panel, Record& record);
	/// @brief Restores the defaults of the core widget types' properties: labels, text areas, rows, columns, grids.
	/// @param panel The panel.
	/// @param record Its record, for its type.
	static void resetWidget(Panel& panel, const Record& record);
	/// @brief Restores the record's defaults that its node's properties set.
	/// @param record The record.
	static void resetRecord(Record& record);
	/// @brief Applies a property every panel has (id, color, visible, cursor, tooltip, disabled, transition, scroll,
	/// overlay); this and the other property groups are in CommonProperties.cpp.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param property The property.
	/// @param look Collects values set at the end.
	/// @return false if it isn't one of these.
	bool commonProperty(Panel& panel, Record& record, const KeyValue& property, Look& look) const;
	/// @brief Applies one of commonProperty()'s that only the panel takes: id, color, visible, cursor, tooltip, role,
	/// label, description, scroll.
	/// @param panel The panel.
	/// @param property The property.
	/// @param look Collects values set at the end.
	/// @return false if it isn't one of these.
	bool panelOnlyProperty(Panel& panel, const KeyValue& property, Look& look) const;
	/// @brief Applies one of commonProperty()'s that the record keeps: disabled, transition, overlay, focusable,
	/// tabIndex, arrows, mnemonic.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param property The property.
	/// @param look Collects values set at the end.
	/// @return false if it isn't one of these.
	bool recordProperty(Panel& panel, Record& record, const KeyValue& property, Look& look) const;
	/// @brief Applies a property of every panel's shape: radius, borderWidth, borderColor or shadow.
	/// @param panel The panel.
	/// @param slot The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @param look Collects values set at the end (the border colour, which may transition).
	/// @return false if it isn't one of these.
	/// @throws DocumentError If the value is malformed.
	bool shapeProperty(Panel& panel, Slot slot, const json& value, const std::string& at, Look& look) const;
	/// @brief Reads "overlay": where the panel floats, and whether it is open.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param value The "overlay" object.
	/// @param at Its place, for errors.
	/// @param look Gets whether it is open.
	void overlay(Panel& panel, Record& record, const json& value, const std::string& at, Look& look) const;
	/// @brief Reads an overlay's "open": a state path, which a click outside can then close, or a value.
	/// @param record The panel's record, which keeps the path.
	/// @param open The value.
	/// @param at Its place, for errors.
	/// @param look Gets whether it is open.
	void overlayOpen(Record& record, const json& open, const std::string& at, Look& look) const;
	/// @brief Traps the focus in a modal overlay while it is open, and gives it back when it closes.
	/// @param panel The panel.
	/// @param record Its record, with "modal" read.
	/// @param look Whether it is open.
	void showModal(Panel& panel, Record& record, const Look& look) const;
	/// @brief Reads "snapshot": an image name, or `{ "image": name, "scale": 0.25 }`.
	/// @param value The value.
	/// @param at Its place, for errors.
	/// @param look Gets the name and the scale.
	/// @throws DocumentError If it is malformed.
	static void snapshot(const json& value, const std::string& at, Look& look);
	/// @brief Draws the panel into the image its "snapshot" names, out of the layout, or back into it when the key
	/// is gone (its image then goes too).
	/// @param panel The panel.
	/// @param record Its record, which keeps the image drawn into.
	/// @param look The name and scale read, if any.
	/// @param isRoot Whether it is the window's root.
	/// @throws DocumentError On the root, or with "overlay".
	void showSnapshot(Panel& panel, Record& record, const Look& look, bool isRoot) const;
	/// @brief Applies a property of labels, text areas, rows, columns or grids.
	/// @param look The panel as each type, and the values set at the end.
	/// @param slot The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if the panel's type has no such property.
	bool widgetProperty(Look& look, Slot slot, const json& value, const std::string& at) const;
	/// @brief Applies a property of labels and text areas.
	/// @param look The panel as a label (and maybe a text area), and the values set at the end.
	/// @param slot The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if it isn't one of these.
	bool textProperty(Look& look, Slot slot, const json& value, const std::string& at) const;
	/// @brief Applies a property of rows and columns.
	/// @param stack The panel.
	/// @param slot The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if it isn't one of these.
	static bool stackProperty(Stack& stack, Slot slot, const json& value, const std::string& at);
	/// @brief Applies a property of grids.
	/// @param grid The panel.
	/// @param slot The property.
	/// @param value Its value.
	/// @param at Its place, for errors.
	/// @return false if it isn't one of these.
	static bool gridProperty(Grid& grid, Slot slot, const json& value, const std::string& at);
	/// @brief Keeps a property for each of the record's property sets that has it (a type's own, a built-in widget's).
	/// @param record The record.
	/// @param key The property.
	/// @param value Its value.
	/// @param look Collects it.
	/// @return false if none has it.
	static bool registeredProperty(const Record& record, std::string_view key, const json& value, Look& look);
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

/// @brief Applies a state every node may give assistive technologies: "expanded", "selected" or "live"
/// (AccessibilityProperties.cpp).
/// @param panel The panel.
/// @param slot The property.
/// @param value Its value.
/// @param at Its place, for errors.
/// @return false if it isn't one of these.
bool accessibleStateProperty(Panel& panel, Slot slot, const json& value, const std::string& at);

} // namespace affresco::format
