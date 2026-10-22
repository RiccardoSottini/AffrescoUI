#pragma once

/// @file
/// @brief The Adaptive Cards 1.6 adapter's converter: its elements (AdaptiveCards.cpp) and its actions and inputs
/// (AdaptiveCardActions.cpp).

#include "Layout.hpp"

#include <optional>
#include <string>
#include <vector>

namespace affresco::format::adapters::detail {

/// @brief Converts one Adaptive Card into a screen.
class CardConverter {
public:
	/// @brief Starts an empty screen.
	/// @param options How it is written.
	explicit CardConverter(const Options& options) : out_(options) {}

	/// @brief Converts the card.
	/// @param card The card.
	/// @return The screen and its warnings.
	/// @throws DocumentError If it isn't a card, or is past a limit.
	[[nodiscard]] Converted convert(const json& card);

private:
	/// @brief Elements one below the other ("body", "items").
	/// @param elements The elements.
	/// @param at Their place.
	/// @param width The width they have.
	/// @param depth How deeply they nest.
	/// @return Their column.
	[[nodiscard]] Built elements(const json& elements, const std::string& at, float width, int depth);
	/// @brief One element, or its fallback when it has none of ours.
	/// @return The node; a node with "visible": false for "isVisible": false.
	[[nodiscard]] std::vector<Built> element(const json& element, const std::string& at, float width, int depth);
	/// @brief An element of a type it knows.
	/// @return The nodes, or std::nullopt for a type it doesn't know.
	[[nodiscard]] std::optional<std::vector<Built>> elementOfType(const std::string& type, const json& element,
	                                                              const std::string& at, float width, int depth);
	/// @brief Container: its items, on a background for a "style".
	[[nodiscard]] Built container(const json& element, const std::string& at, float width, int depth);
	/// @brief TextBlock and RichTextBlock.
	[[nodiscard]] Built textBlock(const json& element, const std::string& text, float width);
	/// @brief Image.
	[[nodiscard]] Built image(const json& element, const std::string& at, float width, float size);
	/// @brief ColumnSet: its columns side by side, by their width.
	[[nodiscard]] Built columnSet(const json& element, const std::string& at, float width, int depth);
	/// @brief FactSet: a title and a value on each line.
	[[nodiscard]] Built factSet(const json& element, const std::string& at, float width);

	/// @brief The inputs: Input.Text, Number, Date, Time, Toggle and ChoiceSet, bound to `state.inputs.<id>`.
	[[nodiscard]] Built input(const std::string& type, const json& element, const std::string& at, float width);
	/// @brief Input.ChoiceSet: a list of its choices.
	[[nodiscard]] Built choiceSet(const json& element, const std::string& key);
	/// @brief Actions side by side, and below them the cards that Action.ShowCard shows.
	/// @param actions The actions.
	/// @param at Their place.
	/// @param width The width they have.
	/// @param depth How deeply they nest.
	/// @return The nodes.
	[[nodiscard]] std::vector<Built> actions(const json& actions, const std::string& at, float width, int depth);
	/// @brief The steps an action runs: an "emit" (Submit, Execute, OpenUrl) or a "toggle" (ShowCard). Each Submit,
	/// Execute and ShowCard takes the next number N: its data is `state.actions.aN`, its card `state.cards.aN`.
	/// @param action The action.
	/// @param at Its place.
	/// @return The steps, or null for an action with no equivalent (with a warning).
	[[nodiscard]] json actionSteps(const json& action, const std::string& at);
	/// @brief A node's onClick from its "selectAction", if it has one.
	/// @param node The node.
	/// @param element The element.
	/// @param at Its place.
	void selectAction(json& node, const json& element, const std::string& at);

	/// @brief Text from the card as the screen shows it: `${name}` of the card's data as a binding to
	/// `state.data.name`, the rest as written (see Output::literal()).
	/// @param text The text.
	/// @param at Its place, for the warning about an expression it can't convert.
	/// @return The text, or a binding.
	[[nodiscard]] json templated(const std::string& text, const std::string& at);
	/// @brief An element's text field, templated.
	/// @param element The element.
	/// @param key The field.
	/// @param at The element's place.
	/// @return The text, or a binding ("" if it has none).
	[[nodiscard]] std::string field(const json& element, const char* key, const std::string& at);

	Output out_;              ///< The screen.
	std::size_t actions_ = 0; ///< Actions numbered so far (see actionSteps()).
};

} // namespace affresco::format::adapters::detail
