#pragma once

/// @file
/// @brief The A2UI 0.8 adapter's converter: its messages (A2ui.cpp) and its components (A2uiComponents.cpp).

#include "Layout.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace affresco::format::adapters::detail {

/// @brief A value from A2UI (a BoundValue: a literal, a path into the data model, or both) as the screen shows it.
struct BoundValue {
	json value;                      ///< A literal (made safe), or a binding `"{{state.model.x}}"`; null if absent.
	std::size_t characters = 0;      ///< How long its text is expected to be.
	std::optional<std::string> path; ///< The state path it is bound to, when it is one (not inside a template).
};

/// @brief Where a component is built: inside a template's repeat or not.
struct Scope {
	bool inTemplate = false; ///< Relative paths read the item.
	int depth = 0;           ///< How deeply it nests.
};

/// @brief Converts a stream of A2UI messages, one at a time, into a screen and its revisions.
class A2uiConverter {
public:
	/// @brief Starts with no surface.
	/// @param options How the screen is written.
	explicit A2uiConverter(const Options& options) : out_(options) {}

	/// @brief Takes one message.
	/// @param message The message.
	/// @param at Its place, for errors and warnings.
	/// @throws DocumentError If it isn't an A2UI message, or is past a limit.
	void message(const json& message, const std::string& at);
	/// @brief The screen the messages made.
	/// @return The screen, with the revisions for what came after it began rendering.
	/// @throws DocumentError If there was no component.
	[[nodiscard]] Converted finish();

private:
	/// @brief Takes components (surfaceUpdate).
	/// @param update The message's body.
	/// @param at Its place.
	void surfaceUpdate(const json& update, const std::string& at);
	/// @brief Takes data (dataModelUpdate).
	/// @param update The message's body.
	/// @param at Its place.
	void dataModelUpdate(const json& update, const std::string& at);
	/// @brief Whether a message is for the surface this converts, taking the first one it sees.
	/// @param body The message's body.
	/// @param at Its place, for the warning about another surface.
	/// @return Whether it is.
	[[nodiscard]] bool ours(const json& body, const std::string& at);
	/// @brief Builds the root component, as the screen's content.
	/// @return The content.
	[[nodiscard]] Built buildRoot();

	/// @brief Builds a component by id.
	/// @param id Its id.
	/// @param width The width it has.
	/// @param scope Where it is built.
	/// @return The node.
	[[nodiscard]] Built build(const std::string& id, float width, const Scope& scope);
	/// @brief Builds a component of a known type.
	/// @param type Its type ("Text", "Row"...).
	/// @param body Its properties.
	/// @param id Its id.
	/// @param width The width it has.
	/// @param scope Where it is built.
	/// @return The node.
	[[nodiscard]] Built buildType(const std::string& type, const json& body, const std::string& id, float width,
	                              const Scope& scope);
	/// @brief Text.
	[[nodiscard]] Built text(const json& body, const std::string& at, float width, const Scope& scope);
	/// @brief Image.
	[[nodiscard]] Built image(const json& body, const std::string& at, float width, const Scope& scope);
	/// @brief Row, Column and List: their children, listed or from a template.
	[[nodiscard]] Built container(const json& body, const std::string& id, bool across, float width,
	                              const Scope& scope);
	/// @brief A template's children: one component repeated for every item of a list in the data model.
	[[nodiscard]] Built repeated(const json& from, const std::string& id, bool across, float width,
	                             const Scope& scope);
	/// @brief Card: its child on a card.
	[[nodiscard]] Built card(const json& body, const std::string& id, float width, const Scope& scope);
	/// @brief Tabs: a bar of titles, and the chosen tab's child.
	[[nodiscard]] Built tabs(const json& body, const std::string& id, float width, const Scope& scope);
	/// @brief Button: its child, sending its action as a "userAction" event.
	[[nodiscard]] Built button(const json& body, const std::string& id, float width, const Scope& scope);
	/// @brief CheckBox.
	[[nodiscard]] Built checkBox(const json& body, const std::string& id, float width, const Scope& scope);
	/// @brief TextField.
	[[nodiscard]] Built textField(const json& body, const std::string& id, float width, const Scope& scope);
	/// @brief MultipleChoice: a list of its options.
	[[nodiscard]] Built multipleChoice(const json& body, const std::string& id, const Scope& scope);
	/// @brief Slider.
	[[nodiscard]] Built slider(const json& body, const std::string& id, const Scope& scope);

	/// @brief A BoundValue as the screen shows it; a path with a literal starts the data model with the literal.
	/// @param value The BoundValue (or nothing).
	/// @param at Its place.
	/// @param scope Where it is read.
	/// @return It.
	[[nodiscard]] BoundValue bound(const json& value, const std::string& at, const Scope& scope);
	/// @brief A data model path as an expression: `/a/b` is `state.model.a.b`, a relative one in a template `item.a`.
	/// @param path The path.
	/// @param scope Where it is read.
	/// @return The expression.
	[[nodiscard]] static std::string pathExpression(const std::string& path, const Scope& scope);
	/// @brief A value bound to the state, or kept in `state.inputs` when the source gives none to bind.
	/// @param value The BoundValue.
	/// @param id The component's id.
	/// @param initial The value to start with, when it isn't bound.
	/// @return The state path, or std::nullopt inside a template (where items are read, not written).
	[[nodiscard]] std::optional<std::string> bindable(const BoundValue& value, const std::string& id,
	                                                  const json& initial);
	/// @brief The steps a Button's action runs.
	/// @param action The action, `{"name", "context": [{"key", "value"}]}`.
	/// @param id The button's id.
	/// @param at Its place.
	/// @param scope Where it is built.
	/// @return An "emit" step.
	[[nodiscard]] json actionSteps(const json& action, const std::string& id, const std::string& at,
	                               const Scope& scope);

	Output out_;                             ///< The screen.
	std::string surface_;                    ///< The surface's id, once a message named one.
	std::map<std::string, json> components_; ///< The surface's components, by id: `{"Type": {...}}`.
	std::string root_;                       ///< The root component's id, once rendering began.
	std::optional<json> document_;           ///< The screen, as it was when rendering began.
	std::vector<json> revisions_;            ///< What came after.
	std::set<std::string> visiting_;         ///< The components being built, to stop at a cycle.
};

} // namespace affresco::format::adapters::detail
