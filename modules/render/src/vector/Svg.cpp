/// @file
/// @brief SVG icons: the element tree walked with inherited styles, each shape filled and stroked into a Canvas.

#include "Svg.hpp"

#include "MeshRasterizer.hpp"
#include "PathFill.hpp"
#include "SvgStyle.hpp"
#include "SvgXml.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vector {

namespace {

constexpr std::size_t byteLimit = std::size_t{1} << 20; ///< The largest document.
constexpr std::size_t elementLimit = 4096;              ///< The most elements.
constexpr std::size_t depthLimit = 32;                  ///< The deepest nesting.
constexpr float sideLimit = 2048.0f;                    ///< The longest side of the image, in pixels.
constexpr std::size_t workLimit = std::size_t{1} << 26; ///< The most pixels the canvas visits.
constexpr std::size_t fillWorkLimit = workLimit;        ///< The most edge visits of all the fills together.
constexpr float circleHandle = 0.5523f;                 ///< Where a quarter circle's cubic handles are, in radii.
constexpr std::size_t gradientLimit = 256;              ///< The most gradients a document defines.
constexpr std::size_t stopLimit = 64;                   ///< The most stops of one gradient.

/// @brief A gradient a fill or a stroke names (`linearGradient`, `radialGradient`): pad spread only.
struct Gradient {
	bool radial = false;                                   ///< A radial one (cx, cy, r), else linear (x1, y1, x2, y2).
	bool userSpace = false;                                ///< gradientUnits="userSpaceOnUse"; else the shape's box.
	std::array<float, 4> geometry{0.0f, 0.0f, 1.0f, 0.0f}; ///< x1 y1 x2 y2, or cx cy r and nothing.
	Affine2 transform;                                     ///< gradientTransform.
	std::vector<std::pair<float, Color>> stops;            ///< Offset (0 to 1, rising) and straight colour.
	std::string inherits;                                  ///< The gradient its href names, for its stops.
};

/// @brief Elements whose contents aren't drawn.
constexpr std::array<std::string_view, 13> skippedElements{
    "defs",   "clipPath", "mask",  "symbol", "linearGradient", "radialGradient", "pattern",
    "marker", "text",     "style", "title",  "desc",           "metadata"};

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

/// @brief A circle (radius r) or an ellipse (radii rx and ry).
/// @param tag The circle or ellipse element.
/// @return Its path, empty unless both radii are positive.
[[nodiscard]] VectorPath roundShape(const XmlTag& tag) {
	const bool circle = tag.name == "circle";
	const Vec2 radius{numberOf(tag, circle ? "r" : "rx"), numberOf(tag, circle ? "r" : "ry")};
	if(!(radius.x > 0.0f) || !(radius.y > 0.0f)) return {};
	return ellipse({numberOf(tag, "cx"), numberOf(tag, "cy")}, radius);
}

/// @brief A polyline or a polygon: its points are path data's implicit lines after a move.
/// @param tag The polyline or polygon element.
/// @return Its path, empty if it has no points.
/// @throws std::invalid_argument If its points are broken.
[[nodiscard]] VectorPath pointsShape(const XmlTag& tag) {
	const std::string_view points = tag.attribute("points").value_or("");
	if(points.find_first_not_of(" \t\r\n,") == std::string_view::npos) return {};
	return VectorPath::parse("M" + std::string(points) + (tag.name == "polygon" ? "Z" : ""));
}

/// @brief The path a shape element draws.
/// @param tag The element.
/// @return The path, empty if the element isn't a shape or draws nothing.
/// @throws std::invalid_argument If its path data or points are broken.
[[nodiscard]] VectorPath shapeOf(const XmlTag& tag) {
	const std::string_view name = tag.name;
	if(name == "path") return VectorPath::parse(tag.attribute("d").value_or(""));
	if(name == "rect") return rectangle(tag);
	if(name == "circle" || name == "ellipse") return roundShape(tag);
	if(name == "line") {
		VectorPath path;
		path.moveTo({numberOf(tag, "x1"), numberOf(tag, "y1")});
		path.lineTo({numberOf(tag, "x2"), numberOf(tag, "y2")});
		return path;
	}
	if(name == "polyline" || name == "polygon") return pointsShape(tag);
	return {};
}

/// @brief A gradient's colour at a place along it, its stops interpolated (straight alpha), padded at both ends.
/// @param stops The stops, by offset.
/// @param t Where.
/// @return The colour.
[[nodiscard]] Color colorAt(const std::vector<std::pair<float, Color>>& stops, float t) {
	if(stops.empty()) return {0.0f, 0.0f, 0.0f, 0.0f};
	if(!(t > stops.front().first)) return stops.front().second;
	for(std::size_t i = 1; i < stops.size(); ++i)
		if(t <= stops[i].first) {
			const auto& [from, a] = stops[i - 1];
			const auto& [to, b] = stops[i];
			const float k = to > from ? (t - from) / (to - from) : 1.0f;
			return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k, a.a + (b.a - a.a) * k};
		}
	return stops.back().second;
}

