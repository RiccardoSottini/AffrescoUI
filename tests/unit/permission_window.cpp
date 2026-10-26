/// @file
/// @brief The window that asks for an app's capabilities (examples/shared/host/permissions.screen.json): a shell or an
/// interpreter is named for what it gives (AppCapability::runsAnyCommand()), the app's reason is quoted as the app's,
/// a card grows with its text, and a click anywhere on it flips its box once.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using affresco::json;
using affresco::format::AppCapability;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A capability.
/// @param kind What it reaches.
/// @param name Its origin, root or program.
/// @param write For a folder: written.
/// @return It.
AppCapability capability(AppCapability::Kind kind, std::string name = {}, bool write = false) {
	AppCapability made;
	made.kind = kind;
	made.name = std::move(name);
	made.write = write;
	return made;
}

/// @brief Whether a subtree holds a label with a text.
/// @param panel The subtree's root.
/// @param text The text.
/// @return Whether it does.
bool showsLabel(const affresco::Panel& panel, const std::string& text) {
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel); label && label->text() == text) return true;
	for(const auto& child : panel.children())
		if(showsLabel(*child, text)) return true;
	return false;
}

/// @brief The panels of a subtree that are of a type, in order.
/// @tparam T The type.
/// @param panel The subtree's root.
/// @param found Receives them.
template <typename T> void collect(affresco::Panel& panel, std::vector<T*>& found) {
	if(auto* typed = dynamic_cast<T*>(&panel)) found.push_back(typed);
	for(const auto& child : panel.children()) collect(*child, found);
}

/// @brief The label of a subtree with a text.
/// @param panel The subtree's root.
/// @param text The text.
/// @return It, or nullptr.
affresco::Label* labelWith(affresco::Panel& panel, const std::string& text) {
	std::vector<affresco::Label*> labels;
	collect(panel, labels);
	for(affresco::Label* label : labels)
		if(label->text() == text) return label;
	return nullptr;
}

/// @brief Left-clicks at the middle of a panel.
/// @param window The window.
/// @param panel The panel.
void clickMiddle(affresco::Window& window, const affresco::Panel& panel) {
	const affresco::Rect& bounds = panel.bounds();
	const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
	window.routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
}

} // namespace

TEST_CASE("permissions: a shell or an interpreter runs any command, whatever its path, case, version or .exe") {
	using Kind = AppCapability::Kind;
	for(const char* name : {"shell", "sh", "bash", "zsh", "fish", "cmd", "CMD.EXE", "pwsh", "powershell.exe",
	                        "python3", "python3.12", "/usr/bin/python3", "C:\\Python\\python.exe", "node", "ruby",
	                        "perl", "osascript", "env"}) {
		CAPTURE(name);
		CHECK(capability(Kind::Program, name).runsAnyCommand());
	}
	for(const char* name : {"git", "clangd", "lldb-dap", "shellcheck", "nodemon-free"}) {
		CAPTURE(name);
		CHECK_FALSE(capability(Kind::Program, name).runsAnyCommand());
	}
	CHECK_FALSE(capability(Kind::Folder, "sh").runsAnyCommand());
}

TEST_CASE("permissions: the window names a shell for what it gives, and quotes the app's reason as the app's") {
	const std::filesystem::path file =
	    std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "shared" / "host" / "permissions.screen.json";
	ManualClock clock;
	const auto window = createHeadlessWindow(640, 480, clock);
	std::ifstream in(file, std::ios::binary);
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	affresco::Screen screen = affresco::Screen::fromString(text, file.parent_path());
	screen.setLocale("en");
	const json app = {{"name", "Notes"},
	                  {"capabilities",
	                   {{{"key", "program:sh"}, {"kind", "programAny"}, {"name", "sh"}, {"why", "Runs your build."},
	                     {"allowed", false}},
	                    {{"key", "program:git"}, {"kind", "program"}, {"name", "git"}, {"why", "Shows changes."},
	                     {"allowed", false}}}}};
	screen.onCall("permissions.app", [&](const std::string&) { return app.dump(); });
	screen.build(*window);
	window->render();
	CHECK(showsLabel(window->root(), "Run any command as you, through sh"));
	CHECK(showsLabel(window->root(), "Run the program git"));
	CHECK(showsLabel(window->root(), "Notes says: “Runs your build.”"));
	CHECK_FALSE(showsLabel(window->root(), "Runs your build."));
}

TEST_CASE("permissions: a card grows with its text, and a click on it or on its box flips the box once") {
	const std::filesystem::path file =
	    std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "shared" / "host" / "permissions.screen.json";
	ManualClock clock;
	const auto window = createHeadlessWindow(420, 320, clock);
	std::ifstream in(file, std::ios::binary);
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	affresco::Screen screen = affresco::Screen::fromString(text, file.parent_path());
	screen.setLocale("en");
	const std::string why = "Answers your questions about your habits, with the assistant you set up in Settings.";
	const json app = {{"name", "Streak"},
	                  {"capabilities",
	                   {{{"key", "assistant"}, {"kind", "assistant"}, {"name", ""}, {"why", why}, {"allowed", true}},
	                    {{"key", "files"}, {"kind", "files"}, {"name", ""}, {"why", "Backs up."}, {"allowed", true}}}}};
	screen.onCall("permissions.app", [&](const std::string&) { return app.dump(); });
	screen.build(*window);
	const auto settle = [&] {
		for(int frame = 0; frame < 4; ++frame) {
			clock.advance(250.0);
			window->render();
		}
	};
	settle();

	affresco::Label* reason = labelWith(window->root(), "Streak says: “" + why + "”");
	REQUIRE(reason != nullptr);
	const affresco::Panel* card = reason->parent()->parent();
	REQUIRE(card != nullptr);
	CHECK(reason->bounds().bottom <= card->bounds().bottom);

	std::vector<affresco::Toggle*> boxes;
	collect(window->root(), boxes);
	REQUIRE(boxes.size() == 2);
	CHECK(boxes[0]->isChecked());
	clickMiddle(*window, *reason);
	settle();
	CHECK_FALSE(boxes[0]->isChecked());
	CHECK(screen.stateJson().find(R"("allowed":false)") != std::string::npos);
	clickMiddle(*window, *boxes[0]);
	settle();
	CHECK(boxes[0]->isChecked());
	CHECK(screen.stateJson().find(R"("allowed":false)") == std::string::npos);
}
