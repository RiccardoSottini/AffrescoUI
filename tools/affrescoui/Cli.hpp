#pragma once

/// @file
/// @brief What the commands of the `affrescoui` tool share: exit codes, arguments, files, and the commands.

#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {
class Screen;
}

namespace affresco::cli {

/// @brief How a command ends (tools/affrescoui/README.md, "Exit codes").
enum class Exit {
	Success = 0,  ///< Done; for check and fmt --check, nothing to report.
	Problems = 1, ///< The file has problems: check found some, fmt --check would change it, import was refused.
	Usage = 2,    ///< The command line is wrong.
	Failure = 3   ///< It couldn't be done: a file can't be read or written, no window, a damaged history.
};

/// @brief A command line that is wrong: the tool prints the message and the command's usage, and exits with Usage.
class UsageError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/// @brief A file a command can't work on as it is (it isn't JSON): the tool prints the message and exits with
/// Problems.
class FileProblem : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/// @brief A command line, read against the options a command takes.
class Arguments {
public:
	/// @brief Reads a command line.
	/// @param args What follows the command's name.
	/// @param valued The options that take a value ("--scale", "-o").
	/// @param flags The options that don't ("--json", "--check").
	/// @throws UsageError If an option is unknown or misses its value.
	Arguments(std::span<const std::string_view> args, std::span<const std::string_view> valued,
	          std::span<const std::string_view> flags);

	/// @brief The arguments that aren't options, in order.
	/// @return Them.
	[[nodiscard]] const std::vector<std::string>& positional() const noexcept { return positional_; }

	/// @brief An option's value.
	/// @param name The option.
	/// @return Its value, or std::nullopt if it wasn't given.
	[[nodiscard]] std::optional<std::string> value(std::string_view name) const;

	/// @brief Tells whether a flag was given.
	/// @param name The flag.
	/// @return true if it was.
	[[nodiscard]] bool flag(std::string_view name) const;

	/// @brief The one file a command works on.
	/// @return It.
	/// @throws UsageError If there isn't exactly one positional argument.
	[[nodiscard]] std::filesystem::path file() const;

private:
	std::vector<std::string> positional_;                    ///< Arguments that aren't options.
	std::map<std::string, std::string, std::less<>> values_; ///< Options and their values; flags have "".
};

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its bytes.
/// @throws std::runtime_error If it can't be read.
[[nodiscard]] std::string readFile(const std::filesystem::path& path);

/// @brief Writes a whole file, replacing it.
/// @param path The file.
/// @param content Its bytes.
/// @throws std::runtime_error If it can't be written.
void writeFile(const std::filesystem::path& path, std::string_view content);

/// @brief Registers the application types of the example apps (the Forge template's "code"), when the tool is built
/// with the examples, and makes other unknown types plain panels, as affrescoui_host does.
/// @param screen The screen.
void registerApplicationTypes(Screen& screen);

/// @brief A command of the tool.
struct Command {
	std::string_view name;                          ///< What is typed: "check".
	std::string_view usage;                         ///< Its arguments: "<file> [--json]".
	std::string_view summary;                       ///< One line on what it does.
	Exit (*run)(std::span<const std::string_view>); ///< Runs it with what follows its name.
};

/// @brief Every command, in the order help lists them.
/// @return Them.
[[nodiscard]] std::span<const Command> commands();

/// @brief `run`: shows a screen and lets peers change it, like affrescoui_host.
/// @param args The arguments.
/// @return How it ended.
Exit runScreen(std::span<const std::string_view> args);

/// @brief `check`: the schema, the format's rules and the checker.
/// @param args The arguments.
/// @return How it ended.
Exit checkScreen(std::span<const std::string_view> args);

/// @brief `test`: plays a screen's scenarios and reports pass and fail compactly.
/// @param args The arguments.
/// @return How it ended.
Exit testScreen(std::span<const std::string_view> args);

/// @brief `render`: a picture of a screen's first frame, drawn offscreen.
/// @param args The arguments.
/// @return How it ended.
Exit renderScreen(std::span<const std::string_view> args);

/// @brief `fmt`: writes a file the way AffrescoUI files are written.
/// @param args The arguments.
/// @return How it ended.
Exit formatScreen(std::span<const std::string_view> args);

/// @brief `ids`: gives readable ids to the nodes of a file that have none.
/// @param args The arguments.
/// @return How it ended.
Exit nameScreenNodes(std::span<const std::string_view> args);

/// @brief `export`: a change set from a screen's history on disk.
/// @param args The arguments.
/// @return How it ended.
Exit exportChanges(std::span<const std::string_view> args);

/// @brief `import`: applies a change set to a screen's history on disk.
/// @param args The arguments.
/// @return How it ended.
Exit importChanges(std::span<const std::string_view> args);

/// @brief `import`: with `--from`, writes a screen from another format's document (ImportCommand.cpp); without it,
/// importChanges().
/// @param args The arguments.
/// @return How it ended.
Exit importFile(std::span<const std::string_view> args);

/// @brief `schema`: the format's JSON Schema and SCREEN_FORMAT.md's tables, generated from the registries.
/// @param args The arguments.
/// @return How it ended.
Exit generateSchema(std::span<const std::string_view> args);

/// @brief `sign`: signs a feature's manifest or a catalogue's index, or makes a signing key.
/// @param args The arguments.
/// @return How it ended.
Exit signFile(std::span<const std::string_view> args);

} // namespace affresco::cli
