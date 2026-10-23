#pragma once

/// @file
/// @brief Sinopia's native functions: loading, editing and drawing decks, thumbnails for the sorter, the presenter's
/// audience window, and exporting to PDF and PNG.

#include "deck/Deck.hpp"

#include <affresco/affresco.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sinopia {

/// @brief What the natives share while Sinopia runs.
struct Session {
	std::filesystem::path folder;         ///< The screen's folder: decks are read from its `decks` folder.
	std::filesystem::path exportFolder;   ///< Where exports are written.
	std::filesystem::path saveFolder;     ///< Where `sinopia.save` writes decks; empty: the `decks` folder.
	affresco::Application* app = nullptr; ///< Opens the audience window; null in scenarios.
	affresco::Screen* screen = nullptr;   ///< The workspace.
	affresco::Window* window = nullptr;   ///< The workspace's window; null until it is made.
	std::vector<std::unique_ptr<affresco::Screen>> audiences; ///< Audience screens made (the last is the one shown).
	affresco::Window* audienceWindow = nullptr;               ///< The audience window shown, or null.
	std::unordered_map<std::string, std::size_t> thumbnails;  ///< Each slide's thumbnail, by what it was drawn from.
	std::shared_ptr<const affresco::Bitmap> lastFrame;        ///< The last frame presented, finished.
	std::shared_ptr<const affresco::Bitmap> leaving;          ///< The frame a transition leaves.
	std::string presenting;                                   ///< The slide and build step presented.
	DeckLimits limits;                                        ///< What a deck may hold.
};

/// @brief Registers the deck natives: `sinopia.load`, `sinopia.save`, `sinopia.render`, `sinopia.edit`,
/// `sinopia.thumbnails`, `sinopia.exportPdf` and `sinopia.exportPng` (README.md).
/// @param screen The workspace.
/// @param session What they share.
void registerNatives(affresco::Screen& screen, const std::shared_ptr<Session>& session);

/// @brief Registers the presenting natives: `sinopia.openAudience`, `sinopia.closeAudience` and `sinopia.mirror`.
/// @param screen The workspace.
/// @param session What they share.
void registerPresenting(affresco::Screen& screen, const std::shared_ptr<Session>& session);

/// @brief The panel that draws a slide on the stage, laid out at its size behind the workspace.
/// @param session The session (its screen).
/// @param slide The slide's id.
/// @return The panel, or null when there is no such slide.
[[nodiscard]] affresco::Panel* stagePanel(const Session& session, const std::string& slide);

} // namespace sinopia
