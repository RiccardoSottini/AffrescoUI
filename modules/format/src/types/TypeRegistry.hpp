#pragma once

/// @file
/// @brief format::TypeRegistry: the node types a runtime can build, each with its factory and its own properties.

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/GpuSurface.hpp"
#include "affresco/render/ImageCache.hpp"
#include "affresco/render/Panel.hpp"

#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief The kind of value a property takes, for the catalog and the checker.
enum class ValueKind {
	Any,       ///< Anything; the type's apply function checks it.
	Boolean,   ///< true or false.
	Number,    ///< A number.
	Text,      ///< Text (numbers and booleans are shown as text).
	Color,     ///< "#rrggbb", "@name" or "transparent".
	Font,      ///< A name from "fonts".
	StatePath, ///< A path into the state, e.g. "state.volume".
};

/// @brief One property of a node type.
struct PropertySpec {
	std::string name;                ///< Its key in a node.
	ValueKind kind = ValueKind::Any; ///< The kind of value it takes.
	bool states = true;              ///< Whether a state block ("hover", "pressed", ...) may set it.
	bool transition = false;         ///< Whether "transition" may animate it.
};

class LanguageCatalog;

/// @brief What a type's apply function reads its values with: colours in the current theme, fonts by name.
class PropertyReader {
public:
	virtual ~PropertyReader() = default;

	/// @brief Reads a colour.
	/// @param value "#rrggbb", "@name" or "transparent".
	/// @param at Its place, for errors.
	/// @return The colour.
	/// @throws DocumentError If it is not a colour.
	[[nodiscard]] virtual Color color(const json& value, const std::string& at) const = 0;

	/// @brief Reads a font name.
	/// @param value A name from "fonts".
	/// @param at Its place, for errors.
	/// @return The font.
	/// @throws DocumentError If it isn't a font of the document.
	[[nodiscard]] virtual std::shared_ptr<const Font> font(const json& value, const std::string& at) const = 0;

	/// @brief A font of "fonts" in another size or style, e.g. for a bold run of rich text.
	/// @param base A font of "fonts", or nullptr for "default".
	/// @param pixelHeight The size, in pixels.
	/// @param style The style.
	/// @param at Its place, for errors.
	/// @return The font, or nullptr if @p base isn't a font of "fonts".
	/// @throws DocumentError If the document has too many sizes and styles, or the font can't load.
	[[nodiscard]] virtual std::shared_ptr<const Font> fontVariant(const Font* base, float pixelHeight, FontStyle style,
	                                                              const std::string& at) const = 0;

	/// @brief Where the document's images come from.
	[[nodiscard]] virtual ImageCache& images() const = 0;

	/// @brief Reads an image file: relative to the screen's folder, inside the folders images may come from.
	/// @param value The path.
	/// @param at Its place, for errors.
	/// @return The file.
	/// @throws DocumentError If it isn't text or is outside those folders.
	[[nodiscard]] virtual std::filesystem::path imageFile(const json& value, const std::string& at) const = 0;

	/// @brief The languages the document declares ("languages"), for code editors.
	/// @return Them, or nullptr for none.
	[[nodiscard]] virtual const LanguageCatalog* languages() const { return nullptr; }

protected:
	PropertyReader() = default;
	PropertyReader(const PropertyReader&) = default;
	PropertyReader& operator=(const PropertyReader&) = default;
	PropertyReader(PropertyReader&&) = default;
	PropertyReader& operator=(PropertyReader&&) = default;
};

/// @brief Applies a type's own properties to one of its panels.
///
/// @p values holds the evaluated values the node has, of the names the type registered (a state block's on top);
/// a name that is missing means its default. Throwing DocumentError reports it as is; another std::exception is
/// reported at @p path.
using ApplyFunction =
    std::function<void(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path)>;

