/// @file
/// @brief Apps (SCREEN_FORMAT.md, "Apps: app.json"): a manifest is read whole and every bad part refused, the
/// capabilities a screen uses are found and set against the ones declared, and Screen::fromApp() reads an app's folder
/// with its themes, models and JSON and CSV seeds merged in, keeps its data by its id (the old files moved there once),
/// and refuses a file outside the folder.

#include <affresco/affresco.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/App.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::format::AppCapability;

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

namespace {

/// @brief A manifest that declares one of each: generated test data only.
json fullManifest() {
	return json::parse(R"json({
	"format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes", "description": "Short notes.",
	"icon": "assets/icon.png", "entry": "index.screen.json", "themes": ["theme.json"],
	"capabilities": {
		"network": { "https://*.example.com": "Syncs the notes." },
		"folders": { "workspace": { "access": "write", "why": "Saves notes as files." } },
		"programs": { "git": "Keeps the files' history." },
		"audio": "Plays a sound when a note is due."
	},
	"data": { "models": "models.json", "seed": { "notes": "seed/notes.csv" } },
	"assets": ["assets"]
})json");
}

/// @brief The error parseApp() gives a manifest, or "" if it reads it.
/// @param manifest The manifest.
/// @return The error.
std::string errorOf(const json& manifest) {
	try {
		static_cast<void>(affresco::format::parseApp(manifest));
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return "";
}

/// @brief Writes a file, making its folder.
/// @param file The file.
/// @param text Its text.
void write(const std::filesystem::path& file, const std::string& text) {
	std::filesystem::create_directories(file.parent_path());
	std::ofstream(file) << text;
}

/// @brief A small app's folder: an entry screen showing the notes' count, a theme, models and a CSV seed.
/// @param folder The folder, emptied first.
void writeApp(const std::filesystem::path& folder) {
	std::filesystem::remove_all(folder);
	json manifest = fullManifest();
	manifest.erase("capabilities");
	manifest.erase("icon");
	manifest.erase("assets");
	write(folder / "app.json", manifest.dump());
	write(folder / "index.screen.json", R"json({ "format": "affrescoui-screen", "version": 2,
		"tokens": { "gap": 4 }, "persist": ["state.draft"], "state": { "draft": "" },
		"root": { "type": "label", "id": "count", "text": "{{length(data.notes)}}" } })json");
	write(folder / "theme.json", R"json({ "tokens": { "gap": 8, "radius": 6 } })json");
	write(folder / "models.json", R"json({ "models": { "notes": { "version": 1, "fields": {
		"title": { "type": "string", "required": true }, "done": { "type": "boolean", "default": false } } } } })json");
	write(folder / "seed" / "notes.csv", "id,Title,Done\nn1,First,no\nn2,Second,yes\n");
}

} // namespace

TEST_CASE("app manifest: a full manifest is read, each capability keyed") {
	const affresco::format::AppManifest app = affresco::format::parseApp(fullManifest());
	CHECK(app.id == "notes");
	CHECK(app.name == "Notes");
	CHECK(app.entry == "index.screen.json");
	CHECK(app.themes == std::vector<std::string>{"theme.json"});
	CHECK(app.models == "models.json");
	REQUIRE(app.seed.size() == 1);
	CHECK(app.seed[0].second == "seed/notes.csv");
	REQUIRE(app.capabilities.size() == 4);
	CHECK(app.capabilities[0].key() == "network:https://*.example.com");
	CHECK(app.capabilities[1].key() == "folder:workspace:write");
	CHECK(app.capabilities[2].key() == "program:git");
	CHECK(app.capabilities[3].key() == "audio");
	CHECK(app.capabilities[3].why == "Plays a sound when a note is due.");
}

