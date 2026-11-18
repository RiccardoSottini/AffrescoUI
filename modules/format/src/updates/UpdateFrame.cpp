/// @file
/// @brief format::UpdateFrame: the window-wide work before and after the walk over the panels.

#include "UpdateFrame.hpp"
#include "Engine.hpp"
#include "Contributions.hpp"
#include "PropertyValues.hpp"

#include "affresco/core/Assets.hpp"

#include <format>

namespace affresco::format {

namespace {

/// @brief Gives the window the ring of "window.focusRing" (a colour, { "color", "width", "offset" }, or false; the
/// default ring when it is left out), if it changed. Errors are reported, and leave the ring as it was.
/// @param engine The runtime, with a window.
void showFocusRing(Engine& engine) {
	if(!engine.window) return;
	const json& document = engine.versions.document();
	std::optional<FocusRing> ring = FocusRing{}; // on unless the document says otherwise
	try {
		if(document.contains("window") && document["window"].is_object() && document["window"].contains("focusRing")) {
			ring.reset();
			const json& value = document["window"]["focusRing"];
			const std::string at = "window.focusRing";
			if(value.is_object()) {
				ring.emplace();
				for(const auto& [key, item] : value.items()) {
					if(key == "color") ring->color = engine.document.color(item, at + ".color");
					else if(key == "width") ring->width = pixels(item, at + ".width");
					else if(key == "offset") ring->offset = pixels(item, at + ".offset");
					else fail(std::format("{}.{}", at, key), "unknown property");
				}
			} else if(!value.is_boolean() || value.get<bool>())
				ring = FocusRing{.color = engine.document.color(value, at)};
		}
	} catch(const DocumentError& error) {
		engine.report(error.what());
		return;
	}
	if(ring != engine.window->focusRing()) engine.window->setFocusRing(ring);
}

/// @brief Gives the window the icon of "window.icon" (none when it is left out), if it changed: a file under the rules
/// of an image node's (decoded on the worker thread; one that can't be read shows none), or an asset.
/// @param engine The runtime, with a window.
/// @throws DocumentError If it isn't text, or names a file outside the allowed folders or an asset that isn't a
/// picture: the icon stays as it was.
void showIcon(Engine& engine) {
	const json& document = engine.versions.document();
	const auto settings = document.find("window");
	json value = settings != document.end() && settings->is_object() ? settings->value("icon", json()) : json();
	if(value == engine.icon) return;
	const std::string at = "window.icon";
	if(value.is_null()) {
		engine.iconFile.cancel();
		engine.window->setIcon(nullptr);
	} else if(assetReference(value)) { // decoded once, by its bytes
		std::shared_ptr<const Bitmap> image = engine.document.images().named(engine.document.assetImage(value, at));
		engine.iconFile.cancel();
		engine.window->setIcon(std::move(image));
	} else {
		const std::filesystem::path file = engine.document.imageFile(value, at);
		engine.iconFile = engine.document.images().load(file, [&engine](const std::shared_ptr<const Bitmap>& image) {
			if(engine.window) engine.window->setIcon(image);
		});
	}
	engine.icon = std::move(value);
}

} // namespace

void UpdateFrame::begin(const StateChanges& changes) {
	static_cast<void>(changes); // every update reads the same window-wide values, whatever changed
	Window& window = *engine_.window;
	DocumentContext& context = engine_.document;

	// The screen an outlet shows: the first one, unless navigation chose another that still exists
	context.keepScreen(engine_.versions.document());
	context.globalsChanged(); // window size, history, proposals, screen: refreshed once per update
	// Computed values are cached per state version, and may read the window: a resize makes them stale too
	std::string windowNow = std::to_string(window.width()) + "x" + std::to_string(window.height()) + "@" +
	                        std::to_string(window.contentScale()) +
	                        (window.colorScheme() == ColorScheme::Dark ? "d" : "l");
	if(windowNow != window_) {
		if(!window_.empty()) context.bumpStateVersion();
		window_ = std::move(windowNow);
	}
	context.updateTheme();
	context.updateLocale();

	// Revisions can change the fonts: the default one is the window's
	if(const auto font = context.findFont("default"); font && window.defaultFont() != font.get())
		window.setDefaultFont(font);
	showIcon(engine_); // and the icon: a file outside the allowed folders refuses the revision, as an image node's does
}

void UpdateFrame::end(const StateChanges& changes, bool full) {
	static_cast<void>(changes);
	static_cast<void>(full);
	if(!engine_.handlersSuspended) engine_.watches.watch(engine_.window->root());
	showWindowTitle(engine_);
	showMenuBar(engine_);
	showFocusRing(engine_);
	engine_.owner.onUpdated();
}

} // namespace affresco::format
