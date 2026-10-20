/// @file
/// @brief The automation module, headless: reading scenarios files (and the errors that name the place), key names,
/// state paths, the PNG reader golden images go through, and scenarios played on a small screen.

#include "HistoryFiles.hpp" // TemporaryFolder
#include "Internal.hpp"
#include "Png.hpp"
#include "PngReader.hpp"

#include <affresco/automation/Scenarios.hpp>

#include <doctest/doctest.h>

#include <fstream>
#include <string>
#include <vector>

using affresco::DocumentError;
using affresco::json;
namespace automation = affresco::automation;

namespace {

/// @brief A screen with a counter, a text field that adds what is typed to a list, and a delayed message.
constexpr std::string_view counterScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"window": { "width": 400, "height": 300 },
	"state": { "clicks": 0, "items": [], "draft": "", "message": "" },
	"root": { "id": "root", "children": [
		{ "type": "label", "id": "count", "text": "{{state.clicks}} clicks", "width": 200, "height": 40,
		  "onClick": [{ "increment": "state.clicks" }, { "after": 500, "do": [{ "set": "state.message", "to": "later" }] }] },
		{ "type": "textarea", "id": "field", "bind": "state.draft", "anchors": { "top": 50 }, "width": 200, "height": 30,
		  "onSubmit": [{ "push": "state.items", "value": "{{event.text}}" }, { "set": "state.draft", "to": "" }] },
		{ "type": "label", "id": "item", "text": "{{item}}", "repeat": { "for": "item", "in": "state.items" },
		  "anchors": { "top": 100 }, "width": 200, "height": 20 },
		{ "type": "panel", "id": "hidden", "visible": false, "color": "#ff0000" }
	] }
})json";

/// @brief A folder with the counter screen in it, and a scenarios file next to it.
class Fixture {
public:
	Fixture() {
		std::ofstream(folder_.path() / "counter.screen.json") << counterScreen;
	}

	/// @brief Reads scenarios written for the counter screen.
	/// @param scenarios The "scenarios" list, as JSON text.
	/// @return The file.
	[[nodiscard]] automation::ScenarioFile scenarios(std::string_view scenarios) const {
		return automation::ScenarioFile::parse(R"({"version": 1, "screen": "counter.screen.json", "scenarios": )" +
		                                           std::string(scenarios) + "}",
		                                       folder_.path() / "counter.scenarios.json");
	}

private:
	affresco::test::TemporaryFolder folder_; ///< Holds the screen.
};

/// @brief The error reading a scenarios file gives.
/// @param text The file.
/// @return Its message, or "" if it reads.
std::string errorOf(std::string_view text) {
	try {
		static_cast<void>(automation::ScenarioFile::parse(text, "place.scenarios.json"));
	} catch(const DocumentError& error) {
		return error.what();
	}
	return "";
}

/// @brief A scenarios file with one scenario of one step.
/// @param step The step, as JSON text.
/// @return The file.
std::string oneStep(std::string_view step) {
	return R"({"version": 1, "screen": "a.json", "scenarios": [{"name": "a", "steps": [)" + std::string(step) + "]}]}";
}

} // namespace

