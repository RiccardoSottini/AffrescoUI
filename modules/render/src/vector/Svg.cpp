/// @file
/// @brief SVG icons: the element tree walked with inherited styles, each shape filled and stroked into a Canvas.

#include "Svg.hpp"

#include "MeshRasterizer.hpp"
#include "SvgStyle.hpp"
#include "SvgXml.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vector {

namespace {

constexpr std::size_t byteLimit = std::size_t{1} << 20;    ///< The largest document.
constexpr std::size_t elementLimit = 4096;                 ///< The most elements.
constexpr std::size_t depthLimit = 32;                     ///< The deepest nesting.
constexpr float sideLimit = 2048.0f;                       ///< The longest side of the image, in pixels.
constexpr std::size_t workLimit = std::size_t{1} << 26;    ///< The most pixels the canvas visits.
constexpr float circleHandle = 0.5523f;                    ///< Where a quarter circle's cubic handles are, in radii.
constexpr std::array<std::string_view, 13> skippedElements{ ///< Elements whose contents aren't drawn.
    "defs",  "clipPath", "mask",  "symbol", "linearGradient", "radialGradient", "pattern",
    "marker", "text",    "style", "title",  "desc",           "metadata"};

/// @brief A number attribute.
/// @param tag The element.
/// @param key The attribute.
/// @return Its value, 0 if it has none.
[[nodiscard]] float numberOf(const XmlTag& tag, std::string_view key) {
	const auto value = tag.attribute(key);
	return value ? svgNumber(*value).value_or(0.0f) : 0.0f;
}

/// @brief An ellipse, as four cubic curves.
/// @param centre Its centre.
/// @param radius Its radii.
[[nodiscard]] VectorPath ellipse(Vec2 centre, Vec2 radius) {
	VectorPath path;
	const Vec2 handle{radius.x * circleHandle, radius.y * circleHandle};
	path.moveTo({centre.x + radius.x, centre.y});
	path.cubicTo({centre.x + radius.x, centre.y + handle.y}, {centre.x + handle.x, centre.y + radius.y},
	             {centre.x, centre.y + radius.y});
	path.cubicTo({centre.x - handle.x, centre.y + radius.y}, {centre.x - radius.x, centre.y + handle.y},
	             {centre.x - radius.x, centre.y});
	path.cubicTo({centre.x - radius.x, centre.y - handle.y}, {centre.x - handle.x, centre.y - radius.y},
	             {centre.x, centre.y - radius.y});
	path.cubicTo({centre.x + handle.x, centre.y - radius.y}, {centre.x + radius.x, centre.y - handle.y},
	             {centre.x + radius.x, centre.y});
	path.close();
	return path;
}

/// @brief A rectangle, its corners rounded by rx and ry (either given alone sets both, like SVG).
/// @param tag The rect element.
[[nodiscard]] VectorPath rectangle(const XmlTag& tag) {
	const float x = numberOf(tag, "x"), y = numberOf(tag, "y"), width = numberOf(tag, "width"),
	            height = numberOf(tag, "height");
	VectorPath path;
	if(!(width > 0.0f) || !(height > 0.0f)) return path;
	float rx = tag.attribute("rx") ? numberOf(tag, "rx") : numberOf(tag, "ry");
	float ry = tag.attribute("ry") ? numberOf(tag, "ry") : rx;
	rx = std::clamp(rx, 0.0f, width / 2.0f);
	ry = std::clamp(ry, 0.0f, height / 2.0f);
	const float hx = rx * (1.0f - circleHandle), hy = ry * (1.0f - circleHandle);
	const float right = x + width, bottom = y + height;
	path.moveTo({x + rx, y});
	path.lineTo({right - rx, y});
	path.cubicTo({right - hx, y}, {right, y + hy}, {right, y + ry});
	path.lineTo({right, bottom - ry});
	path.cubicTo({right, bottom - hy}, {right - hx, bottom}, {right - rx, bottom});
	path.lineTo({x + rx, bottom});
	path.cubicTo({x + hx, bottom}, {x, bottom - hy}, {x, bottom - ry});
	path.lineTo({x, y + ry});
	path.cubicTo({x, y + hy}, {x + hx, y}, {x + rx, y});
	path.close();
	return path;
}

/// @brief The path a shape element draws.
/// @param tag The element.
/// @return The path, empty if the element isn't a shape or draws nothing.
/// @throws std::invalid_argument If its path data or points are broken.
[[nodiscard]] VectorPath shapeOf(const XmlTag& tag) {
	const std::string_view name = tag.name;
	if(name == "path") return VectorPath::parse(tag.attribute("d").value_or(""));
	if(name == "rect") return rectangle(tag);
	if(name == "circle") {
		const float radius = numberOf(tag, "r");
		return radius > 0.0f ? ellipse({numberOf(tag, "cx"), numberOf(tag, "cy")}, {radius, radius}) : VectorPath{};
	}
	if(name == "ellipse") {
		const Vec2 radius{numberOf(tag, "rx"), numberOf(tag, "ry")};
		return radius.x > 0.0f && radius.y > 0.0f ? ellipse({numberOf(tag, "cx"), numberOf(tag, "cy")}, radius)
		                                          : VectorPath{};
	}
	if(name == "line") {
		VectorPath path;
		path.moveTo({numberOf(tag, "x1"), numberOf(tag, "y1")});
		path.lineTo({numberOf(tag, "x2"), numberOf(tag, "y2")});
		return path;
	}
	if(name == "polyline" || name == "polygon") { // points are path data's implicit lines after a move
		const std::string_view points = tag.attribute("points").value_or("");
		if(points.find_first_not_of(" \t\r\n,") == std::string_view::npos) return {};
		return VectorPath::parse("M" + std::string(points) + (name == "polygon" ? "Z" : ""));
	}
	return {};
}

/// @brief Walks an SVG document and draws it.
class Renderer {
public:
	/// @brief Reads the root element and sets up the canvas for it.
	/// @param reader The document, read up to the root.
	/// @param root The root element.
	Renderer(XmlReader& reader, const XmlTag& root);

