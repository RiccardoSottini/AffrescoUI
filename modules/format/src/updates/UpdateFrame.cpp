/// @file
/// @brief format::UpdateFrame: the window-wide work before and after the walk over the panels.

#include "UpdateFrame.hpp"
#include "Engine.hpp"
#include "Contributions.hpp"
#include "PropertyValues.hpp"

#include "affresco/core/Assets.hpp"

#include <format>
#include <utility>

namespace affresco::format {

namespace {

/// @brief The ring a "window.focusRing" value gives: a colour, { "color", "width", "offset" }, true (the default
/// ring) or false (none).
/// @param engine The runtime, for the colours.
/// @param value The value.
/// @return The ring, or none.
/// @throws DocumentError If it is wrong.
std::optional<FocusRing> focusRingOf(const Engine& engine, const json& value) {
	const std::string at = "window.focusRing";
	if(value.is_boolean() && !value.get<bool>()) return std::nullopt;
	if(!value.is_object()) return FocusRing{.color = engine.document.color(value, at)};
	FocusRing ring;
	for(const auto& [key, item] : value.items()) {
		if(key == "color") ring.color = engine.document.color(item, at + ".color");
		else if(key == "width") ring.width = pixels(item, at + ".width");
		else if(key == "offset") ring.offset = pixels(item, at + ".offset");
		else fail(std::format("{}.{}", at, key), "unknown property");
	}
	return ring;
}

/// @brief Gives the window the ring of "window.focusRing" (a colour, { "color", "width", "offset" }, or false; the
/// default ring when it is left out), if it changed. Errors are reported, and leave the ring as it was.
/// @param engine The runtime, with a window.
void showFocusRing(Engine& engine) {
	if(!engine.window) return;
	const json& document = engine.versions.document();
	std::optional<FocusRing> ring = FocusRing{}; // on unless the document says otherwise
	try {
		if(document.contains("window") && document["window"].is_object() && document["window"].contains("focusRing"))
			ring = focusRingOf(engine, document["window"]["focusRing"]);
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

UpdateFrame::UpdateFrame(Engine& engine) : engine_(engine) {
	for(std::size_t i = 0; i < partCount; ++i) {
		PartReader& reader = parts_.at(i).emplace(*this, static_cast<Part>(i));
		reader.id = engine_.tracker.add(reader); // read nothing yet: the first update, of everything, evaluates it
	}
}

UpdateFrame::~UpdateFrame() {
	for(const std::optional<PartReader>& reader : parts_)
		if(reader) engine_.tracker.remove(reader->id);
}

void UpdateFrame::PartReader::update(const StateChanges& changes) {
	static_cast<void>(changes);
	if(part_ == Part::Theme || part_ == Part::Locale || part_ == Part::Icon) frame_.evaluate(part_);
	else due = true; // after the walk, as the panels are then
}

std::string UpdateFrame::PartReader::name() const {
	static constexpr std::array<const char*, partCount> names = {"theme",        "strings.locale", "window.icon",
	                                                             "window.title", "window.menuBar", "window.focusRing"};
	return names.at(static_cast<std::size_t>(part_));
}

void UpdateFrame::evaluate(Part part) {
	ReadSet reads;
	{
		const DocumentContext::ReadScope scope(engine_.document, &reads);
		switch(part) {
		case Part::Theme:
			engine_.document.updateTheme();
			break;
		case Part::Locale:
			engine_.document.updateLocale();
			break;
		case Part::Icon:
			showIcon(engine_);
			break; // a file outside the allowed folders refuses the revision
		case Part::Title:
			showWindowTitle(engine_);
			break;
		case Part::MenuBar:
			showMenuBar(engine_);
			break;
		case Part::FocusRing:
			showFocusRing(engine_);
			break;
		}
	}
	if(part == Part::Locale) reads.addPath(std::string(places::globals) + "/locale"); // the host's (setLocale())
	if(part == Part::FocusRing || part == Part::MenuBar)                              // its colours are the theme's
		reads.addPath(std::string(places::globals) + "/theme");
	if(const std::optional<PartReader>& reader = parts_.at(static_cast<std::size_t>(part)))
		engine_.tracker.setReads(reader->id, std::move(reads));
}

bool UpdateFrame::begin(StateChanges& changes) {
	Window& window = *engine_.window;
	DocumentContext& context = engine_.document;
	if(changes.all) context.globalsChanged(); // a revision, or what no place describes: everything is built again
	context.hostValues().updateStarted();

	// The screen an outlet shows: the first one, unless navigation chose another that still exists
	const std::string screen = context.screen();
	context.keepScreen(engine_.versions.document());
	if(context.screen() != screen) context.markGlobalChanged("screen");
	// What `window` holds, and what computed values that read it read
	std::string windowNow = std::format("{}x{}@{}{}{}{}{}", window.width(), window.height(), window.contentScale(),
	                                    window.colorScheme() == ColorScheme::Dark ? 'd' : 'l', window.nativeMenuBar(),
	                                    window.reducedMotion(), window.capsLock());
	bool resized = false;
	if(windowNow != window_) {
		resized = !window_.empty();
		if(resized) context.markGlobalChanged("window");
		window_ = std::move(windowNow);
	}
	const StateChanges marked = context.takeChanges(); // they belong to this update
	if(marked.all) changes.addAll();
	for(const std::string& place : marked.paths) changes.add(place);

	// Revisions can change the fonts: the default one is the window's
	if(const auto font = context.findFont("default"); font && window.defaultFont() != font.get())
		window.setDefaultFont(font);
	return resized;
}

void UpdateFrame::checkPanels() {
	const UpdateStatistics& counters = engine_.reconciler.counters();
	const std::size_t records = engine_.reconciler.records().size();
	if(counters.panelsApplied == 0 && counters.panelsCreated == 0 && records == records_) return;
	records_ = records;
	DocumentContext& context = engine_.document;
	bool marked = false;
	if(context.hostValues().built("keybindings") && keysFingerprint(engine_) != keysBuiltFrom_) {
		context.markVariableChanged("keybindings");
		marked = true;
	}
	if(context.hostValues().built("context") && overlayShown(engine_) != overlayBuiltFrom_) {
		context.markVariableChanged("context");
		marked = true;
	}
	if(marked) engine_.reconciler.requestStateUpdate();
}

void UpdateFrame::end(const StateChanges& changes, bool full) {
	if(!engine_.handlersSuspended) engine_.watches.watch(engine_.window->root());
	for(const Part part : {Part::Title, Part::MenuBar, Part::FocusRing})
		if(std::optional<PartReader>& reader = parts_.at(static_cast<std::size_t>(part));
		   reader && std::exchange(reader->due, false))
			evaluate(part);
	checkPanels();
	lastAll_ = full || changes.all;
	lastChanges_ = changes.paths;
	engine_.owner.onUpdated();
}

std::optional<std::span<const std::string>> UpdateFrame::lastChanges() const noexcept {
	if(lastAll_) return std::nullopt;
	return std::span<const std::string>(lastChanges_);
}

} // namespace affresco::format
