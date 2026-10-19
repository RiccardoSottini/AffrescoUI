/// @file
/// @brief detail::OffscreenImages: pictures of a screen, drawn offscreen by its window.

#include "OffscreenImages.hpp"
#include "Png.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Window.hpp"

#include <filesystem>
#include <random>
#include <utility>

namespace affresco::detail {

namespace {

/// @brief A new folder only this user can open, for one picture's file; removed with everything in it.
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
		if(!path_.empty()) std::filesystem::permissions(path_, std::filesystem::perms::owner_all, error);
	}
	~ScratchFolder() {
		std::error_code error; // nothing to do about a folder that can't be removed
		if(!path_.empty()) std::filesystem::remove_all(path_, error);
	}
	ScratchFolder(const ScratchFolder&) = delete;
	ScratchFolder& operator=(const ScratchFolder&) = delete;

	/// @brief The folder, or an empty path if it couldn't be created.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< See path().
};

} // namespace

std::optional<std::string> OffscreenImages::render(const ImageRequest& request, Image& image) {
	if(!runtime_.window()) return "AffrescoUI screen: render: the screen has no window yet";
	if(rendering_) return "AffrescoUI screen: render: already drawing a picture";

	// Show what was asked for, then put back what the person sees, whatever happens in between
	struct PutBack {
		OffscreenImages& images;  ///< Whose state to put back.
		std::optional<int> shown; ///< The proposal previewed before, if any.
		bool switched = false;    ///< Whether something else was shown.
		~PutBack() {
			if(switched)
				if(const std::optional<std::string> error = images.show(shown))
					log(LogLevel::Warning, "AffrescoUI screen: render: could not show the preview again: " + *error);
			images.rendering_ = false;
		}
	} putBack{*this, versions_.previewing()};

	rendering_ = true;
	if(request.proposal != putBack.shown) {
		putBack.switched = true;
		if(const std::optional<std::string> error = show(request.proposal)) return error;
	}
	return draw(request.scale, image);
}

std::optional<std::string> OffscreenImages::show(std::optional<int> proposal) {
	if(proposal) return versions_.preview(*proposal);
	versions_.endPreview();
	return std::nullopt;
}

std::optional<std::string> OffscreenImages::draw(int scale, Image& image) const {
	// saveScreenshot writes a file: the same path the rendering check takes, in a folder of our own
	const ScratchFolder folder;
	if(folder.path().empty()) return "AffrescoUI screen: render: could not create a temporary folder";
	const std::filesystem::path file = folder.path() / "screen.ppm";
	if(!runtime_.window()->saveScreenshot(file, scale)) return "AffrescoUI screen: render: could not draw the screen";

	const std::optional<RgbImage> pixels = readPpm(file);
	if(!pixels) return "AffrescoUI screen: render: could not read the drawn screen back";
	image.png = encodePng(pixels->width, pixels->height, pixels->rgb);
	image.width = pixels->width;
	image.height = pixels->height;
	return std::nullopt;
}

} // namespace affresco::detail
