/// @file
/// @brief PdfDocument: pages drawn by walking panel trees through a PdfPainter, then the file's structure.

#include "affresco/export/PdfDocument.hpp"

#include "affresco/render/Panel.hpp"

#include "PdfObjects.hpp"
#include "PdfPainter.hpp"
#include "PdfResources.hpp"
#include "PdfShapes.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace affresco {

/// @brief The document being written.
struct PdfDocument::State {
	PdfLimits limits;                 ///< What it may grow to.
	detail::PdfObjects objects;       ///< The file's objects.
	detail::PdfResources resources;   ///< Fonts, images and states, shared by the pages.
	std::uint32_t pagesId;            ///< The page tree's object.
	std::vector<std::uint32_t> pages; ///< The page objects, in order.

	/// @brief An empty document.
	/// @param chosen Its limits.
	explicit State(const PdfLimits& chosen)
	    : limits(chosen), objects(chosen.bytes), resources(objects, chosen), pagesId(objects.reserve()) {}
};

namespace {

/// @brief Draws a panel and its subtree, as the window does: the panel, its children in its bounds, then what it
/// draws over them. Each panel is a graphics state of its own, with its transform and its clip.
/// @param panel The panel.
/// @param painter The page's painter.
/// @param content The page's content (the painter's).
void drawTree(const Panel& panel, detail::PdfPainter& painter, std::string& content) {
	if(!panel.isShown()) return;
	content += "q\n";
	if(panel.isTransformed()) {
		detail::appendMatrix(content, panel.localTransform());
		content += "cm\n";
	}
	painter.clipTo(panel.bounds());
	panel.paintContent(painter);
	painter.flushClip(); // children are clipped to the panel even if it draws nothing itself
	for(const auto& child : panel.children())
		if(!child->isOverlay()) drawTree(*child, painter, content);
	panel.paintContentOver(painter);
	content += "Q\n";
	painter.checkLimit();
}

} // namespace

PdfDocument::PdfDocument(PdfLimits limits) : state_(std::make_unique<State>(limits)) {}

PdfDocument::~PdfDocument() = default;

void PdfDocument::addPage(const Panel& root, Vec2 pageSize) { addPage(root, root.frame(), pageSize); }

void PdfDocument::addPage(const Panel& root, const Rect& area, Vec2 pageSize) {
	if(!state_) throw std::runtime_error("PDF document already finished");
	if(state_->pages.size() >= state_->limits.pages)
		throw std::runtime_error("PDF has too many pages (at most " + std::to_string(state_->limits.pages) + ")");
	const bool sizeInRange = pageSize.x > 0.0f && pageSize.y > 0.0f && pageSize.x <= 14400.0f && pageSize.y <= 14400.0f;
	if(!sizeInRange) // NaN too: every comparison with it is false
		throw std::runtime_error("PDF page size out of range (1 to 14400 points)");
	// Window units onto the page: the area fitted and centred, y down (the page's origin is its bottom-left corner)
	const float scale = area.width() > 0.0f && area.height() > 0.0f
	                        ? std::min(pageSize.x / area.width(), pageSize.y / area.height())
	                        : 1.0f;
	const float left = (pageSize.x - area.width() * scale) / 2.0f - area.left * scale;
	const float top = pageSize.y - (pageSize.y - area.height() * scale) / 2.0f + area.top * scale;
	std::string content;
	detail::appendMatrix(content, {scale, 0.0f, 0.0f, -scale, left, top});
	content += "cm\n";
	detail::appendRect(content, area);
	content += "W n\n";
	detail::PdfPainter painter(content, state_->resources, state_->limits.contentBytes);
	drawTree(root, painter, content);

	const std::uint32_t contents = state_->objects.reserve();
	state_->objects.defineStream(contents, "", content);
	const std::uint32_t page = state_->objects.reserve();
	std::string box;
	detail::appendRect(box, {0.0f, 0.0f, pageSize.x, pageSize.y});
	box.resize(box.size() - 3); // "x y w h": the same numbers as MediaBox's corners, from the origin
	state_->objects.define(page, "<< /Type /Page /Parent " + std::to_string(state_->pagesId) + " 0 R /MediaBox [ " +
	                                 box + "] /Resources " + std::to_string(state_->resources.id()) +
	                                 " 0 R /Contents " + std::to_string(contents) + " 0 R >>");
	state_->pages.push_back(page);
}

std::size_t PdfDocument::pageCount() const noexcept { return state_ ? state_->pages.size() : 0; }

std::vector<std::uint8_t> PdfDocument::finish(std::string_view title) {
	if(!state_) throw std::runtime_error("PDF document already finished");
	const std::unique_ptr<State> state = std::move(state_);
	state->resources.finish();
	std::string kids;
	for(const std::uint32_t page : state->pages) kids += std::to_string(page) + " 0 R ";
	state->objects.define(state->pagesId, "<< /Type /Pages /Kids [ " + kids + "] /Count " +
	                                          std::to_string(state->pages.size()) + " >>");
	const std::uint32_t catalog = state->objects.reserve();
	state->objects.define(catalog, "<< /Type /Catalog /Pages " + std::to_string(state->pagesId) + " 0 R >>");
	const std::uint32_t info = state->objects.reserve();
	std::string infoBody = "<< /Producer (AffrescoUI)";
	if(!title.empty()) infoBody += " /Title " + detail::textString(title);
	state->objects.define(info, infoBody + " >>");
	return state->objects.finish(catalog, info);
}

void PdfDocument::save(const std::filesystem::path& file, std::string_view title) {
	const std::vector<std::uint8_t> bytes = finish(title);
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if(!out) throw std::runtime_error("can't write the PDF file " + file.filename().string());
}

} // namespace affresco
