/// @file
/// @brief The window that asks for an app's capabilities (examples/shared/host/permissions.screen.json): only what
/// can't run, write or reach out starts checked (AppCapability::startsAllowed()), a shell or an interpreter is named
/// for what it gives (AppCapability::runsAnyCommand()), and the app's reason is quoted as the app's.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

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

} // namespace

TEST_CASE("permissions: a program, a folder written, an origin, the assistant and the channels start unchecked") {
	using Kind = AppCapability::Kind;
	CHECK(capability(Kind::Audio).startsAllowed());
	CHECK(capability(Kind::Files).startsAllowed());
	CHECK(capability(Kind::Windows).startsAllowed());
	CHECK(capability(Kind::Folder, "notes").startsAllowed());
	CHECK_FALSE(capability(Kind::Folder, "notes", true).startsAllowed());
	CHECK_FALSE(capability(Kind::Program, "git").startsAllowed());
	CHECK_FALSE(capability(Kind::Network, "https://api.example.com").startsAllowed());
	CHECK_FALSE(capability(Kind::Assistant).startsAllowed());
	CHECK_FALSE(capability(Kind::Channels).startsAllowed());
}

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
