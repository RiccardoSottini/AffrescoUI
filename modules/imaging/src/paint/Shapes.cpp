/// @file
/// @brief Vector content drawn: paths flattened, strokes as unions of segments and round joins, and the pixels of type
/// and shape layers.

#include "affresco/imaging/Shapes.hpp"

#include "affresco/render/Font.hpp"
#include "affresco/render/RichText.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <cmath>
#include <charconv>
#include <numbers>
#include <string_view>

namespace affresco::imaging {

namespace {

/// @brief The names of the content kinds, in ContentKind's order.
constexpr std::array<std::string_view, 3> contentNames{"none", "text", "shape"};
/// @brief The names of the shape kinds, in ShapeKind's order.
constexpr std::array<std::string_view, 5> shapeNames{"rectangle", "ellipse", "polygon", "line", "path"};
/// @brief The names of the alignments, in TextAlign's order.
constexpr std::array<std::string_view, 3> alignNames{"left", "center", "right"};
/// @brief The points of a full turn of an ellipse's outline.
constexpr int ellipseSteps = 96;
/// @brief The points of a quarter turn of a rounded corner, or of a round join.
constexpr int cornerSteps = 10;

/// @brief A vector point as a pixel point.
/// @param point The point.
[[nodiscard]] Point pointOf(VectorPoint point) noexcept { return {point.x, point.y}; }

/// @brief Whether a handle sits on its anchor (so the curve is straight there).
/// @param handle The handle.
/// @param anchor The anchor.
[[nodiscard]] bool onAnchor(VectorPoint handle, VectorPoint anchor) noexcept {
	return std::abs(handle.x - anchor.x) < 1e-3f && std::abs(handle.y - anchor.y) < 1e-3f;
}

/// @brief Adds the points of one curve after its start: a cubic Bézier, or the end alone when it is straight.
/// @param from The anchor it leaves.
/// @param to The anchor it arrives at.
/// @param out Where the points go.
void addCurve(const PathPoint& from, const PathPoint& to, std::vector<Point>& out) {
	if(onAnchor(from.out, from.anchor) && onAnchor(to.in, to.anchor)) {
		out.push_back(pointOf(to.anchor));
		return;
	}
	const float span = std::hypot(from.out.x - from.anchor.x, from.out.y - from.anchor.y) +
	                   std::hypot(to.in.x - from.out.x, to.in.y - from.out.y) +
	                   std::hypot(to.anchor.x - to.in.x, to.anchor.y - to.in.y);
	const int steps = std::clamp(static_cast<int>(std::isfinite(span) ? span / 3.0f : 4.0f), 4, 64);
	for(int step = 1; step <= steps; ++step) {
		const float t = static_cast<float>(step) / static_cast<float>(steps), u = 1.0f - t;
		const float a = u * u * u, b = 3.0f * u * u * t, c = 3.0f * u * t * t, d = t * t * t;
		out.push_back({a * from.anchor.x + b * from.out.x + c * to.in.x + d * to.anchor.x,
		               a * from.anchor.y + b * from.out.y + c * to.in.y + d * to.anchor.y});
	}
}

/// @brief Adds an arc of an ellipse.
/// @param centre Its centre.
/// @param radiusX Its radius across.
/// @param radiusY Its radius down.
/// @param from The angle it starts at, in radians.
/// @param turn The angle it turns through.
/// @param steps Its segments.
/// @param out Where the points go (its start included).
void addArc(Point centre, float radiusX, float radiusY, float from, float turn, int steps, std::vector<Point>& out) {
	for(int step = 0; step <= steps; ++step) {
		const float angle = from + turn * static_cast<float>(step) / static_cast<float>(steps);
		out.push_back({centre.x + radiusX * std::cos(angle), centre.y + radiusY * std::sin(angle)});
	}
}

/// @brief Takes a coverage into another: the larger of the two at each pixel.
/// @param into The coverage that grows.
/// @param more The coverage added.
void unite(Coverage& into, const Coverage& more) {
	for(const auto& [place, tile] : more) {
		auto [found, added] = into.try_emplace(place, tile);
		if(added) continue;
		auto merged = std::make_shared<Tile>(*found->second);
		const auto source = tile->bytes();
		const auto target = merged->bytes();
		for(std::size_t index = 0; index < target.size(); ++index)
			target[index] = std::max(target[index], source[index]);
		found->second = std::move(merged);
	}
}

/// @brief A transparent image of the document's size, or a DocumentError.
/// @param width Its width.
/// @param height Its height.
/// @param format Its format.
/// @param limits The limits.
[[nodiscard]] std::shared_ptr<Image> blankImage(int width, int height, PixelFormat format, const Limits& limits) {
	try {
		return std::make_shared<Image>(width, height, format, limits);
	} catch(const std::invalid_argument& error) {
		throw DocumentError(error.what());
	}
}

/// @brief One line's sample of a glyph's coverage, bilinear, 0 outside its page.
/// @param pixels The page's texels.
/// @param side The page's side.
/// @param u Across, in texels.
/// @param v Down, in texels.
[[nodiscard]] float sampleCoverage(std::span<const std::uint8_t> pixels, int side, float u, float v) noexcept {
	const float x = u - 0.5f, y = v - 0.5f;
	const int left = static_cast<int>(std::floor(x)), top = static_cast<int>(std::floor(y));
	const float fx = x - static_cast<float>(left), fy = y - static_cast<float>(top);
	const auto at = [&](int column, int row) -> float {
		if(column < 0 || row < 0 || column >= side || row >= side) return 0.0f;
		return static_cast<float>(pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(side) +
		                                 static_cast<std::size_t>(column)]) /
		       255.0f;
	};
	const float topRow = at(left, top) * (1.0f - fx) + at(left + 1, top) * fx;
	const float bottomRow = at(left, top + 1) * (1.0f - fx) + at(left + 1, top + 1) * fx;
	return topRow * (1.0f - fy) + bottomRow * fy;
}

/// @brief The lines of a type layer's text.
/// @param text The text.
[[nodiscard]] std::vector<std::u32string> linesOf(const std::string& text) {
	std::vector<std::u32string> lines(1);
	for(const char32_t codepoint : codepointsOf(text)) {
		if(codepoint == U'\n') lines.emplace_back();
		else if(codepoint != U'\r') lines.back().push_back(codepoint);
	}
	return lines;
}

/// @brief Where a line starts across, after its alignment.
/// @param content The content.
/// @param width The line's width.
[[nodiscard]] float lineStart(const LayerContent& content, float width) noexcept {
	switch(content.align) {
	case TextAlign::center:
		return content.origin.x - width / 2.0f;
	case TextAlign::right:
		return content.origin.x - width;
	case TextAlign::left:
		break;
	}
	return content.origin.x;
}

/// @brief A colour as "#rrggbbaa".
/// @param pixel The colour.
[[nodiscard]] std::string hexOfPixel(const Pixel& pixel) {
	const Pixel colour = clamped(pixel);
	std::array<char, 10> text{};
	std::snprintf(text.data(), text.size(), "#%02x%02x%02x%02x", static_cast<unsigned>(byteFromUnit(colour.red)),
	              static_cast<unsigned>(byteFromUnit(colour.green)), static_cast<unsigned>(byteFromUnit(colour.blue)),
	              static_cast<unsigned>(byteFromUnit(colour.alpha)));
	return text.data();
}

/// @brief A colour from "#rrggbb" or "#rrggbbaa".
/// @param value The text.
/// @return It, or std::nullopt.
[[nodiscard]] std::optional<Pixel> pixelOfHex(const json& value) {
	if(!value.is_string()) return std::nullopt;
	std::string text = value.get<std::string>();
	if(text.starts_with('#')) text.erase(0, 1);
	if(text.size() != 6 && text.size() != 8) return std::nullopt;
	std::array<float, 4> channels{0.0f, 0.0f, 0.0f, 1.0f};
	for(std::size_t index = 0; index * 2 < text.size(); ++index) {
		unsigned byte = 0;
		const char* begin = text.data() + index * 2;
		const auto [end, error] = std::from_chars(begin, begin + 2, byte, 16);
		if(error != std::errc{} || end != begin + 2) return std::nullopt;
		channels[index] = unitFromByte(static_cast<std::uint8_t>(byte));
	}
	return Pixel{channels[0], channels[1], channels[2], channels[3]};
}

/// @brief A finite number of an object, clamped to a range, or the fallback.
/// @param value The object.
/// @param key The key.
/// @param low The least.
/// @param high The most.
/// @param fallback When absent.
/// @throws DocumentError If it is present but not a finite number.
[[nodiscard]] float numberIn(const json& value, const char* key, float low, float high, float fallback) {
	const auto found = value.find(key);
	if(found == value.end()) return fallback;
	if(!found->is_number() || !std::isfinite(found->get<double>()))
		throw DocumentError(std::string(key) + " is a number");
	return static_cast<float>(std::clamp(found->get<double>(), static_cast<double>(low), static_cast<double>(high)));
}

/// @brief The index of a name among names, or a DocumentError.
/// @param names The names.
/// @param value The name.
/// @param what What it names, for the error.
[[nodiscard]] std::size_t indexIn(std::span<const std::string_view> names, const json& value, const char* what) {
	if(value.is_string())
		for(std::size_t index = 0; index < names.size(); ++index)
			if(names[index] == value.get<std::string>()) return index;
	throw DocumentError(std::string("unknown ") + what);
}

/// @brief The farthest a coordinate of vector content goes from the image.
constexpr float farthest = 1e6f;

} // namespace

