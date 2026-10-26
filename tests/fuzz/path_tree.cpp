/// @file
/// @brief Fuzzes pathTree (format/src/functions/ExpressionFunctionsPaths.cpp): a file explorer feeds it the names a
/// folder holds, and whoever made the folder chose them.
///
/// The input is paths, one per line; a line starting with `+` is a folder whose children aren't listed yet
/// (`{path, expandable: true}`). The tree gives each path a node exactly once: its id is its parent's id, `/`, its
/// label; every path given has its node; and in each list the folders come first. A path of more parts than the
/// deepest a tree nests is refused, and only then.

#include "Expression.hpp"
#include "Fuzz.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>

using affresco::json;

namespace {

constexpr std::size_t maxLines = 256; ///< Paths in one input, at most.
constexpr std::size_t mostDepth = 127; ///< The deepest a path nests, pathTree's limit (deeper is refused).

/// @brief A path as pathTree normalises it: the parts between slashes, empty ones left out.
/// @param path The path.
/// @return It, or empty if it has no part.
std::string normalised(std::string_view path) {
	std::string id;
	for(std::size_t from = 0; from <= path.size();) {
		const std::size_t slash = std::min(path.find('/', from), path.size());
		const std::string_view part = path.substr(from, slash - from);
		from = slash + 1;
		if(!part.empty()) id += (id.empty() ? "" : "/") + std::string(part);
	}
	return id;
}

/// @brief Checks a list of the tree's items, and their children.
/// @param items The items.
/// @param parent Their parent's id ("" at the top).
/// @param seen The ids met so far.
void check(const json& items, const std::string& parent, std::set<std::string>& seen) {
	bool files = false;
	for(const json& item : items) {
		const std::string id = item.at("id").get<std::string>(), label = item.at("label").get<std::string>();
		if(id != (parent.empty() ? label : parent + "/" + label))
			affresco::fuzz::broken("an id is its parent's and label");
		if(!seen.insert(id).second) affresco::fuzz::broken("a path has one node");
		const bool folder = (item.contains("children") && item["children"].is_array()) || item.contains("expandable");
		if(folder && files) affresco::fuzz::broken("folders come first");
		files = files || !folder;
		if(item.contains("children") && item["children"].is_array()) check(item["children"], id, seen);
	}
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	if(!affresco::fuzz::isUtf8(std::string(text))) return 0; // a document's texts are UTF-8
	json paths = json::array();
	std::set<std::string> given;
	for(std::size_t from = 0; from < text.size() && paths.size() < maxLines;) {
		const std::size_t end = std::min(text.find('\n', from), text.size());
		const std::string_view line = text.substr(from, end - from);
		from = end + 1;
		const bool folder = line.starts_with('+');
		const std::string path(folder ? line.substr(1) : line);
		paths.push_back(folder ? json{{"path", path}, {"expandable", true}} : json(path));
		if(!normalised(path).empty()) given.insert(normalised(path));
	}
	const auto partsOf = [](const std::string& path) { return std::ranges::count(path, '/') + 1; };
	const bool tooDeep = std::ranges::any_of(given, [&](const std::string& path) {
		return static_cast<std::size_t>(partsOf(path)) > mostDepth;
	});
	// The paths are data, as a screen's state holds them: written in the document itself, a text with "{{" in it
	// would be an expression
	const json state = {{"paths", std::move(paths)}};
	static const json window = json::object(), revision = 0, event = json::object();
	static const json globals = json::object();
	static const affresco::format::Scope locals{nullptr, nullptr, {}, {}, {}};
	const affresco::format::Context context{state, window, revision, event, locals, globals, nullptr};
	json tree;
	try {
		tree = affresco::format::evaluate(json{{"$pathTree", json::array({"{{state.paths}}"})}}, context, "fuzz");
	} catch(const affresco::DocumentError& error) {
		if(!tooDeep || !std::string_view(error.what()).contains("nests more than"))
			affresco::fuzz::broken("a list of texts and objects always nests, unless a path is too deep");
		return 0;
	}
	if(tooDeep) affresco::fuzz::broken("a path too deep is refused");
	std::set<std::string> seen;
	check(tree, "", seen);
	for(const std::string& path : given)
		if(!seen.contains(path)) affresco::fuzz::broken("every path given has its node");
	return 0;
}
