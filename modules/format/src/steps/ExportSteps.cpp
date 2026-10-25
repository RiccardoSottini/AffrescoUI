/// @file
/// @brief The export step: export, which draws nodes of the screen as a file the person saves: a vector PDF of one page
/// per node (the export module), a PNG of one node, or the glTF of one scene node (scene3d). Snapshot nodes make good
/// pages: they are laid out at their own size and never shown in the window.

#include "Engine.hpp"
#include "FileSteps.hpp"

#include "affresco/export/PdfDocument.hpp"
#include "affresco/imaging/Codec.hpp"
#include "affresco/scene3d/Gltf.hpp"
#include "affresco/scene3d/SceneJson.hpp"
#include "affresco/scene3d/SceneView.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::format {

namespace {

constexpr std::size_t maxPages = 2000; ///< The most pages one export draws.

/// @brief The nodes to draw, by their ids.
/// @param s The step.
/// @param engine The runtime, which has a window.
/// @return The laid-out panels, in order.
std::vector<Panel*> pagesOf(const Running& s, Engine& engine) {
	const json ids = s.list(s.step.value("pages", json()), s.where + ".pages");
	if(!ids.is_array() || ids.empty() || ids.size() > maxPages)
		fail(s.where + ".pages", "must be a list of 1 to 2000 node ids");
	std::vector<Panel*> pages;
	for(const json& id : ids) {
		const std::string text = string(id, s.where + ".pages");
		Panel* panel = engine.window->root().findById(text);
		if(panel == nullptr) fail(s.where + ".pages", "there is no node '" + text + "' to draw");
		pages.push_back(panel);
	}
	return pages;
}

/// @brief A page's side in points, as the PDF's limits allow.
/// @param side The side asked for.
/// @return It, from 72 to 14400 (1 inch to 200 inches).
float pageSide(double side) {
	return std::isfinite(side) ? std::clamp(static_cast<float>(side), 72.0f, 14400.0f) : 72.0f;
}

/// @brief The PDF of the pages: each fitted to "size" (in points) if given, else to its own size.
/// @param s The step.
/// @param pages The panels.
/// @param title The document's title.
/// @return The file's bytes.
/// @throws std::runtime_error Past the PDF's limits.
std::vector<std::uint8_t> pdfOf(const Running& s, const std::vector<Panel*>& pages, const std::string& title) {
	std::optional<Vec2> size;
	if(s.step.contains("size")) {
		const json given = s.value("size");
		if(!given.is_array() || given.size() != 2 || !given[0].is_number() || !given[1].is_number())
			fail(s.where + ".size", "must be [width, height] in points");
		size = Vec2{pageSide(given[0].get<double>()), pageSide(given[1].get<double>())};
	}
	PdfDocument document;
	for(const Panel* page : pages)
		document.addPage(*page, size.value_or(Vec2{pageSide(page->frame().width()), pageSide(page->frame().height())}));
	return document.finish(title);
}

/// @brief The PNG of one page, "scale" pixels a window unit (2 by default).
/// @param s The step.
/// @param engine The runtime.
/// @param page The panel.
/// @return The file's bytes, or none if the window draws nothing (headless).
std::vector<std::uint8_t> pngOf(const Running& s, Engine& engine, Panel& page) {
	double scale = 2.0;
	if(s.step.contains("scale")) {
		const json given = s.value("scale");
		if(!given.is_number() || !(given.get<double>() >= 0.25 && given.get<double>() <= 8.0))
			fail(s.where + ".scale", "must be a number from 0.25 to 8");
		scale = given.get<double>();
	}
	const std::shared_ptr<const Bitmap> bitmap = engine.window->renderToImage(page, static_cast<float>(scale));
	if(!bitmap) return {};
	std::vector<std::uint8_t> rgba(bitmap->pixels().begin(), bitmap->pixels().end());
	for(std::size_t i = 0; i + 3 < rgba.size(); i += 4) // straight alpha, as imaging keeps it
		if(const unsigned alpha = rgba[i + 3]; alpha != 0 && alpha != 255)
			for(std::size_t c = 0; c < 3; ++c)
				rgba[i + c] = static_cast<std::uint8_t>(std::min(255U, (rgba[i + c] * 255U + alpha / 2) / alpha));
	const imaging::DecodedImage image = imaging::imageFromRgba(bitmap->width(), bitmap->height(), rgba, {});
	return imaging::encodeImageFile(*image.image, imaging::ImageFileFormat::png, std::nullopt);
}

/// @brief The glTF of one scene node: what it shows, with the "animations" given (clips' JSON) if any.
/// @param s The step.
/// @param view The scene node.
/// @param warnings Receives what the file leaves out (textures).
/// @return The file's bytes.
/// @throws std::exception If the clips are malformed.
std::vector<std::uint8_t> gltfOf(const Running& s, const scene3d::SceneView& view, std::vector<std::string>& warnings) {
	const std::vector<scene3d::AnimationClip> clips =
	    s.step.contains("animations") ? scene3d::clipsFromJson(s.value("animations")) : std::vector<scene3d::AnimationClip>{};
	const std::string text = scene3d::exportGltf(view.scene(), view.assets(), clips, &warnings);
	return {text.begin(), text.end()};
}

/// @brief Runs onError at once, with error {code "export", message}.
/// @param s The step.
/// @param message What went wrong.
void exportFailed(const Running& s, const std::string& message) {
	s.steps.engine().report("AffrescoUI screen: export: " + message);
	const Followers followers = followersOf(s);
	followers.outcome->vars = json{{"error", {{"code", "export"}, {"message", message}}}};
	followers.failed();
}

/// @brief "export": "pdf", "png" or "gltf" of the "pages", saved where the person chooses, named after "name" (or
/// "title"), then onDone with result {name, pages, bytes} (and the glTF's warnings).
/// @param s The step.
/// @return true.
bool exportFile(const Running& s) {
	s.steps.checkControl(s.where);
	const std::string kind = string(s.value("export"), s.where + ".export");
	if(kind != "pdf" && kind != "png" && kind != "gltf") fail(s.where + ".export", "must be \"pdf\", \"png\" or \"gltf\"");
	const std::string title = s.step.contains("title") ? string(s.value("title"), s.where + ".title") : "Untitled";
	if(title.size() > 1000) fail(s.where + ".title", "is longer than 1000 bytes");
	std::string name = s.step.contains("name") ? string(s.value("name"), s.where + ".name") : title;
	Engine& engine = s.steps.engine();
	if(!engine.window || dialogRefused(s)) return true; // a check or a preview: nothing to draw
	const std::vector<Panel*> pages = pagesOf(s, engine);
	if(kind != "pdf" && pages.size() != 1) fail(s.where + ".pages", kind == "png" ? "a PNG draws one node" : "a glTF file is of one node");
	const auto* view = dynamic_cast<const scene3d::SceneView*>(pages.front());
	if(kind == "gltf" && view == nullptr) {
		exportFailed(s, "a glTF file is of a scene node");
		return true;
	}
	std::vector<std::uint8_t> bytes;
	std::vector<std::string> warnings;
	try {
		bytes = kind == "pdf"   ? pdfOf(s, pages, title)
		        : kind == "png" ? pngOf(s, engine, *pages.front())
		                        : gltfOf(s, *view, warnings);
	} catch(const std::exception& error) {
		exportFailed(s, error.what());
		return true;
	}
	if(bytes.empty()) {
		exportFailed(s, "the window draws nothing");
		return true;
	}
	if(!name.ends_with("." + kind)) name += "." + kind;
	const std::size_t size = bytes.size();
	json result = {{"pages", pages.size()}, {"bytes", size}};
	if(kind == "gltf") result["warnings"] = warnings;
	saveAfterEvent(s, std::move(bytes), suggestedFileName(name, "Untitled." + kind),
	               {kind == "pdf" ? "PDF" : kind == "png" ? "PNG" : "glTF", {kind}}, std::move(result));
	return true;
}

/// @brief Runs an export step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runExportStep(const Running& s) {
	if(!s.step.contains("export")) return std::nullopt;
	return exportFile(s);
}

} // namespace

void registerExportSteps(StepRegistry& registry) {
	registry.add({"export",
	              {{"export", {"pages", "size", "scale", "title", "name", "onDone", "onError"}, true}},
	              runExportStep});
}

} // namespace affresco::format
