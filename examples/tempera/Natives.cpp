/// @file
/// @brief Tempera's document natives: new, open, save and revert; undo, redo and the History panel; the layers; and
/// the canvas surface, whose tools tell the screen through the "temperaChanged" action.

#include "tempera/Tempera.hpp"

#include <affresco/imaging/Codec.hpp>
#include <affresco/imaging/Composite.hpp>
#include <affresco/imaging/Project.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

namespace tempera {

namespace {

/// @brief The size of a new document unless the arguments say.
constexpr int newWidth = 1200, newHeight = 800;

/// @brief Reads a file the size limits allow.
/// @param path Its path.
/// @param limit The most bytes.
/// @throws affresco::DocumentError When it can't be read or is too big.
[[nodiscard]] std::vector<std::uint8_t> readFile(const std::string& path, std::size_t limit) {
	std::error_code error;
	const auto size = std::filesystem::file_size(path, error);
	if(error) throw affresco::DocumentError("Can't read " + std::filesystem::path(path).filename().string());
	if(size > limit) throw affresco::DocumentError("The file is larger than " + std::to_string(limit >> 20) + " MB");
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	std::ifstream file(path, std::ios::binary);
	file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if(!file) throw affresco::DocumentError("Can't read " + std::filesystem::path(path).filename().string());
	return bytes;
}

/// @brief Opens a project, PNG or JPEG from its bytes as the document, with its working space.
/// @param session The session.
/// @param bytes The file.
/// @throws affresco::DocumentError When it is malformed or beyond the limits.
void openBytes(Session& session, std::vector<std::uint8_t> bytes) {
	auto file = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
	std::unique_ptr<imaging::Document> document;
	std::optional<imaging::ColorProfile> profile;
	if(imaging::isProject(*file)) {
		imaging::LoadedProject project = imaging::loadProject(file, session.limits);
		document = std::move(project.document);
		profile = std::move(project.profile);
		session.message.clear();
	} else {
		imaging::DecodedImage decoded = imaging::decodeImageFile(*file, session.limits);
		const imaging::Image& image = *decoded.image;
		document = std::make_unique<imaging::Document>(image.width(), image.height(), session.limits, image.format());
		const std::uint64_t blank = document->structure().active;
		document->addImageLayer("Background", decoded.image);
		document->deleteLayer(blank);
		document->clearHistory();
		profile = std::move(decoded.profile);
		session.message = decoded.warning;
	}
	session.working = profile ? *profile : imaging::ColorProfile::srgb();
	replaceDocument(session, std::move(document));
	updateDisplay(session);
}

/// @brief The flattened document as one rgba8 image; over the background colour for a format without alpha.
/// @param session The session.
/// @param opaque Whether to composite onto the background colour.
[[nodiscard]] imaging::Image flattened(const Session& session, bool opaque) {
	const imaging::Document& document = session.canvas->document();
	imaging::Image image(document.width(), document.height(), imaging::PixelFormat::rgba8, session.limits);
	const imaging::Pixel under = session.settings.background;
	for(int row = 0; row < image.rows(); ++row)
		for(int column = 0; column < image.columns(); ++column) {
			auto pixels = imaging::flatten(document, 0, column * imaging::tileSize, row * imaging::tileSize,
			                               imaging::tileSize, imaging::tileSize);
			for(std::size_t index = 0; opaque && index < pixels.size(); index += 4) {
				const float alpha = imaging::unitFromByte(pixels[index + 3]);
				const std::array<float, 3> background{under.red, under.green, under.blue};
				for(std::size_t channel = 0; channel < 3; ++channel)
					pixels[index + channel] = imaging::byteFromUnit(
					    imaging::unitFromByte(pixels[index + channel]) * alpha + background[channel] * (1.0f - alpha));
				pixels[index + 3] = 255;
			}
			image.setTile(column, row, std::make_shared<imaging::Tile>(imaging::PixelFormat::rgba8, std::move(pixels)));
		}
	return image;
}

/// @brief Saves the document: kept for Revert, and written where the save dialog says when there is a window.
/// @param session The session.
/// @param format "tempera", "png" or "jpeg".
void save(Session& session, const std::string& format) {
	const std::optional<imaging::ColorProfile> profile =
	    session.working == imaging::ColorProfile::srgb() ? std::nullopt : std::optional(session.working);
	std::vector<std::uint8_t> bytes;
	if(format == "png" || format == "jpeg")
		bytes = imaging::encodeImageFile(
		    flattened(session, format == "jpeg"),
		    format == "png" ? imaging::ImageFileFormat::png : imaging::ImageFileFormat::jpeg, profile);
	else if(format == "tempera") bytes = imaging::saveProject(session.canvas->document(), profile);
	else throw affresco::DocumentError("Unknown format " + format);
	session.message = "Saved " + std::to_string(bytes.size() >> 10) + " KB";
	if(session.window != nullptr) {
		const std::string extension = format == "jpeg" ? "jpg" : format;
		const auto chosen = session.window->chooseFiles(
		    {affresco::FileDialogKind::Save, "Save", {{format, {extension}}}, "", "Untitled." + extension, false});
		if(chosen.empty()) return;
		std::ofstream file(chosen.front(), std::ios::binary);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		if(!file) throw affresco::DocumentError("Can't write the file");
	}
	session.saved = std::move(bytes);
}

/// @brief Opens a file the open dialog chooses.
/// @param session The session.
void open(Session& session) {
	if(session.window == nullptr) throw affresco::DocumentError("Opening needs a window");
	const auto chosen = session.window->chooseFiles(
	    {affresco::FileDialogKind::Open, "Open", {{"Images", {"tempera", "png", "jpg", "jpeg"}}}, "", "", false});
	if(chosen.empty()) return;
	const std::size_t limit = std::max(session.limits.imageFileBytes, session.limits.projectFileBytes);
	auto bytes = readFile(chosen.front(), limit);
	session.saved = bytes;
	openBytes(session, std::move(bytes));
}

/// @brief A new document: white, or the generated sample.
/// @param session The session.
/// @param arguments width, height, sample, format ("rgba8" or "rgba16f").
void makeNew(Session& session, const json& arguments) {
	const int width = wholeOf(arguments, "width", newWidth);
	const int height = wholeOf(arguments, "height", newHeight);
	if(width < 1 || height < 1) throw affresco::DocumentError("A document is at least 1 × 1");
	std::unique_ptr<imaging::Document> document;
	if(flagOf(arguments, "sample", false)) {
		document = sampleDocument(width, height, session.limits);
	} else {
		const auto format =
		    textOf(arguments, "format") == "rgba16f" ? imaging::PixelFormat::rgba16f : imaging::PixelFormat::rgba8;
		document = std::make_unique<imaging::Document>(width, height, session.limits, format);
		const std::uint64_t background = document->structure().active;
		document->changeLayer(background, "Layer Properties", [](imaging::Layer& layer) { layer.name = "Background"; });
		imaging::fillSelection(*document, document->layer(background)->pixels, session.settings.background, 1.0f);
		document->clearHistory();
	}
	session.working = imaging::ColorProfile::srgb();
	session.saved.clear();
	session.message.clear();
	replaceDocument(session, std::move(document));
	updateDisplay(session);
}

/// @brief Moves a layer onto another's place in the Layers panel, or a step up or down.
/// @param session The session.
/// @param arguments id; then "onto" (the id of the layer whose place it takes) or "by" (+1 up, -1 down).
void moveLayer(Session& session, const json& arguments) {
	imaging::Document& document = session.canvas->document();
	const std::uint64_t id = layerOf(session, arguments);
	const imaging::Layer& layer = *document.layer(id);
	const auto placeIn = [&](std::uint64_t parent) -> const std::vector<std::uint64_t>& {
		return parent == 0 ? document.structure().root : document.layer(parent)->children;
	};
	if(arguments.contains("onto")) {
		const std::uint64_t onto = layerOf(session, {{"id", arguments["onto"]}});
		if(onto == id) return;
		const imaging::Layer& target = *document.layer(onto);
		const auto& siblings = placeIn(target.parent);
		auto index = static_cast<std::size_t>(std::find(siblings.begin(), siblings.end(), onto) - siblings.begin());
		// Document::moveLayer places it before the layer at index; taking the place of one above means after it.
		if(layer.parent == target.parent &&
		   static_cast<std::size_t>(std::find(siblings.begin(), siblings.end(), id) - siblings.begin()) < index)
			++index;
		document.moveLayer(id, target.parent, index);
		return;
	}
	const auto& siblings = placeIn(layer.parent);
	const auto index = static_cast<long>(std::find(siblings.begin(), siblings.end(), id) - siblings.begin());
	// Any step past the list's length moves it to an end, so a step this far is the same and can't overflow the sum
	constexpr double farthest = 1e9;
	const long by = std::lround(std::clamp(numberOf(arguments, "by", 1.0), -farthest, farthest));
	// Document::moveLayer places it before the layer at index, counted before it leaves, so up skips one more.
	document.moveLayer(id, layer.parent, static_cast<std::size_t>(std::max(0L, index + by + (by > 0 ? 1 : 0))));
}

/// @brief Changes a layer's settings; a slider's changes (opacity, an adjustment) coalesce into one step.
/// @param session The session.
/// @param arguments id, and any of name, visible, opacity (0 to 100), blend, lockPixels, lockPosition, lockAll,
/// clipped, maskEnabled, adjustment.
void setLayer(Session& session, const json& arguments) {
	const std::uint64_t id = layerOf(session, arguments);
	std::optional<imaging::BlendMode> blend;
	if(arguments.contains("blend")) {
		blend = imaging::blendModeOf(textOf(arguments, "blend"));
		if(!blend) throw affresco::DocumentError("Unknown blend mode " + textOf(arguments, "blend"));
	}
	const bool sliding = arguments.contains("opacity") || arguments.contains("adjustment");
	const std::string name = arguments.contains("opacity")      ? "Opacity Change"
	                         : arguments.contains("adjustment") ? "Adjustment Change"
	                         : arguments.contains("visible")    ? "Layer Visibility"
	                         : blend                            ? "Blending Change"
	                                                            : "Layer Properties";
	session.canvas->document().changeLayer(
	    id, name,
	    [&](imaging::Layer& layer) {
		    if(arguments.contains("name")) layer.name = textOf(arguments, "name", layer.name);
		    layer.visible = flagOf(arguments, "visible", layer.visible);
		    layer.opacity = std::clamp(
		        static_cast<float>(numberOf(arguments, "opacity", layer.opacity * 100.0) / 100.0), 0.0f, 1.0f);
		    if(blend) layer.blend = *blend;
		    layer.locks.pixels = flagOf(arguments, "lockPixels", layer.locks.pixels);
		    layer.locks.position = flagOf(arguments, "lockPosition", layer.locks.position);
		    layer.locks.all = flagOf(arguments, "lockAll", layer.locks.all);
		    layer.clipped = flagOf(arguments, "clipped", layer.clipped);
		    layer.maskEnabled = flagOf(arguments, "maskEnabled", layer.maskEnabled);
		    if(layer.kind == imaging::LayerKind::adjustment && arguments.contains("adjustment"))
			    layer.adjustment = imaging::adjustmentFromJson(arguments["adjustment"]);
	    },
	    sliding);
}

/// @brief Adds a pixel layer, a group or an adjustment layer.
/// @param session The session.
/// @param arguments kind ("pixel", "group" or an adjustment's kind), name.
void addLayer(Session& session, const json& arguments) {
	imaging::Document& document = session.canvas->document();
	const std::string kind = textOf(arguments, "kind", "pixel");
	const std::string name = textOf(arguments, "name");
	if(kind == "pixel") {
		document.addPixelLayer(name.empty() ? "Layer " + std::to_string(document.structure().nextId - 1) : name);
	} else if(kind == "group") {
		document.addGroup(name.empty() ? "Group" : name);
	} else {
		const auto adjustment = imaging::adjustmentKindOf(kind);
		if(!adjustment) throw affresco::DocumentError("Unknown layer kind " + kind);
		imaging::Adjustment settings;
		settings.kind = *adjustment;
		document.addAdjustment(name.empty() ? kind : name, settings);
	}
}

/// @brief Registers the history's natives.
/// @param screen The screen.
/// @param session The session.
void registerHistory(affresco::Screen& screen, const std::weak_ptr<Session>& session) {
	addNative(screen, "tempera.undo", session, [](Session& self, const json&) {
		settle(self);
		self.canvas->document().undo();
	});
	addNative(screen, "tempera.redo", session, [](Session& self, const json&) {
		settle(self);
		self.canvas->document().redo();
	});
	addNative(screen, "tempera.history", session, [](Session& self, const json& arguments) {
		settle(self);
		// Past the last step means the last step; clamped first, as the conversion of a huge number is undefined
		constexpr double farthest = 1e9;
		const double position = std::clamp(numberOf(arguments, "position", 0.0), 0.0, farthest);
		self.canvas->document().goTo(static_cast<std::size_t>(position));
	});
}

/// @brief Registers the layers' natives.
/// @param screen The screen.
/// @param session The session.
void registerLayers(affresco::Screen& screen, const std::weak_ptr<Session>& session) {
	const auto onLayer = [&](const std::string& name, std::function<void(imaging::Document&, std::uint64_t)> body) {
		addNative(screen, name, session, [body = std::move(body)](Session& self, const json& arguments) {
			settle(self);
			body(self.canvas->document(), layerOf(self, arguments));
		});
	};
	addNative(screen, "tempera.addLayer", session, [](Session& self, const json& arguments) {
		settle(self);
		addLayer(self, arguments);
	});
	onLayer("tempera.deleteLayer", [](imaging::Document& document, std::uint64_t id) { document.deleteLayer(id); });
	onLayer("tempera.duplicateLayer",
	        [](imaging::Document& document, std::uint64_t id) { document.duplicateLayer(id); });
	onLayer("tempera.selectLayer", [](imaging::Document& document, std::uint64_t id) { document.setActive(id); });
	onLayer("tempera.addMask", [](imaging::Document& document, std::uint64_t id) { document.addMask(id); });
	onLayer("tempera.deleteMask", [](imaging::Document& document, std::uint64_t id) { document.deleteMask(id); });
	addNative(screen, "tempera.moveLayer", session, [](Session& self, const json& arguments) {
		settle(self);
		moveLayer(self, arguments);
	});
	addNative(screen, "tempera.setLayer", session, [](Session& self, const json& arguments) {
		settle(self);
		setLayer(self, arguments);
	});
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): get() is only reached for a number, which it always reads
double numberOf(const json& arguments, std::string_view key, double fallback) noexcept {
	const auto found = arguments.find(key);
	if(found == arguments.end() || !found->is_number()) return fallback;
	const double value = found->get<double>();
	// A document may send any number: narrowing one past a float's range to a float would be undefined
	constexpr double most = std::numeric_limits<float>::max();
	return std::isfinite(value) ? std::clamp(value, -most, most) : fallback;
}

int wholeOf(const json& arguments, std::string_view key, int fallback) noexcept {
	// Clamped before the conversion, which is undefined for a number past an int's range
	constexpr double least = std::numeric_limits<int>::min(), most = std::numeric_limits<int>::max();
	return static_cast<int>(std::clamp(numberOf(arguments, key, fallback), least, most));
}

std::string textOf(const json& arguments, std::string_view key, std::string_view fallback) {
	const auto found = arguments.find(key);
	return found != arguments.end() && found->is_string() ? found->get<std::string>() : std::string(fallback);
}

// NOLINTNEXTLINE(bugprone-exception-escape): get() is only reached for a boolean, which it always reads
bool flagOf(const json& arguments, std::string_view key, bool fallback) noexcept {
	const auto found = arguments.find(key);
	return found != arguments.end() && found->is_boolean() ? found->get<bool>() : fallback;
}

std::uint64_t layerOf(const Session& session, const json& arguments) {
	const imaging::Document& document = session.canvas->document();
	std::uint64_t id = document.structure().active;
	if(const std::string text = textOf(arguments, "id"); !text.empty()) {
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
		if(error != std::errc{} || end != text.data() + text.size()) id = 0;
	}
	if(document.layer(id) == nullptr) throw affresco::DocumentError("No such layer");
	return id;
}

void settle(Session& session) {
	if(session.stroke) session.stroke->finish();
	session.stroke.reset();
	session.preview.reset();
	session.path.clear();
}

void addNative(affresco::Screen& screen, const std::string& name, const std::weak_ptr<Session>& session,
               std::function<void(Session&, const json&)> body) {
	screen.onCall(name, [&screen, session, body = std::move(body)](const std::string& argumentsJson) {
		const auto self = session.lock();
		if(!self) throw affresco::DocumentError("Tempera has closed");
		const json arguments = argumentsJson.empty() ? json::object() : affresco::parseUntrusted(argumentsJson);
		body(*self, arguments.is_object() ? arguments : json::object());
		return snapshot(*self, screen).dump();
	});
}

void registerTempera(affresco::Screen& screen, const std::shared_ptr<Session>& session, affresco::Executor* executor) {
	const std::weak_ptr<Session> weak = session;
	const auto tell = [weak, &screen] {
		const auto self = weak.lock();
		affresco::Panel* root = screen.find("root");
		if(self && root != nullptr) screen.runAction("temperaChanged", *root, snapshot(*self, screen).dump());
	};
	session->changed = [executor, tell] {
		if(executor != nullptr) executor->post(tell);
		else tell();
	};
	session->canvas->onPointer = [weak](const imaging::CanvasPointer& event) {
		const auto self = weak.lock();
		if(self && applyPointer(*self, event) && self->changed) self->changed();
	};
	screen.registerSurface("temperaCanvas", imaging::canvasSurfaceFactory(session->canvas));
	addNative(screen, "tempera.snapshot", weak, [](Session&, const json&) {});
	addNative(screen, "tempera.new", weak, [](Session& self, const json& arguments) { makeNew(self, arguments); });
	addNative(screen, "tempera.open", weak, [](Session& self, const json&) { open(self); });
	addNative(screen, "tempera.save", weak, [](Session& self, const json& arguments) {
		settle(self);
		save(self, textOf(arguments, "format", "tempera"));
	});
	addNative(screen, "tempera.revert", weak, [](Session& self, const json&) {
		if(self.saved.empty()) throw affresco::DocumentError("Nothing saved yet");
		openBytes(self, self.saved);
	});
	addNative(screen, "tempera.fit", weak, [](Session& self, const json&) { fitView(self); });
	registerHistory(screen, weak);
	registerLayers(screen, weak);
	registerEditing(screen, session);
}

} // namespace tempera
