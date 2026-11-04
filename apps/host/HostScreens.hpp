#pragma once

/// @file
/// @brief The host's own windows (the permissions it asks for, the changes it shows): screens of the host's assets,
/// never the app's, in the notation the app was opened in, JSON or Lisp, both drawing the same window.

#include <affresco/affresco.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/notation/Notation.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace host {

/// @brief The file of one of the host's windows, in the notation of the screen the app opens from: an app's entry
/// that is a `.lisp` file (an example opened from its lisp folder) gets the window's `.screen.lisp` twin.
/// @param folder The host's windows' folder (examples/shared/host).
/// @param name The window: "permissions" or "changes".
/// @param entry The app's entry screen, or its bare screen file.
/// @return The window's file.
[[nodiscard]] inline std::filesystem::path hostScreenFile(const std::filesystem::path& folder, const std::string& name,
                                                          const std::filesystem::path& entry) {
	const bool lisp = affresco::notation::notationOf(entry) == affresco::notation::Notation::Lisp;
	return folder / (name + (lisp ? ".screen.lisp" : ".screen.json"));
}

/// @brief Reads one of the host's windows, keeping nothing on disk: a JSON screen as it is, a Lisp one as the program
/// it is (its includes from its own folder).
/// @param file The window's file.
/// @return The screen, not yet built.
/// @throws std::runtime_error If it can't be read.
/// @throws affresco::DocumentError If it isn't a valid screen.
[[nodiscard]] inline affresco::Screen readHostScreen(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if(!in) throw std::runtime_error("could not open the host's window " + file.string());
	return affresco::Screen::fromString(affresco::lisp::jsonText(text, file), file.parent_path());
}

} // namespace host
