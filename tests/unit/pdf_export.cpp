/// @file
/// @brief The export module's PDF writer: a known page's structure (objects, cross-reference offsets, trailer), its
/// fonts (subset, embedded, with widths and a ToUnicode map) and its limits.

#include "Headless.hpp"

#include <affresco/export/PdfDocument.hpp>
#include <affresco/render/Font.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::Panel;
using affresco::PdfDocument;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using namespace affresco::literals;

namespace {

/// @brief Lays out a known page: a rounded card with a border and a shadow, a rotated box and a line of text.
/// @param window The window it goes in.
/// @return The card.
Panel& knownPage(affresco::Window& window) {
	Panel& card = window.add<Panel>();
	card.setPosition(20_px, 20_px);
	card.setSize(360_px, 200_px);
	card.setColor({0.95f, 0.95f, 1.0f, 1.0f});
	card.setCornerRadii(affresco::CornerRadii::all(12.0f));
	card.setBorder(2.0f, {0.2f, 0.3f, 0.8f, 1.0f});
	card.setShadow({{0.0f, 4.0f}, 8.0f, {0.0f, 0.0f, 0.0f, 0.3f}});
	Panel& box = card.add<Panel>();
	box.setPosition(20_px, 100_px);
	box.setSize(60_px, 60_px);
	box.setColor({0.9f, 0.4f, 0.1f, 0.5f});
	box.setTransform(affresco::Affine2::rotation(30.0f));
	auto& label = card.add<affresco::Label>();
	label.setPosition(20_px, 20_px);
	label.setSize(300_px, 30_px);
	label.setFont(affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 18.0f));
	label.setText("Sinopia: Ünïcode");
	window.render();
	return card;
}

/// @brief The whole file as text.
/// @param bytes The file.
std::string text(const std::vector<std::uint8_t>& bytes) { return {bytes.begin(), bytes.end()}; }

/// @brief How many times a text occurs.
/// @param haystack Where to look.
/// @param needle What.
std::size_t count(const std::string& haystack, const std::string& needle) {
	std::size_t found = 0;
	for(std::size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1)) ++found;
	return found;
}

} // namespace

TEST_CASE("pdf export: a known page has a valid structure and exact cross-reference offsets") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 260, clock);
	Panel& card = knownPage(*window);
	PdfDocument document;
	document.addPage(card, {842.0f, 595.0f});
	document.addPage(card, {595.0f, 842.0f});
	CHECK(document.pageCount() == 2);
	const std::string pdf = text(document.finish("Deck – Ünïcode"));

	REQUIRE(pdf.starts_with("%PDF-1.7\n%"));
	REQUIRE(pdf.ends_with("%%EOF\n"));
	// startxref points at the table, and the table's entries at their objects
	const std::size_t startxref = pdf.rfind("startxref\n");
	REQUIRE(startxref != std::string::npos);
	const std::size_t table = std::stoul(pdf.substr(startxref + 10));
	REQUIRE(pdf.compare(table, 5, "xref\n") == 0);
	std::smatch header;
	const std::string afterTable = pdf.substr(table + 5, 40);
	REQUIRE(std::regex_search(afterTable, header, std::regex("^0 ([0-9]+)\n")));
	const std::size_t objects = std::stoul(header[1].str());
	std::size_t entry = table + 5 + static_cast<std::size_t>(header.length(0));
	CHECK(pdf.compare(entry, 20, "0000000000 65535 f\r\n") == 0);
	for(std::size_t id = 1; id < objects; ++id) {
		entry += 20;
		const std::size_t offset = std::stoul(pdf.substr(entry, 10));
		CHECK(pdf.compare(entry + 10, 10, " 00000 n\r\n") == 0);
		CHECK(pdf.compare(offset, std::to_string(id).size() + 7, std::to_string(id) + " 0 obj\n") == 0);
	}
	CHECK(pdf.find("/Size " + std::to_string(objects) + " /Root") != std::string::npos);
	// Every stream's length is its data's
	const std::regex streamPattern("/Length ([0-9]+)[^>]*>>\nstream\n");
	std::size_t streams = 0;
	for(auto it = std::sregex_iterator(pdf.begin(), pdf.end(), streamPattern); it != std::sregex_iterator(); ++it) {
		const std::size_t start = static_cast<std::size_t>(it->position(0) + it->length(0));
		CHECK(pdf.compare(start + std::stoul((*it)[1].str()), 10, "\nendstream") == 0);
		++streams;
	}
	CHECK(streams >= 4); // two pages, the font file and its ToUnicode map
	CHECK(count(pdf, "/Type /Page ") == 2);
	CHECK(pdf.find("/Count 2") != std::string::npos);
	CHECK(pdf.find("/MediaBox [ 0 0 842 595 ]") != std::string::npos);
	CHECK(pdf.find("/Title <FEFF004400650063006B00202013002000DC006E00EF0063006F00640065>") != std::string::npos);
}

