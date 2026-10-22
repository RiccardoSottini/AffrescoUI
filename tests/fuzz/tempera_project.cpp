/// @file
/// @brief Fuzzes the image editor's project files (affresco/imaging/Project.hpp): a damaged or hostile one must be
/// refused, or open within the limits, never crash, hang or read past its bytes.
///
/// The input is a file's bytes. Loading gives a document, whose every tile is then read and which is saved and loaded
/// again, or a DocumentError.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Project.hpp"
#include "Fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	using namespace affresco::imaging;
	Limits limits;
	limits.side = 2048; // keeps each run quick: the grid is still read in full
	try {
		const LoadedProject loaded =
		    loadProject(std::make_shared<const std::vector<std::uint8_t>>(data, data + size), limits);
		const Document& document = *loaded.document;
		if(document.listed().empty()) affresco::fuzz::broken("an opened project has a layer");
		for(const auto& [id, depth] : document.listed()) {
			const Layer& layer = *document.layer(id);
			for(const std::uint64_t image : {layer.pixels, layer.mask})
				if(const Image* pixels = document.image(image))
					for(int row = 0; row < pixels->height(); row += tileSize)
						for(int column = 0; column < pixels->width(); column += tileSize)
							(void)pixels->pixel(column, row);
		}
		const std::vector<std::uint8_t> saved = saveProject(document, loaded.profile);
		try {
			(void)loadProject(std::make_shared<const std::vector<std::uint8_t>>(saved), limits);
		} catch(const affresco::DocumentError&) {
			affresco::fuzz::broken("a project saved again opens");
		}
	} catch(const affresco::DocumentError&) { // NOLINT(bugprone-empty-catch): refusing is the expected outcome
	}
	return 0;
}
