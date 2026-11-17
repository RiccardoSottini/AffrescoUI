/// @file
/// @brief Packages' format (README.md here): a package read and checked whole, written back, the base it was made
/// from, the tree of a source document, and the first summary the person reviews.

#include "affresco/format/Package.hpp"
#include "affresco/format/PackageFiles.hpp"
#include "affresco/format/PackageInstall.hpp"
#include "PackageParts.hpp"

#include "FeatureSignatures.hpp" // canonicalJson(): the tree is the hash of a document's canonical form
#include "Limits.hpp"            // maxDocumentBytes: a package's document and its base are documents

#include "affresco/core/Assets.hpp"
#include "affresco/core/Sha256.hpp"
#include "affresco/format/App.hpp"
#include "affresco/notation/Notation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace affresco::format {

namespace {

/// The keys a package may have.
constexpr auto packageKeys =
    std::to_array<std::string_view>({"$schema", "format", "version", "kind", "name", "release", "description", "author",
	                                 "license", "engine", "base", "document", "state", "assets", "proof", "signature"});
/// The keys a package's base may have.
constexpr auto baseKeys =
    std::to_array<std::string_view>({"app", "name", "release", "notation", "tree", "delta", "files"});
/// The most operations in a base's delta, as in a revision.
constexpr std::size_t maxDeltaOperations = 100000;
/// The longest path of an operation in a base's delta.
constexpr std::size_t maxDeltaPath = 4096;
/// The most state keys a package adds, and the longest one.
constexpr std::size_t maxStateKeys = 10000, maxStateKey = 256;
/// The longest licence and `$schema`.
constexpr std::size_t maxLicenseLength = 64, maxSchemaLength = 1024;
/// The most scenarios in a package's proof.
constexpr std::size_t maxProofScenarios = 1000;

/// @brief Refuses a package.
/// @param place Where in it ("base.files.seed/tasks.csv").
/// @param problem What is wrong.
/// @throws DocumentError Always.
[[noreturn]] void refuse(const std::string& place, const std::string& problem) {
	throw DocumentError("AffrescoUI package: " + place + ": " + problem);
}

/// @brief The number of characters (code points) of UTF-8 text.
/// @param text The text.
/// @return Them.
std::size_t characters(std::string_view text) {
	return static_cast<std::size_t>(
	    std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; }));
}

/// @brief Refuses a key an object may not have, by name.
/// @param object The object.
/// @param keys The keys it may have.
/// @param place Its place, "" for the package.
/// @throws DocumentError If it has another.
void onlyKeys(const json& object, std::span<const std::string_view> keys, const std::string& place) {
	for(const auto& [key, unused] : object.items())
		if(std::ranges::find(keys, key) == keys.end())
			refuse(place.empty() ? key.substr(0, maxStateKey) : place + "." + key.substr(0, maxStateKey),
			       "not a key of a package (" + std::string(place.empty() ? "a package" : place) + " has no such key)");
}

/// @brief A text member of an object.
/// @param object The object.
/// @param key The member.
/// @param place The member's place.
/// @param most Its most characters.
/// @param required Whether it must be there and not be empty.
/// @return Its text, or "" if it is optional and not there.
/// @throws DocumentError If it isn't text, is too long, or is missing.
std::string textOf(const json& object, std::string_view key, const std::string& place, std::size_t most,
                   bool required) {
	const auto found = object.find(key);
	if(found == object.end()) {
		if(required) refuse(place, "missing");
		return {};
	}
	if(!found->is_string()) refuse(place, "expected text");
	const auto& text = found->get_ref<const std::string&>();
	if(required && text.empty()) refuse(place, "empty");
	if(characters(text) > most) refuse(place, "longer than " + std::to_string(most) + " characters");
	return text;
}

