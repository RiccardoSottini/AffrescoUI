/// @file
/// @brief Fuzzes layout and retained geometry: a revision can give any node any layout keys (justify, alignItems,
/// shrink, basis, maxWidth, aspectRatio, cell, a grid's columns and rows as tracks, ...), and the screen lays the tree
/// out, then records what each panel draws.
///
/// The input is a JSON node, put in a screen as the root's only child. A screen that loads is built into a headless
/// window, laid out at two sizes, and its geometry refreshed each time (detail::GeometryCache, no OpenGL).

#include "Fuzz.hpp"

#include "GeometryCache.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace {

/// @brief Whether every edge of a rectangle is a finite number (sizes near the float's limit may add up to infinity).
/// @param rect The rectangle.
/// @return true if so.
bool isFinite(const affresco::Rect& rect) {
	return std::isfinite(rect.left) && std::isfinite(rect.top) && std::isfinite(rect.right) &&
	       std::isfinite(rect.bottom);
}

/// @brief Checks what layout promises about a panel and its subtree: a shown panel lies within what clips it.
/// @param panel The subtree's root.
void checkTree(const affresco::Panel& panel) {
	if(panel.isShown() && isFinite(panel.bounds())) {
		const affresco::Rect& bounds = panel.bounds();
		if(!(bounds.right >= bounds.left) || !(bounds.bottom >= bounds.top))
			affresco::fuzz::broken("a shown panel's bounds aren't inverted");
		if(const affresco::Panel* parent = panel.parent(); parent && !panel.isOverlay() && isFinite(parent->bounds())) {
			const affresco::Rect& clip = parent->bounds();
			if(bounds.left < clip.left || bounds.right > clip.right || bounds.top < clip.top ||
			   bounds.bottom > clip.bottom)
				affresco::fuzz::broken("a shown panel lies within its parent's bounds");
		}
	}
	for(const auto& child : panel.children()) checkTree(*child);
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	affresco::json node;
	try {
		node = affresco::parseUntrusted(affresco::fuzz::text(data, size));
	} catch(const std::exception&) {
		return 0; // not JSON: the json target covers that
	}
	if(!node.is_object()) return 0;
	const affresco::json document = {
	    {"format", "affrescoui-screen"}, {"version", 1}, {"root", {{"children", affresco::json::array({node})}}}};

	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(640, 480, clock);
	std::optional<affresco::Screen> screen;
	try {
		screen.emplace(affresco::Screen::fromString(document.dump()));
		screen->build(*window);
	} catch(const affresco::DocumentError&) {
		return 0; // refused, with a message that names the place
	}

	affresco::detail::GeometryCache cache;
	for(const auto& [width, height] : {std::pair{640, 480}, std::pair{317, 1000}}) {
		window->setSize(width, height);
		static_cast<void>(cache.refresh(affresco::detail::takeFrame(*window)));
		checkTree(window->root());
	}
	return 0;
}
