#pragma once

/// @file
/// @brief Features from catalogues (rfcs/0004-features.md): the publishers a host trusts, the catalogues it reads, and
/// the revisions that install and upgrade a feature from them. Enabling, disabling and removing a feature are
/// revisions on `/features/<name>` like any other (see VERSION_CONTROL.md, "Features").

#include "affresco/core/Json.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace affresco::format {

/// @brief The catalogues a host reads features from, and the publishers whose signatures it accepts. Main thread.
///
/// Nothing is trusted by default: a catalogue, and each manifest in it, must be signed by a key added with
/// trustPublisher(). A feature installed from one can never widen a capability: the manifest's checks refuse services,
/// connections, channels, persistence, permissions and control steps.
class Features {
public:
	/// @brief Starts with no catalogue and no trusted publisher.
	Features();
	/// @brief Forgets them.
	~Features();
	Features(const Features&) = delete;
	Features& operator=(const Features&) = delete;
	/// @brief Takes over another's features.
	Features(Features&&) noexcept;
	/// @brief Takes over another's features.
	/// @return This one.
	Features& operator=(Features&&) noexcept;

	/// @brief Trusts a publisher's key: catalogues and manifests it signed are accepted.
	/// @param publisher "ed25519:<base64url key>".
	/// @throws DocumentError If it isn't a key.
	void trustPublisher(std::string_view publisher);

	/// @brief Reads a local catalogue: its index (at most 8 MiB), signed by a trusted publisher. Its manifests are read
	/// and checked when a feature is installed. Replaces a catalogue of the same name.
	/// @param index The index file; the manifests' URLs are relative to its folder.
	/// @return The catalogue's name.
	/// @throws DocumentError With the reason it was refused (what() names the place).
	std::string addCatalog(const std::filesystem::path& index);

	/// @brief What the catalogues offer, for a screen: one row per feature, its newest release.
	/// @param document The document, to tell what is installed.
	/// @return `[{name, version, publisher, title, description, installed, enabled}]`, by name; `publisher` is the
	/// name the catalogue gives the key, else the key.
	[[nodiscard]] json catalog(const json& document) const;

	/// @brief The revision that installs a feature from a catalogue, after checking its manifest (size, digest,
	/// signature, shape). The revision is reviewed like any other: the document's "permissions" decide.
	/// @param document The document.
	/// @param name The feature.
	/// @param release The release, or "" for the newest.
	/// @param author The revision's author.
	/// @return The revision: `{author, message, set: {"/features/<name>": record}}`.
	/// @throws DocumentError With the reason (not in a catalogue, installed already, refused manifest).
	[[nodiscard]] json installRevision(const json& document, std::string_view name, std::string_view release,
	                                   std::string_view author) const;

	/// @brief The revision that upgrades an installed feature to its newest release, from the same publisher, keeping
	/// whether it is enabled and its order.
	/// @param document The document.
	/// @param name The feature.
	/// @param author The revision's author.
	/// @return The revision.
	/// @throws DocumentError With the reason (not installed, up to date, another publisher, refused manifest).
	[[nodiscard]] json upgradeRevision(const json& document, std::string_view name, std::string_view author) const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_; ///< The catalogues and keys.
};

} // namespace affresco::format
