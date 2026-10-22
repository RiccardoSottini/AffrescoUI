/// @file
/// @brief Unit tests for the A2UI and Adaptive Cards adapters (format::adapters, SCREEN_FORMAT.md "Other formats"):
/// the samples in tests/render/screens convert to their committed screens and build; detection; placeholders and
/// warnings; text that is never evaluated; links and images; A2UI updates as revisions; and the limits.

#include "Headless.hpp"

#include <affresco/Screen.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/format/Adapters.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using affresco::json;
namespace adapters = affresco::format::adapters;

namespace {

/// @brief The render check's screens, where the samples are.
/// @return The folder.
std::filesystem::path screens() { return std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR).parent_path() / "screens"; }

/// @brief A file's text.
/// @param path The file.
/// @return Its text.
std::string read(const std::filesystem::path& path) {
	const std::ifstream file(path, std::ios::binary);
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

/// @brief The options the committed samples were written with (`affrescoui import --font`).
/// @return The options.
adapters::Options sampleOptions() {
	adapters::Options options;
	options.fontFile = "../../../examples/assets/fonts/Roboto-Regular.ttf";
	return options;
}

/// @brief A card with these elements in its body.
/// @param body The elements.
/// @return The card.
json card(json body) { return {{"type", "AdaptiveCard"}, {"version", "1.6"}, {"body", std::move(body)}}; }

/// @brief Whether a screen's warnings mention a text.
/// @param converted The conversion.
/// @param text The text.
/// @return Whether one does.
bool warned(const adapters::Converted& converted, const std::string& text) {
	for(const std::string& warning : converted.warnings)
		if(warning.find(text) != std::string::npos) return true;
	return false;
}

/// @brief Builds a screen document in a window without a display.
/// @param document The document.
/// @param base The folder its files are relative to.
void build(const json& document, const std::filesystem::path& base) {
	const affresco::detail::ManualClock clock;
	const auto window = affresco::detail::createHeadlessWindow(480, 640, clock);
	affresco::Screen screen = affresco::Screen::fromString(document.dump(), base);
	screen.build(*window);
}

} // namespace

TEST_CASE("adapters: the samples convert to their committed screens, which build") {
	const adapters::Converted garden = adapters::fromA2ui(read(screens() / "a2ui.jsonl"), sampleOptions());
	CHECK(garden.screen == json::parse(read(screens() / "a2ui.screen.json")));
	CHECK(garden.warnings.size() == 1);
	CHECK(warned(garden, "Video has no equivalent"));
	REQUIRE(garden.revisions.size() == 1);
	CHECK(garden.revisions[0].dump().find("Community garden: spring") != std::string::npos);
	CHECK_NOTHROW(build(garden.screen, screens()));

	const adapters::Converted loan =
	    adapters::fromAdaptiveCard(json::parse(read(screens() / "adaptive-card.json")), sampleOptions());
	CHECK(loan.screen == json::parse(read(screens() / "adaptive-card.screen.json")));
	CHECK(warned(loan, "Media has no equivalent"));
	CHECK(warned(loan, "Rating has no equivalent: dropped"));
	CHECK(loan.screen["state"]["data"]["member"]["name"] == "Robin");
	CHECK(loan.screen.dump().find("{{state.data.member.name}}") != std::string::npos);
	CHECK_NOTHROW(build(loan.screen, screens()));
}

TEST_CASE("adapters: a screen loads either format, and A2UI's later updates are its revisions") {
	const affresco::Screen garden = affresco::Screen::fromString(read(screens() / "a2ui.jsonl"), screens());
	CHECK(garden.documentJson().find("Community garden: spring") != std::string::npos);
	CHECK(garden.revision() == 1);
	const affresco::Screen loan = affresco::Screen::fromString(read(screens() / "adaptive-card.json"), screens());
	CHECK(json::parse(loan.stateJson())["inputs"]["pickup"] == "east");

	CHECK(adapters::detect(card(json::array())) == adapters::SourceFormat::AdaptiveCards);
	CHECK(adapters::detect(json::parse(R"({"beginRendering": {"root": "r"}})")) == adapters::SourceFormat::A2ui);
	CHECK(adapters::detect(json::parse(R"([{"dataModelUpdate": {}}])")) == adapters::SourceFormat::A2ui);
	CHECK(adapters::detect(json::parse(R"({"format": "affrescoui-screen"})")) == adapters::SourceFormat::Screen);
	CHECK(adapters::detect(json::array()) == adapters::SourceFormat::Screen);
	CHECK_FALSE(adapters::convertForeign(R"({"format": "affrescoui-screen", "version": 2})").has_value());
}

TEST_CASE("adapters: what has no equivalent is a visible placeholder and a warning; text is never evaluated") {
	const adapters::Converted converted = adapters::fromAdaptiveCard(card(json::parse(R"json([
		{"type": "Hologram"},
		{"type": "TextBlock", "text": "{{state.secret}} and {{1+1}}"},
		{"type": "Input.Text", "id": "pin", "style": "password"},
		{"type": "ActionSet", "actions": [{"type": "Action.OpenUrl", "title": "Run", "url": "javascript:alert(1)"}]},
		{"type": "Image", "url": "http://example.com/a.png"},
		{"type": "Image", "url": "../outside.png"},
		{"type": "Image", "url": "https://example.com/b.png"}
	])json")));
	const std::string screen = converted.screen.dump();
	CHECK(screen.find("Unsupported: Hologram") != std::string::npos);
	CHECK(screen.find("Unsupported: Input.Text (password)") != std::string::npos);
	CHECK(warned(converted, "card.body[0]: Hologram has no equivalent"));
	CHECK(warned(converted, "only https links are opened"));
	CHECK(screen.find("javascript:") == std::string::npos);
	CHECK(screen.find("http://") == std::string::npos);
	CHECK(screen.find("outside.png") == std::string::npos);
	CHECK(screen.find("\"source\":\"https://example.com/b.png\"") != std::string::npos);
	// The text is in the state and shown through a binding to it, so {{...}} inside it is never evaluated
	CHECK(converted.screen["state"]["literals"]["k1"] == "{{state.secret}} and {{1+1}}");
	CHECK(screen.find("\"text\":\"{{state.secret}}") == std::string::npos);
	CHECK_NOTHROW(build(converted.screen, screens()));
}

TEST_CASE("adapters: input size, nesting and element count are limited") {
	CHECK_THROWS_AS((void)adapters::fromA2ui(std::string(adapters::maxInputBytes + 1, ' ')), affresco::DocumentError);
	CHECK_FALSE(adapters::convertForeign(std::string(adapters::maxInputBytes + 1, ' ')).has_value());

	json nested = {{"type", "TextBlock"}, {"text", "deep"}};
	for(int depth = 0; depth < adapters::maxDepth + 2; ++depth)
		nested = {{"type", "Container"}, {"items", json::array({nested})}};
	CHECK_THROWS_AS((void)adapters::fromAdaptiveCard(card(json::array({nested}))), affresco::DocumentError);

	json many = json::array();
	for(std::size_t i = 0; i <= adapters::maxElements; ++i) many.push_back({{"type", "TextBlock"}, {"text", "x"}});
	CHECK_THROWS_AS((void)adapters::fromAdaptiveCard(card(many)), affresco::DocumentError);

	CHECK_THROWS_AS((void)adapters::fromAdaptiveCard(json::parse(R"({"type": "Other"})")), affresco::DocumentError);
	CHECK_THROWS_AS((void)adapters::fromA2ui(R"({"unknownMessage": {}})"), affresco::DocumentError);
}
