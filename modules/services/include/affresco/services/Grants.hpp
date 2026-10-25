#pragma once

/// @file
/// @brief What a host may grant from its command line (`--allow`): programs found on the PATH, the login shell, and
/// git under a policy that runs only the commands a source control view needs, on the declaration's own folder.

#include <affresco/services/Provider.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

class ServiceRegistry;

/// @brief Finds a program on the PATH.
/// @param name Its name ("git"; ".exe" is added on Windows).
/// @return Its absolute path, or empty if no folder of the PATH has it.
[[nodiscard]] std::filesystem::path findProgram(std::string_view name);

/// @brief The person's login shell: $SHELL if it is an absolute path to a file, else /bin/sh (cmd.exe on Windows).
/// @return Its absolute path.
[[nodiscard]] std::filesystem::path loginShell();

/// @brief The options every granted git run starts with: no hooks, file system monitor, signing or textconv programs
/// a folder's own configuration could name, no submodule's repository, and paths taken literally.
/// @return The options.
[[nodiscard]] std::vector<std::string> gitOptions();

/// @brief Whether git arguments are a command the git grant runs: `init`; `status --porcelain=v2 -z --branch
/// --untracked-files=all`; `commit -F -`; `for-each-ref --format=%(HEAD)%00%(refname:short) refs/heads/`;
/// `log --format=%H%x00%an%x00%at%x00%s -n N [-- PATH]` (N up to 1000); `show --no-textconv HEAD:PATH`;
/// `add -- PATH...`; `restore [--staged] -- PATH...` (1 to 1000 paths); `switch [-c] BRANCH`.
/// @param arguments The arguments, after the grant's own options.
/// @return Why they are refused, or std::nullopt.
[[nodiscard]] std::optional<std::string> checkGitArguments(const std::vector<std::string>& arguments);

/// @brief Whether a path is one inside a folder: relative, no `..` part, no control character or backslash, at most
/// 4 KiB.
/// @param path The path.
/// @return Whether it is.
[[nodiscard]] bool isFolderPath(std::string_view path) noexcept;

/// @brief Whether a name is a branch name git accepts that the git grant switches to: up to 200 characters of
/// letters, digits and `._/-`, by git's rules on where `.`, `/` and `-` may be.
/// @param name The name.
/// @return Whether it is.
[[nodiscard]] bool isBranchName(std::string_view name) noexcept;

/// @brief Checks a repository's own git config (`.git/config`): every key must be one of a short list of data keys
/// (core's file handling, remote.*, branch.*, user.*, and a few more), never one that may name a program or a file
/// (`filter.*`, `diff.*`, `gpg.*`, `include*`, `core.pager`, `core.sshCommand`, `credential.*`, ...). A line git would
/// continue, a header it might read differently and anything over 1 MiB are refused too.
/// @param text The config.
/// @return Why it is refused, or std::nullopt.
[[nodiscard]] std::optional<std::string> checkGitConfig(std::string_view text);

/// @brief Checks the repository at a folder for the git grant: `.git` a real folder (not a link, nor a file naming a
/// repository elsewhere), no `.git/commondir`, and a config checkGitConfig() accepts. A folder with no `.git` passes
/// (git says itself that there is no repository).
/// @param root The folder.
/// @return Why it is refused, or std::nullopt.
[[nodiscard]] std::optional<std::string> checkGitRepository(const std::filesystem::path& root);

/// @brief The git grant's rule: checkGitArguments(), checkGitRepository(), and the repository of the declaration's
/// root only (never one above it).
/// @return The shaper.
[[nodiscard]] ProcessShaper gitShaper();

/// @brief One of the host's sibling programs an app-programs grant may start (appProgramsShaper()).
struct AppProgram {
	std::string name;    ///< Its file's name, e.g. "affrescoui_launcher" (".exe" is added on Windows).
	bool screen = false; ///< It takes an app to open (a folder, an app.json or a screen file), its first argument.
	bool ask = false;    ///< It takes `--ask TEXT`, a first request for the app's assistant.
};

/// @brief What an app-programs grant starts: programs of the host's own, next to it, each opening an app from a
/// granted folder, with the host's data folder and a socket folder per app (a launcher).
struct AppPrograms {
	std::filesystem::path folder;     ///< Where the programs are: the host's own folder.
	std::vector<AppProgram> programs; ///< The programs, by name.
	/// @brief The folders screens are opened from, by the names a call uses (`"examples"`): canonical, like roots.
	std::map<std::string, std::filesystem::path, std::less<>> roots;
	std::filesystem::path dataDirectory;   ///< Each program gets `--data` this.
	std::filesystem::path socketDirectory; ///< Each app gets `--socket` a folder in this one, of its own.
	bool reveal = false;                   ///< Also `reveal ROOT:FOLDER`: shows a folder in the desktop's file manager.
};

/// @brief Finds one of the host's sibling programs: next to the host, in a build tree's folder for its example
/// (`examples/<name without affrescoui_>/`), or, from a macOS app bundle, next to the bundle or in a bundle of its own.
/// @param folder The host's folder.
/// @param name The program's name.
/// @return Its file, or empty if it isn't there.
[[nodiscard]] std::filesystem::path findAppProgram(const std::filesystem::path& folder, std::string_view name);

/// @brief The app-programs grant's rule. A call's arguments are `[PROGRAM, SCREEN?, "--ask", TEXT?]`: PROGRAM one of
/// @p programs' names; SCREEN, for a program that takes one (and required), `ROOT:PATH` inside that root (links
/// resolved, no `..`, no absolute path): an app's folder (one with an `app.json`), an `app.json` or a `.screen.json`;
/// `--ask TEXT` only for a program that takes it (TEXT at most 4 KiB, no NUL). With `reveal`, `["reveal",
/// "ROOT:FOLDER"]` shows a folder inside a root (`"ROOT:"` the root itself). The rule picks the program file and adds,
/// itself, `--data <dataDirectory>` and `--socket <socketDirectory>/<app>`
/// (`<app>` is `<root>-<the app's folder>`, or the program's name without `affrescoui_`), which the program creates.
/// @param programs What it may start.
/// @return The shaper.
[[nodiscard]] ProcessShaper appProgramsShaper(AppPrograms programs);

/// @brief Whether two folders overlap: one is the other or inside it, once both are resolved (links, `..`).
/// @param first A folder.
/// @param second Another.
/// @return Whether they do; true too if either can't be resolved.
[[nodiscard]] bool rootsOverlap(const std::filesystem::path& first, const std::filesystem::path& second);

/// @brief Grants the host's sibling programs under a name, detached (ProcessGrant::detach: each run is started and
/// outlives the host, its result `{pid}`), under appProgramsShaper().
/// @param services The registry.
/// @param name The grant's name, e.g. "affresco".
/// @param programs What it may start; its roots must be folders, none inside another. A started program inherits the
/// host's environment.
/// @return Why it can't, or std::nullopt.
[[nodiscard]] std::optional<std::string> grantAppPrograms(ServiceRegistry& services, std::string name,
                                                          AppPrograms programs);

/// @brief Grants a program from a command-line spec: `NAME=PROGRAM ARGUMENT...` (the program found on the PATH
/// unless absolute, the arguments split at spaces); or `NAME` alone: `shell` is the login shell (`-l`), `git` is git
/// under gitShaper(), any other the program of that name on the PATH.
/// @param services The registry.
/// @param spec The spec: at most 4 KiB and 32 arguments.
/// @return Why it can't, or std::nullopt.
[[nodiscard]] std::optional<std::string> grantProgram(ServiceRegistry& services, std::string_view spec);

} // namespace affresco::services
