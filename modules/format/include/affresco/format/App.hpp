#pragma once

/// @file
/// @brief Apps (SCREEN_FORMAT.md, "Apps: app.json"): an app is a folder of files that a stock host runs, described by
/// its `app.json`: its name and icon, the screen it opens, its own kits and themes, the capabilities it asks the person
/// for, its data models and seed data, and its assets. The manifest is untrusted input: parseApp() checks it whole.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

/// The longest app.json read, in bytes.
inline constexpr std::size_t maxAppManifestBytes = 256 * 1024;

/// @brief One thing an app asks the person to allow.
struct AppCapability {
	/// @brief What it reaches.
	enum class Kind {
		Network, ///< Servers at an origin ("https://api.example.com"): the screen's `connections`.
		Folder,  ///< A folder of the person's, by the root name the screen's `services` use ("workspace").
		Program, ///< A program the screen's `process` services run: a name on the PATH, "shell" or "git".
		Audio,     ///< Sound on the system's output: the screen's `audio`.
		Assistant, ///< The host's own agent, which answers its `ask` steps with the provider the person set up.
		Files,     ///< The open and save dialogs: `saveFile`, `openFile`, `export` and an image's open and save.
		Windows    ///< The screen's other windows: its `windows` section, opened by `openWindow`.
	};
	Kind kind = Kind::Network; ///< What.
	std::string name;          ///< The origin, root or program; "audio", "assistant", "files" or "windows".
	std::string why;           ///< What the app does with it, shown to the person when it asks.
	bool write = false;        ///< For a folder: written, not only read.

	/// @brief The name the person's decision is remembered by: "network:https://api.example.com", "folder:workspace",
	/// "program:git", "audio", "assistant", "files" or "windows". A folder written is another capability than the
	/// same folder read.
	/// @return The key.
	[[nodiscard]] std::string key() const;
};

/// @brief An app's manifest, as parseApp() read it.
struct AppManifest {
	std::string id;                                        ///< Its id: [a-z0-9-], at most 64; its data is kept by it.
	std::string name;                                      ///< Its name, shown by the host.
	std::string description;                               ///< One line about it, or "".
	std::string icon;                                      ///< Its icon, a file in its folder, or "".
	std::string entry = "index.screen.json";               ///< The screen it opens, a file in its folder.
	std::vector<std::string> themes;                       ///< Its own kits and themes: files merged into the screen.
	std::vector<AppCapability> capabilities;               ///< What it asks the person for.
	std::string models;                                    ///< A file of data models merged into the screen, or "".
	std::vector<std::pair<std::string, std::string>> seed; ///< Collection → its first records' file (JSON or CSV).
	std::vector<std::string> assets;                       ///< Its other files and folders, shipped with it.
};

/// @brief Reads and checks an app's manifest: `{"format": "affrescoui-app", "version": 1, "id", "name",
/// "description", "icon", "entry", "themes", "capabilities", "data": {"models", "seed"}, "assets"}`, every path
/// relative and inside the app's folder, every capability with a reason.
/// @param manifest The manifest.
/// @return What it says.
/// @throws DocumentError "AffrescoUI app: <place>: <problem>" if it isn't one.
[[nodiscard]] AppManifest parseApp(const json& manifest);

/// @brief The capabilities a screen uses: its `connections`' origins (their sign-in's too), its `services`' roots and
/// `process` programs, its `audio`, the assistant if a step anywhere is an `ask`, files if one shows a file dialog
/// (`saveFile`, `openFile`, `export`, an image's open or save), and its other `windows`. Each has no reason.
/// @param document The screen, its includes merged.
/// @return Them, each once, in that order.
[[nodiscard]] std::vector<AppCapability> usedCapabilities(const json& document);

/// @brief The capabilities a screen uses that its app doesn't declare (a folder declared written covers it read).
/// @param manifest The app.
/// @param document Its screen, its includes merged.
/// @return Their keys (AppCapability::key()), empty if every one is declared.
[[nodiscard]] std::vector<std::string> undeclaredCapabilities(const AppManifest& manifest, const json& document);

/// @brief The manifest of a screen opened without one (a bare `.screen.json`): the id and name it is given, its
/// entry, and each capability it uses, with no reason.
/// @param id The id: the screen's name, made fit for one (appId()).
/// @param name Its name.
/// @param entry The screen file's name.
/// @param document The screen, its includes merged.
/// @return The manifest.
[[nodiscard]] AppManifest manifestOf(std::string id, std::string name, std::string entry, const json& document);

/// @brief Makes a name fit for an app's id: lower case, every other character than a letter, a digit or '-' a '-',
/// at most 64 characters; "app" if nothing is left.
/// @param name The name ("Flow", "habit tracker").
/// @return The id ("flow", "habit-tracker").
[[nodiscard]] std::string appId(std::string_view name);

/// @brief Whether a path is one inside an app's folder: relative, no `..` part, no backslash, colon or control
/// character, at most 1024 bytes.
/// @param path The path, as the manifest writes it.
/// @return Whether it is.
[[nodiscard]] bool insideApp(std::string_view path);

} // namespace affresco::format