TEST_CASE("app manifest: every bad part is refused, with its place") {
	const auto with = [](const std::string& pointer, const json& value) {
		json manifest = fullManifest();
		manifest[json::json_pointer(pointer)] = value;
		return errorOf(manifest);
	};
	CHECK(errorOf(fullManifest()).empty());
	CHECK(errorOf(json::array()).contains("expected an object"));
	CHECK(with("/format", "affrescoui-screen").contains("format"));
	CHECK(with("/version", 2).contains("version"));
	CHECK(with("/id", "Notes").contains("id"));
	CHECK(with("/runs", "anything").contains("unknown key 'runs'"));
	CHECK(with("/entry", "../other/index.screen.json").contains("entry"));
	CHECK(with("/entry", "/etc/index.screen.json").contains("entry"));
	CHECK(with("/entry", "index.txt").contains("entry"));
	CHECK(with("/icon", "C:\\icon.png").contains("icon"));
	CHECK(with("/themes/0", "a/../../theme.json").contains("themes[0]"));
	CHECK(with("/name", std::string(101, 'a')).contains("name"));
	CHECK(with("/capabilities/network/https:~1~1example.com~1path", "Why").contains("not an origin"));
	CHECK(with("/capabilities/network/file:~1~1~1", "Why").contains("not an origin"));
	CHECK(with("/capabilities/network/https:~1~1example.com", "").contains("empty"));
	CHECK(with("/capabilities/programs/~1bin~1sh", "Why").contains("program"));
	CHECK(with("/capabilities/folders/workspace/access", "all").contains("access"));
	CHECK(with("/capabilities/folders/workspace/also", 1).contains("workspace"));
	CHECK(with("/capabilities/camera", "Why").contains("unknown kind"));
	CHECK(with("/data/seed/notes", "seed/notes.txt").contains(".json or .csv"));
	CHECK(with("/data/seed/a.b", "seed/a.json").contains("collection"));
	CHECK(with("/data/runs", "x").contains("data"));
}

TEST_CASE("app manifest: ids and paths") {
	CHECK(affresco::format::appId("Flow") == "flow");
	CHECK(affresco::format::appId("habit tracker!") == "habit-tracker");
	CHECK(affresco::format::appId("---") == "app");
	CHECK(affresco::format::appId(std::string(200, 'a')).size() == 64);
	CHECK(affresco::format::insideApp("seed/notes.csv"));
	CHECK(affresco::format::insideApp("./a"));
	CHECK_FALSE(affresco::format::insideApp(""));
	CHECK_FALSE(affresco::format::insideApp(".."));
	CHECK_FALSE(affresco::format::insideApp("a//b"));
	CHECK_FALSE(affresco::format::insideApp("a\nb"));
}

TEST_CASE("app manifest: the capabilities a screen uses, and the ones its app doesn't declare") {
	const json screen = json::parse(R"json({
	"connections": {
		"live": { "url": "wss://Live.Example.com/feed", "auth": { "tokenUrl": "https://user@id.example.org/token" } },
		"local": { "url": "ws://127.0.0.1:7420" } },
	"services": {
		"files": { "root": "workspace", "readOnly": true }, "save": { "root": "workspace" },
		"git": { "provider": "process", "process": "git" } },
	"audio": {}
})json");
	const std::vector<AppCapability> used = affresco::format::usedCapabilities(screen);
	std::vector<std::string> keys;
	keys.reserve(used.size());
	for(const AppCapability& capability : used) keys.push_back(capability.key());
	CHECK(keys == std::vector<std::string>{"network:wss://live.example.com", "network:https://id.example.org",
	                                       "network:ws://127.0.0.1:7420", "folder:workspace:write", "program:git",
	                                       "audio"});
	affresco::format::AppManifest app = affresco::format::parseApp(fullManifest());
	CHECK(affresco::format::undeclaredCapabilities(app, screen) ==
	      std::vector<std::string>{"network:wss://live.example.com", "network:https://id.example.org",
	                               "network:ws://127.0.0.1:7420"});
	app.capabilities[1].write = false; // a folder declared read doesn't cover it written
	CHECK(affresco::format::undeclaredCapabilities(app, screen).size() == 4);
	const json sub = {{"connections", {{"a", {{"url", "https://api.example.com/v1"}}}}}};
	const json bare = {{"connections", {{"a", {{"url", "https://example.com"}}}}}};
	CHECK(affresco::format::undeclaredCapabilities(app, sub).empty());      // "*.example.com" covers its subdomains
	CHECK(affresco::format::undeclaredCapabilities(app, bare).size() == 1); // but not the domain itself
}

