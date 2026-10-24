/// @file
/// @brief format::Rules on assets (VERSION_CONTROL.md, "Assets"): the "assets" section, what a revision may do to the
/// "audio" section, what only the person accepts, the steps a revision runs once, and an asset's bytes checked by
/// the decoders that will read them.

#include "affresco/format/NodeIds.hpp"
#include "affresco/format/Rules.hpp"

#include "AudioPlayers.hpp"
#include "SectionRegistry.hpp"

#include "affresco/core/Assets.hpp"
#include "affresco/render/Bitmap.hpp"

#include <format>
#include <map>
#include <set>
#include <span>

namespace affresco::format {

using expect::fail;

namespace {

/// The name the steps a revision runs get, as an action, while they are checked.
constexpr const char* runAction = "revisionRun";
/// The most a revision's "run" steps take, written as JSON.
constexpr std::size_t maxRunBytes = std::size_t{1} << 20;

/// @brief "assets": what each asset of the document is, `name → {"type", "size", "sha256"}` (version control writes
/// it from a revision's "assets"; its bytes are kept with the history).
/// @param value The value.
/// @param name "assets".
void checkAssets(const json& value, const std::string& name) {
	if(expect::object(value, name).size() > maxAssets) fail(name, "at most " + std::to_string(maxAssets) + " assets");
	for(const auto& [asset, entry] : value.items()) {
		const std::string at = name + "." + asset;
		if(const auto problem = assetNameProblem(asset)) fail(at, *problem);
		expect::object(entry, at);
		for(const auto& [key, item] : entry.items())
			if(key != "type" && key != "size" && key != "sha256") fail(at + "." + key, "expected type, size or sha256");
		if(const auto problem = assetTypeProblem(asset, expect::string(entry.value("type", json()), at + ".type")))
			fail(at + ".type", *problem);
		if(!entry.contains("size") || !entry["size"].is_number_unsigned() ||
		   entry["size"].get<std::size_t>() > maxAssetBytes)
			fail(at + ".size", "expected the number of its bytes, at most " + std::to_string(maxAssetBytes));
		const std::string id = expect::string(entry.value("sha256", json()), at + ".sha256");
		if(id.size() != 64 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
			fail(at + ".sha256", "expected the SHA-256 of its bytes (64 lowercase hex digits)");
	}
}

/// @brief The sounds of a document.
/// @param document The document.
/// @return Its "audio" section, or an empty object.
const json& soundsOf(const json& document) {
	static const json none = json::object();
	const auto it = document.find("audio");
	return it != document.end() && it->is_object() ? *it : none;
}

/// @brief The asset references of nodes that show a file (`"file": "asset:<name>"`), by node id.
/// @param document The document.
/// @return `id → name` (a node without an id by its pointer).
std::map<std::string, std::string> shownAssets(const json& document) {
	std::map<std::string, std::string> shown;
	forEachNode(document, [&](const json& node, const std::string& pointer) {
		if(!node.is_object() || !node.contains("file")) return;
		if(const auto asset = assetReference(node["file"]))
			shown[node.contains("id") && node["id"].is_string() ? "#" + node["id"].get<std::string>() : pointer] =
			    *asset;
	});
	return shown;
}

} // namespace

SectionType assetsSection() { return {"assets", checkAssets}; }

// A revision may give a sound a file of the screen's folder or an asset, or a tone: never a URL or the host's source,
// which reach the network or what the host made
void checkAudioChange(const json& before, const json& after) {
	const json& old = soundsOf(before);
	for(const auto& [name, sound] : soundsOf(after).items()) {
		if(const auto same = old.find(name); same != old.end() && *same == sound) continue;
		const std::string at = "audio." + name;
		if(sound.contains("url") || sound.contains("source"))
			fail(at, "permission denied: a revision may give a sound a \"file\" (one of the screen's, or "
			         "\"asset:<name>\") or a \"tone\", never a \"url\" or a host's \"source\"");
		if(const auto asset = sound.contains("file") ? assetReference(sound["file"]) : std::nullopt) {
			const auto assets = after.find("assets");
			if(assets == after.end() || !assets->is_object() || !assets->contains(*asset))
				fail(at + ".file", "no asset '" + *asset + "': bring it in the revision's \"assets\"");
			if(!(*assets)[*asset].value("type", std::string()).starts_with("audio/"))
				fail(at + ".file", "'" + *asset + "' isn't a sound");
		}
	}
}

std::vector<std::string> Rules::personLines(const json& before, const json& after) const {
	std::vector<std::string> lines;
	const json& oldSounds = soundsOf(before);
	const json& newSounds = soundsOf(after);
	for(const auto& [name, sound] : newSounds.items())
		if(const auto same = oldSounds.find(name); same == oldSounds.end() || *same != sound)
			lines.push_back((same == oldSounds.end() ? "adds the sound '" : "changes the sound '") + name + "'" +
			                (sound.contains("file") && sound["file"].is_string()
			                     ? " (" + sound["file"].get<std::string>().substr(0, 120) + ")"
								 : std::string()));
	for(const auto& [name, sound] : oldSounds.items())
		if(!newSounds.contains(name)) lines.push_back("removes the sound '" + name + "'");
	const auto shownBefore = shownAssets(before);
	for(const auto& [node, asset] : shownAssets(after))
		if(const auto was = shownBefore.find(node); was == shownBefore.end() || was->second != asset)
			lines.push_back("shows the asset '" + asset + "' in " + node);
	return lines;
}

void Rules::checkRun(const json& document, const json& steps) const {
	if(!steps.is_array() || steps.empty()) fail("revision.run", "expected a list of steps");
	if(steps.dump().size() > maxRunBytes) fail("revision.run", "is more than 1 MiB written as JSON");
	// As an action the document gained: no control step, no way to run one, nothing it doesn't define
	std::string name = runAction;
	while(document.contains("actions") && document["actions"].is_object() && document["actions"].contains(name))
		name += "_";
	json withRun = document;
	if(!withRun.contains("actions") || !withRun["actions"].is_object()) withRun["actions"] = json::object();
	withRun["actions"][name] = steps;
	try {
		checkChange(document, withRun);
	} catch(const DocumentError& error) {
		std::string reason = error.what();
		if(const std::size_t prefix = reason.find("revision: "); prefix != std::string::npos)
			reason = reason.substr(prefix + 10);
		fail("revision.run", reason);
	}
}

json Rules::checkAsset(std::string_view name, std::string_view type, std::string_view bytes) const {
	const std::span<const std::uint8_t> data(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
	if(type.starts_with("image/")) { // as an image node and the image step will read it
		try {
			const std::shared_ptr<const Bitmap> bitmap = Bitmap::decode(data, {});
			return {{"width", bitmap->width()}, {"height", bitmap->height()}};
		} catch(const std::exception& error) {
			fail("asset '" + std::string(name) + "'", std::string("the picture doesn't decode: ") + error.what());
		}
	}
	if(type.starts_with("audio/")) {
		try {
			if(const auto seconds = probeSound(std::string(bytes), type)) return {{"duration", *seconds}};
		} catch(const DocumentError& error) {
			fail("asset '" + std::string(name) + "'", error.what());
		}
		return json::object();
	}
	if(type == "application/json") try {
			static_cast<void>(parseUntrusted(bytes));
		} catch(const std::exception& error) {
			fail("asset '" + std::string(name) + "'", std::string("isn't JSON: ") + error.what());
		}
	return json::object();
}

} // namespace affresco::format
