/// @file
/// @brief detail::PdfFonts: font files subset with HarfBuzz (glyph indices kept) and embedded as CID fonts.

#include "PdfFonts.hpp"

#include "PdfShapes.hpp"

#include <hb-ot.h>
#include <hb-subset.h>
#include <hb.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace affresco::detail {

namespace {

/// @brief Frees HarfBuzz objects, for std::unique_ptr.
struct HarfBuzzDeleter {
	/// @brief Frees a blob.
	void operator()(hb_blob_t* blob) const noexcept { hb_blob_destroy(blob); }
	/// @brief Frees a face.
	void operator()(hb_face_t* face) const noexcept { hb_face_destroy(face); }
	/// @brief Frees a font.
	void operator()(hb_font_t* font) const noexcept { hb_font_destroy(font); }
	/// @brief Frees a set.
	void operator()(hb_set_t* set) const noexcept { hb_set_destroy(set); }
	/// @brief Frees a map.
	void operator()(hb_map_t* map) const noexcept { hb_map_destroy(map); }
	/// @brief Frees a subset input.
	void operator()(hb_subset_input_t* input) const noexcept { hb_subset_input_destroy(input); }
};

/// A HarfBuzz object, freed when it goes.
template <typename T> using HarfBuzzPointer = std::unique_ptr<T, HarfBuzzDeleter>;

/// @brief A table's bytes.
/// @param face The face.
/// @param tag The table.
/// @return Its blob (empty when the face hasn't it).
HarfBuzzPointer<hb_blob_t> table(const hb_face_t* face, hb_tag_t tag) {
	return HarfBuzzPointer<hb_blob_t>(hb_face_reference_table(face, tag));
}

/// @brief A big-endian signed 16-bit number of a table.
/// @param blob The table.
/// @param at Its offset.
/// @return It, or 0 past the table's end.
int readInt16(hb_blob_t* blob, unsigned at) noexcept {
	unsigned length = 0;
	const char* data = hb_blob_get_data(blob, &length);
	if(at + 2 > length) return 0;
	return static_cast<std::int16_t>(static_cast<std::uint16_t>((static_cast<unsigned char>(data[at]) << 8) |
	                                                            static_cast<unsigned char>(data[at + 1])));
}

/// @brief The font's PostScript name, with what a PDF name can't hold left out.
/// @param face The face.
/// @return It, or "Font".
std::string postScriptName(hb_face_t* face) {
	std::array<char, 64> text{};
	auto length = static_cast<unsigned>(text.size());
	hb_ot_name_get_utf8(face, HB_OT_NAME_ID_POSTSCRIPT_NAME, HB_LANGUAGE_INVALID, &length, text.data());
	std::string name;
	for(const char c : std::string_view(text.data())) {
		const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
		if(letter) name += c;
	}
	return name.empty() ? "Font" : name;
}

/// @brief Subsets a face to some glyphs, keeping their indices.
/// @param face The face.
/// @param glyphs The glyphs (0, the missing glyph, is added).
/// @return The subset file, or the whole file if it can't be subset.
HarfBuzzPointer<hb_blob_t> subset(hb_face_t* face, const std::set<unsigned>& glyphs) {
	const HarfBuzzPointer<hb_subset_input_t> input(hb_subset_input_create_or_fail());
	if(input) {
		hb_set_t* keep = hb_subset_input_glyph_set(input.get());
		hb_set_add(keep, 0);
		for(const unsigned glyph : glyphs) hb_set_add(keep, glyph);
		hb_subset_input_set_flags(input.get(), HB_SUBSET_FLAGS_RETAIN_GIDS);
		hb_set_t* drop = hb_subset_input_set(input.get(), HB_SUBSET_SETS_DROP_TABLE_TAG);
		for(const hb_tag_t tag : {HB_TAG('G', 'S', 'U', 'B'), HB_TAG('G', 'P', 'O', 'S'), HB_TAG('G', 'D', 'E', 'F')})
			hb_set_add(drop, tag);
		const HarfBuzzPointer<hb_face_t> result(hb_subset_or_fail(face, input.get()));
		if(result) return HarfBuzzPointer<hb_blob_t>(hb_face_reference_blob(result.get()));
	}
	return HarfBuzzPointer<hb_blob_t>(hb_face_reference_blob(face));
}

/// @brief Appends a glyph id as four hex digits.
/// @param out Where it goes.
/// @param glyph The id.
void appendGlyphHex(std::string& out, unsigned glyph) {
	std::array<char, 8> hex{};
	std::snprintf(hex.data(), hex.size(), "<%04X>", glyph & 0xFFFFu);
	out += hex.data();
}

} // namespace