/// @brief Reads a stop's offset: a number or a percentage, clamped to [0, 1].
/// @param text The value.
[[nodiscard]] float offsetOf(std::string_view text) {
	const bool percent = text.ends_with('%');
	if(percent) text.remove_suffix(1);
	return std::clamp(svgNumber(text).value_or(0.0f) / (percent ? 100.0f : 1.0f), 0.0f, 1.0f);
}

/// @brief Reads a stop element: its offset, stop-color and stop-opacity (attributes or style).
/// @param tag The stop.
/// @return Its offset and colour.
[[nodiscard]] std::pair<float, Color> stopOf(const XmlTag& tag) {
	std::string_view color = tag.attribute("stop-color").value_or("black");
	std::string_view opacity = tag.attribute("stop-opacity").value_or("1");
	if(const auto style = tag.attribute("style")) {
		std::string_view rest = *style;
		while(!rest.empty()) {
			const std::size_t end = std::min(rest.find(';'), rest.size());
			const std::string_view declaration = rest.substr(0, end);
			if(const std::size_t colon = declaration.find(':'); colon != std::string_view::npos) {
				std::string_view key = declaration.substr(0, colon);
				while(!key.empty() && key.front() == ' ') key.remove_prefix(1);
				while(!key.empty() && key.back() == ' ') key.remove_suffix(1);
				if(key == "stop-color") color = declaration.substr(colon + 1);
				if(key == "stop-opacity") opacity = declaration.substr(colon + 1);
			}
			rest.remove_prefix(std::min(end + 1, rest.size()));
		}
	}
	Color c = svgColor(color).value_or(Color{0.0f, 0.0f, 0.0f, 1.0f});
	c.a *= std::clamp(svgNumber(opacity).value_or(1.0f), 0.0f, 1.0f);
	return {offsetOf(tag.attribute("offset").value_or("0")), c};
}

