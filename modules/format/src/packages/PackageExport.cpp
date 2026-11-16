/// @file
/// @brief Writing a package from an app and its history (README.md here): the package built from a version's parts,
/// the app's files its manifest reaches, and the scan for what must never be shared.

#include "affresco/format/PackageFiles.hpp"

#include "affresco/core/Assets.hpp"
#include "affresco/format/App.hpp"
#include "affresco/lisp/Document.hpp"
#include "affresco/notation/Notation.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

namespace {

/// The most files read while following includes, as the loader reads at most.
constexpr std::size_t maxIncludeReads = 1024;
/// The deepest includes inside includes, as the loader follows.
constexpr int maxIncludeDepth = 8;

/// @brief Refuses an export.
/// @param place The file or place.
/// @param problem What is wrong.
/// @throws DocumentError Always.
[[noreturn]] void refuse(const std::string& place, const std::string& problem) {
	throw DocumentError("AffrescoUI package: " + place + ": " + problem);
}

/// @brief Whether a path is inside a folder, both canonical.
/// @param path The path.
/// @param folder The folder.
/// @return Whether it is.
bool within(const std::filesystem::path& path, const std::filesystem::path& folder) {
	const auto [end, unused] = std::ranges::mismatch(folder, path);
	return end == folder.end();
}

/// @brief Whether bytes are text a JSON string holds as it is: UTF-8, with no NUL.
/// @param bytes The bytes.
/// @return Whether they are.
bool isText(const std::string& bytes) {
	if(bytes.contains('\0')) return false;
	try {
		static_cast<void>(json(bytes).dump());
		return true;
	} catch(const json::type_error&) {
		return false;
	}
}

/// @brief Collects the files of an app's folder: each reached once, read and checked.
class FileCollector {
public:
	/// @brief Starts on an app's folder.
	/// @param folder The folder, canonical.
	/// @param shared The shared folders beside it, canonical (their files are left out).
	FileCollector(std::filesystem::path folder, std::vector<std::filesystem::path> shared)
	    : folder_(std::move(folder)), shared_(std::move(shared)) {}