std::span<const std::string_view> contentKindNames() noexcept { return contentNames; }

std::span<const std::string_view> shapeKindNames() noexcept { return shapeNames; }

std::span<const std::string_view> textAlignNames() noexcept { return alignNames; }

std::vector<Point> flattenPath(std::span<const PathPoint> points, bool closed) {
	std::vector<Point> out;
	if(points.empty()) return out;
	out.push_back(pointOf(points.front().anchor));
	for(std::size_t index = 1; index < points.size(); ++index) addCurve(points[index - 1], points[index], out);
	if(closed && points.size() > 1) {
		addCurve(points.back(), points.front(), out);
		out.pop_back(); // back at the first point
	}
	return out;
}

std::vector<Point> shapeOutline(const LayerContent& content) {
	const float left = std::min(content.origin.x, content.corner.x),
	            right = std::max(content.origin.x, content.corner.x);
	const float top = std::min(content.origin.y, content.corner.y),
	            bottom = std::max(content.origin.y, content.corner.y);
	const Point centre{(left + right) / 2.0f, (top + bottom) / 2.0f};
	const float radiusX = (right - left) / 2.0f, radiusY = (bottom - top) / 2.0f;
	constexpr float pi = std::numbers::pi_v<float>;
	std::vector<Point> out;
	switch(content.shape) {
	case ShapeKind::rectangle: {
		const float radius = std::clamp(content.radius, 0.0f, std::min(radiusX, radiusY));
		if(radius <= 0.0f) return {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
		addArc({right - radius, top + radius}, radius, radius, -pi / 2.0f, pi / 2.0f, cornerSteps, out);
		addArc({right - radius, bottom - radius}, radius, radius, 0.0f, pi / 2.0f, cornerSteps, out);
		addArc({left + radius, bottom - radius}, radius, radius, pi / 2.0f, pi / 2.0f, cornerSteps, out);
		addArc({left + radius, top + radius}, radius, radius, pi, pi / 2.0f, cornerSteps, out);
		return out;
	}
	case ShapeKind::ellipse:
		addArc(centre, radiusX, radiusY, 0.0f, 2.0f * pi, ellipseSteps, out);
		out.pop_back();
		return out;
	case ShapeKind::polygon: {
		const int sides = std::clamp(content.sides, 3, 64);
		addArc(centre, radiusX, radiusY, -pi / 2.0f, 2.0f * pi, sides, out);
		out.pop_back();
		return out;
	}
	case ShapeKind::path:
		return flattenPath(content.points, true);
	case ShapeKind::line:
		break;
	}
	return out;
}

Coverage strokeCoverage(int width, int height, std::span<const Point> points, bool closed, float strokeWidth,
                        const Limits& limits) {
	Coverage coverage;
	if(points.empty() || !(strokeWidth > 0.0f) || !std::isfinite(strokeWidth)) return coverage;
	if(points.size() > limits.polygonPoints)
		throw DocumentError("imaging: a stroke of " + std::to_string(points.size()) + " points, beyond the limit of " +
		                    std::to_string(limits.polygonPoints));
	const float half = strokeWidth / 2.0f;
	// Each point gets a round join: bound the pixels those discs cover, so a document can't stall the drawing
	if(constexpr double maxJoinPixels = 4e8;
	   static_cast<double>(points.size()) * (2.0 * half + 1.0) * (2.0 * half + 1.0) > maxJoinPixels)
		throw DocumentError("imaging: the stroke is too large to draw (its points times its width)");
	const std::size_t segments = closed ? points.size() : points.size() - 1;
	for(std::size_t index = 0; index < segments; ++index) {
		const Point from = points[index], to = points[(index + 1) % points.size()];
		const float dx = to.x - from.x, dy = to.y - from.y, length = std::hypot(dx, dy);
		if(!(length > 0.0f)) continue;
		const float nx = -dy / length * half, ny = dx / length * half;
		const std::array<Point, 4> quad{Point{from.x + nx, from.y + ny},
		                                {to.x + nx, to.y + ny},
		                                {to.x - nx, to.y - ny},
		                                {from.x - nx, from.y - ny}};
		unite(coverage, polygonCoverage(width, height, quad, limits));
	}
	if(half >= 1.0f) // round joins and caps; thinner strokes need none
		for(const Point point : points) {
			std::vector<Point> disc;
			addArc(point, half, half, 0.0f, 2.0f * std::numbers::pi_v<float>, 4 * cornerSteps, disc);
			disc.pop_back();
			unite(coverage, polygonCoverage(width, height, disc, limits));
		}
	return coverage;
}

void paintCoverage(Image& image, const Coverage& coverage, const Pixel& colour) {
	for(const auto& [place, tile] : coverage) {
		if(place.first < 0 || place.second < 0 || place.first >= image.columns() || place.second >= image.rows())
			continue;
		Tile& target = image.writableTile(place.first, place.second);
		const auto amounts = tile->bytes();
		for(int y = 0; y < tileSize; ++y)
			for(int x = 0; x < tileSize; ++x) {
				const std::uint8_t amount =
				    amounts[(static_cast<std::size_t>(y) * tileSize) + static_cast<std::size_t>(x)];
				if(amount == 0) continue;
				const float alpha = colour.alpha * static_cast<float>(amount) / 255.0f;
				const Pixel below = target.pixel(x, y);
				const float out = alpha + below.alpha * (1.0f - alpha);
				if(out <= 0.0f) continue;
				const auto mix = [&](float over, float under) {
					return (over * alpha + under * below.alpha * (1.0f - alpha)) / out;
				};
				target.setPixel(
				    x, y,
				    {mix(colour.red, below.red), mix(colour.green, below.green), mix(colour.blue, below.blue), out});
			}
	}
}

std::shared_ptr<Image> renderShape(const LayerContent& content, int width, int height, PixelFormat format,
                                   const Limits& limits) {
	auto image = blankImage(width, height, format, limits);
	if(content.shape == ShapeKind::path && content.points.size() > maxPathPoints)
		throw DocumentError("A shape has at most " + std::to_string(maxPathPoints) + " points");
	const std::vector<Point> outline = shapeOutline(content);
	if(content.fill.alpha > 0.0f && outline.size() >= 3)
		paintCoverage(*image, polygonCoverage(width, height, outline, limits), content.fill);
	if(content.stroke.alpha > 0.0f && content.strokeWidth > 0.0f) {
		if(content.shape == ShapeKind::line) {
			const std::array<Point, 2> line{pointOf(content.origin), pointOf(content.corner)};
			paintCoverage(*image, strokeCoverage(width, height, line, false, content.strokeWidth, limits),
			              content.stroke);
		} else {
			paintCoverage(*image, strokeCoverage(width, height, outline, true, content.strokeWidth, limits),
			              content.stroke);
		}
	}
	return image;
}

std::array<float, 4> textBounds(const LayerContent& content, const Font& font) {
	const std::vector<std::u32string> lines = linesOf(content.text);
	const float scale = content.size / std::max(font.pixelHeight(), 1.0f);
	const float advance = content.size * std::max(content.leading, 0.1f);
	float left = content.origin.x, right = content.origin.x;
	for(const std::u32string& line : lines) {
		const float width = font.shape(line).width * scale, start = lineStart(content, width);
		left = std::min(left, start);
		right = std::max(right, start + width);
	}
	const float bottom = content.origin.y + advance * static_cast<float>(lines.size() - 1) +
	                     std::max(font.lineHeight() * scale, content.size);
	return {left, content.origin.y, right, bottom};
}

std::shared_ptr<Image> renderText(const LayerContent& content, const Font& font, int width, int height,
                                  PixelFormat format, const Limits& limits) {
	auto image = blankImage(width, height, format, limits);
	if(content.text.size() > maxTextBytes)
		throw DocumentError("A type layer holds at most " + std::to_string(maxTextBytes) + " bytes of text");
	const std::vector<std::u32string> lines = linesOf(content.text);
	const float advance = content.size * std::max(content.leading, 0.1f);
	const float scale = content.size / std::max(font.pixelHeight(), 1.0f);
	float baseline = content.origin.y + font.ascent() * scale;
	for(const std::u32string& line : lines) {
		const Font::ShapedText& shaped = font.shape(line);
		const float start = lineStart(content, shaped.width * scale);
		for(const Font::ShapedGlyph& placed : shaped.glyphs) {
			const Font::Glyph& glyph = *placed.glyph;
			if(glyph.color) continue; // colour glyphs aren't drawn into a layer yet
			const int side = font.atlasSize(glyph.page);
			const auto pixels = font.atlasPixels(glyph.page);
			if(side <= 0 || pixels.empty()) continue;
			const float x0 = start + (placed.x + glyph.quad.left) * scale;
			const float y0 = baseline + (placed.y + glyph.quad.top) * scale;
			const float x1 = start + (placed.x + glyph.quad.right) * scale;
			const float y1 = baseline + (placed.y + glyph.quad.bottom) * scale;
			if(!(x1 > x0) || !(y1 > y0)) continue;
			const float u0 = glyph.uv.left * static_cast<float>(side), v0 = glyph.uv.top * static_cast<float>(side);
			const float du = (glyph.uv.right - glyph.uv.left) * static_cast<float>(side) / (x1 - x0);
			const float dv = (glyph.uv.bottom - glyph.uv.top) * static_cast<float>(side) / (y1 - y0);
			const int left = std::max(0, static_cast<int>(std::floor(x0))),
			          right = std::min(width, static_cast<int>(std::ceil(x1)));
			const int top = std::max(0, static_cast<int>(std::floor(y0))),
			          bottom = std::min(height, static_cast<int>(std::ceil(y1)));
			for(int y = top; y < bottom; ++y)
				for(int x = left; x < right; ++x) {
					const float u = u0 + (static_cast<float>(x) + 0.5f - x0) * du;
					const float v = v0 + (static_cast<float>(y) + 0.5f - y0) * dv;
					const float amount = sampleCoverage(pixels, side, u, v);
					if(amount <= 0.0f) continue;
					const Pixel below = image->pixel(x, y);
					const float alpha = content.fill.alpha * amount;
					const float out = alpha + below.alpha * (1.0f - alpha);
					image->setPixel(x, y, {content.fill.red, content.fill.green, content.fill.blue, out});
				}
		}
		baseline += advance;
	}
	return image;
}

LayerContent movedContent(LayerContent content, float dx, float dy) {
	const auto move = [&](VectorPoint& point) {
		point.x += dx;
		point.y += dy;
	};
	move(content.origin);
	move(content.corner);
	for(PathPoint& point : content.points) {
		move(point.anchor);
		move(point.in);
		move(point.out);
	}
	return content;
}

json pointsToJson(std::span<const PathPoint> points) {
	json out = json::array();
	for(const PathPoint& point : points)
		out.push_back({{"x", point.anchor.x},
		               {"y", point.anchor.y},
		               {"inX", point.in.x},
		               {"inY", point.in.y},
		               {"outX", point.out.x},
		               {"outY", point.out.y}});
	return out;
}

std::vector<PathPoint> pointsFromJson(const json& value) {
	if(!value.is_array()) throw DocumentError("points is an array");
	if(value.size() > maxPathPoints) throw DocumentError("at most " + std::to_string(maxPathPoints) + " points");
	std::vector<PathPoint> points;
	points.reserve(value.size());
	for(const json& item : value) {
		if(!item.is_object()) throw DocumentError("a point is an object");
		PathPoint point;
		point.anchor = {numberIn(item, "x", -farthest, farthest, 0.0f), numberIn(item, "y", -farthest, farthest, 0.0f)};
		point.in = {numberIn(item, "inX", -farthest, farthest, point.anchor.x),
		            numberIn(item, "inY", -farthest, farthest, point.anchor.y)};
		point.out = {numberIn(item, "outX", -farthest, farthest, point.anchor.x),
		             numberIn(item, "outY", -farthest, farthest, point.anchor.y)};
		points.push_back(point);
	}
	return points;
}

json contentToJson(const LayerContent& content) {
	json value{{"kind", contentNames.at(static_cast<std::size_t>(content.kind))},
	           {"x", content.origin.x},
	           {"y", content.origin.y},
	           {"fill", hexOfPixel(content.fill)}};
	if(content.kind != ContentKind::shape) { // none: both, as a tool's settings for new type and shapes
		value.update({{"text", content.text},
		              {"font", content.font},
		              {"size", content.size},
		              {"leading", content.leading},
		              {"bold", content.bold},
		              {"italic", content.italic},
		              {"align", alignNames.at(static_cast<std::size_t>(content.align))}});
	}
	if(content.kind != ContentKind::text) {
		value.update({{"shape", shapeNames.at(static_cast<std::size_t>(content.shape))},
		              {"x2", content.corner.x},
		              {"y2", content.corner.y},
		              {"radius", content.radius},
		              {"sides", content.sides},
		              {"stroke", hexOfPixel(content.stroke)},
		              {"strokeWidth", content.strokeWidth},
		              {"closed", content.closed}});
		if(content.shape == ShapeKind::path && content.kind == ContentKind::shape)
			value["points"] = pointsToJson(content.points);
	}
	return value;
}

namespace {

/// @brief Reads the type fields of a layer's content (text, font, size, leading, bold, italic, align), each kept
/// when absent.
/// @param value The content's JSON object.
/// @param content The content to update.
/// @throws DocumentError If a field is invalid.
void readTextFields(const json& value, LayerContent& content) {
	if(const auto text = value.find("text"); text != value.end()) {
		if(!text->is_string() || text->get<std::string>().size() > maxTextBytes)
			throw DocumentError("text is at most " + std::to_string(maxTextBytes) + " bytes");
		content.text = text->get<std::string>();
	}
	if(const auto font = value.find("font"); font != value.end()) {
		if(!font->is_string() || font->get<std::string>().size() > 64) throw DocumentError("font is a short name");
		content.font = font->get<std::string>();
	}
	content.size = numberIn(value, "size", 1.0f, 1000.0f, content.size);
	content.leading = numberIn(value, "leading", 0.5f, 5.0f, content.leading);
	if(const auto bold = value.find("bold"); bold != value.end() && bold->is_boolean())
		content.bold = bold->get<bool>();
	if(const auto italic = value.find("italic"); italic != value.end() && italic->is_boolean())
		content.italic = italic->get<bool>();
	if(value.contains("align"))
		content.align = static_cast<TextAlign>(indexIn(alignNames, value["align"], "alignment"));
}

/// @brief Reads the shape fields of a layer's content (shape, radius, sides, strokeWidth, closed, points), each kept
/// when absent.
/// @param value The content's JSON object.
/// @param content The content to update.
/// @throws DocumentError If a field is invalid.
void readShapeFields(const json& value, LayerContent& content) {
	if(value.contains("shape")) content.shape = static_cast<ShapeKind>(indexIn(shapeNames, value["shape"], "shape"));
	content.radius = numberIn(value, "radius", 0.0f, farthest, content.radius);
	content.sides = static_cast<int>(numberIn(value, "sides", 3.0f, 64.0f, static_cast<float>(content.sides)));
	content.strokeWidth = numberIn(value, "strokeWidth", 0.0f, 1000.0f, content.strokeWidth);
	if(const auto closed = value.find("closed"); closed != value.end() && closed->is_boolean())
		content.closed = closed->get<bool>();
	if(value.contains("points")) content.points = pointsFromJson(value["points"]);
}

} // namespace

void readContent(const json& value, LayerContent& content) {
	if(!value.is_object()) throw DocumentError("a layer's content is an object");
	if(value.contains("kind"))
		content.kind = static_cast<ContentKind>(indexIn(contentNames, value["kind"], "content kind"));
	readTextFields(value, content);
	content.origin = {numberIn(value, "x", -farthest, farthest, content.origin.x),
	                  numberIn(value, "y", -farthest, farthest, content.origin.y)};
	content.corner = {numberIn(value, "x2", -farthest, farthest, content.corner.x),
	                  numberIn(value, "y2", -farthest, farthest, content.corner.y)};
	readShapeFields(value, content);
	for(const auto& [key, colour] : {std::pair{"fill", &content.fill}, std::pair{"stroke", &content.stroke}})
		if(const auto found = value.find(key); found != value.end()) {
			const auto parsed = pixelOfHex(*found);
			if(!parsed) throw DocumentError(std::string(key) + " is a colour \"#rrggbb\"");
			*colour = *parsed;
		}
}

} // namespace affresco::imaging