	/// @brief Draws the rest of the document.
	void run();
	/// @brief The drawing.
	[[nodiscard]] const Canvas& canvas() const noexcept { return canvas_; }

private:
	/// @brief Handles an opening tag.
	void open(const XmlTag& tag);
	/// @brief Draws a shape with a style.
	void draw(const VectorPath& path, const SvgStyle& style);

	XmlReader& reader_;            ///< The document.
	Canvas canvas_;                ///< The pixels.
	std::vector<SvgStyle> styles_; ///< The styles of the open elements, the root's first.
	std::size_t skipping_ = 0;     ///< How deep inside skipped elements reading is.
	std::size_t elements_ = 1;     ///< Elements read.
	std::vector<MeshVertex> mesh_; ///< Reused for each fill and stroke.
};

/// @brief The size of an image for a root element: twice its width and height (or its viewBox's, or 300 by 150), the
/// longest side at most sideLimit.
/// @param root The svg element.
/// @param viewBox Its viewBox numbers.
[[nodiscard]] Vec2 imageSize(const XmlTag& root, const std::vector<float>& viewBox) {
	Vec2 size{numberOf(root, "width"), numberOf(root, "height")};
	if(!(size.x > 0.0f) || !(size.y > 0.0f)) size = viewBox.size() == 4 ? Vec2{viewBox[2], viewBox[3]} : Vec2{300, 150};
	size = {size.x * 2.0f, size.y * 2.0f};
	const float scale = std::min(1.0f, sideLimit / std::max(size.x, size.y));
	return {std::max(std::round(size.x * scale), 1.0f), std::max(std::round(size.y * scale), 1.0f)};
}

/// @brief The transform from a root element's units to the image's pixels: its viewBox fitted inside and centred.
/// @param root The svg element.
/// @param viewBox Its viewBox numbers.
/// @param size The image's size.
[[nodiscard]] PathTransform rootTransform(const XmlTag& root, const std::vector<float>& viewBox, Vec2 size) {
	Rect box{0.0f, 0.0f, numberOf(root, "width"), numberOf(root, "height")};
	if(viewBox.size() == 4 && viewBox[2] > 0.0f && viewBox[3] > 0.0f)
		box = {viewBox[0], viewBox[1], viewBox[0] + viewBox[2], viewBox[1] + viewBox[3]};
	if(!(box.width() > 0.0f) || !(box.height() > 0.0f)) return {{2.0f, 2.0f}, {}};
	const float scale = std::min(size.x / box.width(), size.y / box.height());
	return {{scale, scale},
	        {(size.x - box.width() * scale) / 2.0f - box.left * scale,
	         (size.y - box.height() * scale) / 2.0f - box.top * scale}};
}

/// @brief The canvas for a root element.
[[nodiscard]] Canvas canvasFor(const XmlTag& root) {
	const Vec2 size = imageSize(root, svgNumbers(root.attribute("viewBox").value_or("")));
	return {static_cast<int>(size.x), static_cast<int>(size.y), workLimit};
}

Renderer::Renderer(XmlReader& reader, const XmlTag& root) : reader_(reader), canvas_(canvasFor(root)) {
	const std::vector<float> viewBox = svgNumbers(root.attribute("viewBox").value_or(""));
	SvgStyle style;
	style.transform = rootTransform(
	    root, viewBox, {static_cast<float>(canvas_.width()), static_cast<float>(canvas_.height())});
	style.apply(root);
	if(!root.selfClosing) styles_.push_back(std::move(style));
}

void Renderer::run() {
	while(!styles_.empty()) {
		const std::optional<XmlTag> tag = reader_.next();
		if(!tag) throw std::runtime_error("AffrescoUI: broken SVG (the svg element isn't closed)");
		if(tag->closing) {
			if(skipping_ > 0) --skipping_;
			else styles_.pop_back();
			continue;
		}
		if(++elements_ > elementLimit) throw std::runtime_error("AffrescoUI: SVG refused: too many elements");
		open(*tag);
	}
}

void Renderer::open(const XmlTag& tag) {
	if(skipping_ > 0 || std::ranges::find(skippedElements, tag.name) != skippedElements.end()) {
		if(!tag.selfClosing) ++skipping_;
		return;
	}
	SvgStyle style = styles_.back();
	style.apply(tag);
	draw(shapeOf(tag), style);
	if(tag.selfClosing) return;
	if(styles_.size() >= depthLimit) throw std::runtime_error("AffrescoUI: SVG refused: nested too deep");
	styles_.push_back(std::move(style));
}

void Renderer::draw(const VectorPath& path, const SvgStyle& style) {
	if(path.empty()) return;
	const auto contours = path.flatten(style.transform);
	if(style.fill && style.fill->a * style.opacity * style.fillOpacity > 0.0f) {
		mesh_.clear();
		appendFill(contours, style.fillRule, mesh_);
		canvas_.fill(mesh_, {style.fill->r, style.fill->g, style.fill->b,
		                     style.fill->a * style.opacity * style.fillOpacity});
	}
	if(style.stroke && style.stroke->a * style.opacity * style.strokeOpacity > 0.0f && style.line.width > 0.0f) {
		const float scale = std::sqrt(std::abs(style.transform.scale.x * style.transform.scale.y));
		StrokeStyle line = style.line;
		line.width *= scale;
		for(float& length : line.dash) length *= scale;
		mesh_.clear();
		appendStroke(contours, line, mesh_);
		canvas_.fill(mesh_, {style.stroke->r, style.stroke->g, style.stroke->b,
		                     style.stroke->a * style.opacity * style.strokeOpacity});
	}
}

} // namespace

