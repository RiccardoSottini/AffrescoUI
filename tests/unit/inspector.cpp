/// @file
/// @brief Unit tests for the inspector (modules/screen/src/Inspector.cpp, VERSION_CONTROL.md "Inspector"), headless:
/// off by default, reviewers only, bindings as written and never a value, the statistics it reads and an event trace
/// that never holds a typed character.

#include "Headless.hpp"
#include "Helpers.hpp"
#include "Inspector.hpp"       // modules/screen/src
#include "OffscreenImages.hpp" // modules/screen/src

#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <string>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::Inspector;
using affresco::detail::ManualClock;
using affresco::test::MemoryChannel;
using affresco::test::permissionsScreen;

namespace {

/// @brief A screen with a secret in its state, shown by a bound label.
constexpr const char* secretScreen = R"json({
	"format": "affrescoui-screen", "version": 1,
	"state": { "password": "hunter2", "count": 0 },
	"root": { "id": "root", "type": "panel", "children": [
		{ "id": "secret", "type": "label", "text": "{{state.password}}", "width": 200, "height": 40,
		  "onClick": [{ "increment": "state.count" }] },
		{ "id": "count", "type": "label", "text": "{{state.count}}", "y": 60, "width": 200, "height": 40 }
	] }
})json";

/// @brief A screen's document and runtime, built in a headless window.
struct Built {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the runtime.
	affresco::format::Rules rules;                                                    ///< The format's rules.
	affresco::vcs::Repository repository{rules, json::parse(secretScreen)};           ///< The document.
	affresco::format::Runtime runtime{repository, "."};                               ///< The panels.

	/// @brief Builds the screen.
	Built() {
		runtime.build(*window);
		window->render();
	}
};

/// @brief What the overlay says.
/// @param window Its window.
/// @return The information's text, or "" with no overlay.
std::string overlayText(const affresco::Window& window) {
	const affresco::Panel* layer = window.inspection();
	if(!layer) return "";
	for(const auto& child : layer->children())
		if(const auto* label = dynamic_cast<const affresco::Label*>(child.get())) return label->text();
	return "";
}

/// @brief Whether the trace has a line.
/// @param inspector The inspector.
/// @param line The line.
/// @return true if it has.
bool traced(const Inspector& inspector, const std::string& line) {
	return std::ranges::find(inspector.trace(), line) != inspector.trace().end();
}

} // namespace

TEST_CASE("inspector: only a reviewer turns it on, and a host without one says so") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, permissionsScreen());
	affresco::format::Runtime runtime(repository, ".");
	affresco::detail::OffscreenImages images(repository, runtime);

	MemoryChannel agentChannel;
	const affresco::channel::Session agent(agentChannel, repository, "agent", {}, &images);
	agentChannel.sent.clear(); // the hello
	agentChannel.receive(R"({"type": "inspect", "on": true, "id": 1})");
	REQUIRE(agentChannel.sent.size() == 1);
	CHECK(agentChannel.sent[0]["type"] == "rejected");
	CHECK(agentChannel.sent[0]["error"].get<std::string>().find("may not review") != std::string::npos);

	MemoryChannel studioChannel;
	const affresco::channel::Session studio(studioChannel, repository, "studio", {}, &images);
	studioChannel.sent.clear();
	studioChannel.receive(R"({"type": "inspect", "on": "yes", "id": 2})");
	studioChannel.receive(R"({"type": "inspect", "on": true, "id": 3})"); // no window yet
	REQUIRE(studioChannel.sent.size() == 2);
	CHECK(studioChannel.sent[0]["type"] == "rejected");
	CHECK(studioChannel.sent[1]["error"].get<std::string>().find("needs a window") != std::string::npos);

	MemoryChannel plainChannel;
	const affresco::channel::Session plain(plainChannel, repository, "studio");
	plainChannel.sent.clear();
	plainChannel.receive(R"({"type": "inspect", "on": true, "id": 4})");
	REQUIRE(plainChannel.sent.size() == 1);
	CHECK(plainChannel.sent[0]["error"].get<std::string>().find("has no inspector") != std::string::npos);
}

