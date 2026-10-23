#pragma once

/// @file
/// @brief The shipped templates and their folders, as examples/templates.json lists them.

#include <affresco/core/Json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

/// @brief The templates' list: each one's name and its folder.
/// @param examples The examples folder.
/// @return The "templates" object of templates.json.
/// @throws std::runtime_error If it can't be read.
inline affresco::json templateFolders(const std::filesystem::path& examples) {
	std::ifstream file(examples / "templates.json");
	if(!file) throw std::runtime_error("could not open " + (examples / "templates.json").string());
	return affresco::json::parse(file).at("templates");
}

/// @brief The templates' names ("kanban" for flow/index.screen.json), in name order.
/// @param examples The examples folder.
/// @return Their names.
inline std::vector<std::string> templateNames(const std::filesystem::path& examples) {
	std::vector<std::string> names;
	const affresco::json folders = templateFolders(examples); // named: a loop over a temporary's items dangles
	for(const auto& [name, folder] : folders.items()) names.push_back(name);
	std::ranges::sort(names);
	return names;
}

/// @brief A template's folder, where its screen, scenarios and policy are.
/// @param examples The examples folder.
/// @param name E.g. "kanban".
/// @return E.g. examples/flow.
inline std::filesystem::path templateDirectory(const std::filesystem::path& examples, const std::string& name) {
	return examples / templateFolders(examples).at(name).get<std::string>();
}
