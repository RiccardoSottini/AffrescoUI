/// @file
/// @brief Screen::listenEditor(): the nREPL server for the screen's program (net::EditorServer), with the question in
/// which the person pairs each connection and the mapping between an editor's paths and the program's files.

#include "ScreenImpl.hpp"

#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef AFFRESCOUI_VERSION
#define AFFRESCOUI_VERSION "" // set by the build (modules/screen/CMakeLists.txt)
#endif

namespace affresco {

namespace {

/// @brief The question in which the person pairs an editor's connection.
/// @param text The text the connection first wants to evaluate, shown whole.
/// @return The question.
Confirmation pairingQuestion(std::string text) {
	return {.title = "Let this editor evaluate as you?",
	        .message =
	            "A program on this computer connected as an editor. If you say yes, the Lisp it sends runs as if "
	            "you typed it here, until it disconnects. This is the first thing it asks to run: read all of "
	            "it. If you did not just connect an editor, say no.",
	        .text = std::move(text),
	        .accept = "Let It Evaluate",
	        .decline = "Refuse"};
}

/// @brief The program's file an editor's path stands for: the path as the program names its files (from the entry
/// file's folder, `/` between parts, `..` kept) if the program has it, else the one program file with the same file
/// name, else "". Nothing is read from disk.
/// @param console The program.
/// @param folder The entry file's folder, absolute and normal; empty for a screen not opened from a file.
/// @param editorPath The path the editor sent.
/// @return The program's file, or "".
std::string programFileOf(const ProgramConsole& console, const std::filesystem::path& folder,
                          const std::string& editorPath) {
	if(editorPath.empty()) return {};
	const std::vector<std::string> files = console.files();
	const std::filesystem::path path = std::filesystem::path(editorPath).lexically_normal();
	if(!folder.empty()) {
		const std::string relative = (path.is_absolute() ? path.lexically_relative(folder) : path).generic_string();
		if(!relative.empty() && std::ranges::find(files, relative) != files.end()) return relative;
	}
	const std::filesystem::path name = path.filename();
	std::string found;
	for(const std::string& file : files) {
		if(std::filesystem::path(file).filename() != name) continue;
		if(!found.empty()) return {}; // two files of that name: which one is the editor's isn't known
		found = file;
	}
	return found;
}

} // namespace

std::uint16_t Screen::listenEditor(Executor& executor, net::EditorServerOptions options) {
	Impl* impl = impl_.get();
	ProgramConsole& console = impl->runtime.console();
	if(!console.hasProgram())
		throw std::runtime_error(
		    "AffrescoUI screen: an editor connects to an app written as a program: this one is JSON");
	Window* window = impl->runtime.window();
	if(!window) throw std::runtime_error("AffrescoUI screen: an editor connects once the screen has a window");
	if(impl->editor) throw std::runtime_error("AffrescoUI screen: an editor server is listening already");
	options.version = AFFRESCOUI_VERSION;
	// The entry file's folder, for the editor's paths; none for a screen built from a text
	const std::filesystem::path folder = impl->entryFile == "screen"
	                                         ? std::filesystem::path()
	                                         : std::filesystem::absolute(impl->screenFolder).lexically_normal();
	// The question shown now, if any: the server asks one at a time, and takes it away when its connection goes
	const auto question = std::make_shared<std::uint64_t>(0);
	net::EditorHost host;
	host.console = &console;
	host.pair = [&console, window, question](std::string text,
	                                         std::function<void(bool)> answered) -> std::optional<std::string> {
		if(std::optional<std::string> why = console.unshowable(text)) return why;
		const std::uint64_t id = window->confirm(pairingQuestion(std::move(text)),
		                                         [question, answered = std::move(answered)](bool accepted) {
			                                         *question = 0;
			                                         answered(accepted);
		                                         });
		if(id == 0) return "the app is asking the person something else: try again";
		*question = id;
		return std::nullopt;
	};
	host.cancelPairing = [window, question] { window->dismissConfirmation(std::exchange(*question, 0)); };
	// The closures keep their own copy of the folder (not const), so moving them never copies a path
	host.programFile = [&console, folder = folder](const std::string& editorPath) {
		return programFileOf(console, folder, editorPath);
	};
	host.pathOf = [folder = folder](const std::string& file) {
		if(folder.empty() || file.empty()) return std::string();
		return (folder / file).lexically_normal().string();
	};
	impl->editor = std::make_unique<net::EditorServer>(executor, std::move(host), std::move(options));
	return impl->editor->port();
}

} // namespace affresco
