/// @file
/// @brief VectorPath: building, bounds and flattening, and the parser of SVG path data (arcs become cubic curves).

#include "affresco/render/VectorPath.hpp"

#include "VectorMath.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace affresco {

using vector::minus;
using vector::plus;
using vector::times;

void VectorPath::moveTo(Vec2 point) {
	verbs_.push_back(Verb::move);
	points_.push_back(point);
}

void VectorPath::lineTo(Vec2 point) {
	verbs_.push_back(Verb::line);
	points_.push_back(point);
}

void VectorPath::cubicTo(Vec2 first, Vec2 second, Vec2 end) {
	verbs_.push_back(Verb::cubic);
	points_.insert(points_.end(), {first, second, end});
}

void VectorPath::close() { verbs_.push_back(Verb::close); }

std::optional<Rect> VectorPath::bounds() const noexcept {
	if(points_.empty()) return std::nullopt;
	Rect box{points_.front().x, points_.front().y, points_.front().x, points_.front().y};
	for(const Vec2 point : points_) {
		box.left = std::min(box.left, point.x);
		box.top = std::min(box.top, point.y);
		box.right = std::max(box.right, point.x);
		box.bottom = std::max(box.bottom, point.y);
	}
	return box;
}

namespace {

/// @brief Collects flattened contours, within flattenPointLimit points.
class Flattener {
public:
	/// @brief Starts a contour.
	/// @param point Its first point, in window units.
	void start(Vec2 point) {
		finish();
		current_.points.push_back(point);
	}

	/// @brief Starts a contour at a point unless one is under way (a line after Z starts at the closed one's start).
	/// @param point Where.
	void ensureStarted(Vec2 point) {
		if(current_.points.empty()) start(point);
	}

	/// @brief Adds a point to the current contour, unless it repeats the last one or the limit is reached.
	/// @param point In window units.
	void add(Vec2 point) {
		if(count_ >= flattenPointLimit) return;
		if(current_.points.empty() || !(current_.points.back() == point)) {
			current_.points.push_back(point);
			++count_;
		}
	}

	/// @brief Adds a cubic curve from the last point, as enough straight lines to stay within the tolerance.
	/// @param from The last point; @p first, @p second, @p end The curve's other points, in window units.
	/// @param tolerance How far the lines may stray from the curve.
	void addCubic(Vec2 from, Vec2 first, Vec2 second, Vec2 end, float tolerance) {
		// The second differences bound how far a flattened curve strays: 3/4 of their size over n squared
		const Vec2 bendStart = plus(minus(from, times(first, 2.0f)), second);
		const Vec2 bendEnd = plus(minus(first, times(second, 2.0f)), end);
		const float bend = std::max(vector::length(bendStart), vector::length(bendEnd));
		const float wanted = std::ceil(std::sqrt(0.75f * bend / std::max(tolerance, 0.01f)));
		const int steps = std::isfinite(wanted) ? std::clamp(static_cast<int>(wanted), 1, 64) : 1;
		for(int step = 1; step <= steps; ++step) {
			const float t = static_cast<float>(step) / static_cast<float>(steps), u = 1.0f - t;
			const float a = u * u * u, b = 3.0f * u * u * t, c = 3.0f * u * t * t, d = t * t * t;
			add({a * from.x + b * first.x + c * second.x + d * end.x, a * from.y + b * first.y + c * second.y + d * end.y});
		}
	}

	/// @brief Closes the current contour.
	void close() {
		current_.closed = true;
		finish();
	}

	/// @brief Ends the current contour and hands every contour over.
	[[nodiscard]] std::vector<VectorPath::Contour> take() {
		finish();
		return std::move(contours_);
	}

	/// @brief The current contour's last point, or the origin.
	[[nodiscard]] Vec2 last() const noexcept { return current_.points.empty() ? Vec2{} : current_.points.back(); }

private:
	/// @brief Ends the current contour, keeping it if it has a point.
	void finish() {
		if(!current_.points.empty()) contours_.push_back(std::move(current_));
		current_ = {};
	}

	std::vector<VectorPath::Contour> contours_; ///< The finished contours.
	VectorPath::Contour current_;               ///< The contour being added to.
	std::size_t count_ = 0;                     ///< The points so far.
};

} // namespace

