/// @file
/// @brief format::editing: the "image" step's file operations: newImage, open, save and revert. Files are read and
/// written only where the person chooses in the window's file dialogs.

#include "ImageOperations.hpp"

#include "affresco/imaging/Codec.hpp"
#include "affresco/imaging/Composite.hpp"
#include "affresco/imaging/Project.hpp"
#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <utility>

namespace affresco::format::editing {

namespace {

/// @brief The longest file extension a document names.
constexpr std::size_t maxExtensionLength = 16;

/// @brief Reads a file the size limits allow.
/// @param path Its path.
/// @param limit The most bytes.
/// @return Its bytes.
/// @throws DocumentError When it can't be read or is too big.
[[nodiscard]] std::vector<std::uint8_t> readFile(const std::string& path, std::size_t limit) {
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if(error) throw DocumentError("Can't read " + std::filesystem::path(path).filename().string());
	if(size > limit) throw DocumentError("The file is larger than " + std::to_string(limit >> 20) + " MB");
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	std::ifstream file(path, std::ios::binary);
	file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if(!file) throw DocumentError("Can't read " + std::filesystem::path(path).filename().string());
	return bytes;
}

/// @brief A document of one layer, "Background", holding a decoded picture.
/// @param decoded The picture.
/// @param limits The document's bounds.
/// @return The document, with no history.
[[nodiscard]] std::unique_ptr<imaging::Document> documentOf(const imaging::DecodedImage& decoded,
                                                           const imaging::Limits& limits) {
	const imaging::Image& pixels = *decoded.image;
	auto document = std::make_unique<imaging::Document>(pixels.width(), pixels.height(), limits, pixels.format());
	const std::uint64_t blank = document->structure().active;
	document->addImageLayer("Background", decoded.image);
	document->deleteLayer(blank);
	document->clearHistory();
	return document;
}

/// @brief Opens a project, PNG or JPEG from its bytes as the document, with its working space.
/// @param image The image.
/// @param bytes The file.
/// @throws DocumentError When it is malformed or beyond the limits.
void openBytes(ImageDocument& image, std::vector<std::uint8_t> bytes) {
	const auto file = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
	std::unique_ptr<imaging::Document> document;
	std::optional<imaging::ColorProfile> profile;
	if(imaging::isProject(*file)) {
		imaging::LoadedProject project = imaging::loadProject(file, image.limits);
		document = std::move(project.document);
		profile = std::move(project.profile);
		image.message.clear();
	} else {
		imaging::DecodedImage decoded = imaging::decodeImageFile(*file, image.limits);
		document = documentOf(decoded, image.limits);
		profile = std::move(decoded.profile);
		image.message = decoded.warning;
	}
	image.working = profile ? *profile : imaging::ColorProfile::srgb();
	replaceDocument(image, std::move(document));
	updateDisplay(image);
}

/// @brief The flattened document as one rgba8 image; over the background colour for a format without alpha.
/// @param image The image.
/// @param opaque Whether to composite onto the background colour.
/// @return The flattened image.
[[nodiscard]] imaging::Image flattened(const ImageDocument& image, bool opaque) {
	const imaging::Document& document = image.canvas->document();
	imaging::Image flat(document.width(), document.height(), imaging::PixelFormat::rgba8, image.limits);
	const imaging::Pixel under = image.settings.background;
	for(int row = 0; row < flat.rows(); ++row)
		for(int column = 0; column < flat.columns(); ++column) {
			auto pixels = imaging::flatten(document, 0, column * imaging::tileSize, row * imaging::tileSize,
			                               imaging::tileSize, imaging::tileSize);
			for(std::size_t index = 0; opaque && index < pixels.size(); index += 4) {
				const float alpha = imaging::unitFromByte(pixels[index + 3]);
				const std::array<float, 3> background{under.red, under.green, under.blue};
				for(std::size_t channel = 0; channel < 3; ++channel)
					pixels[index + channel] = imaging::byteFromUnit(
					    imaging::unitFromByte(pixels[index + channel]) * alpha + background[channel] * (1.0f - alpha));
				pixels[index + 3] = 255;
			}
			flat.setTile(column, row, std::make_shared<imaging::Tile>(imaging::PixelFormat::rgba8, std::move(pixels)));
		}
	return flat;
}

/// @brief A file extension argument: letters and digits, at most maxExtensionLength.
/// @param with The arguments.
/// @param key Its key.
/// @param fallback When absent.
/// @return It.
/// @throws DocumentError If it has other characters or is too long.
[[nodiscard]] std::string extensionOf(const json& with, std::string_view key, std::string_view fallback) {
	std::string extension = textOf(with, key, fallback);
	if(extension.empty() || extension.size() > maxExtensionLength ||
	   !std::ranges::all_of(extension, [](unsigned char c) { return std::isalnum(c) != 0; }))
		throw DocumentError("An extension is 1 to 16 letters or digits");
	return extension;
}

/// @brief newImage: a new document of one layer filled with the background colour (or transparent).
/// @param image The image.
/// @param with width, height (the definition's by default), format ("rgba8" or "rgba16f"), background (a colour or
/// "transparent"; the background colour by default).
/// @throws DocumentError If the size is beyond the limits or the colour malformed.
void newImage(ImageDocument& image, const json& with, Window*) {
	const int width = wholeOf(with, "width", image.definition.width);
	const int height = wholeOf(with, "height", image.definition.height);
	if(width < 1 || height < 1 || width > image.limits.side || height > image.limits.side ||
	   static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > image.limits.pixels)
		throw DocumentError("A document is from 1 × 1 to " + std::to_string(image.limits.side) + " pixels a side");
	const std::string format = textOf(with, "format", "rgba8");
	if(format != "rgba8" && format != "rgba16f") throw DocumentError("Unknown format " + format);
	imaging::Pixel fill = image.settings.background;
	if(const std::string background = textOf(with, "background"); background == "transparent")
		fill = {0.0f, 0.0f, 0.0f, 0.0f};
	else if(!background.empty()) {
		const auto colour = colorOf(background);
		if(!colour) throw DocumentError("Not a colour");
		fill = *colour;
	}
	settle(image);
	image.working = imaging::ColorProfile::srgb();
	image.saved.clear();
	image.message.clear();
	replaceDocument(image,
	                blankDocument(width, height,
	                              format == "rgba8" ? imaging::PixelFormat::rgba8 : imaging::PixelFormat::rgba16f, fill,
	                              image.limits));
	updateDisplay(image);
}

/// @brief open: opens a file the open dialog chooses (a cancel changes nothing).
/// @param image The image.
/// @param with projectExtension (the project files' extension, "project" by default).
/// @param window Shows the dialog; opening needs one.
/// @throws DocumentError If there is no window, or the file can't be read or is malformed.
void openFile(ImageDocument& image, const json& with, Window* window) {
	if(window == nullptr) throw DocumentError("Opening needs a window");
	const std::string project = extensionOf(with, "projectExtension", "project");
	const auto chosen =
	    window->chooseFiles({FileDialogKind::Open, "Open", {{"Images", {project, "png", "jpg", "jpeg"}}}, "", "", false});
	if(chosen.empty()) return;
	settle(image);
	const std::size_t limit = std::max(image.limits.imageFileBytes, image.limits.projectFileBytes);
	auto bytes = readFile(chosen.front(), limit);
	image.saved = bytes;
	openBytes(image, std::move(bytes));
}

/// @brief save: encodes the document, keeps the bytes for revert, and writes them where the save dialog says when
/// there is a window (a cancel writes nothing).
/// @param image The image.
/// @param with format ("project", "png" or "jpeg"; "project" by default), extension (the file's; "project", "png" or
/// "jpg" by default).
/// @param window Shows the dialog; without one the bytes are only kept.
/// @throws DocumentError If the format is unknown or the file can't be written.
void saveFile(ImageDocument& image, const json& with, Window* window) {
	settle(image);
	const std::string format = textOf(with, "format", "project");
	const std::optional<imaging::ColorProfile> profile =
	    image.working == imaging::ColorProfile::srgb() ? std::nullopt : std::optional(image.working);
	std::vector<std::uint8_t> bytes;
	if(format == "png" || format == "jpeg")
		bytes = imaging::encodeImageFile(
		    flattened(image, format == "jpeg"),
		    format == "png" ? imaging::ImageFileFormat::png : imaging::ImageFileFormat::jpeg, profile);
	else if(format == "project") bytes = imaging::saveProject(image.canvas->document(), profile);
	else throw DocumentError("Unknown format " + format);
	const std::string extension = extensionOf(with, "extension", format == "jpeg" ? "jpg" : format);
	image.message = "Saved " + std::to_string(bytes.size() >> 10) + " KB";
	if(window != nullptr) {
		const auto chosen =
		    window->chooseFiles({FileDialogKind::Save, "Save", {{format, {extension}}}, "", "Untitled." + extension, false});
		if(chosen.empty()) return;
		std::ofstream file(chosen.front(), std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		if(!file) throw DocumentError("Can't write the file");
	}
	image.saved = std::move(bytes);
}

/// @brief revert: opens again the bytes last saved or opened.
/// @param image The image.
/// @throws DocumentError If nothing was saved or opened.
void revert(ImageDocument& image, const json&, Window*) {
	if(image.saved.empty()) throw DocumentError("Nothing saved yet");
	settle(image);
	openBytes(image, image.saved);
}

} // namespace

void openAsset(ImageDocument& image, std::string_view type, const std::string& bytes) {
	settle(image);
	const std::span<const std::uint8_t> file(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
	if(type != "image/svg+xml") {
		std::vector<std::uint8_t> copy(file.begin(), file.end());
		image.saved = copy;
		openBytes(image, std::move(copy));
		return;
	}
	// An SVG is rasterised by the render module, then opened as the PNG of its pixels (which revert reads)
	std::shared_ptr<const Bitmap> bitmap;
	try {
		bitmap = Bitmap::decode(file, {});
	} catch(const std::exception& error) {
		throw DocumentError(std::string("The SVG can't be drawn: ") + error.what());
	}
	std::vector<std::uint8_t> rgba(bitmap->pixels().begin(), bitmap->pixels().end());
	for(std::size_t i = 0; i + 3 < rgba.size(); i += 4) // straight alpha, as imaging keeps it
		if(const unsigned alpha = rgba[i + 3]; alpha != 0 && alpha != 255)
			for(std::size_t c = 0; c < 3; ++c)
				rgba[i + c] = static_cast<std::uint8_t>(std::min(255U, (rgba[i + c] * 255U + alpha / 2) / alpha));
	imaging::DecodedImage decoded = imaging::imageFromRgba(bitmap->width(), bitmap->height(), rgba, image.limits);
	image.saved = imaging::encodeImageFile(*decoded.image, imaging::ImageFileFormat::png, std::nullopt);
	image.working = imaging::ColorProfile::srgb();
	image.message.clear();
	replaceDocument(image, documentOf(decoded, image.limits));
	updateDisplay(image);
}

std::vector<ImageOperation> fileOperations() {
	return {
	    {"newImage", {"width", "height", "format", "background"}, "Replaces the image with a new one of one layer.",
	     newImage},
	    {"open",
	     {"projectExtension", "asset"},
	     "Opens a project, PNG or JPEG file the person chooses, or an asset (PNG, JPEG or SVG) the screen has.",
	     openFile},
	    {"save", {"format", "extension"}, "Saves the image where the person chooses, and keeps it for revert.",
	     saveFile},
	    {"revert", {}, "Opens again what was last saved or opened.", revert},
	};
}

} // namespace affresco::format::editing
