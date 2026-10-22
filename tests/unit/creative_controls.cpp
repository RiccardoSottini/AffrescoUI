/// @file
/// @brief Unit tests for the creative controls: the gradient and curve maths, the editors' operations and bounds, the
/// colour picker's fields, swatches and recent colours, the JSON values, and the curve() and gradientColor() functions.

#include "CreativeValues.hpp"
#include "Expression.hpp"

#include <affresco/render/ColorPicker.hpp>
#include <affresco/render/CurvesEditor.hpp>
#include <affresco/render/GradientEditor.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace affresco;

namespace {

/// @brief Evaluates a value with a small state ("points", "gradient"), as a screen would.
/// @param value The value, e.g. "{{curve(state.points, 0.5)}}".
/// @return The result.
/// @throws DocumentError If the value is invalid.
json evaluate(const json& value) {
	static const json state = json::parse(R"({"points": [[0, 0], [0.5, 0.8], [1, 1]], "line": [[0, 0.2], [1, 0.6]],
	    "gradient": {"kind": "linear", "angle": 90, "stops": [{"at": 0, "color": "#000000ff"},
	    {"at": 1, "color": "#ffffffff"}]}})");
	static const json window = {{"width", 800}, {"height", 600}}, revision = 1, event = json::object();
	static const json globals = json::object();
	static const format::Scope locals{};
	const format::Context context{state, window, revision, event, locals, globals};
	return format::evaluate(value, context, "test");
}

/// @brief Counts the changes a signal reports.
/// @param signal The signal.
/// @param count Incremented on each change.
template <typename Signal> void countChanges(Signal& signal, int& count) {
	signal.connect([&count](const auto&) { ++count; });
}

} // namespace

TEST_CASE("gradient maths: normalising, sampling and positions") {
	Gradient empty;
	normalizeGradient(empty);
	REQUIRE(empty.stops.size() == 2);
	CHECK(empty.stops[0].color == colors::black);
	CHECK(empty.stops[1].color == colors::white);

	Gradient messy{Gradient::Kind::Linear, -90.0, {{1.5, colors::white}, {-1.0, colors::black}}};
	normalizeGradient(messy);
	CHECK(messy.angle == doctest::Approx(270.0));
	CHECK(messy.stops[0].at == 0.0);
	CHECK(messy.stops[1].at == 1.0);

	const Color middle = sampleGradient(messy.stops, 0.5);
	CHECK(middle.r == doctest::Approx(0.5).epsilon(0.01));
	CHECK(sampleGradient({}, 0.5) == colors::transparent);

	const Gradient right{Gradient::Kind::Linear, 90.0, {}};
	CHECK(gradientPosition(right, 0.0, 0.5) == doctest::Approx(0.0));
	CHECK(gradientPosition(right, 1.0, 0.5) == doctest::Approx(1.0));
	const Gradient radial{Gradient::Kind::Radial, 0.0, {}};
	CHECK(gradientPosition(radial, 0.5, 0.5) == doctest::Approx(0.0));
	CHECK(gradientPosition(radial, 1.0, 1.0) == doctest::Approx(1.0));

	Gradient many;
	for(int index = 0; index < 50; ++index) many.stops.push_back({index / 49.0, colors::black});
	normalizeGradient(many);
	CHECK(many.stops.size() == maxGradientStops);
}

TEST_CASE("curve maths: interpolations stay within the square and pass through the points") {
	std::vector<CurvePoint> none;
	normalizeCurve(none);
	REQUIRE(none.size() == 2);
	CHECK(evaluateCurve(none, CurveInterpolation::Monotone, 0.3) == doctest::Approx(0.3));

	const std::vector<CurvePoint> points{{0.0, 0.0}, {0.5, 0.8}, {1.0, 1.0}};
	CHECK(evaluateCurve(points, CurveInterpolation::Linear, 0.25) == doctest::Approx(0.4));
	CHECK(evaluateCurve(points, CurveInterpolation::Monotone, 0.5) == doctest::Approx(0.8));
	double previous = 0.0;
	for(int step = 0; step <= 100; ++step) { // monotone data gives a monotone curve
		const double value = evaluateCurve(points, CurveInterpolation::Monotone, step / 100.0);
		CHECK(value >= previous - 1e-9);
		previous = value;
	}
	CHECK(evaluateCurve(points, CurveInterpolation::Bezier, 0.0) == doctest::Approx(0.0));
	CHECK(evaluateCurve(points, CurveInterpolation::Bezier, 1.0) == doctest::Approx(1.0));
	CHECK(evaluateCurve(points, CurveInterpolation::Bezier, 0.5) < 0.8); // near, not through, the middle point

	std::vector<CurvePoint> many;
	for(int index = 0; index < 50; ++index) many.push_back({index / 49.0, 2.0});
	normalizeCurve(many);
	CHECK(many.size() == maxCurvePoints);
	CHECK(many.front().y == 1.0);
}

