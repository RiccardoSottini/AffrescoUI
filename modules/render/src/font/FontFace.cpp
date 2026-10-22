/// @file
/// @brief detail::FontFiles and FontFace: reading font files once, and opening them with FreeType and HarfBuzz.

#include "font/FontFace.hpp"

#include FT_OUTLINE_H

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>

namespace affresco::detail {

namespace {

/// @brief Frees a HarfBuzz blob or face, for std::unique_ptr: only needed while a face is opened.
struct HarfBuzzDeleter {
	/// @brief Frees a blob.
	void operator()(hb_blob_t* blob) const noexcept { hb_blob_destroy(blob); }
	/// @brief Frees a face.
	void operator()(hb_face_t* face) const noexcept { hb_face_destroy(face); }
};

/// How much bold thickens outlines, as a share of the em (FreeType's own FT_GlyphSlot_Embolden uses 1/24).
constexpr float boldShare = 1.0f / 24.0f;
/// How far italic slants outlines: horizontal shift per unit of height (the tangent of about 11 degrees).
constexpr float italicSlant = 0.2f;

/// @brief The error for a file that isn't a usable font.
/// @param path The file.
/// @param why What is wrong, e.g. "not a font".
/// @return The error.
std::runtime_error unusable(const std::filesystem::path& path, const std::string& why) {
	return std::runtime_error("AffrescoUI: not a usable font (" + why + "): " + path.string());
}

} // namespace

FreeTypeLibrary startFreeType() {
	FT_Library library = nullptr;
	if(FT_Init_FreeType(&library) != 0) throw std::runtime_error("AffrescoUI: FreeType could not start");
	return FreeTypeLibrary(library);
}

std::shared_ptr<const FontFile> FontFiles::read(const std::filesystem::path& path) {
	const std::filesystem::path key = path.lexically_normal();
	if(const auto found = files_.find(key); found != files_.end())
		if(auto file = found->second.lock()) return file;
	std::erase_if(files_, [](const auto& entry) { return entry.second.expired(); });

	// Only regular files, and not too large: a revision can name any path, such as a device that never ends
	std::error_code error;
	const bool regular = std::filesystem::is_regular_file(path, error);
	const std::uintmax_t size = regular ? std::filesystem::file_size(path, error) : 0;
	if(!regular || error) throw std::runtime_error("AffrescoUI: could not open font file " + path.string());
	if(size > largestFile)
		throw std::runtime_error("AffrescoUI: font file larger than " + std::to_string(largestFile >> 20) +
		                         " MiB: " + path.string());

	std::ifstream stream(path, std::ios::binary);
	if(!stream) throw std::runtime_error("AffrescoUI: could not open font file " + path.string());
	auto file = std::make_shared<FontFile>();
	file->bytes.resize(static_cast<std::size_t>(size));
	stream.read(reinterpret_cast<char*>(file->bytes.data()), static_cast<std::streamsize>(size));
	if(stream.gcount() != static_cast<std::streamsize>(size))
		throw std::runtime_error("AffrescoUI: could not read font file " + path.string());

	files_[key] = file;
	return file;
}

std::size_t FontFiles::bytes() const noexcept {
	std::size_t total = 0;
	for(const auto& [path, file] : files_)
		if(const auto alive = file.lock()) total += alive->bytes.size();
	return total;
}

FontFace::FontFace(FT_Library library, std::shared_ptr<const FontFile> file, Size size,
                   const std::filesystem::path& path, FontStyle style)
    : file_(std::move(file)), style_(style) {
	openOutlines(library, path);
	setSize(size, path);
	openShaper(path);
}

void FontFace::openOutlines(FT_Library library, const std::filesystem::path& path) {
	const std::vector<unsigned char>& bytes = file_->bytes;
	FT_Face face = nullptr;
	if(bytes.empty() || FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face) != 0)
		throw unusable(path, "not a font FreeType reads");
	face_.reset(face);
	if(!FT_IS_SCALABLE(face))
		throw unusable(path, "no outlines; bitmap colour fonts such as CBDT emoji aren't supported yet");
}