TEST_CASE("app manifest: a screen that asks the agents uses the assistant, which its app declares with a reason") {
	json manifest = fullManifest();
	manifest["capabilities"]["assistant"] = "Answers questions about the notes.";
	const affresco::format::AppManifest app = affresco::format::parseApp(manifest);
	REQUIRE(app.capabilities.size() == 5);
	CHECK(app.capabilities[4].kind == AppCapability::Kind::Assistant);
	CHECK(app.capabilities[4].key() == "assistant");
	CHECK(app.capabilities[4].why == "Answers questions about the notes.");
	manifest["capabilities"]["assistant"] = "";
	CHECK(errorOf(manifest).find("capabilities.assistant") != std::string::npos);

	const json asks = json::parse(R"json({ "actions": { "send": [{ "if": "{{true}}", "then": [{ "ask": "{{state.draft}}" }] }] },
		"root": { "children": [{ "type": "button", "onClick": { "ask": "Hello" } }] } })json");
	const json data = json::parse(R"json({ "state": { "ask": "a key" }, "strings": { "en": { "ask": "Ask" } },
		"permissions": { "ask": ["agent"] } })json");
	CHECK(affresco::format::usedCapabilities(asks).size() == 1);
	CHECK(affresco::format::usedCapabilities(asks)[0].key() == "assistant");
	CHECK(affresco::format::usedCapabilities(data).empty()); // a state key or a string is no step
	CHECK(affresco::format::undeclaredCapabilities(app, asks).empty());
	CHECK(affresco::format::undeclaredCapabilities(affresco::format::parseApp(fullManifest()), asks) ==
	      std::vector<std::string>{"assistant"});
}

TEST_CASE("app manifest: file dialogs use files, other windows use windows, each declared with a reason") {
	json manifest = fullManifest();
	manifest["capabilities"]["files"] = "Saves the notes where you choose.";
	manifest["capabilities"]["windows"] = "Shows the notes on a second display.";
	const affresco::format::AppManifest app = affresco::format::parseApp(manifest);
	REQUIRE(app.capabilities.size() == 6);
	CHECK(app.capabilities[4].key() == "files");
	CHECK(app.capabilities[5].key() == "windows");
	manifest["capabilities"]["files"] = 3;
	CHECK(errorOf(manifest).find("capabilities.files") != std::string::npos);

	const auto keys = [](const json& screen) {
		std::vector<std::string> found;
		for(const AppCapability& capability : affresco::format::usedCapabilities(screen))
			found.push_back(capability.key());
		return found;
	};
	using Keys = std::vector<std::string>;
	CHECK(keys(json::parse(R"({ "actions": { "a": [{ "saveFile": "{{state.notes}}" }] } })")) == Keys{"files"});
	CHECK(keys(json::parse(R"({ "actions": { "a": [{ "export": "pdf", "pages": ["page"] }] } })")) == Keys{"files"});
	CHECK(keys(json::parse(R"({ "actions": { "a": [{ "image": "save", "of": "picture" }] } })")) == Keys{"files"});
	CHECK(keys(json::parse(R"({ "actions": { "a": [{ "image": "open", "with": { "asset": "a.png" } }] } })")).empty());
	CHECK(keys(json::parse(R"({ "windows": { "audience": { "image": "slide" } } })")) == Keys{"windows"});
	// An action named like a step, and an adapter's binding, are no step
	CHECK(keys(json::parse(R"({ "actions": { "saveFile": [{ "set": "state.a", "to": 1 }] } })")).empty());
	CHECK(keys(json::parse(R"({ "adapters": { "a": { "bindings": { "current": { "export": "x.y" } } } } })")).empty());
	CHECK(affresco::format::undeclaredCapabilities(app, json::parse(R"({ "windows": { "a": { "image": "b" } } })"))
	          .empty());
}

TEST_CASE("app manifest: fromApp merges the app's theme, models and CSV seed; the screen's own entries win") {
	const auto folder = std::filesystem::temp_directory_path() / "affrescoui-app-test";
	writeApp(folder / "notes");
	const auto data = folder / "data";
	{
		affresco::Screen screen = affresco::Screen::fromApp(folder / "notes", data);
		REQUIRE(screen.app() != nullptr);
		CHECK(screen.app()->id == "notes");
		const json document = json::parse(screen.documentJson());
		CHECK(document["tokens"]["gap"] == 4);    // the screen's own
		CHECK(document["tokens"]["radius"] == 6); // the theme's
		CHECK(screen.data().size("notes") == 2);
		const std::optional<affresco::data::Record> second = screen.data().get("notes", "n2");
		REQUIRE(second.has_value());
		CHECK(second->fields["title"] == "Second");
		CHECK(second->fields["done"] == true);
		(void)screen.data().insert("notes", {{"title", "Third"}}, "app");
	}
	CHECK(std::filesystem::exists(data / "apps" / "notes" / "data.sqlite"));
	{
		affresco::Screen again = affresco::Screen::fromApp(folder / "notes" / "app.json", data);
		CHECK(again.data().size("notes") == 3); // the seed is only the first records
	}
	std::filesystem::remove_all(folder);
}

