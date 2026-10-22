/// @file
/// @brief format::referenceTables(): the tables of SCREEN_FORMAT.md, generated from the registries and the reference.

#include "Reference.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <stdexcept>

namespace affresco::format {

namespace {

/// @brief The header of a table.
/// @param table Its name.
/// @return Its first two lines.
std::string_view header(std::string_view table) {
	if(table == "events") return "| Handler | When |\n|---|---|\n";
	if(table == "steps") return "| Step | Example |\n|---|---|\n";
	return "| Key | Meaning |\n|---|---|\n";
}

/// @brief The reference a table's keys are in, and the keys the registries know of it.
/// @param engine The engine.
/// @param table The table's name.
/// @return {reference, known keys}.
std::pair<std::span<const KeyReference>, std::vector<std::string>> keysOf(const Engine& engine,
                                                                          std::string_view table) {
	if(table == "sections" || table == "capabilities") return {sectionKeys(), sectionNames(engine)};
	if(table == "steps") return {stepKeys(), stepKeyNames(engine)};
	return {nodeKeys(), nodeKeyNames(engine)};
}

/// @brief Writes a row, unless none of its keys is known.
/// @param engine The engine: "{types}" in the meaning becomes its built-in types, "`panel` (default), `label`, …".
/// @param text The table.
/// @param names Its keys, "`a`, `b`".
/// @param about Its meaning.
void addRow(const Engine& engine, std::string& text, const std::string& names, std::string_view about) {
	if(names.empty()) return;
	std::string meaning(about);
	if(const std::size_t at = meaning.find("{types}"); at != std::string::npos) {
		std::string types;
		for(const NodeType* type : engine.types.inOrder())
			if(!type->application)
				types += (types.empty() ? "`" : ", `") + type->name + (types.empty() ? "` (default)" : "`");
		meaning.replace(at, 7, types);
	}
	text += "| " + names + " | " + meaning + " |\n";
}

/// @brief A table: a row for each entry with an about, joined by the entries without one after it.
/// @param engine The engine: a row lists only the keys it has (a name with "…" always).
/// @param table The table's name.
/// @return Its text.
/// @throws std::invalid_argument If no entry is in it.
std::string tableText(const Engine& engine, std::string_view table) {
	const auto [keys, known] = keysOf(engine, table);
	std::string text(header(table)), names;
	std::string_view about;
	bool any = false;
	for(const KeyReference& key : keys) {
		if(key.table != table) continue;
		any = true;
		if(!key.about.empty()) {
			addRow(engine, text, names, about);
			names.clear();
			about = key.about;
		}
		if(key.name.find("…") != std::string_view::npos || std::ranges::find(known, key.name) != known.end())
			names += (names.empty() ? "`" : ", `") + std::string(key.name) + "`";
	}
	addRow(engine, text, names, about);
	if(!any) throw std::invalid_argument("SCREEN_FORMAT.md: no reference table \"" + std::string(table) + "\"");
	return text;
}

} // namespace

std::string referenceTables(const Engine& engine, std::string_view markdown) {
	constexpr std::string_view open = "<!-- generated: ", close = " -->\n", end = "<!-- end generated -->";
	std::string result;
	std::size_t at = 0;
	for(std::size_t start = markdown.find(open); start != std::string_view::npos; start = markdown.find(open, at)) {
		const std::size_t name = start + open.size(), named = markdown.find(close, name);
		const std::size_t stop = named == std::string_view::npos ? named : markdown.find(end, named);
		if(stop == std::string_view::npos)
			throw std::invalid_argument("SCREEN_FORMAT.md: a \"generated\" marker without its end");
		result += markdown.substr(at, named + close.size() - at);
		result += tableText(engine, markdown.substr(name, named - name));
		at = stop;
	}
	result += markdown.substr(at);
	return result;
}

} // namespace affresco::format
