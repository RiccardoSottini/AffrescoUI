/// @file
/// @brief `affrescoui source`: where a node is written (SCREEN_FORMAT.md, "Pointing at a node"), as an agent reads it
/// with `getSource`, from a screen or an app loaded as `check` loads it: no window, no channel, no history on disk.

#include "Cli.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/notation/Notation.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace affresco::cli {

namespace {

/// @brief A screen or an app, loaded with nothing kept on disk: an app's folder, its manifest or its entry screen as
/// the app (as the host opens it), any other screen file as itself.
/// @param file The file or folder.
/// @return The screen, not built.
/// @throws std::runtime_error If a file can't be read; DocumentError if it isn't a valid screen or app.
Screen loaded(const std::filesystem::path& file) {
	std::filesystem::path manifest;
	if(std::filesystem::is_directory(file)) manifest = notation::namedDocument(file, "app");
	else if(notation::isNamedDocument(file, "app")) manifest = file;
	else manifest = Screen::appOf(file);
	if(!manifest.empty()) return Screen::fromApp(manifest, {}, Screen::History::InMemory);
	return Screen::fromString(lisp::jsonText(readFile(file), file), file.parent_path());
}

/// @brief A program file's path inside its folder, as a key under /program names it: refused if the name is absolute
/// or climbs out (`..`), if the file (its links followed) is outside the folder, or isn't a regular file of at most
/// 16 MiB. The key is the document's, so it may name any file: this keeps `source` from reading one it shouldn't.
/// @param folder The program's folder.
/// @param name The file's name, as the source's `file` gives it.
/// @return The file's path, or empty if refused.
std::filesystem::path fileInside(const std::filesystem::path& folder, const std::string& name) {
	static constexpr std::uintmax_t mostBytes = 16U * 1024U * 1024U;
	const std::filesystem::path relative(name);
	if(name.empty() || relative.has_root_name() || relative.has_root_directory()) return {};
	if(std::ranges::any_of(relative, [](const std::filesystem::path& part) { return part == ".."; })) return {};
	std::error_code error;
	const std::filesystem::path root = std::filesystem::canonical(folder, error);
	if(error) return {};
	const std::filesystem::path path = std::filesystem::canonical(root / relative, error);
	if(error || !std::filesystem::is_regular_file(path, error) || error) return {};
	const std::uintmax_t size = std::filesystem::file_size(path, error);
	if(error || size > mostBytes) return {};
	const std::filesystem::path inside = path.lexically_relative(root);
	if(inside.empty() || *inside.begin() == "..") return {};
	return path;
}

/// @brief The line of a file a program's node is written on: the source's `line` counts from its definition's first
/// line, so the definition's text is found in the file and its line added.
/// @param source What Screen::source() gave for a program.
/// @param folder The program's folder, where its files are.
/// @return The line in the file, from 1, or 0 if the definition isn't found there as written (a shared add-on's file,
/// a text cut) or the file isn't one fileInside() lets it read.
std::size_t lineInFile(const json& source, const std::filesystem::path& folder) {
	const std::string text = source.value("text", "");
	if(text.empty() || source.value("cut", false)) return 0;
	const std::filesystem::path path = fileInside(folder, source.value("file", ""));
	if(path.empty()) return 0;
	std::string file;
	try {
		file = readFile(path);
	} catch(const std::exception&) {
		return 0;
	}
	const std::size_t at = file.find(text);
	if(at == std::string::npos) return 0;
	const auto before =
	    static_cast<std::size_t>(std::count(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(at), '\n'));
	return before + source["line"].get<std::size_t>();
}

/// @brief Prints a source as a person reads it: one line saying where (`<address>  <file>:<line>  (<kind>)` for a
/// program's definition, `<address>  <file>` for a JSON document's node), then the text.
/// @param source What Screen::source() gave.
/// @param folder The screen's folder, where a program's files are.
void printSource(const json& source, const std::filesystem::path& folder) {
	std::cout << source.value("address", "");
	if(source.contains("file")) std::cout << "  " << source["file"].get<std::string>();
	if(source.contains("line")) {
		if(const std::size_t line = lineInFile(source, folder); line > 0) std::cout << ':' << line;
		else std::cout << "  (line " << source["line"].get<std::size_t>() << " of the definition)";
	}
	if(source.contains("kind")) std::cout << "  (" << source["kind"].get<std::string>() << ')';
	const std::string text = source.value("text", "");
	std::cout << '\n' << text;
	if(!text.empty() && text.back() != '\n') std::cout << '\n';
	if(source.value("cut", false)) std::cout << "[cut at 16 KiB]\n";
}

} // namespace

Exit printNodeSource(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	const Arguments arguments(args, {}, flags);
	if(arguments.positional().size() != 2)
		throw UsageError("expected a file and what to find: #id, a pointer or an address");
	const std::filesystem::path file = arguments.positional()[0];
	if(!std::filesystem::exists(file)) throw std::runtime_error(file.string() + ": no such file");

	json source;
	setLogSink([](LogLevel, std::string_view) {}); // a screen's load warnings aren't the answer
	try {
		const Screen screen = loaded(file);
		source = screen.source(arguments.positional()[1]);
	} catch(const DocumentError& error) {
		setLogSink({});
		throw FileProblem(error.what());
	} catch(...) {
		setLogSink({});
		throw;
	}
	setLogSink({});
	if(arguments.flag("--json")) std::cout << source.dump() << '\n';
	else printSource(source, std::filesystem::is_directory(file) ? file : file.parent_path());
	return Exit::Success;
}

} // namespace affresco::cli