std::vector<VectorPath::Contour> VectorPath::flatten(const PathTransform& transform, float tolerance) const {
	Flattener flattener;
	std::size_t next = 0;
	Vec2 start{}, current{};
	for(const Verb verb : verbs_) {
		switch(verb) {
		case Verb::move:
			start = current = transform.apply(points_[next++]);
			flattener.start(current);
			break;
		case Verb::line:
			flattener.ensureStarted(current);
			current = transform.apply(points_[next++]);
			flattener.add(current);
			break;
		case Verb::cubic: {
			const Vec2 first = transform.apply(points_[next]), second = transform.apply(points_[next + 1]);
			const Vec2 end = transform.apply(points_[next + 2]);
			next += 3;
			flattener.ensureStarted(current);
			flattener.addCubic(current, first, second, end, tolerance);
			current = end;
			break;
		}
		case Verb::close:
			flattener.close();
			current = start;
			break;
		}
	}
	return flattener.take();
}

namespace {

/// @brief Reads SVG path data into a VectorPath.
class PathParser {
public:
	/// @brief Prepares to read.
	/// @param data The data.
	/// @param limits What is accepted.
	PathParser(std::string_view data, const VectorPath::Limits& limits) : data_(data), limits_(limits) {}

	/// @brief Reads the whole data.
	/// @return The path.
	VectorPath read() {
		if(data_.size() > limits_.dataBytes)
			fail("path data is " + std::to_string(data_.size()) + " bytes, more than the limit of " +
			     std::to_string(limits_.dataBytes));
		skipSpace();
		if(at_ < data_.size() && data_[at_] != 'M' && data_[at_] != 'm') fail("path data must start with M");
		char command = 0;
		while(skipSpace(), at_ < data_.size()) {
			if(std::isalpha(static_cast<unsigned char>(data_[at_])) != 0) command = data_[at_++];
			else if(command == 0 || command == 'Z' || command == 'z') fail("expected a command");
			if(++commands_ > limits_.commands)
				fail("more than " + std::to_string(limits_.commands) + " commands");
			runCommand(command);
			// Pairs after a move are lines
			if(command == 'M') command = 'L';
			else if(command == 'm') command = 'l';
		}
		return std::move(path_);
	}

private:
	/// @brief Runs one command, reading its numbers.
	/// @param command Its letter.
	void runCommand(char command) {
		const bool relative = std::islower(static_cast<unsigned char>(command)) != 0;
		const Vec2 base = relative ? current_ : Vec2{};
		switch(std::toupper(static_cast<unsigned char>(command))) {
		case 'M': moveTo(plus(base, pair())); break;
		case 'L': lineTo(plus(base, pair())); break;
		case 'H': lineTo({base.x + number(), current_.y}); break;
		case 'V': lineTo({current_.x, base.y + number()}); break;
		case 'C': runCubic(base, false); break;
		case 'S': runCubic(base, true); break;
		case 'Q': runQuadratic(base, false); break;
		case 'T': runQuadratic(base, true); break;
		case 'A': runArc(base); break;
		case 'Z': close(); break;
		default: fail(std::string("unknown command '") + command + "'");
		}
	}

	/// @brief Starts a contour.
	void moveTo(Vec2 point) {
		path_.moveTo(point);
		current_ = start_ = control_ = point;
		curve_ = Curve::none;
	}

	/// @brief Adds a line.
	void lineTo(Vec2 point) {
		ensureStarted();
		path_.lineTo(point);
		current_ = control_ = point;
		curve_ = Curve::none;
	}

	/// @brief Closes the contour; the next command starts at its start.
	void close() {
		ensureStarted();
		path_.close();
		current_ = control_ = start_;
		curve_ = Curve::none;
		closed_ = true;
	}

	/// @brief After Z, a drawing command starts a new contour at the old one's start, as SVG says.
	void ensureStarted() {
		if(!closed_) return;
		path_.moveTo(start_);
		closed_ = false;
	}

	/// @brief Reads C or S (@p smooth): a cubic curve, the first control point reflected for S.
	void runCubic(Vec2 base, bool smooth) {
		const Vec2 first = smooth ? reflected(Curve::cubic) : plus(base, pair());
		const Vec2 second = plus(base, pair()), end = plus(base, pair());
		addCubic(first, second, end);
		curve_ = Curve::cubic;
	}