void FontFace::setSize(Size size, const std::filesystem::path& path) {
	// A font's height spans its ascender to its descender, as stb_truetype's did, so sizes stay what they were
	FT_Face face = face_.get();
	const auto units =
	    static_cast<float>(size.of == SizeOf::em ? face->units_per_EM : face->ascender - face->descender);
	if(!(units > 0.0f) || !(size.pixels > 0.0f) || !std::isfinite(size.pixels)) throw unusable(path, "no height");
	if(size.pixels > largestSize) // checked before any conversion to an integer: 1e30 doesn't fit one
		throw unusable(path, "larger than " + std::to_string(static_cast<int>(largestSize)) + " pixels");
	scale_ = size.pixels / units;
	FT_Size_RequestRec request{size.of == SizeOf::em ? FT_SIZE_REQUEST_TYPE_NOMINAL : FT_SIZE_REQUEST_TYPE_REAL_DIM, 0,
	                           std::lround(size.pixels * 64.0f), 0, 0};
	if(FT_Request_Size(face, &request) != 0) throw unusable(path, "it has no size " + std::to_string(size.pixels));
	color_ = FT_HAS_COLOR(face);
}

void FontFace::openShaper(const std::filesystem::path& path) {
	// HarfBuzz reads the same bytes; positions are in font units, its default scale
	const std::vector<unsigned char>& bytes = file_->bytes;
	const std::unique_ptr<hb_blob_t, HarfBuzzDeleter> blob(hb_blob_create(reinterpret_cast<const char*>(bytes.data()),
	                                                                      static_cast<unsigned>(bytes.size()),
	                                                                      HB_MEMORY_MODE_READONLY, nullptr, nullptr));
	const std::unique_ptr<hb_face_t, HarfBuzzDeleter> shapingFace(hb_face_create(blob.get(), 0));
	if(hb_face_get_glyph_count(shapingFace.get()) == 0) throw unusable(path, "HarfBuzz finds no glyphs");
	shaper_.reset(hb_font_create(shapingFace.get()));
	// Shaping widens advances as the outlines are thickened, and slants mark positions as they are
	if(style_.bold) hb_font_set_synthetic_bold(shaper_.get(), boldShare, 0.0f, false);
	if(style_.italic) hb_font_set_synthetic_slant(shaper_.get(), italicSlant);
}

unsigned FontFace::glyphIndex(char32_t codepoint) const noexcept {
	// Beyond Unicode, the conversion to FT_ULong would still look it up: some cmap tables would then match
	if(codepoint > 0x10FFFF) return 0;
	return FT_Get_Char_Index(face_.get(), codepoint);
}

char32_t FontFace::codepointNamed(const std::string& name) const noexcept {
	FT_Face face = face_.get();
	if(!FT_HAS_GLYPH_NAMES(face) || name.empty()) return 0;
	const FT_UInt glyph = FT_Get_Name_Index(face, name.c_str());
	if(glyph == 0) return 0;
	// The face's characters, in order, until one maps to the glyph: icon fonts map each once, in the private use area
	FT_UInt index = 0;
	for(FT_ULong code = FT_Get_First_Char(face, &index); index != 0; code = FT_Get_Next_Char(face, code, &index))
		if(index == glyph) return static_cast<char32_t>(code);
	return 0;
}

float FontFace::emPixels() const noexcept { return static_cast<float>(face_->units_per_EM) * scale_; }

float FontFace::ascent() const noexcept { return static_cast<float>(face_->ascender) * scale_; }

float FontFace::lineHeight() const noexcept { return static_cast<float>(face_->height) * scale_; }

float FontFace::advance(unsigned glyph) const noexcept {
	return static_cast<float>(hb_font_get_glyph_h_advance(shaper_.get(), glyph)) * scale_;
}

std::optional<FontFace::Bitmap> FontFace::render(unsigned glyph) const noexcept {
	// Unhinted, so outlines keep their exact shapes and widths; embedded bitmaps would be another size
	const FT_Int32 flags = FT_LOAD_NO_HINTING | (color_ ? FT_LOAD_COLOR : FT_LOAD_NO_BITMAP);
	FT_Face face = face_.get();
	if(FT_Load_Glyph(face, glyph, flags) != 0) return std::nullopt;
	if(face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) synthesize(face->glyph->outline);
	if(face->glyph->format != FT_GLYPH_FORMAT_BITMAP && FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
		return std::nullopt;

	const FT_Bitmap& bitmap = face->glyph->bitmap;
	const bool isColor = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
	if(!isColor && bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) return std::nullopt; // e.g. a one-bit bitmap
	return Bitmap{&bitmap, face->glyph->bitmap_left, face->glyph->bitmap_top, isColor};
}

void FontFace::synthesize(FT_Outline& outline) const noexcept {
	if(style_.italic) {
		const FT_Matrix slant{0x10000, static_cast<FT_Fixed>(std::lround(italicSlant * 65536.0f)), 0, 0x10000};
		FT_Outline_Transform(&outline, &slant);
	}
	if(style_.bold) FT_Outline_Embolden(&outline, static_cast<FT_Pos>(std::lround(emPixels() * boldShare * 64.0f)));
}

} // namespace affresco::detail
