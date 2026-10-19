#pragma once

/// @file
/// @brief What the tests of histories on disk share (history.cpp, vcs_history.cpp): a temporary folder, reading and
/// writing files, and forging commits and refs the way a program that ignores the rules could.

#include <affresco/core/Json.hpp>

#include "Sha256.hpp" // modules/vcs/src: to forge a history with valid hashes

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace affresco::test {

/// @brief A new, empty folder for one test, removed with everything in it when the test ends.
class TemporaryFolder {
public:
	/// @brief Creates the folder.
	TemporaryFolder() {
		static std::atomic<int> count = 0;
		path_ =
		    std::filesystem::temp_directory_path() /
		    ("affrescoui-history-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
			 "-" + std::to_string(count++));
		std::filesystem::create_directories(path_);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	/// @brief Removes the folder.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path_, ignored);
	}

	/// @brief The folder.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< The folder.
};

/// @brief Reads a whole file.
/// @param file The file.
/// @return Its text.
inline std::string readText(const std::filesystem::path& file) {
	const std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

/// @brief Replaces a file's text.
/// @param file The file.
/// @param text The new text.
inline void writeText(const std::filesystem::path& file, const std::string& text) {
	std::ofstream(file, std::ios::binary | std::ios::trunc) << text;
}

/// @brief The file a loose object is stored in.
/// @param history The history folder.
/// @param id The object's id.
/// @return objects/<2>/<62>.
inline std::filesystem::path objectFile(const std::filesystem::path& history, const std::string& id) {
	return history / "objects" / id.substr(0, 2) / id.substr(2);
}

/// @brief Counts the files with a name ending in ".tmp" in a folder and below.
/// @param folder The folder.
/// @return How many there are.
inline int temporaryFiles(const std::filesystem::path& folder) {
	int count = 0;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(folder))
		if(entry.path().extension() == ".tmp") ++count;
	return count;
}

/// @brief Writes an object into a history folder.
/// @param history The history folder.
/// @param id Its id.
/// @param text Its text.
inline void writeObject(const std::filesystem::path& history, const std::string& id, const std::string& text) {
	std::filesystem::create_directories(objectFile(history, id).parent_path());
	writeText(objectFile(history, id), text);
}

/// @brief Writes a commit and its document as version 1 of the history did, the way a program that ignores the rules
/// could: well formed, with valid ids.
/// @param history The history folder.
/// @param commit The commit: parents, author, committer, time, proposal, message, patch, document.
/// @return Its id.
inline std::string forgeCommit(const std::filesystem::path& history, json commit) {
	using affresco::vcs::detail::sha256;
	const std::string document = commit.at("document").dump(), tree = sha256(document);
	std::string object = "tree " + tree + "\n";
	for(const json& parent : commit.at("parents")) object += "parent " + parent.get<std::string>() + "\n";
	object += "author " + commit.at("author").get<std::string>() + "\ncommitter " +
	          commit.at("committer").get<std::string>() + "\ntime " + commit.at("time").get<std::string>() +
	          "\npatch " + sha256(commit.at("patch").dump()) + "\n";
	if(const int proposal = commit.value("proposal", 0)) object += "proposal " + std::to_string(proposal) + "\n";
	object += "\n" + commit.at("message").get<std::string>();
	const std::string id = sha256(object);
	commit.erase("document");
	commit["tree"] = tree;
	writeObject(history, tree, document);
	writeObject(history, id, commit.dump());
	return id;
}

/// @brief Rewrites the refs file the way a program that ignores the rules could, with a valid checksum.
/// @tparam Change A function that takes the refs as `json&`.
/// @param history The history folder.
/// @param change Changes the refs (without their checksum).
template <typename Change> void forgeRefs(const std::filesystem::path& history, Change change) {
	json refs = json::parse(readText(history / "refs.json"));
	refs.erase("checksum");
	change(refs);
	refs["checksum"] = affresco::vcs::detail::sha256(refs.dump());
	writeText(history / "refs.json", refs.dump(2));
}

} // namespace affresco::test
