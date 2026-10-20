/// @file
/// @brief detail::GlyphAtlas: shelf packing into coverage and colour pages.

#include "text/GlyphAtlas.hpp"

#include <algorithm>
#include <bit>

namespace affresco::detail {

namespace {

constexpr int smallestAddedPage = 1024; ///< Coverage pages added later are at least this large, so they fill slowly.
constexpr int smallestColorPage = 512;  ///< Colour pages start at this size: four bytes per texel.

} // namespace

GlyphAtlas::Page::Page(int pageSize, AtlasKind kind)
    : size(pageSize), bytesPerTexel(kind == AtlasKind::color ? 4 : 1),
      pixels(static_cast<std::size_t>(pageSize) * static_cast<std::size_t>(pageSize) *
	             static_cast<std::size_t>(bytesPerTexel),
	         0),
      packer(pageSize, kind == AtlasKind::color ? 0 : whiteRows) {
	if(kind == AtlasKind::color) return;
	pixels[0] = pixels[1] = pixels[static_cast<std::size_t>(size)] = pixels[static_cast<std::size_t>(size) + 1] = 255;
}

GlyphAtlas::GlyphAtlas(int firstPageSize, int shelfHeight) : shelfHeight_(shelfHeight) {
	const Page& first = coveragePages_.emplace_back(firstPageSize, AtlasKind::coverage);
	bytes_ = first.pixels.size();
}

std::optional<GlyphAtlas::Spot> GlyphAtlas::place(AtlasKind kind, int width, int height, bool mayAddPage) {
	std::vector<Page>& list = pages(kind);
	for(std::size_t p = 0; p < list.size(); ++p)
		if(const auto at = list[p].packer.place(width, height, shelfHeight_))
			return Spot{static_cast<std::uint32_t>(p), (*at)[0], (*at)[1]};
	if(!mayAddPage) return std::nullopt;

	// Colour pages hold about 16 lines of glyphs, coverage pages at least as many as the first
	const auto lines = static_cast<int>(std::bit_ceil(16u * static_cast<unsigned>(shelfHeight_)));
	const int wanted = kind == AtlasKind::color ? std::max(smallestColorPage, lines)
	                                            : std::max(coveragePages_.front().size, smallestAddedPage);
	const int size = std::min(wanted, largestPage);
	if(width > size || height > size) return std::nullopt; // larger than a whole page
	const std::size_t texels = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
	const std::size_t added = texels * (kind == AtlasKind::color ? 4 : 1);
	if(bytes_ + added > limitBytes) return std::nullopt;

	Page& page = list.emplace_back(size, kind);
	bytes_ += added;
	const auto at = page.packer.place(width, height, shelfHeight_);
	if(!at) return std::nullopt; // not reached: an empty page has room for what fits in one
	return Spot{static_cast<std::uint32_t>(list.size() - 1), (*at)[0], (*at)[1]};
}

void GlyphAtlas::forgetChanges() noexcept {
	for(Page& page : coveragePages_) page.changes.clear();
	for(Page& page : colorPages_) page.changes.clear();
}

} // namespace affresco::detail
