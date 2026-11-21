#pragma once

/// @file
/// @brief A package's file (Package.hpp, VERSION_CONTROL.md, "Sharing a version: packages"): read whole and within its
/// limits, made from what an app's history holds, and scanned for what must never be shared: the person's paths and
/// anything that looks like a credential. What a version is made of comes from vcs::Repository::versionParts(); the
/// files of an app's folder from appFiles(). Nothing here opens an app's records, its saved state or its history.

#include "affresco/format/App.hpp"
#include "affresco/format/Package.hpp"

#include "affresco/core/Json.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief Reads a package's file: refuses more than maxPackageBytes before it is parsed, then parsePackage().
/// @param text The file's bytes.
/// @return The package.
/// @throws DocumentError "AffrescoUI package: <place>: <problem>" if it isn't one, or is too large.
/// @throws json::parse_error If it isn't JSON.
[[nodiscard]] Package readPackage(std::string_view text);

/// @brief What the person says of a version they share: everything else comes from the app and its history.
struct PackageOptions {
	PackageKind kind = PackageKind::Changes; ///< Changes, or App to put the app's files in too (appFiles()).
	std::string name;                        ///< The version's name; "" for the app's own name.
	std::string release = "1.0.0";           ///< The version's own release, three numbers.
	std::string description;                 ///< What the person writes about it, or "".
	std::string author;                      ///< A name the person types, or "": a claim, never a proof.
	std::string license;                     ///< Its licence, or "".
	std::string engine;                      ///< The AffrescoUI release writing it, three numbers.
};

/// @brief An asset's bytes as the history holds them, by the asset's name; nullptr if it has none.
using AssetBytesOf = std::function<std::shared_ptr<const std::string>(std::string_view name)>;

/// @brief Builds a package from what a version is made of, and checks it as parsePackage() would read it, so that the
/// tool never writes a file the reader refuses.
/// @param parts vcs::Repository::versionParts(): `{base, document, state, assets, delta}`.
/// @param options What the person says of it.
/// @param app The app's manifest: its id, name and release name the base.
/// @param notation "json" or "lisp": the notation of the app's entry.
/// @param assetBytes The bytes of each asset `parts.assets` names.
/// @param files For kind App, appFiles(); else null.
/// @return The package.
/// @throws DocumentError If an asset's bytes are missing, or the result isn't a package parsePackage() reads.
[[nodiscard]] Package buildPackage(const json& parts, const PackageOptions& options, const AppManifest& app,
                                   std::string_view notation, const AssetBytesOf& assetBytes, const json& files);

/// @brief The files of an app's folder that its manifest reaches, and nothing else: the manifest, its icon, the entry
/// and its includes (transitively, from each including file's folder, as the loader resolves them), its themes, its
/// models, its seed files and its assets (a folder's files, recursively). Files under the `shared` folder beside the
/// app (or beside an example's two notations) are left out: every apps folder has them. No link is followed out of
/// the folder.
/// @param manifestPath The app's app.json or app.lisp.
/// @return `{path: {"text"} | {"data": base64}}`, each path relative to the app's folder.
/// @throws DocumentError "AffrescoUI package: <path>: <problem>" for a file outside the folder (and not shared), one
/// that can't be read, one over maxPackageFileBytes, or more than maxPackageFiles.
[[nodiscard]] json appFiles(const std::filesystem::path& manifestPath);

/// @brief What must never be in a shared file: any of the person's private texts (their home folder, data folder and
/// the app's own folder), and text that looks like a credential: `-----BEGIN`, `sk-` and 20 or more letters and
/// digits, `ghp_`, a Slack token (`xoxb-`, `xoxp-`, ...), `AKIA` and 16 capitals or digits, `Bearer ` and 20 or more
/// characters. Every text and key of the package is scanned.
/// @param package The package.
/// @param privateTexts The texts it must not hold; one shorter than 4 characters is ignored.
/// @return One line per problem, naming its place and the kind of text, never the text itself; empty if none.
[[nodiscard]] std::vector<std::string> scrubProblems(const Package& package, std::span<const std::string> privateTexts);

} // namespace affresco::format