/// @brief A widget's value: what its "bind" mirrors, and what it reports when the user changes it.
struct ValueHooks {
	/// The property that holds the value when the node isn't bound, e.g. "checked".
	const char* property = nullptr;
	/// Shows the value: the bound state value if @p bound is set, else @p properties' value property (or its default).
	/// @p lastText is the unbound text shown last time, for widgets that set text only when it changes.
	void (*show)(Panel& panel, const json* bound, const json& properties,
	             std::optional<std::string>& lastText) = nullptr;
	/// Connects the panel's signal for a value the user changed; @p changed gets the new value.
	Connection (*listen)(Panel& panel, std::function<void(const json& value)> changed) = nullptr;
	/// Disconnects what listen() connected.
	void (*stopListening)(Panel& panel, Connection connection) = nullptr;
};

/// @brief Connections a widget's events hold (WidgetEvents); unused places are 0.
using EventConnections = std::array<Connection, 4>;
/// @brief Runs a node's handler for a widget's event: the handler's name (e.g. "onClose") and the event.
using EventSink = std::function<void(std::string_view handler, json event)>;

/// @brief A widget's own events besides its value, e.g. a tab's "onClose": run as its node's handlers.
struct WidgetEvents {
	/// Connects the panel's signals; @p sink runs a handler of the node. Returns the connections.
	EventConnections (*listen)(Panel& panel, EventSink sink) = nullptr;
	/// Disconnects what listen() connected.
	void (*stopListening)(Panel& panel, const EventConnections& connections) = nullptr;
};

/// @brief How a widget takes part in drag and drop besides its node's "drag" and "onDrop".
struct DragHooks {
	/// The data of a drag that starts at @p press when the node's "drag" gives none; null: nothing to drag there.
	json (*data)(const Panel& panel, Vec2 press) = nullptr;
	/// Shows where a drag over the widget at @p position would drop (nullptr: hides it), and returns the fields the
	/// drop event gets besides "data", e.g. {"index": 2}.
	json (*dropAt)(Panel& panel, const Vec2* position) = nullptr;
	/// Whether the widget handles a drop on itself (a tab strip reorders): its "onDrop" isn't run for that.
	bool ownDrops = false;
};

/// @brief The properties a kind of panel has besides those of every panel, and how they are applied.
struct PropertySet {
	/// Its properties, in the order the catalog lists them. Behaviour keys ("bind", "text", "checked", "autofocus")
	/// are listed for the catalog; the runtime handles them itself, so apply() never sees them.
	std::vector<PropertySpec> properties;
	/// Applies them; empty for the built-in types whose properties the runtime applies itself, which then only
	/// describe them for the catalog.
	ApplyFunction apply;
	/// Whether apply() runs on every update (the built-in widgets, whose colours follow the theme), or only when the
	/// values or the document's appearance (its theme, colours, fonts) changed (an application's types).
	bool everyUpdate = false;
	/// For a panel of another type that has these properties too: the built-in widgets give them to subclasses, e.g.
	/// an application's TextEditor has those of "editor". Null: only panels of its own type.
	bool (*appliesTo)(const Panel& panel) = nullptr;
	/// The widget's value, if it has one (it can then "bind").
	ValueHooks value;
	/// Whether a click on the widget is its own, like a native control's: it changed the widget's value, so it doesn't
	/// bubble to an ancestor's "onClick" (unless the node has one of its own).
	bool keepsClicks = false;
	/// Its own events, if any.
	WidgetEvents events;
	/// Its part in drag and drop, if any.
	DragHooks drag;

	/// @brief Finds one of its properties.
	/// @param name The name.
	/// @return The property, or nullptr.
	[[nodiscard]] const PropertySpec* find(std::string_view name) const noexcept;
};

/// @brief The property sets that apply to one panel: its type's, then those of the built-in widgets it is a subclass
/// of. Unused places are null.
using PropertySets = std::array<std::shared_ptr<const PropertySet>, 3>;

