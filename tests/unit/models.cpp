/// @file
/// @brief Data models: a repeat over rows the application owns, updated row by row, gives the panels a full build
/// gives; agents see a model's count, shape and a sample, never every row.

#include "Headless.hpp"
#include "Models.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <vector>

using affresco::json;
using affresco::ListModel;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A list of files from a model, with a count that reads the state and a row that reads both.
constexpr const char* files = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"state": { "selected": "b" },
	"actions": { "select": [{ "set": "state.selected", "to": "{{event.v}}" }] },
	"root": { "type": "column", "children": [
		{ "type": "label", "id": "selected", "text": "{{state.selected}}" },
		{ "type": "row", "repeat": { "for": "file", "in": "models.files", "index": "i" }, "children": [
			{ "type": "label", "text": "{{i}} {{file.name}} {{file.size}}" },
			{ "type": "label", "when": "{{file.name == state.selected}}", "text": "*" }
		] }
	] }
})json";

/// @brief Writes a panel tree down: each panel's frame, visibility and label text, depth first.
/// @param panel The subtree's root.
/// @param out Gets the text.
void describe(const affresco::Panel& panel, std::string& out) {
	const affresco::Rect& frame = panel.frame();
	out += std::to_string(frame.left) + "," + std::to_string(frame.top) + " " + std::to_string(frame.width()) + "x" +
	       std::to_string(frame.height()) + (panel.isVisible() ? " v" : " h");
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel)) out += " '" + label->text() + "'";
	out += " [";
	for(const auto& child : panel.children()) describe(*child, out);
	out += "]";
}

/// @brief A file row.
/// @param name Its name.
/// @param size Its size.
/// @return The row.
json file(const std::string& name, int size) { return {{"name", name}, {"size", size}}; }

/// @brief The tree a full build of the same document, state and model gives.
/// @param screen The screen updated incrementally.
/// @param model Its model.
/// @param clock The clock.
/// @return describe() of it.
std::string builtAgain(const affresco::Screen& screen, const std::shared_ptr<ListModel>& model,
                       const ManualClock& clock) {
	json document = json::parse(screen.documentJson());
	document["state"] = json::parse(screen.stateJson());
	const auto window = createHeadlessWindow(640, 480, clock); // outlives the screen
	affresco::Screen fresh = affresco::Screen::fromString(document.dump());
	fresh.setModel("files", model);
	fresh.build(*window);
	window->render();
	std::string out;
	describe(window->root(), out);
	return out;
}

/// @brief Runs one random change: of a row, of the rows, or of the state.
/// @param screen The screen.
/// @param model Its model.
/// @param sender A panel to send the state's change from.
/// @param random The generator.
void change(affresco::Screen& screen, ListModel& model, affresco::Panel& sender, std::mt19937& random) {
	const auto pick = [&](std::size_t n) { return static_cast<std::size_t>(random() % n); };
	const std::string names[] = {"a", "b", "c"};
	const std::string name = names[pick(3)];
	const std::size_t count = model.size();
	switch(pick(5)) {
	case 0:
		if(count) model.set(pick(count), file(name, static_cast<int>(pick(100))));
		break;
	case 1: model.insert(pick(count + 1), {file(name, 1), file(name, 2)}); break;
	case 2:
		if(count) model.remove(pick(count), 1 + pick(2));
		break;
	case 3:
		if(pick(4) == 0) model.reset({file(name, 7)});
		break;
	default: screen.runAction("select", sender, json{{"v", name}}.dump()); break;
	}
}

} // namespace

TEST_CASE("format: a data model is named by \"models.<name>\" only") {
	using affresco::format::modelName;
	CHECK(modelName("models.files") == "files");
	CHECK(modelName("{{models.files}}") == "files");
	CHECK(modelName("models.my_files2") == "my_files2");
	CHECK_FALSE(modelName("state.files"));
	CHECK_FALSE(modelName("models."));
	CHECK_FALSE(modelName("models.files.rows"));
	CHECK_FALSE(modelName("models.1files"));
	CHECK_FALSE(modelName("{{models.files + 1}}"));
}

TEST_CASE("format: a repeat over a data model, changed row by row, gives the panels a full build gives") {
	const ManualClock clock;
	for(unsigned seed = 1; seed <= 10; ++seed) {
		auto model = std::make_shared<ListModel>(std::vector<json>{file("a", 1), file("b", 2), file("c", 3)});
		const auto window = createHeadlessWindow(640, 480, clock);
		affresco::Screen screen = affresco::Screen::fromString(files);
		screen.setModel("files", model);
		screen.build(*window);
		window->render();
		std::mt19937 random(seed);
		for(int step = 0; step < 30; ++step) {
			const int batch = 1 + static_cast<int>(random() % 3); // one update for several changes, sometimes
			for(int i = 0; i < batch; ++i) change(screen, *model, window->root(), random);
			window->render();
			std::string incremental;
			describe(window->root(), incremental);
			INFO("seed " << seed << ", step " << step);
			REQUIRE(incremental == builtAgain(screen, model, clock));
		}
	}
}

TEST_CASE("format: agents see a data model's count, shape and one sample row") {
	affresco::Screen screen = affresco::Screen::fromString(files);
	std::vector<json> rows;
	for(int k = 0; k < 10000; ++k) rows.push_back(file("f" + std::to_string(k), k));
	screen.setModel("files", std::make_shared<ListModel>(std::move(rows)));
	const json models = json::parse(screen.catalogJson())["models"];
	REQUIRE(models.contains("files"));
	CHECK(models["files"]["count"] == 10000);
	CHECK(models["files"]["shape"] == json{{"name", "string"}, {"size", "number"}});
	CHECK(models["files"]["sample"] == file("f0", 0));
	CHECK(models.dump().size() < 200); // never every row
}

TEST_CASE("format: a repeat over a model nobody registered names the models there are") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(640, 480, clock);
	affresco::Screen screen = affresco::Screen::fromString(files);
	screen.setModel("logs", std::make_shared<ListModel>());
	CHECK_THROWS_WITH_AS(screen.build(*window), doctest::Contains("no data model 'files'"), affresco::ScreenError);
	CHECK_THROWS_AS(screen.setModel("bad name", std::make_shared<ListModel>()), std::invalid_argument);
}

TEST_CASE("format: a list model tells what changed, by range") {
	ListModel model({file("a", 1)});
	std::vector<affresco::ModelChange> changes;
	[[maybe_unused]] const auto connection =
	    model.onChange.connect([&](const affresco::ModelChange& change) { changes.push_back(change); });
	model.insert(1, {file("b", 2), file("c", 3)});
	model.set(0, file("z", 9));
	model.remove(1, 5); // past the end: only what is there
	REQUIRE(changes.size() == 3);
	CHECK(changes[0].kind == affresco::ModelChange::Kind::Inserted);
	CHECK(changes[0].first == 1);
	CHECK(changes[0].count == 2);
	CHECK(changes[1].kind == affresco::ModelChange::Kind::Changed);
	CHECK(changes[2].count == 2);
	CHECK(model.size() == 1);
	CHECK(model.row(0) == file("z", 9));
	CHECK(model.row(4).is_null());
}