TEST_CASE("pdf export: text embeds a subset font with widths and a ToUnicode map, and shapes become paths") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 260, clock);
	Panel& card = knownPage(*window);
	PdfDocument document;
	document.addPage(card, {842.0f, 595.0f});
	const std::string pdf = text(document.finish());

	CHECK(pdf.find("/Subtype /Type0") != std::string::npos);
	CHECK(pdf.find("/Encoding /Identity-H") != std::string::npos);
	CHECK(pdf.find("/CIDFontType2") != std::string::npos);
	CHECK(pdf.find("/CIDToGIDMap /Identity") != std::string::npos);
	CHECK(std::regex_search(pdf, std::regex("/BaseFont /[A-Z]{6}\\+Roboto-Regular")));
	CHECK(pdf.find("/W [ ") != std::string::npos);
	CHECK(pdf.find("beginbfchar") != std::string::npos);
	CHECK(pdf.find("<00DC>") != std::string::npos); // Ü maps back to its character
	// The embedded file is a subset: much smaller than Roboto
	std::smatch length;
	REQUIRE(std::regex_search(pdf, length, std::regex("/Length1 ([0-9]+)")));
	CHECK(std::stoul(length[1].str()) <
	      std::filesystem::file_size(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf") / 2);
	CHECK(count(pdf, " Tj\n") == 15); // one per glyph that draws: 16 characters, less a space
	CHECK(pdf.find("Tf ") != std::string::npos);
	CHECK(pdf.find(" c\n") != std::string::npos);      // the card's rounded corners
	CHECK(pdf.find("f* Q") != std::string::npos);      // its border, a ring
	CHECK(pdf.find("/Ga128 gs") != std::string::npos); // the half-transparent box
	CHECK(pdf.find("cm\n") != std::string::npos);
}

TEST_CASE("pdf export: limits refuse too many pages and too large a file") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 260, clock);
	Panel& card = knownPage(*window);
	affresco::PdfLimits limits;
	limits.pages = 1;
	PdfDocument few(limits);
	few.addPage(card, {100.0f, 100.0f});
	CHECK_THROWS_AS(few.addPage(card, {100.0f, 100.0f}), std::runtime_error);
	limits.pages = 10;
	limits.bytes = 2000;
	PdfDocument small(limits);
	CHECK_THROWS_AS(
	    {
		    small.addPage(card, {100.0f, 100.0f}); // the page alone may already be past it
		    static_cast<void>(small.finish());
	    },
	    std::runtime_error);
	PdfDocument once;
	once.addPage(card, {100.0f, 100.0f});
	static_cast<void>(once.finish());
	CHECK_THROWS_AS(static_cast<void>(once.finish()), std::runtime_error);
	CHECK_THROWS_AS(PdfDocument().addPage(card, {0.0f, 100.0f}), std::runtime_error);
}