/// @brief Reads a gradient element's attributes (its stops come after it).
/// @param tag The linearGradient or radialGradient.
/// @return The gradient, without stops.
[[nodiscard]] Gradient gradientOf(const XmlTag& tag) {
	Gradient gradient;
	gradient.radial = tag.name == "radialGradient";
	gradient.userSpace = tag.attribute("gradientUnits").value_or("") == "userSpaceOnUse";
	const auto number = [&](std::string_view key, float fallback) {
		const auto value = tag.attribute(key);
		if(!value) return fallback;
		std::string_view text = *value;
		const bool percent = text.ends_with('%');
		if(percent) text.remove_suffix(1);
		return svgNumber(text).value_or(fallback * (percent ? 100.0f : 1.0f)) / (percent ? 100.0f : 1.0f);
	};
	gradient.geometry =
	    gradient.radial
	        ? std::array<float, 4>{number("cx", 0.5f), number("cy", 0.5f), number("r", 0.5f), 0.0f}
	        : std::array<float, 4>{number("x1", 0.0f), number("y1", 0.0f), number("x2", 1.0f), number("y2", 0.0f)};
	if(const auto transform = tag.attribute("gradientTransform")) gradient.transform = svgTransform(*transform);
	std::string_view href = tag.attribute("href").value_or(tag.attribute("xlink:href").value_or(""));
	if(href.starts_with('#')) gradient.inherits = std::string(href.substr(1, 128));
	return gradient;
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
	/// @brief Fills a mesh with a colour, or with the gradient a paint names.
	/// @param color The colour (its alpha times the opacities).
	/// @param gradient The gradient's id, or "".
	/// @param opacity The opacities.
	/// @param toUser From pixels to the shape's units.
	/// @param box The shape's box, in its units.
	void paint(const Color& color, const std::string& gradient, float opacity, const Affine2& toUser, const Rect& box);
	/// @brief Reads a gradient and its stops, up to its closing tag.
	/// @param tag The gradient's tag.
	void readGradient(const XmlTag& tag);

	XmlReader& reader_;                                       ///< The document.
	Canvas canvas_;                                           ///< The pixels.
	std::vector<SvgStyle> styles_;                            ///< The styles of the open elements, the root's first.
	std::size_t skipping_ = 0;                                ///< How deep inside skipped elements reading is.
	std::size_t elements_ = 1;                                ///< Elements read.
	std::size_t fillWork_ = 0;                                ///< The fills' edge visits so far.
	std::vector<MeshVertex> mesh_;                            ///< Reused for each fill and stroke.
	std::vector<std::pair<std::string, Gradient>> gradients_; ///< The gradients defined so far, by id.
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
[[nodiscard]] Affine2 rootTransform(const XmlTag& root, const std::vector<float>& viewBox, Vec2 size) {
	Rect box{0.0f, 0.0f, numberOf(root, "width"), numberOf(root, "height")};
	if(viewBox.size() == 4 && viewBox[2] > 0.0f && viewBox[3] > 0.0f)
		box = {viewBox[0], viewBox[1], viewBox[0] + viewBox[2], viewBox[1] + viewBox[3]};
	if(!(box.width() > 0.0f) || !(box.height() > 0.0f)) return Affine2::scaling({2.0f, 2.0f});
	const float scale = std::min(size.x / box.width(), size.y / box.height());
	return {scale,
	        0.0f,
	        0.0f,
	        scale,
	        (size.x - box.width() * scale) / 2.0f - box.left * scale,
	        (size.y - box.height() * scale) / 2.0f - box.top * scale};
}

/// @brief The canvas for a root element.
[[nodiscard]] Canvas canvasFor(const XmlTag& root) {
	const Vec2 size = imageSize(root, svgNumbers(root.attribute("viewBox").value_or("")));
	return {static_cast<int>(size.x), static_cast<int>(size.y), workLimit};
}

Renderer::Renderer(XmlReader& reader, const XmlTag& root) : reader_(reader), canvas_(canvasFor(root)) {
	const std::vector<float> viewBox = svgNumbers(root.attribute("viewBox").value_or(""));
	SvgStyle style;
	style.transform =
	    rootTransform(root, viewBox, {static_cast<float>(canvas_.width()), static_cast<float>(canvas_.height())});
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
	if(tag.name == "linearGradient" || tag.name == "radialGradient") { // in defs too: paint, not drawn
		readGradient(tag);
		return;
	}
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

void Renderer::readGradient(const XmlTag& tag) {
	Gradient gradient = gradientOf(tag);
	if(!tag.selfClosing)
		for(;;) {
			const std::optional<XmlTag> inner = reader_.next();
			if(!inner) throw std::runtime_error("AffrescoUI: broken SVG (a gradient isn't closed)");
			if(++elements_ > elementLimit) throw std::runtime_error("AffrescoUI: SVG refused: too many elements");
			if(inner->closing && inner->name == tag.name) break;
			if(!inner->closing && inner->name == "stop" && gradient.stops.size() < stopLimit) {
				auto stop = stopOf(*inner);
				if(!gradient.stops.empty()) stop.first = std::max(stop.first, gradient.stops.back().first);
				gradient.stops.push_back(stop);
			}
		}
	if(gradient.stops.empty() && !gradient.inherits.empty()) // href: the stops of a gradient defined before
		for(const auto& [id, known] : gradients_)
			if(id == gradient.inherits) gradient.stops = known.stops;
	const auto id = tag.attribute("id");
	if(id && !id->empty() && gradients_.size() < gradientLimit) gradients_.emplace_back(std::string(*id), gradient);
}

void Renderer::paint(const Color& color, const std::string& gradient, float opacity, const Affine2& toUser,
                     const Rect& box) {
	if(gradient.empty()) {
		canvas_.fill(mesh_, {color.r, color.g, color.b, color.a * opacity});
		return;
	}
	const auto found = std::ranges::find(gradients_, gradient, &std::pair<std::string, Gradient>::first);
	if(found == gradients_.end()) return; // a gradient not defined before its use paints nothing
	const Gradient& g = found->second;
	// From pixels to the gradient's own space: the shape's units, its box's when the units are the box's
	Affine2 toGradient = toUser;
	if(!g.userSpace) {
		if(!(box.width() > 0.0f) || !(box.height() > 0.0f)) return;
		toGradient = Affine2{1.0f / box.width(),     0.0f, 0.0f, 1.0f / box.height(), -box.left / box.width(),
		                     -box.top / box.height()} *
		             toGradient;
	}
	const auto inverse = g.transform.inverse();
	if(!inverse) return;
	toGradient = *inverse * toGradient;
	const std::array<float, 4> q = g.geometry;
	const Vec2 axis{q[2] - q[0], q[3] - q[1]};
	const float length = axis.x * axis.x + axis.y * axis.y;
	canvas_.fill(mesh_, [&](Vec2 pixel) {
		const Vec2 p = toGradient.apply(pixel);
		float t = 0.0f;
		if(g.radial) t = q[2] > 0.0f ? std::hypot(p.x - q[0], p.y - q[1]) / q[2] : 1.0f;
		else if(length > 0.0f) t = ((p.x - q[0]) * axis.x + (p.y - q[1]) * axis.y) / length;
		Color c = colorAt(g.stops, std::clamp(t, 0.0f, 1.0f));
		c.a *= opacity;
		return c;
	});
}

void Renderer::draw(const VectorPath& path, const SvgStyle& style) {
	if(path.empty()) return;
	// Without rotation or skew, flattened straight into pixels; otherwise at the transform's scale, then mapped through
	// the whole of it
	const Affine2& m = style.transform;
	const bool aligned = m.b == 0.0f && m.c == 0.0f;
	const float scale =
	    aligned ? std::sqrt(std::abs(m.a * m.d)) : std::max(std::sqrt(std::abs(m.determinant())), 1.0e-6f);
	auto contours = aligned ? path.flatten({{m.a, m.d}, {m.tx, m.ty}}) : path.flatten({{scale, scale}, {}});
	if(!aligned)
		for(auto& contour : contours)
			for(Vec2& point : contour.points) point = m.apply({point.x / scale, point.y / scale});
	Rect box{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
	         -std::numeric_limits<float>::max()};
	if(!style.fillGradient.empty() || !style.strokeGradient.empty()) // the shape's box, for a gradient in its units
		for(const auto& contour : path.flatten({}))
			for(const Vec2 point : contour.points)
				box = {std::min(box.left, point.x), std::min(box.top, point.y), std::max(box.right, point.x),
				       std::max(box.bottom, point.y)};
	const Affine2 toUser = m.inverse().value_or(Affine2{});
	if(style.fill && style.fill->a * style.opacity * style.fillOpacity > 0.0f) {
		mesh_.clear();
		// Each fill is bounded (meshWorkLimit), but a document of many that each cost as much decoded for seconds
		const std::size_t workLeft = fillWorkLimit - std::min(fillWork_, fillWorkLimit);
		fillWork_ += vector::appendFill(contours, style.fillRule, mesh_, 1.0f, std::min(workLeft, meshWorkLimit));
		paint(*style.fill, style.fillGradient, style.opacity * style.fillOpacity, toUser, box);
	}
	if(style.stroke && style.stroke->a * style.opacity * style.strokeOpacity > 0.0f && style.line.width > 0.0f) {
		StrokeStyle line = style.line;
		line.width *= scale;
		for(float& length : line.dash) length *= scale;
		mesh_.clear();
		appendStroke(contours, line, mesh_);
		paint(*style.stroke, style.strokeGradient, style.opacity * style.strokeOpacity, toUser, box);
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
	if(bytes.size() > std::min(byteLimit, limits.fileBytes))
		throw std::runtime_error("AffrescoUI: SVG larger than allowed");
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