	/// @brief Reads Q or T (@p smooth): a quadratic curve, which is added as the cubic curve equal to it.
	void runQuadratic(Vec2 base, bool smooth) {
		const Vec2 control = smooth ? reflected(Curve::quadratic) : plus(base, pair());
		const Vec2 end = plus(base, pair()), from = current_;
		addCubic(plus(from, times(minus(control, from), 2.0f / 3.0f)), plus(end, times(minus(control, end), 2.0f / 3.0f)),
		         end);
		control_ = control;
		curve_ = Curve::quadratic;
	}

	/// @brief Adds a cubic curve and moves on to its end.
	void addCubic(Vec2 first, Vec2 second, Vec2 end) {
		ensureStarted();
		path_.cubicTo(first, second, end);
		control_ = second;
		current_ = end;
	}

	/// @brief Reads A: an elliptical arc, added as cubic curves.
	void runArc(Vec2 base) {
		const float radiusX = std::abs(number()), radiusY = std::abs(number()), rotation = number();
		const bool large = flag(), sweep = flag();
		const Vec2 end = plus(base, pair());
		appendArc(current_, {radiusX, radiusY}, rotation, large, sweep, end);
		control_ = current_ = end;
		curve_ = Curve::none;
	}

	/// @brief The kinds of curve a smooth command can follow.
	enum class Curve : std::uint8_t { none, cubic, quadratic };

	/// @brief The control point a smooth curve starts with: the last one reflected, if the last command was a curve
	/// of the kind, else the current point.
	[[nodiscard]] Vec2 reflected(Curve kind) const noexcept {
		return curve_ == kind ? minus(times(current_, 2.0f), control_) : current_;
	}

	void appendArc(Vec2 from, Vec2 radii, float degrees, bool large, bool sweep, Vec2 to);
	void appendArcSegments(Vec2 center, Vec2 radii, float rotation, float startAngle, float sweepAngle);
	void skipSpace() noexcept;
	[[nodiscard]] Vec2 pair();
	[[nodiscard]] float number();
	[[nodiscard]] bool flag();
	[[noreturn]] void fail(const std::string& message) const;

