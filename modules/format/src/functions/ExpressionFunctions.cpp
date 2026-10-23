/// @file
/// @brief The registry of expression functions: the groups' tables put together, lookups, and what the catalog and
/// the format guide say about them.

#include "ExpressionFunctions.hpp"

#include "Expression.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace affresco::format::detail {

std::span<const Function> functions() {
	// Put together once; constant afterwards
	static const std::vector<Function> all = [] {
		std::vector<Function> rows;
		for(const std::span<const Function> group :
		    {logicFunctions(), numberFunctions(), textFunctions(), listFunctions(), itemFunctions(), timeFunctions(),
		     fuzzyFunctions(), colorFunctions(), keyframeFunctions(), localeFunctions(), dateFunctions(),
		     formFunctions(), diffFunctions()})
			rows.insert(rows.end(), group.begin(), group.end());
		return rows;
	}();
	return all;
}

const Function* findFunction(std::string_view name) {
	for(const Function& function : functions())
		if(function.name == name) return &function;
	return nullptr;
}

const Function* findOperator(std::string_view name) {
	if(!name.starts_with('$')) return nullptr;
	name.remove_prefix(1);
	for(const Function& function : functions())
		if(function.name == name) return &function;
	return nullptr;
}

std::string_view trim(std::string_view text) noexcept {
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
	while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
	return text;
}

namespace {

/// @brief How the guide writes a function: `clamp(value, low, high)`, with its remark.
/// @param function The function.
/// @return The text.
std::string signature(const Function& function) {
	std::string text = "`" + std::string(function.name) + "(" + std::string(function.arguments) + ")`";
	if(!function.remark.empty()) text += " (" + std::string(function.remark) + ")";
	return text;
}

/// @brief The guide's list of a group's functions, wrapped between functions at the guide's width (120) and
/// continued with two spaces, as SCREEN_FORMAT.md is written.
/// @param lead What the list starts with: "- functions:".
/// @param group The group.
/// @param end What it ends with: ";" or ".".
/// @return The lines, each ending with a newline.
std::string listOf(std::string_view lead, Group group, std::string_view end) {
	constexpr std::size_t width = 120;
	std::vector<std::string> items;
	for(const Function& function : functions())
		if(function.group == group) items.push_back(signature(function) + ",");
	if(!items.empty()) items.back().replace(items.back().size() - 1, 1, end);

	std::string result, line(lead);
	for(const std::string& item : items) {
		if(line.size() + 1 + item.size() > width) {
			result += line + "\n";
			line = " ";
		}
		line += " " + item;
	}
	return result + line + "\n";
}

} // namespace

} // namespace affresco::format::detail

namespace affresco::format {

json operatorNames() {
	json names = json::array();
	for(const detail::Function& function : detail::functions()) names.push_back("$" + std::string(function.name));
	return names;
}

json functionCatalog() {
	json catalog = json::object();
	for(const detail::Function& function : detail::functions()) {
		catalog[std::string(function.name)] = std::string(function.arguments) + ": " + std::string(function.about);
	}
	return catalog;
}

std::string functionGuide() {
	using detail::Group;
	return detail::listOf("- functions:", Group::Functions, ";") +
	       detail::listOf("- per item, with the item as `it` and its position as `index`:", Group::Items, ".");
}

} // namespace affresco::format
