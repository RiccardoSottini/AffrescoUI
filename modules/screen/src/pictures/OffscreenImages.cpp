/// @file
/// @brief detail::OffscreenImages: pictures of a screen, drawn offscreen by its window.

#include "pictures/OffscreenImages.hpp"
#include "Png.hpp"
#include "ScratchFolder.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace affresco::detail {

/// @brief Shows what was shown before a picture, then lets the runtime run handlers again: when the picture is drawn,
/// or when drawing it fails or throws.
class OffscreenImages::PutBack {
public:
	/// @brief Remembers what the person sees.
	/// @param owner Whose state to put back.
	/// @param previewed The proposal previewed now, if any.
	PutBack(OffscreenImages& owner, std::optional<int> previewed) : images(owner), shown(previewed) {}
	PutBack(const PutBack&) = delete;
	PutBack& operator=(const PutBack&) = delete;
	PutBack(PutBack&&) = delete;
	PutBack& operator=(PutBack&&) = delete;
	/// @brief Shows what was shown before, then lets the runtime run handlers again. Never throws: an exception
	/// leaving a destructor ends the program.
	~PutBack() {
		try {
			showAgain();
		} catch(...) { // no memory even for the message
			log(LogLevel::Error, "AffrescoUI screen: render: could not put the screen back");
		}
		images.runtime_.setHandlersSuspended(false);
		images.rendering_ = false;
	}

	OffscreenImages& images;  ///< Whose state to put back.
	std::optional<int> shown; ///< The proposal previewed before, if any.
	bool switched = false;    ///< Whether something else was shown.

private:
	/// @brief Shows the proposal that was shown before, or main, and logs what fails.
	void showAgain() const {
		if(!switched) return;
		try {
			if(const std::optional<std::string> error = images.show(shown))
				log(LogLevel::Warning, "AffrescoUI screen: render: could not show the preview again: " + *error);
		} catch(const std::exception& failure) { // e.g. main's screen failing to rebuild
			log(LogLevel::Error,
			    std::string("AffrescoUI screen: render: could not put the screen back: ") + failure.what());
		}
	}
};

std::optional<std::string> OffscreenImages::render(const ImageRequest& request, Image& image) {
	if(!runtime_.window()) return "AffrescoUI screen: render: the screen has no window yet";
	if(rendering_) return "AffrescoUI screen: render: already drawing a picture";
	// Checked before switching: putting the person's preview back would reset what they changed in it
	if(request.proposal && std::ranges::none_of(versions_.proposals(), [&](const ProposalInfo& proposal) {
		   return proposal.id == *request.proposal;
	   }))
		return "AffrescoUI screen: no proposal " + std::to_string(*request.proposal);

	// What main deferred runs now, on main: once the proposal is shown, it would run there and be lost with it
	runtime_.window()->runDeferredTasks();

	// Show what was asked for, then put back what the person sees, whatever happens in between
	PutBack putBack{*this, versions_.previewing()};

	rendering_ = true;
	runtime_.setHandlersSuspended(true); // until PutBack has put the screen back
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
	RgbImage pixels;
	if(std::optional<std::string> error = drawPixels(scale, pixels)) return error;
	image.png = encodePng(pixels.width, pixels.height, pixels.rgb);
	image.width = pixels.width;
	image.height = pixels.height;
	return std::nullopt;
}

std::optional<std::string> OffscreenImages::drawPixels(int scale, RgbImage& pixels) const {
	// saveScreenshot writes a file: the same path the rendering check takes, in a folder of our own
	const ScratchFolder folder;
	if(folder.path().empty()) return "AffrescoUI screen: render: could not create a temporary folder";
	const std::filesystem::path file = folder.path() / "screen.ppm";
	runtime_.waitForImages(); // a picture shows every image, not those decoded so far
	inspector_.setVisible(false); // a picture shows the screen, not the overlay
	const bool drawn = runtime_.window()->saveScreenshot(file, scale);
	inspector_.setVisible(true);
	if(!drawn) return "AffrescoUI screen: render: could not draw the screen";

	std::optional<RgbImage> read = readPpm(file);
	if(!read) return "AffrescoUI screen: render: could not read the drawn screen back";
	pixels = std::move(*read);
	return std::nullopt;
}

std::optional<std::string> OffscreenImages::drawBoth(int proposal, int scale, RgbImage& before, RgbImage& after,
                                                     json& main, std::string& document) {
	if(!runtime_.window()) return "AffrescoUI screen: evidence: the screen has no window yet";
	if(rendering_) return "AffrescoUI screen: evidence: already drawing a picture";
	runtime_.window()->runDeferredTasks(); // as render(): main's tasks run on main
	PutBack putBack{*this, versions_.previewing()};
	rendering_ = true;
	runtime_.setHandlersSuspended(true);
	putBack.switched = true;
	if(std::optional<std::string> error = show(std::nullopt)) return error;
	if(std::optional<std::string> error = drawPixels(scale, before)) return error;
	main = versions_.document();
	if(std::optional<std::string> error = show(proposal)) return error;
	document = versions_.document().dump(); // while previewed, the document is the proposal's
	return drawPixels(scale, after);
}

} // namespace affresco::detail