	std::string_view data_;           ///< The data.
	VectorPath::Limits limits_;       ///< What is accepted.
	std::size_t at_ = 0;              ///< Where reading is.
	std::size_t commands_ = 0;        ///< Commands read.
	VectorPath path_;                 ///< The path so far.
	Vec2 current_{};                  ///< The current point.
	Vec2 start_{};                    ///< Where the contour started.
	Vec2 control_{};                  ///< The last curve's last control point.
	Curve curve_ = Curve::none;       ///< The kind of the last command, if a curve.
	bool closed_ = false;             ///< Whether the last command was Z.
};

void PathParser::appendArc(Vec2 from, Vec2 radii, float degrees, bool large, bool sweep, Vec2 to) {
	ensureStarted();
	if(from == to) return;
	if(radii.x <= 0.0f || radii.y <= 0.0f) { // SVG: an arc without radii is a line
		path_.lineTo(to);
		return;
	}
	// The SVG implementation notes, F.6.5: from end points to centre
	const double rotation = static_cast<double>(degrees) * std::numbers::pi / 180.0;
	const double cosine = std::cos(rotation), sine = std::sin(rotation);
	const double halfX = (from.x - to.x) / 2.0, halfY = (from.y - to.y) / 2.0;
	const double x1 = cosine * halfX + sine * halfY, y1 = -sine * halfX + cosine * halfY;
	double rx = radii.x, ry = radii.y;
	const double grow = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
	if(grow > 1.0) { // radii too small to reach: scaled up, as SVG says
		rx *= std::sqrt(grow);
		ry *= std::sqrt(grow);
	}
	const double numerator = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
	const double denominator = rx * rx * y1 * y1 + ry * ry * x1 * x1;
	double factor = denominator > 0.0 ? std::sqrt(std::max(0.0, numerator / denominator)) : 0.0;
	if(large == sweep) factor = -factor;
	const double cx1 = factor * rx * y1 / ry, cy1 = -factor * ry * x1 / rx;
	const Vec2 center{static_cast<float>(cosine * cx1 - sine * cy1 + (from.x + to.x) / 2.0),
	                  static_cast<float>(sine * cx1 + cosine * cy1 + (from.y + to.y) / 2.0)};
	const double startAngle = std::atan2((y1 - cy1) / ry, (x1 - cx1) / rx);
	double sweepAngle = std::atan2((-y1 - cy1) / ry, (-x1 - cx1) / rx) - startAngle;
	if(sweep && sweepAngle < 0.0) sweepAngle += 2.0 * std::numbers::pi;
	if(!sweep && sweepAngle > 0.0) sweepAngle -= 2.0 * std::numbers::pi;
	appendArcSegments(center, {static_cast<float>(rx), static_cast<float>(ry)}, static_cast<float>(rotation),
	                  static_cast<float>(startAngle), static_cast<float>(sweepAngle));
	current_ = to;
}

void PathParser::appendArcSegments(Vec2 center, Vec2 radii, float rotation, float startAngle, float sweepAngle) {
	// Each piece of at most a quarter turn is one cubic curve, its handles 4/3 tan(angle / 4) long
	const int pieces = std::clamp(static_cast<int>(std::ceil(std::abs(sweepAngle) / (std::numbers::pi_v<float> / 2.0f))),
	                              1, 4);
	const float step = sweepAngle / static_cast<float>(pieces), handle = 4.0f / 3.0f * std::tan(step / 4.0f);
	const float cosine = std::cos(rotation), sine = std::sin(rotation);
	const auto onEllipse = [&](float x, float y) {
		return Vec2{center.x + cosine * radii.x * x - sine * radii.y * y, center.y + sine * radii.x * x + cosine * radii.y * y};
	};
	float angle = startAngle;
	for(int piece = 0; piece < pieces; ++piece) {
		const float next = angle + step;
		const float c0 = std::cos(angle), s0 = std::sin(angle), c1 = std::cos(next), s1 = std::sin(next);
		path_.cubicTo(onEllipse(c0 - handle * s0, s0 + handle * c0), onEllipse(c1 + handle * s1, s1 - handle * c1),
		              onEllipse(c1, s1));
		angle = next;
	}
}

void PathParser::skipSpace() noexcept {
	while(at_ < data_.size() && (std::isspace(static_cast<unsigned char>(data_[at_])) != 0 || data_[at_] == ','))
		++at_;
}

Vec2 PathParser::pair() {
	const float x = number();
	return {x, number()};
}

float PathParser::number() {
	skipSpace();
	const std::size_t begin = at_;
	const auto digit = [&] { return at_ < data_.size() && data_[at_] >= '0' && data_[at_] <= '9'; };
	double sign = 1.0, mantissa = 0.0;
	int exponent = 0, digits = 0;
	if(at_ < data_.size() && (data_[at_] == '-' || data_[at_] == '+')) sign = data_[at_++] == '-' ? -1.0 : 1.0;
	for(; digit(); ++at_, ++digits) mantissa = mantissa * 10.0 + (data_[at_] - '0');
	if(at_ < data_.size() && data_[at_] == '.') // "0.5.5" is 0.5 then .5
		for(++at_; digit(); ++at_, ++digits, --exponent) mantissa = mantissa * 10.0 + (data_[at_] - '0');
	if(digits == 0) {
		at_ = begin;
		fail("expected a number");
	}
	if(at_ + 1 < data_.size() && (data_[at_] == 'e' || data_[at_] == 'E') &&
	   (std::isdigit(static_cast<unsigned char>(data_[at_ + 1])) != 0 || data_[at_ + 1] == '-' || data_[at_ + 1] == '+')) {
		++at_;
		const int exponentSign = data_[at_] == '-' ? -1 : 1;
		if(data_[at_] == '-' || data_[at_] == '+') ++at_;
		int written = 0;
		for(; digit(); ++at_) written = std::min(written * 10 + (data_[at_] - '0'), 1000);
		exponent += exponentSign * written;
	}
	const double value = sign * mantissa * std::pow(10.0, exponent);
	if(!std::isfinite(value) || std::abs(value) > static_cast<double>(limits_.magnitude)) {
		at_ = begin;
		fail("number out of range");
	}
	return static_cast<float>(value);
}

bool PathParser::flag() {
	skipSpace();
	if(at_ < data_.size() && (data_[at_] == '0' || data_[at_] == '1')) return data_[at_++] == '1';
	fail("expected an arc flag, 0 or 1");
}

void PathParser::fail(const std::string& message) const {
	throw std::invalid_argument(message + " at character " + std::to_string(at_));
}

} // namespace

VectorPath VectorPath::parse(std::string_view data, const Limits& limits) { return PathParser(data, limits).read(); }

VectorPath VectorPath::parse(std::string_view data) { return parse(data, Limits{}); }

} // namespace affresco