TEST_CASE("GradientEditor: add, move and remove stops, within bounds") {
	GradientEditor editor;
	int changes = 0;
	countChanges(editor.onGradientChanged, changes);
	REQUIRE(editor.gradient().stops.size() == 2);
	REQUIRE(editor.addStop(0.5));
	CHECK(editor.selectedStop() == 1);
	CHECK(editor.gradient().stops[1].color.r == doctest::Approx(0.5).epsilon(0.01));
	CHECK(editor.moveStop(1, 0.9) == 1);
	CHECK(editor.moveStop(0, 0.95) == 1); // passes the stop at 0.9: sorted, the selection follows
	CHECK(editor.selectedStop() == 1);
	CHECK(editor.gradient().stops[1].at == doctest::Approx(0.95));
	editor.setStopColor(1, colors::white);
	CHECK(editor.removeStop(1));
	CHECK_FALSE(editor.removeStop(0)); // two stops stay
	CHECK(changes == 5);

	editor.changeKind(Gradient::Kind::Radial);
	editor.changeAngle(400.0);
	CHECK(editor.gradient().kind == Gradient::Kind::Radial);
	CHECK(editor.gradient().angle == doctest::Approx(40.0));

	while(editor.addStop(0.5)) {
	}
	CHECK(editor.gradient().stops.size() == maxGradientStops);
	const int before = changes;
	editor.setGradient(editor.gradient());
	CHECK(changes == before); // setGradient() doesn't report
}

TEST_CASE("CurvesEditor: add, move and remove points per channel, within bounds") {
	CurvesEditor editor;
	int changes = 0;
	editor.onCurveChanged.connect([&changes](CurveChannel) { ++changes; });
	REQUIRE(editor.addPoint(0.5, 0.7));
	CHECK(editor.curve(CurveChannel::Rgb).size() == 3);
	editor.movePoint(1, 2.0, 0.2); // x stays between its neighbours
	CHECK(editor.curve(CurveChannel::Rgb)[1].x <= 1.0);
	CHECK(editor.curve(CurveChannel::Rgb)[1].y == doctest::Approx(0.2));
	CHECK(editor.removePoint(1));
	CHECK_FALSE(editor.removePoint(0));
	CHECK(changes == 3);

	editor.setChannel(CurveChannel::Red); // hidden channels: RGB only
	CHECK(editor.channel() == CurveChannel::Rgb);
	editor.setShowsChannels(true);
	editor.setChannel(CurveChannel::Red);
	REQUIRE(editor.addPoint(0.3, 0.6));
	CHECK(editor.curve(CurveChannel::Red).size() == 3);
	CHECK(editor.curve(CurveChannel::Rgb).size() == 2);

	while(editor.addPoint(0.5, 0.5)) {
	}
	CHECK(editor.curve(CurveChannel::Red).size() == maxCurvePoints);
	const std::vector<double> bins(5000, 1.0);
	editor.setHistogram(bins); // kept to maxHistogramBins
}

