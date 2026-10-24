/// @file
/// @brief vcs::Repository's assets: files a revision brings, named in the document's "assets" section and kept as
/// blobs by their SHA-256; the steps a revision runs once ("run"); why only the person accepts either; and the
/// attachments the person gives the app (VERSION_CONTROL.md, "Assets").

#include "affresco/vcs/Repository.hpp"

#include "history/HistoryStore.hpp"
#include "permissions/Permissions.hpp"
#include "revisions/Revisions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <format>

namespace affresco::vcs {

using expect::fail;

namespace {

/// The most steps one revision runs once.
constexpr std::size_t maxRunSteps = 1000;

/// @brief A document's assets section.
/// @param view A document or a view of one.
/// @return Its "assets" object, or an empty one.
const json& manifestOf(const json& view) {
	static const json none = json::object();
	const auto it = view.find("assets");
	return it != view.end() && it->is_object() ? *it : none;
}

/// @brief A size for people: "4.2 MB", "820 KB", "96 bytes".
/// @param bytes The size.
/// @return The text.
std::string sizeText(std::size_t bytes) {
	if(bytes >= 1'000'000) return std::format("{:.1f} MB", static_cast<double>(bytes) / 1e6);
	if(bytes >= 1'000) return std::format("{} KB", bytes / 1'000);
	return std::to_string(bytes) + " bytes";
}

/// @brief Makes a file's own name a valid asset name: lowercase, other characters as '-', at most 100 bytes, its
/// extension kept ("My Beat (take 2).WAV" gives "my-beat-take-2-.wav").
/// @param name The file's name.
/// @return The asset name ("file" plus the extension if nothing is left).
std::string assetNameFor(std::string_view name) {
	if(const std::size_t slash = name.find_last_of("/\\"); slash != std::string_view::npos)
		name = name.substr(slash + 1);
	std::string result;
	for(const char c : name) {
		const char lower = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
		const bool plain = (lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9');
		result.push_back(plain || lower == '.' || lower == '_' || lower == '-' ? lower : '-');
	}
	while(!result.empty() && !(std::isalnum(static_cast<unsigned char>(result.front())) != 0)) result.erase(0, 1);
	std::string extension(assetExtension(result));
	if(result.size() > maxAssetName) result = result.substr(0, maxAssetName - extension.size() - 1) + "." + extension;
	if(result.empty() || result.front() == '.') result = "file" + (extension.empty() ? "" : "." + extension);
	return result;
}

} // namespace

/* What a revision brings */

json Repository::changesOf(const json& revision, StagedBlobs& staged) const {
	json patch = json::array();
	if(revision.contains("assets")) {
		const json& assets = revision["assets"];
		if(!assets.is_array()) fail("revision.assets", "expected a list of assets");
		if(assets.size() > maxAssets) fail("revision.assets", "more than " + std::to_string(maxAssets) + " assets");
		std::size_t total = 0;
		std::vector<std::string> names;
		for(std::size_t i = 0; i < assets.size(); ++i) {
			const std::string at = std::format("revision.assets[{}]", i);
			const json& asset = expect::object(assets[i], at);
			const std::string name = expect::string(asset.value("name", json()), at + ".name");
			if(const auto problem = assetNameProblem(name)) fail(at + ".name", *problem);
			if(std::ranges::find(names, name) != names.end()) fail(at + ".name", "'" + name + "' is there twice");
			names.push_back(name);

			const int sources = static_cast<int>(asset.contains("text")) + static_cast<int>(asset.contains("data")) +
			                    static_cast<int>(asset.contains("attachment"));
			if(sources != 1) fail(at, "give exactly one of \"text\", \"data\" (base64) and \"attachment\"");
			std::string type = asset.contains("type") ? expect::string(asset["type"], at + ".type") : "";
			std::shared_ptr<const std::string> bytes;
			if(asset.contains("attachment")) {
				const std::string id = expect::string(asset["attachment"], at + ".attachment");
				const auto it = std::ranges::find(attachments_, id, [](const auto& a) { return a.info.id; });
				if(it == attachments_.end())
					fail(at + ".attachment", "no attachment '" + id.substr(0, 80) +
					                             "' (attachments wait for this run only: ask the person again)");
				if(!type.empty() && type != it->info.type)
					fail(at + ".type", "the attachment is " + it->info.type + ", not " + type.substr(0, 60));
				type = it->info.type;
				bytes = it->bytes;
			} else {
				if(type.empty()) fail(at + ".type", "expected the asset's media type, e.g. \"image/svg+xml\"");
				if(asset.contains("text")) {
					const std::string& text = expect::string(asset["text"], at + ".text");
					if(text.size() > maxAssetBytes)
						fail(at + ".text", "an asset is at most " + std::to_string(maxAssetBytes >> 20) + " MiB");
					bytes = std::make_shared<const std::string>(text);
				} else {
					auto decoded = decodeBase64(expect::string(asset["data"], at + ".data"), maxAssetBytes);
					if(!decoded)
						fail(at + ".data",
						     "expected base64 of at most " + std::to_string(maxAssetBytes >> 20) + " MiB");
					bytes = std::make_shared<const std::string>(std::move(*decoded));
				}
			}
			if(const auto problem = assetTypeProblem(name, type)) fail(at + ".type", *problem);
			total += bytes->size();
			if(total > maxRevisionAssetBytes)
				fail(at, "a revision brings at most " + std::to_string(maxRevisionAssetBytes >> 20) + " MiB of assets");
			const std::string id = detail::sha256(*bytes);
			try {
				static_cast<void>(checkBytes(name, type, id, *bytes));
			} catch(const DocumentError& error) {
				fail(at, detail::reasonOf(error));
			}
			staged.add(id, bytes);
			patch.push_back({{"op", "add"},
			                 {"path", "/assets/" + name},
			                 {"value", {{"type", type}, {"size", bytes->size()}, {"sha256", id}}}});
		}
	}
	const bool runs = revision.contains("run");
	if(revision.contains("patch") || revision.contains("set") || revision.contains("add") ||
	   revision.contains("remove") || revision.contains("move") || (patch.empty() && !runs)) {
		json edits = detail::patchOf(revision);
		for(json& op : edits) patch.push_back(std::move(op));
	}
	return patch;
}

json Repository::runOf(const json& revision) {
	if(!revision.contains("run")) return nullptr;
	const json& run = revision["run"];
	if(!run.is_array() || run.empty()) fail("revision.run", "expected a list of steps");
	if(run.size() > maxRunSteps) fail("revision.run", "more than " + std::to_string(maxRunSteps) + " steps");
	for(std::size_t i = 0; i < run.size(); ++i)
		static_cast<void>(expect::object(run[i], std::format("revision.run[{}]", i)));
	return run;
}

json Repository::checkBytes(const std::string& name, const std::string& type, const std::string& id,
                            const std::string& bytes) const {
	const std::string key = id + " " + type;
	if(const auto known = checkedBlobs_.find(key); known != checkedBlobs_.end()) return known->second;
	if(bytes.size() > maxAssetBytes)
		fail("asset '" + name + "'", "an asset is at most " + std::to_string(maxAssetBytes >> 20) + " MiB");
	if(const auto problem = assetBytesProblem(type, bytes)) fail("asset '" + name + "'", *problem);
	json details = rules_.checkAsset(name, type, bytes);
	if(!details.is_object()) details = json::object();
	if(checkedBlobs_.size() >= 4 * maxAssets) checkedBlobs_.clear(); // a cache: bounded, and checked again if needed
	return checkedBlobs_.emplace(key, std::move(details)).first->second;
}

/* The manifest */

void Repository::checkManifest(const json& before, const json& after) const {
	const auto it = after.find("assets");
	if(it == after.end()) return;
	if(!it->is_object()) fail("assets", "expected an object of assets by name");
	if(it->size() > maxAssets && it->size() > manifestOf(before).size())
		fail("assets", "more than " + std::to_string(maxAssets) + " assets");
	const json& old = manifestOf(before);
	for(const auto& [name, entry] : it->items()) {
		if(const auto same = old.find(name); same != old.end() && *same == entry) continue;
		const std::string at = "assets." + name;
		if(const auto problem = assetNameProblem(name)) fail(at, *problem);
		if(!entry.is_object() || entry.size() != 3 || !entry.contains("type") || !entry.contains("size") ||
		   !entry.contains("sha256"))
			fail(at, R"(expected {"type", "size", "sha256"}: a revision's "assets" write it)");
		const std::string type = expect::string(entry["type"], at + ".type");
		if(const auto problem = assetTypeProblem(name, type)) fail(at + ".type", *problem);
		const std::string id = expect::string(entry["sha256"], at + ".sha256");
		const std::shared_ptr<const std::string> bytes = detail::isObjectId(id) ? blob(id) : nullptr;
		if(!bytes) fail(at + ".sha256", "the history has no such bytes: bring the asset in the revision's \"assets\"");
		if(!entry["size"].is_number_unsigned() || entry["size"].get<std::size_t>() != bytes->size())
			fail(at + ".size", "expected " + std::to_string(bytes->size()) + ", the size of its bytes");
		static_cast<void>(checkBytes(name, type, id, *bytes));
	}
}

std::vector<std::string> Repository::personNeeds(const json& before, const json& after, const json& run) const {
	std::vector<std::string> lines;
	const json& old = manifestOf(before);
	const json& now = manifestOf(after);
	for(const auto& [name, entry] : now.items())
		if(const auto same = old.find(name); same == old.end() || *same != entry)
			lines.push_back((same == old.end() ? "brings the asset '" : "changes the asset '") + name + "' (" +
			                entry.value("type", std::string("?")) + ", " +
			                sizeText(entry.value("size", std::size_t{0})) + ")");
	for(const auto& [name, entry] : old.items())
		if(!now.contains(name)) lines.push_back("removes the asset '" + name + "'");
	if(run.is_array() && !run.empty())
		lines.push_back("runs " + std::to_string(run.size()) + (run.size() == 1 ? " step" : " steps") +
		                " once when it is accepted");
	for(std::string& line : rules_.personLines(before, after)) lines.push_back(std::move(line));
	return lines;
}

json Repository::assetChanges(const json& before, const json& after) {
	json changes = json::array();
	const json& old = manifestOf(before);
	const json& now = manifestOf(after);
	for(const auto& [name, entry] : now.items())
		if(const auto same = old.find(name); (same == old.end() || *same != entry) && entry.is_object())
			changes.push_back({{"name", name},
			                   {"type", entry.value("type", std::string())},
			                   {"size", entry.value("size", std::size_t{0})}});
	for(const auto& [name, entry] : old.items())
		if(!now.contains(name) && entry.is_object())
			changes.push_back({{"name", name},
			                   {"type", entry.value("type", std::string())},
			                   {"size", entry.value("size", std::size_t{0})},
			                   {"removed", true}});
	return changes;
}

/* Bytes */

std::shared_ptr<const std::string> Repository::blob(const std::string& id) const {
	if(const auto known = blobs_.find(id); known != blobs_.end()) return known->second;
	detail::HistoryStore* store = history_ ? history_.get() : loading_;
	if(!store || !store->blobSize(id)) return nullptr;
	try {
		auto bytes = std::make_shared<const std::string>(store->readBlob(id));
		if(detail::sha256(*bytes) != id) {
			affresco::log(LogLevel::Error, "AffrescoUI history: " + store->blobFile(id).string() +
			                                   ": its content doesn't match its SHA-256: the file is damaged");
			return nullptr;
		}
		return blobs_.emplace(id, std::move(bytes)).first->second;
	} catch(const DocumentError& error) {
		affresco::log(LogLevel::Error, error.what());
		return nullptr;
	}
}

std::vector<AssetInfo> Repository::assets() const {
	std::vector<AssetInfo> result;
	for(const auto& [name, entry] : manifestOf(document_).items()) {
		if(!entry.is_object()) continue;
		result.push_back({name, entry.value("type", std::string()), entry.value("size", std::size_t{0}),
		                  entry.value("sha256", std::string())});
	}
	std::ranges::sort(result, {}, &AssetInfo::name);
	return result;
}

std::shared_ptr<const std::string> Repository::assetBytes(std::string_view name) const {
	const json& manifest = manifestOf(document_);
	const auto it = manifest.find(std::string(name));
	if(it == manifest.end() || !it->is_object() || !it->contains("sha256") || !(*it)["sha256"].is_string())
		return nullptr;
	return blob((*it)["sha256"].get<std::string>());
}

void Repository::writeBlobs() {
	for(const auto& [id, bytes] : blobs_)
		if(!history_->hasBlob(id)) {
			if(const auto size = history_->blobSize(id); size && *size == bytes->size()) history_->knowBlob(id);
			else history_->writeBlob(id, *bytes);
		}
}

void Repository::expectBlobs(detail::HistoryStore& store, const json& document, const std::filesystem::path& where) {
	for(const auto& [name, entry] : manifestOf(document).items()) {
		const std::string id = entry.is_object() ? entry.value("sha256", std::string()) : std::string();
		const auto size = detail::isObjectId(id) ? store.blobSize(id) : std::nullopt;
		if(!size || !entry.contains("size") || entry["size"] != *size)
			throw detail::HistoryError(where, "the asset '" + name + "' has no bytes of its size in " +
			                                      (store.directory() / "blobs").string() + ": the history is damaged");
		store.knowBlob(id);
	}
}

/* Attachments */

Attached Repository::attach(std::string_view name, std::string_view type, std::string bytes, json details) {
	const std::string asset = assetNameFor(name);
	if(const auto problem = assetTypeProblem(asset, type)) return {{}, *problem};
	if(bytes.size() > maxAssetBytes)
		return {{}, "a file is attached up to " + std::to_string(maxAssetBytes >> 20) + " MiB"};
	const std::string id = detail::sha256(bytes);
	json checked;
	try {
		checked = checkBytes(asset, std::string(type), id, bytes);
	} catch(const DocumentError& error) {
		return {{}, detail::reasonOf(error)};
	}
	if(!details.is_object()) details = json::object();
	for(const auto& [key, value] : checked.items()) details[key] = value;

	PendingAttachment pending;
	pending.info = {"att-" + std::to_string(nextAttachment_++) + "-" + id.substr(0, 12), asset, std::string(type),
	                bytes.size(), std::move(details)};
	pending.sha256 = id;
	pending.bytes = std::make_shared<const std::string>(std::move(bytes));
	// The oldest go first: the person's latest files are the ones a request is about
	std::size_t total = pending.bytes->size();
	for(const PendingAttachment& a : attachments_) total += a.bytes->size();
	while(!attachments_.empty() && (attachments_.size() >= maxAttachments || total > maxAttachmentBytes)) {
		total -= attachments_.front().bytes->size();
		attachments_.erase(attachments_.begin());
	}
	attachments_.push_back(std::move(pending));
	return {attachments_.back().info, {}};
}

std::optional<AttachmentInfo> Repository::attachment(std::string_view id) const {
	const auto it = std::ranges::find(attachments_, id, [](const PendingAttachment& a) { return a.info.id; });
	if(it == attachments_.end()) return std::nullopt;
	return it->info;
}

} // namespace affresco::vcs
