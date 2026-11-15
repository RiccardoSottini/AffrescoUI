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

/// @brief The line of a file a program's node is written on: the source's `line` counts from its definition's first
/// line, so the definition's text is found in the file and its line added.
/// @param source What Screen::source() gave for a program.
/// @param folder The program's folder, where its files are.
/// @return The line in the file, from 1, or 0 if the definition isn't found there as written (a shared add-on's file,
/// a text cut).
std::size_t lineInFile(const json& source, const std::filesystem::path& folder) {
	const std::string text = source.value("text", "");
	if(text.empty() || source.value("cut", false)) return 0;
	std::string file;
	try {
		file = readFile(folder / source.value("file", ""));
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