/// @brief One font file and the glyphs drawn from it.
struct PdfFonts::Face {
	std::span<const unsigned char> bytes; ///< The file.
	HarfBuzzPointer<hb_blob_t> blob;      ///< Over the file's bytes, not copied.
	HarfBuzzPointer<hb_face_t> face;      ///< The file's first font.
	std::string name;                     ///< Its resource name.
	std::size_t index = 0;                ///< Its index in the document.
	std::uint32_t id = 0;                 ///< Its Type0 font object.
	unsigned unitsPerEm = 1000;           ///< See unitsPerEm().
	std::set<unsigned> glyphs;            ///< What is drawn from it.
};

PdfFonts::PdfFonts(PdfObjects& objects, std::size_t limit) : objects_(objects), limit_(limit) {}

PdfFonts::~PdfFonts() = default;

std::size_t PdfFonts::face(std::span<const unsigned char> bytes) {
	if(const auto found = byFile_.find(bytes.data()); found != byFile_.end()) return found->second;
	auto face = std::make_unique<Face>();
	face->bytes = bytes;
	face->blob.reset(hb_blob_create(reinterpret_cast<const char*>(bytes.data()), static_cast<unsigned>(bytes.size()),
	                                HB_MEMORY_MODE_READONLY, nullptr, nullptr));
	face->face.reset(hb_face_create(face->blob.get(), 0));
	face->unitsPerEm = std::max(1u, hb_face_get_upem(face->face.get()));
	face->index = faces_.size();
	face->name = "F" + std::to_string(faces_.size() + 1);
	face->id = objects_.reserve();
	faces_.push_back(std::move(face));
	byFile_.emplace(bytes.data(), faces_.size() - 1);
	return faces_.size() - 1;
}

const std::string& PdfFonts::name(std::size_t face) const noexcept { return faces_[face]->name; }

unsigned PdfFonts::unitsPerEm(std::size_t face) const noexcept { return faces_[face]->unitsPerEm; }

void PdfFonts::use(std::size_t face, unsigned glyph) { faces_[face]->glyphs.insert(glyph); }

void PdfFonts::finish(std::string& resources) {
	for(const auto& face : faces_) {
		write(*face);
		resources += "/" + face->name + " " + std::to_string(face->id) + " 0 R ";
	}
}

namespace {

/// @brief The /W array: each glyph's advance, in thousandths of an em.
/// @param face The face.
/// @param glyphs The glyphs.
/// @param unitsPerEm Its units per em.
/// @return It.
std::string widths(hb_face_t* face, const std::set<unsigned>& glyphs, unsigned unitsPerEm) {
	const HarfBuzzPointer<hb_font_t> font(hb_font_create(face));
	std::string out = "[ ";
	for(const unsigned glyph : glyphs) {
		out += std::to_string(glyph) + " [ ";
		appendNumber(out, hb_font_get_glyph_h_advance(font.get(), glyph) * 1000.0 / unitsPerEm);
		out += "] ";
	}
	return out + "]";
}

/// @brief The ToUnicode map: the characters each glyph stands for, from the file's character map, so text can be
/// searched and copied. A glyph no character maps to (a ligature) is left out.
/// @param face The face.
/// @param glyphs The glyphs.
/// @return The CMap program.
std::string toUnicode(hb_face_t* face, const std::set<unsigned>& glyphs) {
	const HarfBuzzPointer<hb_map_t> mapping(hb_map_create());
	const HarfBuzzPointer<hb_set_t> codepoints(hb_set_create());
	hb_face_collect_nominal_glyph_mapping(face, mapping.get(), codepoints.get());
	std::map<unsigned, std::uint32_t> characters; // the lowest character of each glyph
	hb_codepoint_t codepoint = HB_SET_VALUE_INVALID;
	while(hb_set_next(codepoints.get(), &codepoint)) {
		const unsigned glyph = hb_map_get(mapping.get(), codepoint);
		if(glyphs.contains(glyph)) characters.try_emplace(glyph, codepoint);
	}
	std::string out = "/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n"
	                  "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
	                  "/CMapName /Adobe-Identity-UCS def /CMapType 2 def\n"
	                  "1 begincodespacerange <0000> <FFFF> endcodespacerange\n";
	std::size_t inBlock = 0;
	std::size_t left = characters.size();
	for(const auto& [glyph, character] : characters) {
		if(inBlock == 0) out += std::to_string(std::min<std::size_t>(100, left)) + " beginbfchar\n";
		appendGlyphHex(out, glyph);
		out += ' ' + utf16Hex(character) + '\n';
		--left;
		if(++inBlock == 100) {
			out += "endbfchar\n";
			inBlock = 0;
		}
	}
	if(inBlock != 0) out += "endbfchar\n";
	return out + "endcmap CMapName currentdict /CMap defineresource pop end end\n";
}

/// @brief The font descriptor's metrics, in thousandths of an em: bounding box, ascent, descent, cap height.
/// @param face The face.
/// @param unitsPerEm Its units per em.
/// @return The entries.
std::string metrics(hb_face_t* face, unsigned unitsPerEm) {
	const HarfBuzzPointer<hb_font_t> font(hb_font_create(face));
	const HarfBuzzPointer<hb_blob_t> head = table(face, HB_TAG('h', 'e', 'a', 'd'));
	const double perUnit = 1000.0 / unitsPerEm;
	std::string out = "/FontBBox [ ";
	for(const unsigned at : {36u, 38u, 40u, 42u}) appendNumber(out, readInt16(head.get(), at) * perUnit);
	hb_font_extents_t extents{};
	hb_font_get_h_extents(font.get(), &extents);
	hb_position_t capHeight = extents.ascender;
	hb_ot_metrics_get_position(font.get(), HB_OT_METRICS_TAG_CAP_HEIGHT, &capHeight);
	out += "] /Ascent ";
	appendNumber(out, extents.ascender * perUnit);
	out += "/Descent ";
	appendNumber(out, extents.descender * perUnit);
	out += "/CapHeight ";
	appendNumber(out, capHeight * perUnit);
	return out;
}

/// @brief A subset font's name: six capitals that differ per font, a plus, and its PostScript name (ISO 32000-1,
/// 9.6.4).
/// @param index The font's index in the document.
/// @param face The face.
/// @return It.
std::string subsetName(std::size_t index, hb_face_t* face) {
	std::string tag = "AAAAAA";
	for(std::size_t i = tag.size(); i-- > 0 && index != 0; index /= 26) tag[i] = static_cast<char>('A' + index % 26);
	return tag + "+" + postScriptName(face);
}

} // namespace