bool looksLikeSvg(std::span<const std::uint8_t> bytes) {
	const std::string_view text(reinterpret_cast<const char*>(bytes.data()), std::min<std::size_t>(bytes.size(), 4096));
	const std::size_t start = text.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
	if(start == std::string_view::npos || text[start] != '<') return false;
	return text.find("<svg", start) != std::string_view::npos;
}

std::shared_ptr<const Bitmap> rasterizeSvg(std::span<const std::uint8_t> bytes, const Bitmap::Limits& limits) {
	if(bytes.size() > std::min(byteLimit, limits.fileBytes)) throw std::runtime_error("AffrescoUI: SVG larger than allowed");
	const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	XmlReader reader(text);
	std::optional<XmlTag> root = reader.next();
	if(!root || root->closing || root->name != "svg") throw std::runtime_error("AffrescoUI: not an SVG document");
	try {
		Renderer renderer(reader, *root);
		renderer.run();
		const Canvas& canvas = renderer.canvas();
		return Bitmap::fromPixels(canvas.width(), canvas.height(), canvas.straightPixels(), limits);
	} catch(const std::invalid_argument& problem) {
		throw std::runtime_error(std::string("AffrescoUI: broken SVG (") + problem.what() + ")");
	}
}

} // namespace affresco::vector