TEST_CASE("automation: a scenarios file that is wrong says where") {
	CHECK(errorOf(oneStep(R"({"clik": "#a"})")).starts_with("AffrescoUI scenarios: place.scenarios.json: "
	                                                        "scenarios[0].steps[0]: unknown step"));
	CHECK(errorOf(oneStep(R"({"click": "#a", "type": "x"})")).contains("one step does one thing: 'click' and 'type'"));
	CHECK(errorOf(oneStep(R"({"click": "a"})")).contains("steps[0].click: expected [x, y] in window units, or \"#id\""));
	CHECK(errorOf(oneStep(R"({"key": "cmd+nope"})")).contains("steps[0].key: expected a key: enter,"));
	CHECK(errorOf(oneStep(R"({"wait": -1})")).contains("steps[0].wait: expected milliseconds"));
	CHECK(errorOf(oneStep(R"({"click": "#a", "colour": 1})")).contains("steps[0].colour: unknown key here"));
	CHECK(errorOf(oneStep(R"({"expect": {"state": "cards[x]", "is": 1}})")).contains("expect.state: expected a path"));
	CHECK(errorOf(oneStep(R"({"expect": {"state": "computed.total", "is": 1}})")).contains("computed values aren't read"));
	CHECK(errorOf(oneStep(R"({"expect": {"state": "a"}})")).contains("\"state\" needs \"is\""));
	CHECK(errorOf(oneStep(R"({"expect": {"node": "#a", "property": "colour", "is": 1}})"))
	          .contains("expect.property: expected one of text, textColor"));
	CHECK(errorOf(oneStep(R"({"expect": {"node": "#a", "text": "x", "exists": true}})")).contains("expected one of"));
	CHECK(errorOf(oneStep(R"({"expect": {"image": "../escape"}})")).contains("expect.image: expected a name"));
	CHECK(errorOf(oneStep(R"({"drag": "#a"})")).contains("\"drag\" needs \"to\""));
	CHECK(errorOf(R"({"version": 1, "screen": "a.json", "scenarios": [{"name": "a", "steps": []}, {"name": "a", "steps": []}]})")
	          .contains("scenarios[1].name: 'a' is the name of another scenario"));
	CHECK(errorOf(R"({"scenarios": [{"name": "a", "screen": "a.json"}]})").contains("version: expected 1"));
	CHECK(errorOf(R"({"version": 1, "scenarios": [{"name": "a"}]})").contains("scenarios[0]: no \"screen\""));
	CHECK(errorOf("{").contains("place.scenarios.json"));
	CHECK(errorOf(oneStep(R"({"note": "only a comment"}, {"click": [1, 2], "note": "and a click"})")).empty());
}

TEST_CASE("automation: keys, state paths and file names") {
	using automation::detail::parseKey;
	using automation::detail::parseStatePath;
	CHECK(parseKey("enter").value().key == affresco::Key::Enter);
	CHECK(parseKey("cmd+shift+z").value().key == affresco::Key::Z);
	CHECK(parseKey("cmd+shift+z").value().mods.bits == 0x9);
	CHECK(parseKey("f12").value().key == affresco::Key::F12);
	CHECK(parseKey("7").value().key == affresco::Key::Num7);
#ifdef __APPLE__
	CHECK(parseKey("mod+s").value().mods.bits == 0x8); // a screen's "mod+s": Cmd on macOS
#else
	CHECK(parseKey("mod+s").value().mods.bits == 0x2); // and Ctrl elsewhere
#endif
	CHECK_FALSE(parseKey("f13"));
	CHECK_FALSE(parseKey("hyper+a"));
	CHECK_FALSE(parseKey("a+"));

	CHECK(parseStatePath("cards[0].title") == std::vector<std::string>{"cards", "0", "title"});
	CHECK(parseStatePath("board['my-list'].length") == std::vector<std::string>{"board", "my-list", "length"});
	CHECK_FALSE(parseStatePath("cards..title"));
	CHECK_FALSE(parseStatePath("cards[x]"));
	CHECK_FALSE(parseStatePath(""));

	CHECK(automation::detail::isSafeFileName("kanban-first_frame.2"));
	CHECK_FALSE(automation::detail::isSafeFileName("../golden"));
	CHECK_FALSE(automation::detail::isSafeFileName("a/b"));
	CHECK_FALSE(automation::detail::isSafeFileName(".hidden"));
}

TEST_CASE("automation: golden images decode to the pixels they were encoded from") {
	std::vector<std::uint8_t> rgb(std::size_t{37} * 23 * 3);
	for(std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<std::uint8_t>((i * 7) % 251);
	const std::string png = affresco::detail::encodePng(37, 23, rgb);
	const std::vector<std::uint8_t> bytes(png.begin(), png.end());
	std::string error;
	const auto decoded = automation::detail::decodePng(bytes, error);
	REQUIRE(decoded);
	CHECK(decoded.value().width == 37);
	CHECK(decoded.value().height == 23);
	CHECK(decoded.value().rgb == rgb);

	SUBCASE("a stored deflate block") {
		// zlib header, one final stored block of 3 bytes, then their Adler-32
		const std::vector<std::uint8_t> stream = {0x78, 0x01, 0x01, 0x03, 0x00, 0xfc, 0xff, 'a', 'b', 'c',
		                                          0x02, 0x4d, 0x01, 0x27};
		const auto out = automation::detail::inflateZlib(stream, 3, error);
		REQUIRE(out);
		CHECK(out.value() == std::vector<std::uint8_t>{'a', 'b', 'c'});
		CHECK_FALSE(automation::detail::inflateZlib(stream, 2, error));
		CHECK(error.contains("longer"));
	}
	SUBCASE("what isn't a golden image is refused, never trusted") {
		std::vector<std::uint8_t> damaged = bytes;
		damaged.resize(damaged.size() - 20);
		CHECK_FALSE(automation::detail::decodePng(damaged, error));
		damaged = bytes;
		damaged[damaged.size() - 20] ^= 0x55; // inside the compressed data or its checksum
		CHECK_FALSE(automation::detail::decodePng(damaged, error));
		CHECK_FALSE(automation::detail::decodePng(std::vector<std::uint8_t>{'P', 'N', 'G'}, error));
		CHECK(error == "not a PNG file");
		std::vector<std::uint8_t> huge = bytes;
		huge[16] = 0x7f; // a width of about two billion pixels
		CHECK_FALSE(automation::detail::decodePng(huge, error));
		CHECK(error.contains("more than"));
	}
}

TEST_CASE("automation: scenarios play headless, on a clock that moves only when they wait") {
	const Fixture fixture;
	automation::Options options;
	options.pictures = false; // no OpenGL here: a scenario that draws must say so, not crash

	const automation::Report report = automation::run(fixture.scenarios(R"([
		{ "name": "count", "steps": [
			{ "click": "#count" },
			{ "click": "#count" },
			{ "expect": { "state": "clicks", "is": 2 } },
			{ "expect": { "node": "#count", "text": "2 clicks" } },
			{ "expect": { "state": "message", "is": "" } },
			{ "wait": 499 },
			{ "expect": { "state": "message", "is": "" } },
			{ "wait": 1 },
			{ "expect": { "state": "message", "is": "later" } }
		] },
		{ "name": "type", "steps": [
			{ "click": "#field" },
			{ "type": "héllo" },
			{ "expect": { "node": "#field", "property": "focused", "is": true } },
			{ "key": "enter" },
			{ "expect": { "state": "items.length", "is": 1 } },
			{ "expect": { "state": "items[0]", "is": "héllo" } },
			{ "expect": { "node": "#item", "text": "héllo" } },
			{ "expect": { "node": "#item", "nth": 1, "exists": false } },
			{ "expect": { "node": "#hidden", "property": "shown", "is": false } },
			{ "expect": { "node": "#count", "property": "width", "is": 200 } }
		] },
		{ "name": "fails", "steps": [
			{ "expect": { "state": "clicks", "is": 5 } },
			{ "expect": { "node": "#nothing", "exists": true } },
			{ "click": "#nothing" },
			{ "click": "#count" }
		] },
		{ "name": "draws", "steps": [{ "expect": { "image": "first-frame" } }] }
	])"),
	                                                  options);

	REQUIRE(report.scenarios.size() == 4);
	CHECK(report.scenarios[0].passed());
	CHECK(report.scenarios[1].passed());
	for(const automation::Failure& failure : report.scenarios[1].failures) INFO(failure.problem);
	CHECK(report.passed() == 2);
	CHECK(report.failed() == 2);

	// Failed expectations are recorded and the scenario goes on; a step that can't be played ends it
	const automation::ScenarioResult& fails = report.scenarios[2];
	REQUIRE(fails.failures.size() == 3);
	CHECK(fails.failures[0].problem == "state clicks is 0, expected 5");
	CHECK(fails.failures[1].problem == "#nothing doesn't exist, expected it to");
	CHECK(fails.failures[2].step == 3);
	CHECK(fails.failures[2].problem == "click: no shown node '#nothing'");
	CHECK(fails.stepsPlayed == 2);

	CHECK(report.scenarios[3].failures.at(0).problem.contains("this run has no OpenGL"));

	CHECK(report.text() == "counter.scenarios.json: 2 passed, 2 failed\n"
	                       "FAILED fails, step 1: state clicks is 0, expected 5\n"
	                       "FAILED fails, step 2: #nothing doesn't exist, expected it to\n"
	                       "FAILED fails, step 3: click: no shown node '#nothing'\n"
	                       "FAILED draws: could not start: it draws pictures, and this run has no OpenGL\n");
	const json summary = report.toJson();
	CHECK(summary["passed"] == 2);
	CHECK(summary["failures"].size() == 4);
	CHECK(summary["failures"][0] == json{{"scenario", "fails"}, {"step", 1}, {"problem", "state clicks is 0, expected 5"}});
}

TEST_CASE("automation: only the scenarios asked for, and on another document") {
	const Fixture fixture;
	automation::Options options;
	options.only = {"second"};
	options.document = std::string(counterScreen).replace(std::string(counterScreen).find("\"clicks\": 0"), 11,
	                                                       "\"clicks\": 7");
	const automation::Report report = automation::run(fixture.scenarios(R"([
		{ "name": "first", "steps": [{ "expect": { "state": "clicks", "is": 0 } }] },
		{ "name": "second", "steps": [{ "expect": { "state": "clicks", "is": 7 } }] }
	])"),
	                                                  options);
	REQUIRE(report.scenarios.size() == 1);
	CHECK(report.scenarios[0].name == "second");
	CHECK(report.scenarios[0].passed());
}