/// @brief A GPU surface the host registered (Screen::registerSurface): what a "surface" node may name.
struct RegisteredSurface {
	SurfaceFactory factory; ///< Makes it.
	SurfaceOptions options; ///< Its framebuffer's options.
};
/// @brief The GPU surfaces the host registered, by name.
using SurfaceRegistry = std::map<std::string, RegisteredSurface, std::less<>>;

/// @brief A node type: how to create its panel, and its own properties.
struct NodeType {
	using Factory = std::function<std::unique_ptr<Panel>()>; ///< Creates a panel of the type.

	std::string name;                       ///< Its "type" in a node.
	std::string about;                      ///< One line for the catalog.
	Factory create;                         ///< Creates its panel.
	std::shared_ptr<const PropertySet> own; ///< Its own properties; never null.
	bool application = false;               ///< Registered by the application (the catalog's "customTypes").
};

/// @brief The node types a runtime can build: the built-in ones, which register from their own files
/// (registerBuiltInTypes()), and those the application registers (Runtime::registerType), through the same add().
///
/// Invariants: every type has a factory and a property set. A type added under the name of one already there replaces
/// it, built-in or not. Owned by one runtime; main thread only. Callers: the reconciler (create(), setsFor()), the
/// properties subsystem (setsFor(), transitionable()) and the catalog (types()).
class TypeRegistry {
public:
	/// @brief Adds a type, or replaces the one of the same name.
	/// @param type The type; its property set may be null for none.
	void add(NodeType type);

	/// @brief Adds properties that panels of other types take, chosen by PropertySet::appliesTo (e.g. "icon" on
	/// labels).
	/// @param set The properties; appliesTo must be set.
	void addProperties(std::shared_ptr<const PropertySet> set);

	/// @brief Finds a type.
	/// @param name Its name.
	/// @return The type, or nullptr.
	[[nodiscard]] const NodeType* find(std::string_view name) const;

	/// @brief Creates a panel of a type.
	/// @param type The "type".
	/// @param path Its place, for errors.
	/// @param placeholders Whether an unknown type becomes a plain panel (with a warning) rather than an error.
	/// @return The panel.
	/// @throws DocumentError If the type is unknown, or its factory returns nothing.
	[[nodiscard]] std::unique_ptr<Panel> create(const std::string& type, const std::string& path,
	                                            bool placeholders) const;

	/// @brief The property sets of a panel of a type that apply or show something: its own, then the built-in
	/// widgets' it inherits.
	/// @param type The type, or nullptr (the root, a placeholder).
	/// @param panel The panel.
	/// @return The sets.
	[[nodiscard]] PropertySets setsFor(const NodeType* type, const Panel& panel) const;

	/// @brief Tells whether a property of any type can transition.
	/// @param name The property.
	/// @return true if a type registered it with PropertySpec::transition.
	[[nodiscard]] bool transitionable(std::string_view name) const;

	/// @brief Every type, by name.
	[[nodiscard]] const std::map<std::string, NodeType, std::less<>>& types() const noexcept { return types_; }
	/// @brief Every type, in the order first added (a type replaced keeps its place).
	/// @return The types.
	[[nodiscard]] std::vector<const NodeType*> inOrder() const;
	/// @brief The properties that panels of other types take (addProperties()), in the order added.
	[[nodiscard]] const std::vector<std::shared_ptr<const PropertySet>>& inherited() const noexcept {
		return inherited_;
	}
	/// @brief The GPU surfaces the host registered, which "surface" nodes name (shared with that type's apply).
	[[nodiscard]] const std::shared_ptr<SurfaceRegistry>& surfaces() const noexcept { return surfaces_; }

private:
	std::map<std::string, NodeType, std::less<>> types_;        ///< See types().
	std::vector<std::string> order_;                            ///< Their names, in the order first added.
	std::vector<std::shared_ptr<const PropertySet>> inherited_; ///< Sets with appliesTo, in the order added.
	std::shared_ptr<SurfaceRegistry> surfaces_ = std::make_shared<SurfaceRegistry>(); ///< See surfaces().
};

