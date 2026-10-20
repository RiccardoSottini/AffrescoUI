#pragma once

/// @file
/// @brief ImageView: a panel that shows an image, fitted to it.

#include "affresco/render/Bitmap.hpp"
#include "affresco/render/ImageCache.hpp"
#include "affresco/render/Panel.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace affresco {

/// @brief How an image fills its panel, like CSS object-fit.
enum class ImageFit : std::uint8_t {
	contain, ///< All of it, as large as fits, centred: the rest of the panel shows its colour.
	cover,   ///< The whole panel, as small as covers it, centred: the image's edges are cut off.
	fill,    ///< The whole panel, stretched.
};

/// @brief A panel that shows an image: one given to it, a file's (decoded on an ImageCache's worker thread, shown
/// when it arrives), or one the host names (shown again each time the host changes it).
///
/// Transparent by default. Until a file's image arrives, the image shown before stays; a file that can't be decoded
/// shows nothing. Main thread only, like every panel; what it asked of a cache is cancelled when it is destroyed.
class ImageView : public Panel {
public:
	/// @brief An empty view.
	ImageView();

	/// @brief Shows an image, and forgets the file or name asked for before.
	/// @param bitmap The image, or nullptr for none.
	void setBitmap(const std::shared_ptr<const Bitmap>& bitmap);
	/// @brief The image shown, or nullptr.
	[[nodiscard]] const std::shared_ptr<const Bitmap>& bitmap() const noexcept { return bitmap_; }
	/// @brief Shows a file's image, once the cache has it. Asking for the file shown already does nothing.
	/// @param cache The cache; it may be destroyed before the view.
	/// @param file The file.
	void showFile(ImageCache& cache, const std::filesystem::path& file);
	/// @brief Shows an image the host names, and follows it as the host changes it.
	/// @param cache The cache; it may be destroyed before the view.
	/// @param name The name.
	void showNamed(ImageCache& cache, const std::string& name);

	/// @brief Sets how the image fills the panel (default: contain).
	/// @param fit The fit.
	void setFit(ImageFit fit);
	/// @brief How the image fills the panel.
	[[nodiscard]] ImageFit fit() const noexcept { return fit_; }

	/// @brief Where an image is drawn in a frame: for cover, past the frame (the panel clips it).
	/// @param fit The fit.
	/// @param frame The panel's frame.
	/// @param width The image's width, in pixels.
	/// @param height The image's height, in pixels.
	/// @return The image's rectangle.
	[[nodiscard]] static Rect fitted(ImageFit fit, const Rect& frame, int width, int height) noexcept;

protected:
	void paint(Painter& painter) const override;

private:
	/// @brief Shows what a request delivered.
	/// @param bitmap The image, or nullptr.
	void deliver(const std::shared_ptr<const Bitmap>& bitmap);

	std::shared_ptr<const Bitmap> bitmap_; ///< See bitmap().
	ImageFit fit_ = ImageFit::contain;     ///< See fit().
	std::string source_;                   ///< What was asked of a cache ("file:..." or "name:..."), or empty.
	ImageCache::Request request_;          ///< Declared last: cancelled first, while the view is whole.
};

} // namespace affresco