TEST_CASE("app manifest: fromApp moves the entry screen's old data into the app's folder, once") {
	const auto folder = std::filesystem::temp_directory_path() / "affrescoui-app-move-test";
	writeApp(folder / "notes");
	const auto data = folder / "data";
	write(data / "notes.state.json", R"({"/draft": "kept"})");
	{
		const affresco::Screen screen = affresco::Screen::fromApp(folder / "notes", data);
		CHECK(json::parse(screen.stateJson())["draft"] == "kept");
	}
	CHECK_FALSE(std::filesystem::exists(data / "notes.state.json"));
	CHECK(std::filesystem::exists(data / "apps" / "notes" / "state.json"));
	std::filesystem::remove_all(folder);
}

TEST_CASE("app manifest: an app's entry screen opened on its own is the app; another screen beside it isn't") {
	const auto folder = std::filesystem::temp_directory_path() / "affrescoui-app-entry-test";
	writeApp(folder / "notes");
	write(folder / "notes" / "other.screen.json", R"json({ "format": "affrescoui-screen", "version": 2,
		"root": { "type": "label", "id": "other", "text": "other" } })json");
	CHECK(affresco::Screen::appOf(folder / "notes" / "index.screen.json") == folder / "notes" / "app.json");
	CHECK(affresco::Screen::appOf(folder / "notes" / "other.screen.json").empty());
	{
		affresco::Screen screen = affresco::Screen::fromFile(folder / "notes" / "index.screen.json", folder / "data",
		                                                     affresco::Screen::History::InMemory);
		REQUIRE(screen.app() != nullptr);
		CHECK(screen.data().size("notes") == 2); // the seed, as the host opens it
		CHECK(json::parse(screen.documentJson())["tokens"]["radius"] == 6);
	}
	CHECK(affresco::Screen::fromFile(folder / "notes" / "other.screen.json", folder / "data").app() == nullptr);
	write(folder / "notes" / "app.json", "{");
	CHECK_THROWS_AS(static_cast<void>(affresco::Screen::appOf(folder / "notes" / "index.screen.json")),
	                affresco::ScreenError);
	std::filesystem::remove_all(folder);
}

TEST_CASE("app manifest: fromApp refuses what isn't an app, a bad part, and a file outside the folder") {
	const auto folder = std::filesystem::temp_directory_path() / "affrescoui-app-refused-test";
	const auto app = folder / "notes";
	const auto refused = [&] {
		try {
			static_cast<void>(affresco::Screen::fromApp(app, folder / "data", affresco::Screen::History::InMemory));
		} catch(const affresco::ScreenError& error) {
			return std::string(error.what());
		}
		return std::string();
	};
	writeApp(app);
	CHECK(refused().empty());
	std::filesystem::remove(app / "app.json");
	CHECK(refused().contains("app.json"));
	writeApp(app);
	write(app / "theme.json", R"({ "tokens": {}, "actions": { "run": [] } })");
	CHECK(refused().contains("\"actions\""));
	writeApp(app);
	write(folder / "outside.json", R"({ "tokens": {} })");
	std::filesystem::create_directory_symlink(folder, app / "linked");
	json manifest = json::parse(std::ifstream(app / "app.json"));
	manifest["themes"] = {"linked/outside.json"};
	write(app / "app.json", manifest.dump());
	CHECK(refused().contains("leads out of the app's folder"));
	writeApp(app);
	write(app / "seed" / "notes.csv", "Title,Done\nFirst,no\n,yes\n");
	CHECK(refused().contains("data.seed.notes"));
	writeApp(app);
	write(app / "models.json", R"({ "models": { "other": { "version": 1, "fields": {} } } })");
	CHECK(refused().contains("no model \"notes\""));
	std::filesystem::remove_all(folder);
}

// NOLINTEND(bugprone-unchecked-optional-access)