/// @brief Registers the built-in node types (Types.cpp, and the widgets' own files).
/// @param registry The registry.
void registerBuiltInTypes(TypeRegistry& registry);

/// @brief Registers "toggle" and "checkbox" (ToggleType.cpp).
/// @param registry The registry.
void registerToggleTypes(TypeRegistry& registry);
/// @brief Registers "slider" (SliderType.cpp).
/// @param registry The registry.
void registerSliderType(TypeRegistry& registry);
/// @brief Registers "editor" (EditorType.cpp).
/// @param registry The registry.
void registerEditorType(TypeRegistry& registry);
/// @brief Registers "diff" (DiffType.cpp).
/// @param registry The registry.
void registerDiffType(TypeRegistry& registry);
/// @brief Registers "image" (ImageType.cpp).
/// @param registry The registry.
void registerImageType(TypeRegistry& registry);
/// @brief Registers "surface" (SurfaceType.cpp).
/// @param registry The registry.
void registerSurfaceType(TypeRegistry& registry);
/// @brief Registers "split" (SplitType.cpp).
/// @param registry The registry.
void registerSplitType(TypeRegistry& registry);
/// @brief Registers "tabs" (TabsType.cpp).
/// @param registry The registry.
void registerTabsType(TypeRegistry& registry);
/// @brief Registers "list" and "tree" (ListType.cpp).
/// @param registry The registry.
void registerListTypes(TypeRegistry& registry);
/// @brief Registers "icon", a property of labels (IconProperty.cpp).
/// @param registry The registry.
void registerIconProperty(TypeRegistry& registry);
/// @brief Registers "rotate", "scale", "skew", "translate" and "transformOrigin", properties of every panel
/// (TransformProperties.cpp).
/// @param registry The registry.
void registerTransformProperties(TypeRegistry& registry);
/// @brief Registers "runs", "selectable" and "selectionColor", properties of labels (RunsProperty.cpp).
/// @param registry The registry.
void registerRichTextProperties(TypeRegistry& registry);
/// @brief Registers "path" (PathType.cpp).
/// @param registry The registry.
void registerPathType(TypeRegistry& registry);
/// @brief Registers "colorPicker" (ColorPickerType.cpp).
/// @param registry The registry.
void registerColorPickerType(TypeRegistry& registry);
/// @brief Registers "gradient" (GradientType.cpp).
/// @param registry The registry.
void registerGradientType(TypeRegistry& registry);
/// @brief Registers "curves" (CurvesType.cpp).
/// @param registry The registry.
void registerCurvesType(TypeRegistry& registry);
/// @brief Registers "canvas" (CanvasType.cpp).
/// @param registry The registry.
void registerCanvasType(TypeRegistry& registry);
/// @brief Registers "graph" (GraphType.cpp).
/// @param registry The registry.
void registerGraphType(TypeRegistry& registry);
/// @brief Registers "timeline" (TimelineType.cpp).
/// @param registry The registry.
void registerTimelineType(TypeRegistry& registry);
/// @brief Registers "calendar" (CalendarType.cpp).
/// @param registry The registry.
void registerCalendarType(TypeRegistry& registry);
/// @brief Registers secret, maxLength and accept on text fields (TextFieldProperties.cpp).
/// @param registry The registry.
void registerTextFieldProperties(TypeRegistry& registry);
/// @brief Registers "codeInput" (CodeInputType.cpp).
/// @param registry The registry.
void registerCodeInputType(TypeRegistry& registry);
/// @brief Registers "carousel" (CarouselType.cpp).
/// @param registry The registry.
void registerCarouselType(TypeRegistry& registry);
/// @brief Registers "scene" (SceneType.cpp).
/// @param registry The registry.
void registerSceneType(TypeRegistry& registry);

} // namespace affresco::format