	/// @brief Adds a file, or a folder's files (recursively).
	/// @param path The path, absolute or relative to the app's folder.
	/// @param named How the app names it, for errors.
	/// @return The file's canonical path, or nothing for a shared one or a folder.
	/// @throws DocumentError If it is outside the folder (and not shared), can't be read, or is too large.
	std::optional<std::filesystem::path> add(const std::filesystem::path& path, const std::string& named) {
		std::error_code error;
		std::filesystem::path real = std::filesystem::canonical(folder_ / path, error);
		if(error) refuse(named, "can't be read (it isn't there?)");
		if(std::ranges::any_of(shared_, [&real](const auto& shared) { return within(real, shared); }))
			return std::nullopt;
		if(!within(real, folder_))
			refuse(named, "outside the app's folder (a package holds only the app's own files and its shared ones "
			              "stay behind)");
		if(std::filesystem::is_directory(real, error)) {
			for(auto it = std::filesystem::recursive_directory_iterator(real, error);
			    !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
				if(it->is_regular_file(error) || it->is_symlink(error))
					static_cast<void>(add(it->path(), relative(it->path())));
			if(error) refuse(named, "its folder can't be read");
			return std::nullopt;
		}
		const std::string name = relative(real);
		if(files_.contains(name)) return real;
		if(files_.size() >= maxPackageFiles)
			refuse(named, "more than " + std::to_string(maxPackageFiles) + " files in the app");
		files_[name] = entryOf(real, name);
		return real;
	}

	/// @brief Adds a screen part and, transitively, the files it includes, as the loader resolves them: each from the
	/// folder of the file that includes it.
	/// @param path The part, relative to the app's folder.
	/// @param named How the app names it, relative to its folder ("parts/../look.json"), for errors.
	/// @param depth Includes inside includes so far.
	/// @throws DocumentError As add() does, or if a part isn't JSON or Lisp.
	void addPart(const std::filesystem::path& path, const std::string& named, int depth) {
		const std::optional<std::filesystem::path> real = add(path, named);
		if(!real || std::ranges::find(parts_, *real) != parts_.end()) return;
		parts_.push_back(*real);
		if(depth > maxIncludeDepth || parts_.size() > maxIncludeReads) refuse(named, "includes nested too deeply");
		const json& entry = files_[relative(*real)];
		if(!entry.contains("text")) refuse(named, "not a screen's part (it isn't text)");
		json part;
		try {
			part = lisp::readDocument(entry["text"].get_ref<const std::string&>(), *real);
		} catch(const json::parse_error& problem) {
			refuse(named, std::string("not JSON: ") + problem.what());
		} catch(const DocumentError& problem) {
			refuse(named, problem.what());
		}
		if(!part.is_object() || !part.contains("include")) return;
		const json includes = part["include"].is_string() ? json::array({part["include"]}) : part["include"];
		if(!includes.is_array()) refuse(named + ".include", "expected a list of files");
		for(const json& include : includes) {
			if(!include.is_string()) refuse(named + ".include", "expected file names");
			const std::filesystem::path next = real->parent_path() / include.get<std::string>();
			addPart(next.lexically_normal(),
			        (std::filesystem::path(named).parent_path() / include.get<std::string>()).generic_string(),
			        depth + 1);
		}
	}

	/// @brief The files collected.
	/// @return `{path: {"text"} | {"data"}}`.
	[[nodiscard]] json files() const { return files_; }

private:
	/// @brief A path relative to the app's folder, '/'-separated.
	/// @param path The path, absolute.
	/// @return It.
	[[nodiscard]] std::string relative(const std::filesystem::path& path) const {
		return path.lexically_relative(folder_).generic_string();
	}

	/// @brief Reads a file into its entry.
	/// @param file The file, canonical.
	/// @param name Its name in the package.
	/// @return `{"text"}` or `{"data"}`.
	/// @throws DocumentError If it can't be read or is too large.
	static json entryOf(const std::filesystem::path& file, const std::string& name) {
		std::error_code error;
		const auto size = std::filesystem::file_size(file, error);
		if(error) refuse(name, "can't be read");
		if(size > maxPackageFileBytes)
			refuse(name, "larger than " + std::to_string(maxPackageFileBytes >> 20U) +
			                 " MiB, the most a package's file "
			                 "holds");
		std::ifstream in(file, std::ios::binary);
		std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if(!in && !in.eof()) refuse(name, "can't be read");
		if(isText(bytes)) return {{"text", std::move(bytes)}};
		return {{"data", encodeBase64(bytes)}};
	}

	std::filesystem::path folder_;              ///< The app's folder.
	std::vector<std::filesystem::path> shared_; ///< The shared folders beside it.
	json files_ = json::object();               ///< The files, by path.
	std::vector<std::filesystem::path> parts_;  ///< The screen parts read, so each is followed once.
};

/// @brief The shared folders beside an app's folder: `../shared`, and for one notation of an example `../../shared`.
/// @param folder The app's folder, canonical.
/// @return Those that are there, canonical.
std::vector<std::filesystem::path> sharedFolders(const std::filesystem::path& folder) {
	std::vector<std::filesystem::path> result;
	const auto addShared = [&result](const std::filesystem::path& beside) {
		std::error_code error;
		const std::filesystem::path shared = beside / "shared";
		if(std::filesystem::symlink_status(shared, error).type() == std::filesystem::file_type::directory)
			result.push_back(std::filesystem::canonical(shared, error));
	};
	addShared(folder.parent_path());
	if(isNotationFolder(folder)) addShared(folder.parent_path().parent_path());
	return result;
}

/// @brief A credential's look: a prefix, and how many characters of a kind follow it.
struct CredentialPattern {
	std::string_view prefix;         ///< What it starts with.
	std::string_view kind;           ///< What it looks like, for the message.
	std::size_t length = 0;          ///< How many characters of that kind follow at least.
	bool (*accepts)(char) = nullptr; ///< That kind (nullptr for none).
};

/// @brief Whether a character is an ASCII letter or digit.
/// @param c The character.
/// @return Whether it is.
bool isAlnum(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

/// @brief Whether a character is an ASCII capital or digit.
/// @param c The character.
/// @return Whether it is.
bool isUpperOrDigit(char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

/// @brief Whether a character can be part of a token: not a space, a line break or a quote.
/// @param c The character.
/// @return Whether it can.
bool isTokenCharacter(char c) { return c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '"'; }

/// What looks like a credential.
constexpr std::array<CredentialPattern, 10> credentialPatterns{{
    {"-----BEGIN", "a private key or certificate"},
    {"ghp_", "a code host's token"},
    {"xoxb-", "a chat service's token"},
    {"xoxa-", "a chat service's token"},
    {"xoxp-", "a chat service's token"},
    {"xoxr-", "a chat service's token"},
    {"xoxs-", "a chat service's token"},
    {"sk-", "an API key", 20, isAlnum},
    {"AKIA", "a cloud access key", 16, isUpperOrDigit},
    {"Bearer ", "a bearer token", 20, isTokenCharacter},
}};

/// @brief What a credential-looking text in some text looks like.
/// @param text The text.
/// @return What it looks like ("a private key", ...), or nothing.
std::optional<std::string_view> credentialIn(std::string_view text) {
	for(const CredentialPattern& pattern : credentialPatterns) {
		for(std::size_t at = text.find(pattern.prefix); at != std::string_view::npos;
		    at = text.find(pattern.prefix, at + 1)) {
			const std::size_t from = at + pattern.prefix.size();
			std::size_t n = 0;
			while(pattern.accepts != nullptr && from + n < text.size() && pattern.accepts(text[from + n])) ++n;
			if(n >= pattern.length) return pattern.kind;
		}
	}
	return std::nullopt;
}

/// @brief Scans a value's texts and keys for what must never be shared.
class Scrubber {
public:
	/// @brief Starts a scan.
	/// @param privateTexts The texts the package must not hold.
	explicit Scrubber(std::span<const std::string> privateTexts) {
		for(const std::string& text : privateTexts)
			if(text.size() >= 4) private_.push_back(text);
	}

	/// @brief Scans a value.
	/// @param value The value.
	/// @param place Its place ("" for the package).
	void scan(const json& value, const std::string& place) {
		if(value.is_string()) {
			check(value.get_ref<const std::string&>(), place);
		} else if(value.is_object()) {
			for(const auto& [key, item] : value.items()) {
				std::string at = place;
				if(!at.empty()) at += '.';
				at += key;
				std::string keyPlace = place.empty() ? std::string("the package") : place;
				if(check(key, keyPlace.append(", a key"))) {
					scan(item, place.empty() ? std::string("the package") : place);
					continue; // the key isn't printed in the places below it
				}
				// a file's or an asset's bytes are scanned as they are, not as base64
				if(key == "data" && item.is_string()) {
					if(const auto bytes = decodeBase64(item.get_ref<const std::string&>(), maxAssetBytes)) {
						check(*bytes, at);
						continue;
					}
				}
				scan(item, at);
			}
		} else if(value.is_array()) {
			for(std::size_t i = 0; i < value.size(); ++i) scan(value[i], place + "[" + std::to_string(i) + "]");
		}
	}

	/// @brief The problems found.
	/// @return One line each.
	[[nodiscard]] std::vector<std::string> problems() && { return std::move(problems_); }

private:
	/// @brief Checks one text.
	/// @param text The text.
	/// @param place Its place.
	/// @return Whether it holds something it mustn't.
	bool check(std::string_view text, const std::string& place) {
		bool found = false;
		for(const std::string& hidden : private_)
			if(text.contains(hidden)) {
				problems_.push_back(place + ": holds one of your own paths (a package must not say where your files "
				                            "are)");
				found = true;
				break;
			}
		if(const auto credential = credentialIn(text)) {
			problems_.push_back(place + ": holds what looks like " + std::string(*credential) +
			                    " (a package must hold no keys or tokens)");
			found = true;
		}
		return found;
	}

	std::vector<std::string> private_;  ///< The texts it mustn't hold.
	std::vector<std::string> problems_; ///< The problems found.
};

} // namespace

json appFiles(const std::filesystem::path& manifestPath) {
	std::error_code error;
	const std::filesystem::path manifestFile = std::filesystem::canonical(manifestPath, error);
	if(error) refuse(manifestPath.filename().string(), "can't be read");
	const std::filesystem::path folder = manifestFile.parent_path();
	FileCollector collector(folder, sharedFolders(folder));
	const std::string manifestName = manifestFile.filename().string();
	static_cast<void>(collector.add(manifestFile, manifestName));
	const json files = collector.files();
	const auto& text = files[manifestName].value("text", std::string());
	AppManifest app;
	try {
		app = parseApp(notation::readDocument(text, manifestFile));
	} catch(const json::parse_error& problem) {
		refuse(manifestName, std::string("not JSON: ") + problem.what());
	} catch(const DocumentError& problem) {
		refuse(manifestName, problem.what());
	}
	if(!app.icon.empty()) static_cast<void>(collector.add(app.icon, app.icon));
	collector.addPart(app.entry, app.entry, 0);
	for(const std::string& theme : app.themes) collector.addPart(theme, theme, 0);
	if(!app.models.empty()) collector.addPart(app.models, app.models, 0);
	for(const auto& [collection, seed] : app.seed) static_cast<void>(collector.add(seed, seed));
	for(const std::string& asset : app.assets) static_cast<void>(collector.add(asset, asset));
	return collector.files();
}

std::vector<std::string> scrubProblems(const Package& package, std::span<const std::string> privateTexts) {
	Scrubber scrubber(privateTexts);
	scrubber.scan(packageJson(package), "");
	return std::move(scrubber).problems();
}

Package buildPackage(const json& parts, const PackageOptions& options, const AppManifest& app,
                     std::string_view notation, const AssetBytesOf& assetBytes, const json& files) {
	Package package;
	package.kind = options.kind;
	package.name = options.name.empty() ? app.name : options.name;
	package.release = options.release;
	package.description = options.description;
	package.author = options.author;
	package.license = options.license;
	package.engine = options.engine;
	package.base.app = app.id;
	package.base.name = app.name;
	package.base.release = app.release;
	package.base.notation = std::string(notation);
	package.base.tree = documentTree(parts.at("base"));
	package.base.delta = parts.at("delta");
	if(options.kind == PackageKind::App) package.base.files = files;
	package.document = parts.at("document");
	if(const json& state = parts.at("state"); !state.empty()) package.state = state;
	const json listed = package.document.value("assets", json::object());
	json assets = json::object();
	for(const json& name : parts.at("assets")) {
		const auto& asset = name.get_ref<const std::string&>();
		const std::shared_ptr<const std::string> bytes = assetBytes ? assetBytes(asset) : nullptr;
		if(!bytes) refuse("assets." + asset, "the history doesn't hold its bytes");
		assets[asset] = {{"type", listed.value(asset, json::object()).value("type", "")},
		                 {"data", encodeBase64(*bytes)}};
	}
	if(!assets.empty()) package.assets = std::move(assets);
	return parsePackage(packageJson(package));
}

} // namespace affresco::format