/// @brief Whether text is a release: three numbers of 1 to 9 digits, "1.2.0".
/// @param text The text.
/// @return Whether it is.
bool isRelease(std::string_view text) {
	int parts = 0;
	std::size_t digits = 0;
	for(const char c : text) {
		if(c >= '0' && c <= '9') {
			if(++digits > 9) return false;
		} else if(c == '.' && digits > 0 && parts < 2) {
			++parts;
			digits = 0;
		} else {
			return false;
		}
	}
	return parts == 2 && digits > 0;
}

/// @brief A release member: three numbers.
/// @param object The object.
/// @param key The member.
/// @param place Its place.
/// @param required Whether it must be there.
/// @return Its text, or "".
/// @throws DocumentError If it isn't one.
std::string releaseOf(const json& object, std::string_view key, const std::string& place, bool required) {
	std::string text = textOf(object, key, place, 29, required);
	if(object.contains(key) && !isRelease(text)) refuse(place, "expected three numbers, as \"1.2.0\"");
	return text;
}

/// @brief Whether text is an app's id: lowercase letters, digits and '-', 1 to 64, not starting or ending with '-'.
/// @param id The text.
/// @return Whether it is.
bool isAppId(std::string_view id) {
	if(id.empty() || id.size() > 64 || id.front() == '-' || id.back() == '-') return false;
	return std::ranges::all_of(id, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

/// @brief Whether text is a tree: "sha256:" and 64 lowercase hex digits.
/// @param tree The text.
/// @return Whether it is.
bool isTree(std::string_view tree) {
	constexpr std::string_view prefix = "sha256:";
	if(tree.size() != prefix.size() + 64 || !tree.starts_with(prefix)) return false;
	return std::ranges::all_of(tree.substr(prefix.size()),
	                           [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

/// @brief Checks a base's delta: a list of add, remove and replace operations, each with a path and, but for a
/// remove, a value.
/// @param delta The delta.
/// @throws DocumentError If it isn't one.
void checkDelta(const json& delta) {
	if(!delta.is_array()) refuse("base.delta", "expected a list of operations");
	if(delta.size() > maxDeltaOperations)
		refuse("base.delta", "more than " + std::to_string(maxDeltaOperations) + " operations");
	static constexpr auto keys = std::to_array<std::string_view>({"op", "path", "value"});
	for(std::size_t i = 0; i < delta.size(); ++i) {
		const std::string place = "base.delta[" + std::to_string(i) + "]";
		const json& operation = delta[i];
		if(!operation.is_object()) refuse(place, "expected an operation");
		onlyKeys(operation, keys, place);
		const std::string op = textOf(operation, "op", place + ".op", 7, true);
		if(op != "add" && op != "remove" && op != "replace")
			refuse(place + ".op", "expected \"add\", \"remove\" or \"replace\" (a package's delta moves, copies and "
			                      "tests nothing)");
		const auto path = operation.find("path");
		if(path == operation.end() || !path->is_string()) refuse(place + ".path", "expected a JSON Pointer");
		const auto& pointer = path->get_ref<const std::string&>();
		if(pointer.size() > maxDeltaPath || (!pointer.empty() && pointer.front() != '/'))
			refuse(place + ".path", "expected a JSON Pointer of at most 4096 bytes");
		if((op == "remove") == operation.contains("value"))
			refuse(place, op == "remove" ? "a remove has no value" : "missing its value");
	}
}

/// @brief Checks that a value is a document the format accepts: an object, nested and sized within its limits.
/// @param document The value.
/// @param place Its place.
/// @throws DocumentError If it isn't.
void checkDocument(const json& document, const std::string& place) {
	if(!document.is_object()) refuse(place, "expected a source document (an object)");
	if(nestingDepth(document) > maxDocumentNesting)
		refuse(place, "nested more than " + std::to_string(maxDocumentNesting) + " levels");
	if(approximateBytes(document, maxDocumentBytes) > maxDocumentBytes)
		refuse(place, "larger than " + std::to_string(maxDocumentBytes >> 20U) + " MiB");
}

/// @brief A file's or an asset's bytes: its "text", or its base64 "data", never both.
/// @param entry The file or asset.
/// @param place Its place.
/// @param most Its most bytes.
/// @return The bytes.
/// @throws DocumentError If it has neither, both, or too many bytes.
std::string bytesOf(const json& entry, const std::string& place, std::size_t most) {
	const bool text = entry.contains("text"), data = entry.contains("data");
	if(text == data) refuse(place, R"(expected its "text" or its base64 "data", one of them)");
	const json& value = entry[text ? "text" : "data"];
	if(!value.is_string()) refuse(place + (text ? ".text" : ".data"), "expected text");
	const auto& content = value.get_ref<const std::string&>();
	if(text) {
		if(content.size() > most) refuse(place, "larger than " + std::to_string(most >> 20U) + " MiB");
		return content;
	}
	std::optional<std::string> bytes = decodeBase64(content, most);
	if(!bytes)
		refuse(place + ".data", "not base64, or larger than " + std::to_string(most >> 20U) + " MiB once decoded");
	return std::move(*bytes);
}

/// @brief Checks a package's app files: within the limits, each inside the app's folder, with a manifest of the base's
/// app whose entry is among them.
/// @param files The files.
/// @param app The base's app id.
/// @throws DocumentError If they aren't.
void checkFiles(const json& files, const std::string& app) {
	if(!files.is_object()) refuse("base.files", "expected the app's files, by path");
	if(files.size() > maxPackageFiles) refuse("base.files", "more than " + std::to_string(maxPackageFiles) + " files");
	for(const auto& [path, file] : files.items()) {
		const std::string place = "base.files." + path.substr(0, 1024);
		if(!insideApp(path) || path.ends_with('/') || path.contains("//") || path == "." || path.starts_with("./") ||
		   path.contains("/./") || path.ends_with("/."))
			refuse(place, "not a file inside the app's folder");
		if(!file.is_object() || file.size() != 1) refuse(place, R"(expected {"text"} or {"data"})");
		static_cast<void>(bytesOf(file, place, maxPackageFileBytes));
	}
	const std::string_view manifestName = files.contains("app.json") ? "app.json" : "app.lisp";
	if(!files.contains(manifestName)) refuse("base.files", "no app.json or app.lisp: the app's manifest is missing");
	const std::string place = "base.files." + std::string(manifestName);
	const std::string text = bytesOf(files[manifestName], place, maxAppManifestBytes);
	AppManifest manifest;
	try {
		manifest = parseApp(notation::readDocument(text, std::string(manifestName)));
	} catch(const json::parse_error& error) {
		refuse(place, std::string("not JSON: ") + error.what());
	} catch(const DocumentError& error) {
		refuse(place, error.what());
	}
	if(manifest.id != app) refuse(place, "the app's id is '" + manifest.id + "', not the base's '" + app + "'");
	if(!files.contains(manifest.entry)) refuse(place, "its entry '" + manifest.entry + "' isn't among the files");
}

/// @brief Checks that the document's "assets" lists an asset with its type, size and SHA-256.
/// @param listed The document's "assets".
/// @param name The asset's name.
/// @param type Its type.
/// @param bytes Its bytes.
/// @param place Its place, for errors.
/// @throws DocumentError If it doesn't.
void checkListed(const json& listed, const std::string& name, const std::string& type, const std::string& bytes,
                 const std::string& place) {
	const auto entry = listed.is_object() ? listed.find(name) : listed.end();
	if(entry == listed.end() || !entry->is_object()) refuse(place, R"(the document's "assets" doesn't list it)");
	if(entry->value("sha256", "") != sha256::hex(bytes) || entry->value("size", json()) != bytes.size() ||
	   entry->value("type", "") != type)
		refuse(place, R"(not the bytes the document's "assets" lists (type, size and SHA-256))");
}

/// @brief Checks a package's assets: names, types and bytes as an asset's, each listed in the document's "assets" with
/// its size and SHA-256.
/// @param assets The assets.
/// @param document The package's document.
/// @throws DocumentError If they aren't.
void checkAssets(const json& assets, const json& document) {
	if(!assets.is_object()) refuse("assets", "expected the assets, by name");
	if(assets.size() > maxAssets) refuse("assets", "more than " + std::to_string(maxAssets) + " assets");
	const json listed = document.value("assets", json::object());
	for(const auto& [name, asset] : assets.items()) {
		const std::string place = "assets." + name.substr(0, maxAssetName + 1);
		if(const auto problem = assetNameProblem(name)) refuse(place, *problem);
		if(!asset.is_object() || asset.size() != 2 || !asset.contains("type") || !asset["type"].is_string())
			refuse(place, R"(expected {"type", "text"} or {"type", "data"})");
		const auto& type = asset["type"].get_ref<const std::string&>();
		if(const auto problem = assetTypeProblem(name, type)) refuse(place + ".type", *problem);
		const std::string bytes = bytesOf(asset, place, maxAssetBytes);
		if(const auto problem = assetBytesProblem(type, bytes)) refuse(place, *problem);
		checkListed(listed, name, type, bytes, place);
	}
}

/// @brief Checks the state keys a package adds: an object of at most maxStateKeys keys, each absent from the base's
/// own state.
/// @param state The state.
/// @param base The base's document.
/// @throws DocumentError If they aren't.
void checkState(const json& state, const json& base) {
	if(!state.is_object()) refuse("state", "expected the state keys the version adds, by key");
	if(state.size() > maxStateKeys) refuse("state", "more than " + std::to_string(maxStateKeys) + " keys");
	const json own = base.value("state", json::object());
	for(const auto& [key, unused] : state.items()) {
		if(key.size() > maxStateKey) refuse("state", "a key longer than 256 bytes");
		if(own.is_object() && own.contains(key))
			refuse("state." + key, "the base has this key: a package adds only state keys its base doesn't have");
	}
}

/// @brief Reads a package's base, but its files.
/// @param value The package.
/// @return The base.
/// @throws DocumentError If it isn't one.
PackageBase baseFrom(const json& value) {
	const auto found = value.find("base");
	if(found == value.end() || !found->is_object()) refuse("base", "expected the release the version was made from");
	const json& base = *found;
	onlyKeys(base, baseKeys, "base");
	PackageBase result;
	result.app = textOf(base, "app", "base.app", 64, true);
	if(!isAppId(result.app)) refuse("base.app", "expected an app's id: lowercase letters, digits and '-'");
	result.name = textOf(base, "name", "base.name", maxPackageNameLength, true);
	result.release = releaseOf(base, "release", "base.release", false);
	result.notation = textOf(base, "notation", "base.notation", 4, true);
	if(result.notation != "json" && result.notation != "lisp") refuse("base.notation", R"(expected "json" or "lisp")");
	result.tree = textOf(base, "tree", "base.tree", 71, true);
	if(!isTree(result.tree)) refuse("base.tree", "expected \"sha256:\" and 64 lowercase hex digits");
	if(!base.contains("delta")) refuse("base.delta", "missing");
	checkDelta(base["delta"]);
	result.delta = base["delta"];
	if(base.contains("files")) result.files = base["files"];
	return result;
}

/// @brief Reads a package's header: everything but its base, document and optional sections.
/// @param value The package.
/// @param package Receives it.
/// @throws DocumentError If it isn't one.
void readHeader(const json& value, Package& package) {
	if(value.value("format", json()) != packageFormat) refuse("format", "expected \"affrescoui-package\"");
	if(value.value("version", json()) != 1) refuse("version", "expected 1");
	const std::string kind = textOf(value, "kind", "kind", 7, true);
	if(kind != "changes" && kind != "app") refuse("kind", R"(expected "changes" or "app")");
	package.kind = kind == "app" ? PackageKind::App : PackageKind::Changes;
	package.name = textOf(value, "name", "name", maxPackageNameLength, true);
	package.release = releaseOf(value, "release", "release", true);
	package.description = textOf(value, "description", "description", maxPackageDescriptionLength, false);
	package.author = textOf(value, "author", "author", maxPackageNameLength, false);
	package.license = textOf(value, "license", "license", maxLicenseLength, false);
	package.engine = releaseOf(value, "engine", "engine", true);
	static_cast<void>(textOf(value, "$schema", "$schema", maxSchemaLength, false));
}

/// @brief Reads a package's optional sections: its proof and its signature, which is verified.
/// @param value The package.
/// @param package Receives them.
/// @throws DocumentError If one isn't valid, or the signature isn't its publisher's.
void readExtras(const json& value, Package& package) {
	if(const auto proof = value.find("proof"); proof != value.end()) {
		static constexpr auto keys = std::to_array<std::string_view>({"scenarios"});
		if(!proof->is_object() || !proof->contains("scenarios") || !(*proof)["scenarios"].is_array())
			refuse("proof", "expected {\"scenarios\": [...]}");
		onlyKeys(*proof, keys, "proof");
		if((*proof)["scenarios"].size() > maxProofScenarios) refuse("proof.scenarios", "more than 1000 scenarios");
		package.proof = *proof;
	}
	if(const auto signature = value.find("signature"); signature != value.end()) {
		static constexpr auto keys = std::to_array<std::string_view>({"publisher", "value"});
		if(!signature->is_object()) refuse("signature", R"(expected {"publisher", "value"})");
		onlyKeys(*signature, keys, "signature");
		try {
			static_cast<void>(verifySignature(value, "signature"));
		} catch(const DocumentError& error) {
			refuse("signature", std::string("doesn't verify: ") + error.what());
		}
		package.signature = *signature;
	}
}

/// @brief How a package's signature stands: "none", "trusted:<name>" or "unknown:<the key's first 16 characters>".
/// @param signature The package's signature (verified when read), or null.
/// @param trustedPublishers `{"ed25519:<key>": "<name>"}`, or anything else for none.
/// @return It.
std::string signatureOf(const json& signature, const json& trustedPublishers) {
	if(!signature.is_object() || !signature.contains("publisher") || !signature["publisher"].is_string()) return "none";
	const auto& publisher = signature["publisher"].get_ref<const std::string&>();
	if(trustedPublishers.is_object()) {
		const auto found = trustedPublishers.find(publisher);
		if(found != trustedPublishers.end() && found->is_string()) return "trusted:" + found->get<std::string>();
	}
	constexpr std::string_view prefix = "ed25519:";
	return "unknown:" + publisher.substr(publisher.starts_with(prefix) ? prefix.size() : 0, 16);
}

} // namespace

namespace detail {

json definitionsOf(const json& document) {
	json result = json::object();
	const auto program = document.find("program");
	if(program == document.end() || !program->is_object()) return result;
	for(const auto& [file, definitions] : program->items()) {
		if(!definitions.is_object()) continue;
		for(const auto& [name, text] : definitions.items()) {
			std::string key = file;
			key.append("/").append(name);
			result[key] = text;
		}
	}
	return result;
}

} // namespace detail

std::string documentTree(const json& sourceDocument) { return "sha256:" + sha256::hex(canonicalJson(sourceDocument)); }

Package parsePackage(const json& value) {
	if(!value.is_object()) refuse("package", "expected an object");
	onlyKeys(value, packageKeys, "");
	Package package;
	readHeader(value, package);
	package.base = baseFrom(value);
	if(!value.contains("document")) refuse("document", "missing");
	package.document = value["document"];
	checkDocument(package.document, "document");
	if(package.kind == PackageKind::App) {
		if(package.base.files.is_null()) refuse("base.files", "a package of kind \"app\" holds the app's files");
		checkFiles(package.base.files, package.base.app);
	} else if(!package.base.files.is_null()) {
		refuse("base.files", "only a package of kind \"app\" holds files");
	}
	const json base = baseOf(package);
	if(value.contains("state")) {
		checkState(value["state"], base);
		package.state = value["state"];
	}
	if(value.contains("assets")) {
		checkAssets(value["assets"], package.document);
		package.assets = value["assets"];
	}
	readExtras(value, package);
	return package;
}

Package readPackage(std::string_view text) {
	if(text.size() > maxPackageBytes) refuse("file", "larger than " + std::to_string(maxPackageBytes >> 20U) + " MiB");
	return parsePackage(parseUntrusted(text));
}

json packageJson(const Package& package) {
	json base = {{"app", package.base.app}, {"name", package.base.name}};
	if(!package.base.release.empty()) base["release"] = package.base.release;
	base["notation"] = package.base.notation;
	base["tree"] = package.base.tree;
	base["delta"] = package.base.delta.is_null() ? json::array() : package.base.delta;
	if(!package.base.files.is_null()) base["files"] = package.base.files;

	json value = {{"format", packageFormat},
	              {"version", 1},
	              {"kind", package.kind == PackageKind::App ? "app" : "changes"},
	              {"name", package.name},
	              {"release", package.release}};
	if(!package.description.empty()) value["description"] = package.description;
	if(!package.author.empty()) value["author"] = package.author;
	if(!package.license.empty()) value["license"] = package.license;
	value["engine"] = package.engine;
	value["base"] = std::move(base);
	value["document"] = package.document;
	for(const auto& [key, section] : {std::pair<const char*, const json*>{"state", &package.state},
	                                  {"assets", &package.assets},
	                                  {"proof", &package.proof},
	                                  {"signature", &package.signature}})
		if(!section->is_null()) value[key] = *section;
	return value;
}

json baseOf(const Package& package) {
	checkDelta(package.base.delta);
	json base;
	try {
		base = package.document.patch(package.base.delta);
	} catch(const json::exception& error) {
		refuse("base.delta", std::string("doesn't apply to the document: ") + error.what());
	}
	checkDocument(base, "base");
	if(documentTree(base) != package.base.tree)
		refuse("base.tree", "the file doesn't hold the base it names (its delta gives another document)");
	return base;
}

json packageSummary(const Package& package, const json& installedBase, const json& trustedPublishers) {
	PackageReceiver receiver;
	receiver.copy = installedBase;
	return packageSummary(package, receiver, trustedPublishers);
}

json packageSummary(const Package& package, const PackageReceiver& receiver, const json& trustedPublishers) {
	const json base = baseOf(package);
	const json& installedBase = receiver.copy;
	json assets = json::array();
	if(package.assets.is_object())
		for(const auto& [name, asset] : package.assets.items()) {
			const json listed = package.document.value("assets", json::object()).value(name, json::object());
			assets.push_back({{"name", name}, {"type", asset.value("type", "")}, {"size", listed.value("size", 0)}});
		}
	const json before = detail::definitionsOf(base), after = detail::definitionsOf(package.document);
	json added = json::array(), changed = json::array(), removed = json::array();
	for(const auto& [name, text] : after.items()) {
		if(!before.contains(name)) added.push_back(name);
		else if(before[name] != text) changed.push_back(name);
	}
	for(const auto& [name, unused] : before.items())
		if(!after.contains(name)) removed.push_back(name);
	json summaryBase = {{"app", package.base.app}, {"name", package.base.name}, {"release", package.base.release}};
	summaryBase["exact"] = installedBase.is_null() ? json() : json(documentTree(installedBase) == package.base.tree);
	json summary = {{"kind", package.kind == PackageKind::App ? "app" : "changes"},
	                {"name", package.name},
	                {"release", package.release},
	                {"author", package.author},
	                {"engine", package.engine},
	                {"base", std::move(summaryBase)},
	                {"bytes", packageJson(package).dump().size()},
	                {"files", package.base.files.is_object() ? package.base.files.size() : std::size_t{0}},
	                {"assets", std::move(assets)},
	                {"state", package.state.is_object() ? package.state : json::object()},
	                {"definitions",
	                 {{"added", std::move(added)}, {"changed", std::move(changed)}, {"removed", std::move(removed)}}},
	                {"signature", signatureOf(package.signature, trustedPublishers)}};
	detail::addInstallParts(summary, package, base, receiver);
	return summary;
}

} // namespace affresco::format
