#pragma once

/// @file
/// @brief Node ids: where a screen's nodes are, the ids they have, and readable ids for the nodes that have none.
///
/// Every node has a stable id (SCREEN_FORMAT.md, "Every type"), so that agents, edits, rules and diffs address it by
/// name ("#label-blocked/text") instead of by position ("/root/children/0/children/3/text"). Templates get theirs from
/// `affrescoui ids`; a node a revision adds without one gets one in the commit (DocumentRules::nameNodes).

#include "affresco/core/Json.hpp"

#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

/// @brief Calls a function for every node of a document, parents first, in document order: "root", then "screens",
/// "templates" and the components' "node"s. A component's `{"slot": true}` placeholder is not a node.
/// @param document The document.
/// @param visit Gets each node and its JSON Pointer.
void forEachNode(const json& document, const std::function<void(const json& node, const std::string& pointer)>& visit);

/// @brief An id that more than one node has.
struct DuplicateId {
	std::string id;                    ///< The id.
	std::vector<std::string> pointers; ///< The nodes that have it, as JSON Pointers, in document order.
};

/// @brief The ids that more than one node of a document has.
/// @param document The document.
/// @return Each such id once, in the order its second node comes.
[[nodiscard]] std::vector<DuplicateId> duplicateIds(const json& document);

/// @brief Every id the nodes of a document have, sorted, each once.
/// @param document The document.
/// @return The ids.
[[nodiscard]] std::vector<std::string> nodeIds(const json& document);

/// @brief Writes a node's JSON Pointer the way errors and findings name places: "/root/children/2" is
/// "root.children[2]", "/components/chip/node" is "components.chip.node".
/// @param pointer The pointer.
/// @return The place.
[[nodiscard]] std::string placeOf(std::string_view pointer);

/// @brief Tells whether a JSON Pointer is where a node goes: "/root", "/screens/<name>", "/templates/<name>",
/// "/components/<name>/node", or one of their "children" ("/children/<index>" or "/children/-") at any depth.
/// @param pointer The pointer.
/// @return true if a value there is a node.
[[nodiscard]] bool isNodePlace(std::string_view pointer);

/// @brief Gives readable ids to nodes that have none, unique among the ids a document already has.
///
/// An id is the node's type, then up to three words from what it shows or does: its text, placeholder, bound path,
/// repeat variable, tooltip, style or click action ("label-blocked", "textarea-new-card", "row-card"), else its
/// parent's id ("panel-top-bar"). Words are lower-case ASCII joined by '-'; a taken id gets "-2", "-3", ... A "use"
/// is named after its component ("move-button-2"). Its id replaces the id of the component's node in what is built,
/// as every property of a use does: the component's own id addresses the component in edits.
class NodeNamer {
public:
	/// @brief A namer for nodes going into a document.
	/// @param document The document, whose ids are taken.
	explicit NodeNamer(const json& document);

	/// Gets the JSON Pointer of each node named and the id it got.
	using Visit = std::function<void(const std::string& pointer, const std::string& id)>;

	/// @brief Gives an id to a node and to each node inside it that has none.
	/// @param node The node, changed in place.
	/// @param pointer Where it is (or goes), for @p visit.
	/// @param parentId The id of the node it goes into, or "" (words for a node that shows nothing).
	/// @param visit Gets each node named (may be empty).
	void name(json& node, const std::string& pointer, std::string_view parentId, const Visit& visit = {});

	/// @brief Gives ids to the nodes of a whole document that have none.
	/// @param document The document, changed in place.
	/// @return Each node named, as its JSON Pointer and the id it got, in document order.
	[[nodiscard]] static std::vector<std::pair<std::string, std::string>> nameAll(json& document);

private:
	/// @brief The id a node gets, reserved so that no other node gets it.
	/// @param node The node.
	/// @param parentId Its parent's id, or "".
	/// @return The id.
	std::string newId(const json& node, std::string_view parentId);

	/// @brief The type a node is built as, in words: its "type", else its style's (and so on), else "panel"; a "use"
	/// is its component's name.
	/// @param node The node.
	/// @return The words, joined by '-'.
	[[nodiscard]] std::string typeOf(const json& node) const;

	json styles_;                 ///< The document's "styles", which may give a node its type.
	std::set<std::string> taken_; ///< Every id in the document, and those given out.
};

} // namespace affresco::format
