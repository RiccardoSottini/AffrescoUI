/// @file
/// @brief The core node types (panel, label, textarea, stack, row, column, grid, outlet): their factories, and their
/// properties for the catalog. The runtime applies these properties itself (Properties.cpp); the widgets (editor,
/// toggle, checkbox, slider, image) and the labels' "icon" register from their own files, with their apply functions.
/// The catalog lists the types in the order they are registered here.

#include "LayoutProperties.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Label.hpp"
#include "affresco/render/Stack.hpp"
#include "affresco/render/TextArea.hpp"

#include <initializer_list>
#include <utility>

namespace affresco::format {

namespace {

/// @brief A property set that only describes its properties.
/// @param properties The properties, in catalog order.
/// @return The set.
std::shared_ptr<const PropertySet> described(std::initializer_list<PropertySpec> properties) {
	auto set = std::make_shared<PropertySet>();
	set->properties = properties;
	return set;
}

/// @brief The properties of every panel, which "panel" lists. Structural keys ("children", "repeat", handlers, ...)
/// are listed too, as the catalog always has.
/// @return The set.
std::shared_ptr<const PropertySet> commonProperties() {
	return described({{"id", ValueKind::Text},
	                  {"style"},
	                  {"children"},
	                  {"x"},
	                  {"y"},
	                  {"width"},
	                  {"height"},
	                  {"minWidth"},
	                  {"minHeight"},
	                  {"anchors"},
	                  {"fill"},
	                  {"grow", ValueKind::Number},
	                  {"color", ValueKind::Color, true, true},
	                  {"radius"},
	                  {"borderWidth", ValueKind::Number},
	                  {"borderColor", ValueKind::Color, true, true},
	                  {"shadow"},
	                  {"cache", ValueKind::Boolean},
	                  {"visible", ValueKind::Boolean},
	                  {"cursor", ValueKind::Text},
	                  {"tooltip", ValueKind::Text},
	                  {"disabled", ValueKind::Boolean},
	                  {"scroll"},
	                  {"overlay"},
	                  {"focusable", ValueKind::Boolean},
	                  {"tabIndex", ValueKind::Number},
	                  {"arrows", ValueKind::Text},
	                  {"mnemonic", ValueKind::Text},
	                  {"transition"},
	                  {"hover"},
	                  {"pressed"},
	                  {"focused"},
	                  {"dragging"},
	                  {"dragOver"},
	                  {"repeat"},
	                  {"when"},
	                  {"let"},
	                  {"use"},
	                  {"with"},
	                  {"keys"},
	                  {"every"},
	                  {"drag"},
	                  {"context"},
	                  {"locked", ValueKind::Boolean},
	                  {"note", ValueKind::Text}});
}

/// @brief The properties of labels and text areas.
/// @return The specs, in catalog order.
std::vector<PropertySpec> textProperties() {
	return {{"text", ValueKind::Text},
	        {"textColor", ValueKind::Color, true, true},
	        {"font", ValueKind::Font},
	        {"padding", ValueKind::Number},
	        {"align"},
	        {"wrap", ValueKind::Boolean},
	        {"fitText"}};
}

/// @brief A set with the layout keys every node has added at its end (LayoutProperties.cpp).
/// @param set The set.
/// @return A copy of it, with them.
std::shared_ptr<const PropertySet> withLayoutItems(const std::shared_ptr<const PropertySet>& set) {
	auto extended = std::make_shared<PropertySet>(*set);
	for(PropertySpec spec : layoutItemProperties()) extended->properties.push_back(std::move(spec));
	return extended;
}

/// @brief Adds a core type.
/// @param registry The registry.
/// @param name Its name.
/// @param about One line for the catalog.
/// @param create Its factory.
/// @param own Its properties.
void addCore(TypeRegistry& registry, std::string name, std::string about, NodeType::Factory create,
             std::shared_ptr<const PropertySet> own) {
	registry.add({std::move(name), std::move(about), std::move(create), std::move(own), false});
}

} // namespace

void registerBuiltInTypes(TypeRegistry& registry) {
	addCore(
	    registry, "panel", "A rectangle; children are placed by their anchors",
	    [] { return std::make_unique<Panel>(); }, withLayoutItems(commonProperties()));

	auto label = std::make_shared<PropertySet>();
	label->properties = textProperties();
	label->properties.push_back({"icon", ValueKind::Text}); // applied by IconProperty.cpp, listed here for the catalog
	// Applied by RunsProperty.cpp, listed here for the catalog
	label->properties.push_back({"runs", ValueKind::Any, false});
	label->properties.push_back({"selectable", ValueKind::Boolean, false});
	label->properties.push_back({"selectionColor", ValueKind::Color});
	addCore(registry, "label", "Text", [] { return std::make_unique<Label>(); }, label);

	auto textarea = std::make_shared<PropertySet>();
	textarea->properties = textProperties();
	for(PropertySpec spec : std::initializer_list<PropertySpec>{{"placeholder", ValueKind::Text},
	                                                            {"placeholderColor", ValueKind::Color},
	                                                            {"bind", ValueKind::StatePath},
	                                                            {"check"},
	                                                            {"autofocus", ValueKind::Boolean}})
		textarea->properties.push_back(std::move(spec));
	addCore(registry, "textarea", "A single-line text input", [] { return std::make_unique<TextArea>(); }, textarea);
	registerEditorType(registry);

	const auto column = [] { return std::make_unique<Stack>(); };
	addCore(registry, "stack", "Children in a column or a row", column,
	        stackProperties({{"gap", ValueKind::Number},
	                         {"direction", ValueKind::Text},
	                         {"from", ValueKind::Text},
	                         {"wrap", ValueKind::Boolean}}));
	addCore(
	    registry, "row", "A stack in a row",
	    [] {
		    auto row = std::make_unique<Stack>();
		    row->setDirection(Direction::Row);
		    return row;
	    },
	    stackProperties({{"gap", ValueKind::Number}, {"from", ValueKind::Text}, {"wrap", ValueKind::Boolean}}));
	addCore(registry, "column", "A stack in a column", column,
	        stackProperties({{"gap", ValueKind::Number}, {"from", ValueKind::Text}, {"wrap", ValueKind::Boolean}}));
	addCore(
	    registry, "grid", "Children in columns and rows of tracks", [] { return std::make_unique<Grid>(); },
	    gridProperties());
	registerToggleTypes(registry);
	registerSliderType(registry);
	registerImageType(registry);
	registerSplitType(registry);
	registerTabsType(registry);
	registerListTypes(registry);
	registerIconProperty(registry);
	registerRichTextProperties(registry);
	registerPathType(registry);
	addCore(
	    registry, "outlet", "Shows the current screen from \"screens\"",
	    [] {
		    auto outlet = std::make_unique<Panel>();
		    outlet->setColor(colors::transparent);
		    return outlet;
	    },
	    described({}));
}

} // namespace affresco::format
