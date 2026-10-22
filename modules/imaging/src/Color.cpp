/// @file
/// @brief Colour management through Little CMS 2, on its default context (thread-safe for transforms): profiles are
/// opened from memory, checked before use, and every transform works in place on RGBA floats, so alpha is never
/// touched.

#include "affresco/imaging/Color.hpp"

#include "affresco/core/Json.hpp"

#include <lcms2.h>

#include <algorithm>
#include <array>

namespace affresco::imaging {

namespace {

/// @brief A grey pixel with three channels after it (green, blue, alpha), as floats: a grey profile's RGBA layout.
constexpr cmsUInt32Number grayWithExtras =
    FLOAT_SH(1) | COLORSPACE_SH(PT_GRAY) | EXTRA_SH(3) | CHANNELS_SH(1) | BYTES_SH(4);

/// @brief The CIE xyY of D65, the white of sRGB and P3.
constexpr cmsCIExyY d65{0.3127, 0.3290, 1.0};

/// @brief Builds a curve of the sRGB transfer function (IEC 61966-2-1).
[[nodiscard]] cmsToneCurve* srgbCurve() noexcept {
	const std::array<cmsFloat64Number, 5> parameters{2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045};
	return cmsBuildParametricToneCurve(nullptr, 4, parameters.data());
}

/// @brief Names a profile built here.
/// @param profile The profile.
/// @param name Its description.
void describe(cmsHPROFILE profile, const char* name) noexcept {
	cmsMLU* text = cmsMLUalloc(nullptr, 1);
	cmsMLUsetASCII(text, "en", "US", name);
	cmsWriteTag(profile, cmsSigProfileDescriptionTag, text);
	cmsMLUfree(text);
}

/// @brief An RGB profile from primaries and a curve for all three channels.
/// @param primaries Red, green and blue, xyY.
/// @param curve The transfer function, freed here.
/// @param name Its description.
[[nodiscard]] cmsHPROFILE rgbProfile(const cmsCIExyYTRIPLE& primaries, cmsToneCurve* curve, const char* name) noexcept {
	const std::array<cmsToneCurve*, 3> curves{curve, curve, curve};
	cmsHPROFILE profile = cmsCreateRGBProfile(&d65, &primaries, curves.data());
	cmsFreeToneCurve(curve);
	if(profile != nullptr) describe(profile, name);
	return profile;
}

/// @brief What a profile's colour space is, or nothing for the unsupported.
/// @param profile The profile.
[[nodiscard]] std::optional<ColorModel> modelOf(cmsHPROFILE profile) noexcept {
	switch(cmsGetColorSpace(profile)) {
	case cmsSigRgbData:
		return ColorModel::rgb;
	case cmsSigGrayData:
		return ColorModel::gray;
	case cmsSigCmykData:
		return ColorModel::cmyk;
	default:
		return std::nullopt;
	}
}

/// @brief The pixel layout Little CMS reads or writes for a model.
/// @param model RGB or grey.
[[nodiscard]] cmsUInt32Number layoutOf(ColorModel model) noexcept {
	return model == ColorModel::gray ? grayWithExtras : TYPE_RGBA_FLT;
}

/// @brief Little CMS's number for an intent.
/// @param intent The intent.
[[nodiscard]] cmsUInt32Number intentOf(RenderingIntent intent) noexcept {
	switch(intent) {
	case RenderingIntent::perceptual:
		return INTENT_PERCEPTUAL;
	case RenderingIntent::relativeColorimetric:
		return INTENT_RELATIVE_COLORIMETRIC;
	case RenderingIntent::saturation:
		return INTENT_SATURATION;
	case RenderingIntent::absoluteColorimetric:
		return INTENT_ABSOLUTE_COLORIMETRIC;
	}
	return INTENT_PERCEPTUAL;
}

/// @brief Whether a profile's class and connection space are ones we convert through.
/// @param profile The profile.
[[nodiscard]] bool usable(cmsHPROFILE profile) noexcept {
	const cmsProfileClassSignature kind = cmsGetDeviceClass(profile);
	const bool device = kind == cmsSigInputClass || kind == cmsSigDisplayClass || kind == cmsSigOutputClass ||
	                    kind == cmsSigColorSpaceClass;
	const cmsColorSpaceSignature connection = cmsGetPCS(profile);
	return device && (connection == cmsSigXYZData || connection == cmsSigLabData) && modelOf(profile).has_value();
}

} // namespace

ColorProfile::ColorProfile(void* handle) {
	auto state = std::make_shared<State>();
	state->handle = {handle, [](void* profile) { cmsCloseProfile(profile); }};
	if(handle == nullptr) throw DocumentError("imaging: the colour profile could not be built");
	if(!usable(handle)) throw DocumentError("imaging: an unsupported colour profile (only RGB, grey and CMYK)");
	state->model = *modelOf(handle);
	std::array<char, 256> text{};
	if(cmsGetProfileInfoASCII(handle, cmsInfoDescription, "en", "US", text.data(), text.size()) > 0)
		state->name = text.data();
	if(state->name.empty()) state->name = "Untitled profile";
	cmsUInt32Number size = 0;
	if(!cmsSaveProfileToMem(handle, nullptr, &size) || size == 0)
		throw DocumentError("imaging: the colour profile could not be saved");
	state->bytes.resize(size);
	if(!cmsSaveProfileToMem(handle, state->bytes.data(), &size))
		throw DocumentError("imaging: the colour profile could not be saved");
	state_ = std::move(state);
}

ColorProfile ColorProfile::srgb() { return ColorProfile(cmsCreate_sRGBProfile()); }

ColorProfile ColorProfile::displayP3() {
	const cmsCIExyYTRIPLE primaries{{0.680, 0.320, 1.0}, {0.265, 0.690, 1.0}, {0.150, 0.060, 1.0}};
	return ColorProfile(rgbProfile(primaries, srgbCurve(), "Display P3"));
}

ColorProfile ColorProfile::wideRgb() {
	const cmsCIExyYTRIPLE primaries{{0.640, 0.330, 1.0}, {0.210, 0.710, 1.0}, {0.150, 0.060, 1.0}};
	return ColorProfile(rgbProfile(primaries, cmsBuildGamma(nullptr, 563.0 / 256.0), "Wide RGB"));
}

ColorProfile ColorProfile::gray() {
	cmsToneCurve* curve = srgbCurve();
	cmsHPROFILE profile = cmsCreateGrayProfile(cmsD50_xyY(), curve);
	cmsFreeToneCurve(curve);
	if(profile != nullptr) describe(profile, "Grey, sRGB curve");
	return ColorProfile(profile);
}

ColorProfile ColorProfile::parse(std::span<const std::uint8_t> bytes, const Limits& limits) {
	if(bytes.size() > limits.profileBytes) throw DocumentError("imaging: the colour profile is too big");
	if(bytes.size() < 132) throw DocumentError("imaging: the colour profile is too short");
	const std::uint32_t declared = static_cast<std::uint32_t>(bytes[0]) << 24 |
	                               static_cast<std::uint32_t>(bytes[1]) << 16 |
	                               static_cast<std::uint32_t>(bytes[2]) << 8 | bytes[3];
	if(declared > bytes.size()) throw DocumentError("imaging: the colour profile is cut short");
	cmsHPROFILE handle = cmsOpenProfileFromMem(bytes.data(), static_cast<cmsUInt32Number>(bytes.size()));
	if(handle == nullptr) throw DocumentError("imaging: a malformed colour profile");
	ColorProfile profile(handle);
	// Tags are read lazily: building a conversion reads the ones that matter, so a broken one fails here
	const ColorProfile reference = srgb();
	if(profile.model() == ColorModel::cmyk) ColorTransform(reference, reference, RenderingIntent::perceptual, profile);
	else ColorTransform(profile, reference, RenderingIntent::perceptual);
	return profile;
}

void ColorTransform::Release::operator()(void* handle) const noexcept { cmsDeleteTransform(handle); }

ColorTransform::ColorTransform(const ColorProfile& from, const ColorProfile& to, RenderingIntent intent,
                               const std::optional<ColorProfile>& proof)
    : toGray_(to.model() == ColorModel::gray) {
	if(from.model() == ColorModel::cmyk || to.model() == ColorModel::cmyk)
		throw DocumentError("imaging: CMYK profiles are only proofed against");
	const cmsUInt32Number input = layoutOf(from.model()), output = layoutOf(to.model());
	// In place, the extra channels (alpha; green and blue around grey) are never written, so alpha stays
	const cmsUInt32Number flags = proof ? cmsFLAGS_SOFTPROOFING : 0;
	void* handle = proof ? cmsCreateProofingTransform(from.handle(), input, to.handle(), output, proof->handle(),
	                                                  intentOf(intent), INTENT_RELATIVE_COLORIMETRIC, flags)
	                     : cmsCreateTransform(from.handle(), input, to.handle(), output, intentOf(intent), flags);
	if(handle == nullptr) throw DocumentError("imaging: the colour conversion could not be built");
	handle_.reset(handle);
}

ColorTransform::~ColorTransform() = default;
ColorTransform::ColorTransform(ColorTransform&&) noexcept = default;
ColorTransform& ColorTransform::operator=(ColorTransform&&) noexcept = default;

void ColorTransform::apply(std::span<Pixel> pixels) const noexcept {
	static_assert(sizeof(Pixel) == 4 * sizeof(float), "a Pixel is four packed floats");
	if(pixels.empty() || !handle_) return;
	cmsDoTransform(handle_.get(), pixels.data(), pixels.data(), static_cast<cmsUInt32Number>(pixels.size()));
	if(toGray_)
		for(Pixel& pixel : pixels) pixel.green = pixel.blue = pixel.red;
}

Pixel ColorTransform::apply(Pixel pixel) const noexcept {
	apply(std::span<Pixel>(&pixel, 1));
	return pixel;
}

std::vector<float> ColorTransform::table(int side) const {
	side = std::clamp(side, 2, colorTableSide);
	const auto edge = static_cast<std::size_t>(side);
	std::vector<Pixel> cube(edge * edge * edge);
	const float step = 1.0f / static_cast<float>(side - 1);
	for(std::size_t index = 0; index < cube.size(); ++index)
		cube[index] = {static_cast<float>(index % edge) * step, static_cast<float>(index / edge % edge) * step,
		               static_cast<float>(index / (edge * edge)) * step, 1.0f};
	apply(cube);
	std::vector<float> values;
	values.reserve(cube.size() * 3);
	for(const Pixel& pixel : cube) values.insert(values.end(), {pixel.red, pixel.green, pixel.blue});
	return values;
}

} // namespace affresco::imaging
