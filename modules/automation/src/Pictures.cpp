/// @file
/// @brief detail::takePicture: golden images, pixel for pixel, with a picture of what differs.

#include "Pictures.hpp"

#include "Png.hpp" // the screen module's PNG encoder and PPM reader
#include "PngReader.hpp"

#include "affresco/render/Window.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

namespace affresco::automation::detail {

namespace {

/// @brief Reads a whole file as bytes.
/// @param path The file.
/// @return Its bytes, or std::nullopt if it can't be read.
std::optional<std::vector<std::uint8_t>> readBytes(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if(!file) return std::nullopt;
	return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

/// @brief Writes a picture as a PNG file.
/// @param path The file.
/// @param width In pixels.
/// @param height In pixels.
/// @param rgb width * height * 3 bytes.
/// @return Whether it was written.
bool writePng(const std::filesystem::path& path, int width, int height, std::span<const std::uint8_t> rgb) {
	const std::string png = affresco::detail::encodePng(width, height, rgb);
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(png.data(), static_cast<std::streamsize>(png.size()));
	return !png.empty() && static_cast<bool>(file);
}

/// @brief Compares a frame with its golden image, pixel by pixel, and draws what differs: the golden image faded to
/// grey, the pixels that differ in red.
/// @param frame What was drawn.
/// @param golden The golden image, of the same size.
/// @param diff Receives the picture of what differs (left empty when nothing does).
/// @return How many pixels differ.
std::size_t comparePixels(std::span<const std::uint8_t> frame, std::span<const std::uint8_t> golden,
                          std::vector<std::uint8_t>& diff) {
	if(std::ranges::equal(frame, golden)) return 0;
	std::size_t changed = 0;
	diff.resize(golden.size());
	for(std::size_t i = 0; i + 2 < golden.size(); i += 3) {
		const bool same = frame[i] == golden[i] && frame[i + 1] == golden[i + 1] && frame[i + 2] == golden[i + 2];
		changed += same ? 0 : 1;
		// Faded, so the red stands out: the same formula as tests/render/check.py always used
		const auto grey = static_cast<std::uint8_t>((golden[i] + golden[i + 1] + golden[i + 2]) / 6 + 128);
		diff[i] = same ? grey : 255;
		diff[i + 1] = same ? grey : 0;
		diff[i + 2] = same ? grey : 0;
	}
	return changed;
}

/// @brief Compares a frame with its golden image, which exists.
/// @param frame The frame.
/// @param picture Filled in: the outcome, and the problem when it differs.
/// @param golden The golden image's file.
/// @param options Where frames that differ go.
void compareWithGolden(const affresco::detail::RgbImage& frame, Picture& picture, const std::filesystem::path& golden,
                       const Options& options) {
	picture.outcome = Picture::Outcome::Differs;
	const std::optional<std::vector<std::uint8_t>> file = readBytes(golden);
	std::string error = "it can't be read";
	const std::optional<Pixels> expected = file ? decodePng(*file, error) : std::nullopt;
	if(!expected) {
		picture.problem = "golden image " + golden.string() + ": " + error;
		return;
	}
	if(expected->width != frame.width || expected->height != frame.height) {
		picture.problem = std::to_string(frame.width) + "x" + std::to_string(frame.height) + ", golden is " +
		                  std::to_string(expected->width) + "x" + std::to_string(expected->height) +
		                  " (another display scale?)";
		return;
	}
	std::vector<std::uint8_t> diff;
	picture.changedPixels = comparePixels(frame.rgb, expected->rgb, diff);
	if(picture.changedPixels == 0) {
		picture.outcome = Picture::Outcome::Same;
		return;
	}
	picture.problem = std::to_string(picture.changedPixels) + " pixels differ";
	if(options.output.empty()) return;
	const std::filesystem::path base = options.output / picture.name;
	if(writePng(base.string() + ".png", frame.width, frame.height, frame.rgb) &&
	   writePng(base.string() + ".diff.png", frame.width, frame.height, diff))
		picture.problem += " (see " + base.string() + ".diff.png)";
}

} // namespace

Picture takePicture(Window& window, int scale, const std::string& name, PictureKind kind, const Options& options,
                    const std::filesystem::path& scratch) {
	Picture picture{name, Picture::Outcome::Differs, 0, {}};
	const std::filesystem::path drawn = scratch / "frame.ppm";
	std::optional<affresco::detail::RgbImage> frame;
	if(window.saveScreenshot(drawn, scale)) frame = affresco::detail::readPpm(drawn);
	if(!frame) {
		picture.problem = "could not draw the window";
		return picture;
	}

	if(kind == PictureKind::Capture) {
		if(options.output.empty()) picture.problem = "a capture needs a folder to write to (--out)";
		else if(!writePng(options.output / (name + ".png"), frame->width, frame->height, frame->rgb))
			picture.problem = "could not write " + (options.output / (name + ".png")).string();
		else picture.outcome = Picture::Outcome::Saved;
		return picture;
	}

	if(options.goldens.empty()) {
		picture.problem = "no golden images folder to compare with (--goldens)";
		return picture;
	}
	const std::filesystem::path golden = options.goldens / (name + ".png");
	const bool record = options.policy == Goldens::Update ||
	                    (options.policy == Goldens::RecordMissing && !std::filesystem::exists(golden));
	if(record) {
		if(writePng(golden, frame->width, frame->height, frame->rgb)) picture.outcome = Picture::Outcome::Recorded;
		else picture.problem = "could not write " + golden.string();
		return picture;
	}
	if(!std::filesystem::exists(golden)) {
		picture.problem = "no golden image " + golden.string() + " (record it with --update)";
		return picture;
	}
	compareWithGolden(*frame, picture, golden, options);
	return picture;
}

} // namespace affresco::automation::detail
