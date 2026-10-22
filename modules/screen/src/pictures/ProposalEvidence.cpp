/// @file
/// @brief OffscreenImages::evidence(): what a proposal does, drawn and played by the host itself, so an author can't
/// supply or forge any of it (VERSION_CONTROL.md, "Evidence").

#include "pictures/OffscreenImages.hpp"
#include "pictures/ProposalEvidence.hpp"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace affresco::detail {

std::array<int, 4> changedRegion(const RgbImage& before, const RgbImage& after) {
	if(before.width != after.width || before.height != after.height) return {0, 0, after.width, after.height};
	int left = after.width, top = after.height, right = -1, bottom = -1;
	const auto row = static_cast<std::size_t>(after.width) * 3;
	for(int y = 0; y < after.height; ++y) {
		const std::uint8_t* a = before.rgb.data() + static_cast<std::size_t>(y) * row;
		const std::uint8_t* b = after.rgb.data() + static_cast<std::size_t>(y) * row;
		if(std::memcmp(a, b, row) == 0) continue;
		top = std::min(top, y);
		bottom = y;
		for(int x = 0; x < after.width; ++x)
			if(std::memcmp(a + static_cast<std::size_t>(x) * 3, b + static_cast<std::size_t>(x) * 3, 3) != 0) {
				left = std::min(left, x);
				right = std::max(right, x);
			}
	}
	if(right < 0) return {0, 0, 0, 0};
	return {left, top, right - left + 1, bottom - top + 1};
}

namespace {

/// @brief Collects the text of every node with an id, depth first.
/// @param node A node.
/// @param texts Receives `id -> node`, in order.
void collectTexts(const json& node, std::vector<std::pair<std::string, const json*>>& texts) {
	if(!node.is_object()) return;
	if(node.contains("id") && node["id"].is_string() && node.contains("text") && node["text"].is_string())
		texts.emplace_back(node["id"].get<std::string>(), &node);
	if(node.contains("children") && node["children"].is_array())
		for(const json& child : node["children"]) collectTexts(child, texts);
}

/// @brief One changed text, for a diff view.
/// @param id The node.
/// @param original Its text on main, or nullptr.
/// @param modified Its text in the proposal, or nullptr.
/// @return `{"node", "original", "modified", "language"}`.
json textChange(const std::string& id, const json* original, const json* modified) {
	const json& node = modified ? *modified : *original;
	const json language = node.contains("language") && node["language"].is_string() ? node["language"] : json("text");
	return {{"node", "#" + id},
	        {"original", original ? original->at("text") : json("")},
	        {"modified", modified ? modified->at("text") : json("")},
	        {"language", language}};
}

} // namespace

json changedTexts(const json& main, const json& proposal) {
	std::vector<std::pair<std::string, const json*>> before, after;
	if(main.contains("root")) collectTexts(main["root"], before); // pointers into the documents themselves
	if(proposal.contains("root")) collectTexts(proposal["root"], after);
	const auto find = [](const auto& texts, const std::string& id) -> const json* {
		const auto it = std::ranges::find(texts, id, &std::pair<std::string, const json*>::first);
		return it == texts.end() ? nullptr : it->second;
	};
	json changes = json::array();
	for(const auto& [id, node] : after) {
		const json* original = find(before, id);
		if(!original || original->at("text") != node->at("text")) changes.push_back(textChange(id, original, node));
	}
	for(const auto& [id, node] : before)
		if(!find(after, id)) changes.push_back(textChange(id, node, nullptr));
	return changes;
}

std::optional<std::string> OffscreenImages::evidence(int proposal, int scale, Evidence& evidence) {
	const std::vector<ProposalInfo> open = versions_.proposals();
	const auto it = std::ranges::find(open, proposal, &ProposalInfo::id);
	if(it == open.end()) return "AffrescoUI screen: no proposal " + std::to_string(proposal);

	Comparison drawn;
	if(std::optional<std::string> error = drawBoth(proposal, scale, drawn)) return error;
	const auto& [before, after, main, document] = drawn;
	evidence.before = {encodePng(before.width, before.height, before.rgb), before.width, before.height};
	evidence.after = {encodePng(after.width, after.height, after.rgb), after.width, after.height};
	evidence.region = changedRegion(before, after);
	evidence.behaviour = it->behaviour; // worked out by the repository from the patch, never by its author
	evidence.texts = changedTexts(main, json::parse(document));
	// Played after the screen is put back: on a screen of their own, connected to nobody
	evidence.scenarios = scenarioCheck_ ? scenarioCheck_(document) : json::array();
	return std::nullopt;
}

} // namespace affresco::detail