TEST_CASE("inspector: a panel's node and bindings as written, never what they give") {
	Built built;
	const affresco::Panel* secret = built.runtime.find("secret");
	REQUIRE(secret);
	const json about = built.runtime.inspect(*secret);
	CHECK(about["node"] == "#secret");
	CHECK(about["type"] == "label");
	CHECK(about["bindings"]["text"] == "{{state.password}}");
	CHECK(about["handlers"] == json::array({"onClick"}));
	CHECK(about.dump().find("hunter2") == std::string::npos);
}

TEST_CASE("inspector: off by default; on, it describes the panel under the pointer") {
	Built built;
	Inspector inspector(built.repository, built.runtime);
	CHECK_FALSE(inspector.on());
	CHECK(built.window->inspection() == nullptr);

	inspector.setShowStatistics(false);
	REQUIRE_FALSE(inspector.setOn(true));
	built.window->routeMouseMove({20.0f, 20.0f});
	built.window->render();
	const std::string text = overlayText(*built.window);
	CHECK(text.find("#secret  (label)") != std::string::npos);
	CHECK(text.find("bounds 0, 0, 200 x 40") != std::string::npos);
	CHECK(text.find("text = \"{{state.password}}\"") != std::string::npos);
	CHECK(text.find("statistics left out") != std::string::npos);
	CHECK(text.find("hunter2") == std::string::npos);
	// The overlay isn't hit: the pointer still reaches the label
	CHECK(built.window->panelAt({20.0f, 20.0f}) == built.runtime.find("secret"));

	REQUIRE_FALSE(inspector.setOn(false));
	CHECK(built.window->inspection() == nullptr);
}

TEST_CASE("inspector: the trace names clicks, keys and commits, never a typed character") {
	Built built;
	Inspector inspector(built.repository, built.runtime);
	REQUIRE_FALSE(inspector.setOn(true));

	built.window->routeMouseButton({affresco::MouseButton::Button1, affresco::Action::Press, {}, {20.0f, 20.0f}});
	built.window->routeMouseButton({affresco::MouseButton::Button1, affresco::Action::Release, {}, {20.0f, 20.0f}});
	built.window->routeKey({affresco::Key::Escape, 0, affresco::Action::Press, {}});
	built.window->routeKey({affresco::Key::H, 0, affresco::Action::Press, {}});
	built.window->routeChar({U'h'});
	REQUIRE(built.repository.submit({{"author", "agent"}, {"set", {{"#count/width", 180}}}}).status ==
	        RevisionStatus::Applied);

	CHECK(traced(inspector, "click #secret"));
	CHECK(traced(inspector, "key Escape"));
	CHECK(traced(inspector, "key a key"));
	CHECK(traced(inspector, "commit #1 by agent"));
	for(const std::string& line : inspector.trace()) CHECK(line.find('h') == std::string::npos);

	// It keeps the latest only
	for(int index = 0; index < 100; ++index)
		built.window->routeKey({affresco::Key::Tab, 0, affresco::Action::Press, {}});
	CHECK(inspector.trace().size() == Inspector::traceLength);
}

TEST_CASE("inspector: frames and updates are counted") {
	Built built;
	const std::uint64_t frames = built.window->frameStatistics().frames;
	const std::uint64_t updates = built.runtime.lastUpdate().updates;
	built.window->render(); // nothing changed: no frame
	CHECK(built.window->frameStatistics().frames == frames);

	built.window->routeMouseButton({affresco::MouseButton::Button1, affresco::Action::Press, {}, {20.0f, 20.0f}});
	built.window->routeMouseButton({affresco::MouseButton::Button1, affresco::Action::Release, {}, {20.0f, 20.0f}});
	built.window->render();
	CHECK(built.window->frameStatistics().frames == frames + 1);
	CHECK(built.runtime.lastUpdate().updates > updates);
	CHECK(built.runtime.lastUpdate().panelsApplied >= 1);
}