void PdfFonts::write(Face& face) {
	hb_face_t* hbFace = face.face.get();
	const HarfBuzzPointer<hb_blob_t> file = subset(hbFace, face.glyphs);
	unsigned length = 0;
	const char* data = hb_blob_get_data(file.get(), &length);
	if(length > limit_ - std::min(limit_, written_))
		throw std::runtime_error("PDF fonts larger than their limit (" + std::to_string(limit_) + " bytes)");
	written_ += length;
	// OpenType with CFF outlines goes in as an OpenType font program; TrueType outlines as a TrueType one
	const bool cff = hb_blob_get_length(table(hbFace, HB_TAG('C', 'F', 'F', ' ')).get()) != 0;
	const std::uint32_t descendant = objects_.reserve();
	const std::uint32_t descriptor = objects_.reserve();
	const std::uint32_t program = objects_.reserve();
	const std::uint32_t unicode = objects_.reserve();
	const std::string fontName = "/" + subsetName(face.index, hbFace);
	objects_.define(face.id, "<< /Type /Font /Subtype /Type0 /BaseFont " + fontName +
	                             " /Encoding /Identity-H /DescendantFonts [ " + std::to_string(descendant) +
	                             " 0 R ] /ToUnicode " + std::to_string(unicode) + " 0 R >>");
	objects_.define(descendant, "<< /Type /Font /Subtype " + std::string(cff ? "/CIDFontType0" : "/CIDFontType2") +
	                                " /BaseFont " + fontName +
	                                " /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >>"
	                                " /FontDescriptor " +
	                                std::to_string(descriptor) + " 0 R /DW 0 /W " +
	                                widths(hbFace, face.glyphs, face.unitsPerEm) +
	                                (cff ? "" : " /CIDToGIDMap /Identity") + " >>");
	objects_.define(descriptor, "<< /Type /FontDescriptor /FontName " + fontName + " /Flags 4 " +
	                                metrics(hbFace, face.unitsPerEm) + "/ItalicAngle 0 /StemV 80 " +
	                                (cff ? "/FontFile3 " : "/FontFile2 ") + std::to_string(program) + " 0 R >>");
	objects_.defineStream(program, cff ? "/Subtype /OpenType" : "/Length1 " + std::to_string(length),
	                      std::string_view(data, length));
	objects_.defineStream(unicode, "", toUnicode(hbFace, face.glyphs));
}

} // namespace affresco::detail