TEST_CASE("ColorPicker: fields, value, swatches and recent colours") {
	ColorPicker picker;
	int changes = 0;
	picker.onValueChanged.connect([&changes](Color) { ++changes; });
	picker.setValue(Color{1.0f, 0.0f, 0.0f, 1.0f});
	CHECK(changes == 0);
	CHECK(picker.fieldText(ColorPicker::Part::Hex) == "#ff0000");
	CHECK(picker.applyFieldText(ColorPicker::Part::Hex, "#00ff0080"));
	CHECK(picker.value().g == doctest::Approx(1.0f));
	CHECK(picker.value().a == doctest::Approx(128.0f / 255.0f));
	CHECK(picker.applyFieldText(ColorPicker::Part::Rgb, "0, 0, 255"));
	CHECK(picker.value().b == doctest::Approx(1.0f));
	CHECK(picker.applyFieldText(ColorPicker::Part::Hsl, "0, 100%, 50%"));
	CHECK(picker.value().r == doctest::Approx(1.0f));
	CHECK(picker.applyFieldText(ColorPicker::Part::Oklch, "1 0 0"));
	CHECK(picker.value().g == doctest::Approx(1.0f).epsilon(0.01));
	CHECK_FALSE(picker.applyFieldText(ColorPicker::Part::Hex, "not a colour"));
	CHECK(changes == 4);

	for(int index = 0; index < 20; ++index) {
		picker.setValue(Color{index / 20.0f, 0.0f, 0.0f, 1.0f});
		picker.rememberValue();
	}
	CHECK(picker.recentColors().size() == ColorPicker::maxRecent);
	CHECK(picker.recentColors().front().r == doctest::Approx(19.0f / 20.0f));
	const std::vector<Color> swatches(100, colors::black);
	picker.setSwatches(swatches); // kept to maxSwatches
}

TEST_CASE("ColorPicker: the swatches part goes away with the last swatch") {
	struct Picker : ColorPicker {
		using ColorPicker::keyEvent;
	} picker;
	const std::vector<Color> swatches(3, colors::white);
	picker.setSwatches(swatches);
	for(int step = 0; step < 16 && picker.activePart() != ColorPicker::Part::Swatches; ++step)
		picker.keyEvent({Key::Tab, 0, Action::Press, Mods{}});
	REQUIRE(picker.activePart() == ColorPicker::Part::Swatches);

	picker.setSwatches({}); // no recent colour either: nothing for Enter to pick
	CHECK(picker.activePart() == ColorPicker::Part::Plane);
	picker.setValue(colors::black);
	picker.keyEvent({Key::Enter, 0, Action::Press, Mods{}});
	CHECK(picker.value().r == doctest::Approx(0.0f));
}

TEST_CASE("creative values: JSON round trips and lenient reading") {
	CHECK(format::colorToJson(Color{1.0f, 0.0f, 0.0f, 1.0f}) == "#ff0000ff");
	CHECK(format::colorFromJson("#00ff00") == Color{0.0f, 1.0f, 0.0f, 1.0f});
	CHECK_FALSE(format::colorFromJson(3).has_value());

	const json value = json::parse(R"({"kind": "radial", "angle": 45,
	    "stops": [{"at": 0.25, "color": "#ff0000ff"}, {"at": 0.75, "color": "#0000ffff"}, {"at": "x"}]})");
	const Gradient gradient = format::gradientFromJson(value);
	CHECK(gradient.kind == Gradient::Kind::Radial);
	REQUIRE(gradient.stops.size() == 2); // the malformed stop is skipped
	CHECK(format::gradientFromJson(format::gradientToJson(gradient)) == gradient);
	CHECK(format::gradientToJson(gradient)["kind"] == "radial");

	const std::vector<CurvePoint> points = format::curvePointsFromJson(json::parse("[[0, 0.1], [0.5, 0.5], [1]]"));
	REQUIRE(points.size() == 2);
	CHECK(format::curvePointsFromJson(format::curvePointsToJson(points)) == points);
	CHECK(format::curveInterpolationFromName("bezier") == CurveInterpolation::Bezier);
	CHECK_FALSE(format::curveInterpolationFromName("spline").has_value());
	CHECK(format::curveChannelKey(CurveChannel::Rgb) == "points");
	CHECK(format::curveChannelKey(CurveChannel::Blue) == "blue");
}

TEST_CASE("curve() and gradientColor() in expressions") {
	CHECK(evaluate("{{curve(state.points, 0.5)}}").get<double>() == doctest::Approx(0.8));
	CHECK(evaluate("{{curve(state.line, 0.5, 'linear')}}").get<double>() == doctest::Approx(0.4));
	CHECK(evaluate("{{gradientColor(state.gradient, 0)}}") == "#000000ff");
	CHECK(evaluate("{{gradientColor(state.gradient, 1)}}") == "#ffffffff");
	CHECK_THROWS(evaluate("{{curve(state.points)}}"));
}
