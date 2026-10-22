/// @file
/// @brief What a revision says (Revisions.hpp).

#include "revisions/Revisions.hpp"

#include "affresco/core/Portable.hpp"

#include <ctime>

namespace affresco::vcs::detail {

using expect::fail;

/// @brief The current time.
/// @return UTC, ISO 8601.
std::string utcNow() {
	const std::tm now = utcTime(std::time(nullptr));
	char buffer[32];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &now);
	return buffer;
}

/// @brief A revision's author.
/// @param revision The revision.
/// @return "author", or "unknown".
std::string authorOf(const json& revision) {
	const auto it = revision.find("author");
	return it != revision.end() && it->is_string() ? it->get<std::string>() : "unknown";
}

/// @brief A revision's message.
/// @param revision The revision.
/// @return "message" as text.
std::string messageOf(const json& revision) { return toText(revision.value("message", json(""))); }

/// @brief A revision's changes as JSON Patch: its "patch", then its compact edits in a fixed order: "set" (replace,
/// or add where nothing is there yet), "add", "remove", "move". `{"set": {"#statusBar/color": "@topBar"}}`.
/// @param revision The revision.
/// @return The operations ("set" ones keep op "set" until prepare() sees the document).
/// @throws DocumentError If it has neither, or they are malformed.
json patchOf(const json& revision) {
	json patch = json::array();
	if(revision.contains("patch")) {
		if(!revision["patch"].is_array()) fail("revision.patch", "expected a JSON Patch array");
		patch = revision["patch"];
	}
	auto edits = [&](const char* key) -> const json& {
		if(!revision[key].is_object()) fail(std::string("revision.") + key, "expected an object: path -> value");
		return revision[key];
	};
	if(revision.contains("set"))
		for(const auto& [path, value] : edits("set").items())
			patch.push_back({{"op", "set"}, {"path", path}, {"value", value}});
	if(revision.contains("add"))
		for(const auto& [path, value] : edits("add").items())
			patch.push_back({{"op", "add"}, {"path", path}, {"value", value}});
	if(revision.contains("remove")) {
		const json& paths = revision["remove"].is_string() ? json::array({revision["remove"]}) : revision["remove"];
		if(!paths.is_array()) fail("revision.remove", "expected a list of paths");
		for(const json& path : paths)
			patch.push_back({{"op", "remove"}, {"path", expect::string(path, "revision.remove")}});
	}
	if(revision.contains("move"))
		for(const auto& [from, to] : edits("move").items())
			patch.push_back({{"op", "move"}, {"from", from}, {"path", expect::string(to, "revision.move." + from)}});
	if(patch.empty())
		fail("revision", "expected changes: a JSON Patch (\"patch\") or compact edits (\"set\", \"add\", \"remove\", "
		                 "\"move\")");
	return patch;
}

} // namespace affresco::vcs::detail
