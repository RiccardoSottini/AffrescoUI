#pragma once

/// @file
/// @brief Colour management through Little CMS 2: ICC profiles (built in, or parsed from bytes under limits), the
/// conversions between them, soft proofing, and the 3D table the GPU converts the canvas to the display through.

#include "affresco/imaging/Limits.hpp"
#include "affresco/imaging/Pixels.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace affresco::imaging {

/// @brief What a profile's colours are made of.
enum class ColorModel : std::uint8_t {
	rgb,  ///< Red, green and blue: a working space or a display.
	gray, ///< One channel.
	cmyk, ///< Inks: only proofed against, never edited in.
};

/// @brief How colours out of the destination's gamut are brought in.
enum class RenderingIntent : std::uint8_t {
	perceptual,           ///< Everything squeezed together, keeping relations.
	relativeColorimetric, ///< In-gamut colours kept, white points matched, the rest clipped.
	saturation,           ///< Vividness kept, for charts.
	absoluteColorimetric, ///< In-gamut colours kept exactly, the paper's white simulated.
};

/// @brief An ICC profile, immutable and shared between copies.
class ColorProfile {
public:
	/// @brief sRGB, the working space and the display's when nothing says otherwise.
	[[nodiscard]] static ColorProfile srgb();
	/// @brief Display P3: P3 primaries, D65, the sRGB curve.
	[[nodiscard]] static ColorProfile displayP3();
	/// @brief Adobe RGB (1998)-compatible: its primaries, D65, gamma 2.2 (built from numbers, named "Wide RGB").
	[[nodiscard]] static ColorProfile wideRgb();
	/// @brief Grey with the sRGB curve.
	[[nodiscard]] static ColorProfile gray();
	/// @brief A profile from its bytes, as embedded in a PNG, a JPEG or a project.
	/// @param bytes The ICC data.
	/// @param limits limits.profileBytes bounds it.
	/// @throws DocumentError when it is too big, malformed, or not an RGB, grey or CMYK input, display or output
	/// profile.
	[[nodiscard]] static ColorProfile parse(std::span<const std::uint8_t> bytes, const Limits& limits);

	/// @brief Its description, as the status bar shows it ("sRGB IEC61966-2.1", …), at most 255 bytes.
	[[nodiscard]] const std::string& name() const noexcept { return state_->name; }
	/// @brief What its colours are made of.
	[[nodiscard]] ColorModel model() const noexcept { return state_->model; }
	/// @brief Its bytes, to embed in a file.
	[[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return state_->bytes; }
	/// @brief Little CMS's handle (cmsHPROFILE), for transforms.
	[[nodiscard]] void* handle() const noexcept { return state_->handle.get(); }

	/// @brief Whether two profiles have the same bytes.
	[[nodiscard]] friend bool operator==(const ColorProfile& left, const ColorProfile& right) noexcept {
		return left.state_ == right.state_ || left.state_->bytes == right.state_->bytes;
	}

private:
	/// @brief What copies share.
	struct State {
		std::unique_ptr<void, void (*)(void*)> handle{nullptr, nullptr}; ///< The open profile.
		std::string name;                                                ///< Its description.
		ColorModel model = ColorModel::rgb;                              ///< What it's made of.
		std::vector<std::uint8_t> bytes;                                 ///< Its ICC data.
	};

	/// @brief Takes an open profile.
	/// @param handle The cmsHPROFILE, owned from now on.
	/// @throws DocumentError when it can't be used.
	explicit ColorProfile(void* handle);

	std::shared_ptr<const State> state_; ///< The profile.
};

/// @brief A conversion of RGBA float pixels from one profile to another, alpha untouched; optionally proofing a third.
class ColorTransform {
public:
	/// @brief A conversion.
	/// @param from The pixels' profile (RGB or grey).
	/// @param to The destination's (RGB or grey).
	/// @param intent How out-of-gamut colours come in.
	/// @param proof Soft proofing: show on `to` how the pixels would print or show on this one (any model).
	/// @throws DocumentError when Little CMS can't build it.
	ColorTransform(const ColorProfile& from, const ColorProfile& to, RenderingIntent intent,
	               const std::optional<ColorProfile>& proof = std::nullopt);
	/// @brief Releases the conversion.
	~ColorTransform();
	ColorTransform(const ColorTransform&) = delete;
	ColorTransform& operator=(const ColorTransform&) = delete;
	/// @brief Moves a conversion.
	ColorTransform(ColorTransform&&) noexcept;
	/// @brief Moves a conversion.
	ColorTransform& operator=(ColorTransform&&) noexcept;

	/// @brief Converts pixels.
	/// @param pixels Read and written in place; as many as you like (no allocation).
	void apply(std::span<Pixel> pixels) const noexcept;
	/// @brief Converts one pixel.
	/// @param pixel The pixel.
	[[nodiscard]] Pixel apply(Pixel pixel) const noexcept;
	/// @brief The conversion sampled on a cube, for the GPU: red fastest, then green, then blue; three floats each.
	/// @param side Points per edge, 2 to 65.
	[[nodiscard]] std::vector<float> table(int side) const;

private:
	/// @brief Frees Little CMS transforms.
	struct Release {
		/// @brief Deletes one.
		void operator()(void* handle) const noexcept;
	};

	std::unique_ptr<void, Release> handle_; ///< The cmsHTRANSFORM, in place on RGBA floats.
	bool toGray_ = false;                   ///< Whether the destination is grey (its red copied to green and blue).
};

/// @brief The largest side of ColorTransform::table().
constexpr int colorTableSide = 65;

} // namespace affresco::imaging
