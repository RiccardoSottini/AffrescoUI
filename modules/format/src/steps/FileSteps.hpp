#pragma once

/// @file
/// @brief What the file steps share (FileSteps.cpp: saveFile, openFile; ExportSteps.cpp: export): files the person
/// chooses in the OS's dialog, after the event. Their choice is the grant: the document never names a path, and learns
/// only the chosen file's own name.

#include "StepRegistry.hpp"

#include "affresco/render/Window.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace affresco::format {

constexpr std::size_t maxSavedBytes = std::size_t{256} << 20; ///< The largest file a step writes.
constexpr std::size_t maxOpenedBytes = std::size_t{16} << 20; ///< The largest file openFile reads.

/// @brief The steps run once a dialog has answered, and the variables they see.
struct Followers {
	std::shared_ptr<Scope> outcome;  ///< The variables onDone and onError see: set "result" or "error" first.
	std::function<void()> succeeded; ///< Runs onDone.
	std::function<void()> failed;    ///< Runs onError.
};

/// @brief A step's "onDone" and "onError", to run later with the same sender and event.
/// @param s The step.
/// @return Them.
[[nodiscard]] Followers followersOf(const Running& s);

/// @brief A name the save dialog may suggest: the document's text without separators, control characters or the
/// characters file systems refuse, at most 120 bytes.
/// @param text The document's name for it.
/// @param fallback What an empty name becomes, e.g. "Untitled.json".
/// @return The name.
[[nodiscard]] std::string suggestedFileName(std::string_view text, std::string_view fallback);

/// @brief Writes bytes where the person chooses in the save dialog, after the event (a cancel writes nothing), then
/// runs the step's "onDone" with result plus {name}, or "onError" with error {code, message}. Without a window (a
/// check) nothing is shown and nothing runs.
/// @param s The step (its "title" names the dialog).
/// @param bytes What to write.
/// @param name The suggested name.
/// @param filter The kind of file.
/// @param result What onDone sees besides the chosen file's name.
void saveAfterEvent(const Running& s, std::vector<std::uint8_t> bytes, std::string name, FileFilter filter,
                    json result);

} // namespace affresco::format
