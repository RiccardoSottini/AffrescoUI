#pragma once

/// @file
/// @brief detail::ScratchFolder: a new folder only this user can open, removed with everything in it.

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace affresco::detail {

/// @brief A new folder only this user can open, for files a call writes and reads back (a drawn frame); removed with
/// everything in it. Used by the screen's pictures and by automation.
class ScratchFolder {
public:
	/// @brief Creates the folder in the system's temporary directory, under a random name nobody can take first.
	ScratchFolder() {
		std::error_code error;
		const std::filesystem::path temporary = std::filesystem::temp_directory_path(error);
		if(error) return;
		std::random_device random;
		for(int attempt = 0; attempt < 8 && path_.empty(); ++attempt) {
			const std::filesystem::path candidate =
			    temporary / ("affrescoui-image-" + std::to_string(random()) + "-" + std::to_string(random()));
			if(std::filesystem::create_directory(candidate, error)) path_ = candidate; // false if it exists
		}
		if(path_.empty()) return;
		// Private before anything goes in; a folder that can't be made private isn't used
		std::filesystem::permissions(path_, std::filesystem::perms::owner_all, error);
		if(error) {
			std::filesystem::remove(path_, error);
			path_.clear();
		}
	}
	/// @brief Removes the folder and what is in it.
	~ScratchFolder() {
		std::error_code error; // nothing to do about a folder that can't be removed
		if(!path_.empty()) std::filesystem::remove_all(path_, error);
	}
	ScratchFolder(const ScratchFolder&) = delete;
	ScratchFolder& operator=(const ScratchFolder&) = delete;
	ScratchFolder(ScratchFolder&&) = delete;
	ScratchFolder& operator=(ScratchFolder&&) = delete;

	/// @brief The folder, or an empty path if it couldn't be created.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< See path().
};

} // namespace affresco::detail
