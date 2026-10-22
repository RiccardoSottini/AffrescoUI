/// @file
/// @brief Sinopia's deck natives: loading, saving, drawing and editing decks, thumbnails, and PDF and PNG exports.

#include "DeckView.hpp"
#include "Edit.hpp"
#include "Png.hpp"
#include "Sinopia.hpp"

#include <affresco/export/PdfDocument.hpp>

#include <cmath>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>

namespace sinopia {

using affresco::json;

namespace {

/// @brief The arguments of a call, as an object.
/// @param arguments Their JSON text.
/// @return They (an empty object for anything else).
[[nodiscard]] json argumentsOf(const std::string& arguments) {
	json value = affresco::parseUntrusted(arguments);
	return value.is_object() ? value : json::object();
}

/// @brief A file inside a folder: a relative path that stays inside it.
/// @param folder The folder.
/// @param relative The path, relative to it.
/// @return The file.
/// @throws std::runtime_error If the path is absolute or leads outside the folder.
[[nodiscard]] std::filesystem::path inside(const std::filesystem::path& folder, const std::string& relative) {
	const std::filesystem::path path = std::filesystem::path(relative).lexically_normal();
	if(relative.empty() || path.is_absolute() || path.has_root_name() || *path.begin() == "..")
		throw std::runtime_error("Sinopia: " + relative + " is not inside the deck's folder");
	const std::filesystem::path base = std::filesystem::weakly_canonical(folder);
	const std::filesystem::path file = std::filesystem::weakly_canonical(base / path);
	const auto [end, unused] = std::ranges::mismatch(base, file);
	if(end != base.end()) throw std::runtime_error("Sinopia: " + relative + " is not inside the deck's folder");
	return file;
}

/// @brief A name for a file: letters, digits, "-" and "_", at most 64 of them.
/// @param text What it is made from.
/// @return It ("deck" for nothing usable).
[[nodiscard]] std::string fileName(const std::string& text) {
	std::string name;
	for(const char c : text.substr(0, 64)) {
		const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
		name += plain ? c : '-';
	}
	return name.find_first_not_of('-') == std::string::npos ? "deck" : name;
}

/// @brief Reads a deck file from the `decks` folder.
/// @param session The session (its folder and limits).
/// @param file The file, relative to the screen's folder.
/// @return The deck.
/// @throws std::runtime_error If it can't be read, is too large or isn't a deck.
[[nodiscard]] Deck loadDeck(const Session& session, const std::string& file) {
	const std::filesystem::path path =
	    inside(session.folder / "decks", std::filesystem::path(file).filename().string());
	if(!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > session.limits.fileBytes)
		throw std::runtime_error("Sinopia: no deck file " + file + " (or larger than the limit)");
	std::ifstream input(path, std::ios::binary);
	const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	return readDeck(affresco::parseUntrusted(text), session.limits);
}

/// @brief Checks the image an `add` edit places: a picture inside the `decks` folder, within the limits.
/// @param session The session.
/// @param file The image, relative to the `decks` folder.
/// @throws std::runtime_error If it isn't one.
void checkImage(const Session& session, const std::string& file) {
	const int side = session.limits.imageSide;
	const affresco::Bitmap::Limits limits{session.limits.imageFileBytes, side,
	                                      static_cast<std::size_t>(side) * static_cast<std::size_t>(side)};
	(void)affresco::Bitmap::load(inside(session.folder / "decks", file), limits);
}

/// @brief The slides a presentation or an export shows: those not skipped.
/// @param render The deck as renderDeck() gives it.
/// @return Their ids, in order.
[[nodiscard]] std::vector<std::string> shownSlides(const json& render) {
	std::vector<std::string> ids;
	if(render.contains("slides") && render["slides"].is_array())
		for(const json& slide : render["slides"])
			if(slide.is_object() && !slide.value("skip", false)) ids.push_back(slide.value("id", std::string()));
	return ids;
}

/// @brief Draws each slide whose look changed into its thumbnail, `thumb-<id>`, for the sorter and the slide list.
/// @param session The session.
/// @param render The deck as renderDeck() gives it.
/// @return `{ drawn }`: how many were drawn again.
[[nodiscard]] json drawThumbnails(Session& session, const json& render) {
	std::size_t drawn = 0;
	if(session.window == nullptr || !render.contains("slides") || !render["slides"].is_array()) return {{"drawn", 0}};
	const float width = render.value("width", 800.0f);
	for(const json& slide : render["slides"]) {
		const std::string id = slide.is_object() ? slide.value("id", std::string()) : std::string();
		const std::size_t look = std::hash<std::string>{}(slide.dump() + std::to_string(width));
		affresco::Panel* panel = stagePanel(session, id);
		if(panel == nullptr || session.thumbnails[id] == look) continue;
		const std::shared_ptr<const affresco::Bitmap> image = session.window->renderToImage(*panel, 200.0f / width);
		if(!image) continue;
		session.screen->setImage("thumb-" + id, image);
		session.thumbnails[id] = look;
		++drawn;
	}
	return {{"drawn", drawn}};
}

/// @brief Exports the shown slides as a PDF, one page each, at the deck's page size.
/// @param session The session.
/// @param render The deck as renderDeck() gives it.
/// @return `{ file, pages }`.
/// @throws std::runtime_error If it can't be written or is past a limit.
[[nodiscard]] json exportPdf(const Session& session, const json& render) {
	const std::vector<std::string> ids = shownSlides(render);
	if(ids.empty() || ids.size() > session.limits.exportPages)
		throw std::runtime_error("Sinopia: nothing to export, or more pages than the limit");
	const json& page = render.contains("page") && render["page"].is_array() ? render["page"] : json::array({960, 540});
	const affresco::Vec2 size{page.at(0).get<float>(), page.at(1).get<float>()};
	affresco::PdfDocument document;
	for(const std::string& id : ids)
		if(const affresco::Panel* panel = stagePanel(session, id)) document.addPage(*panel, size);
	const std::string title = render.value("title", std::string("Untitled"));
	std::filesystem::create_directories(session.exportFolder);
	const std::filesystem::path file = session.exportFolder / (fileName(title) + ".pdf");
	const std::size_t pages = document.pageCount(); // finishing the document empties it
	document.save(file, title);
	return {{"file", file.filename().string()}, {"pages", pages}};
}

/// @brief Exports each shown slide as a PNG, 1920 pixels wide.
/// @param session The session.
/// @param render The deck as renderDeck() gives it.
/// @return `{ files }`: how many were written (none without a window to draw them).
/// @throws std::runtime_error If one can't be written.
[[nodiscard]] json exportPng(const Session& session, const json& render) {
	const std::vector<std::string> ids = shownSlides(render);
	if(session.window == nullptr || ids.size() > session.limits.exportPages) return {{"files", 0}};
	const float scale = 1920.0f / render.value("width", 800.0f);
	const std::string name = fileName(render.value("title", std::string("Untitled")));
	std::filesystem::create_directories(session.exportFolder);
	std::size_t written = 0;
	for(const std::string& id : ids) {
		affresco::Panel* panel = stagePanel(session, id);
		const auto image = panel != nullptr ? session.window->renderToImage(*panel, scale) : nullptr;
		if(!image) continue;
		std::vector<std::uint8_t> rgb;
		rgb.reserve(image->pixels().size() / 4 * 3);
		for(std::size_t i = 0; i < image->pixels().size(); i += 4)
			rgb.insert(rgb.end(), image->pixels().begin() + static_cast<std::ptrdiff_t>(i),
			           image->pixels().begin() + static_cast<std::ptrdiff_t>(i) + 3);
		std::ofstream output(session.exportFolder / (name + "-" + std::to_string(++written) + ".png"),
		                     std::ios::binary);
		output << affresco::detail::encodePng(image->width(), image->height(), rgb);
		if(!output) throw std::runtime_error("Sinopia: a PNG could not be written");
	}
	return {{"files", written}};
}

} // namespace

affresco::Panel* stagePanel(const Session& session, const std::string& slide) {
	return session.screen != nullptr && !slide.empty() ? session.screen->find("slide-" + slide) : nullptr;
}

void registerNatives(affresco::Screen& screen, const std::shared_ptr<Session>& session) {
	session->screen = &screen;
	screen.onCall("sinopia.load", [session](const std::string& arguments) {
		return writeDeck(loadDeck(*session, argumentsOf(arguments).value("file", std::string()))).dump();
	});
	screen.onCall("sinopia.save", [session](const std::string& arguments) {
		const json with = argumentsOf(arguments);
		const Deck deck = readDeck(with.contains("deck") ? with["deck"] : json(), session->limits);
		const std::string name = fileName(with.value("name", deck.title)) + ".sinopia.json";
		std::ofstream output(session->folder / "decks" / name, std::ios::binary);
		output << writeDeck(deck).dump(1, '\t') << '\n';
		if(!output) throw std::runtime_error("Sinopia: " + name + " could not be written");
		return json({{"file", name}}).dump();
	});
	screen.onCall("sinopia.render", [session](const std::string& arguments) {
		const json with = argumentsOf(arguments);
		return renderDeck(readDeck(with.contains("deck") ? with["deck"] : json(), session->limits)).dump();
	});
	screen.onCall("sinopia.edit", [session](const std::string& arguments) {
		const json with = argumentsOf(arguments);
		if(with.value("op", std::string()) == "add" && with.value("kind", std::string()) == "image")
			checkImage(*session, with.value("image", std::string()));
		return editDeck(with, session->limits).dump();
	});
	const auto withRender = [session](json (*native)(Session&, const json&)) {
		return [session, native](const std::string& arguments) {
			const json with = argumentsOf(arguments);
			return native(*session, with.contains("render") ? with["render"] : json::object()).dump();
		};
	};
	screen.onCall("sinopia.thumbnails", withRender(drawThumbnails));
	screen.onCall("sinopia.exportPdf", withRender([](Session& s, const json& render) { return exportPdf(s, render); }));
	screen.onCall("sinopia.exportPng", withRender([](Session& s, const json& render) { return exportPng(s, render); }));
}

} // namespace sinopia
