#pragma once

/// @file
/// @brief What the person lets an app use (SCREEN_FORMAT.md, "Apps: app.json"): nothing until they allow it, asked in a
/// window of its own the first time, remembered for that app only in `<data folder>/apps/<id>/grants.json`, and
/// applied to the app's screen from its manifest, so a revision of the screen never widens it.

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace host {

/// The person's decisions, by capability key (format::AppCapability::key()): allowed or not.
using Decisions = std::map<std::string, bool, std::less<>>;

/// The capabilities allowed, by key.
using Allowed = std::set<std::string, std::less<>>;

/// @brief Where an app's decisions are kept.
/// @param dataDirectory The data folder.
/// @param id The app's id.
/// @return `<data folder>/apps/<id>/grants.json`.
[[nodiscard]] std::filesystem::path grantsFile(const std::filesystem::path& dataDirectory, std::string_view id);

/// @brief Reads an app's decisions.
/// @param file Its grants file.
/// @return Them; none if the file isn't there, or is damaged (then reported, and the person is asked again).
[[nodiscard]] Decisions loadDecisions(const std::filesystem::path& file);

/// @brief Keeps an app's decisions, making its folder.
/// @param file Its grants file.
/// @param decisions The decisions.
/// @throws std::runtime_error If the file can't be written.
void saveDecisions(const std::filesystem::path& file, const Decisions& decisions);

/// @brief Asks the person, in a window of its own (the prompt screen), which of an app's capabilities it may use.
/// Runs its own affresco::Application, so none may exist during the call.
/// @param app The app.
/// @param asked The capabilities to ask about: those not decided yet.
/// @param prompt The prompt screen (examples/shared/host/permissions.screen.json).
/// @return A decision for each one asked; none if the person closed the window without answering.
[[nodiscard]] Decisions askPerson(const affresco::format::AppManifest& app,
                                  const std::vector<affresco::format::AppCapability>& asked,
                                  const std::filesystem::path& prompt);

/// @brief Lets the app's screen use the capabilities allowed: their origins on its network, their folders for the
/// person to choose (for reading only if declared so), their programs, and its sound. Never one its manifest doesn't
/// declare. One that can't be given (a program not on the PATH) is reported, and the app opens without it.
/// @param screen The app's screen.
/// @param app Its manifest.
/// @param allowed The keys allowed.
/// @param silent Whether to open no audio device (a screenshot).
void applyGrants(affresco::Screen& screen, const affresco::format::AppManifest& app, const Allowed& allowed,
                 bool silent);

} // namespace host
