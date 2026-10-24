/// @file
/// @brief The workloads that need a window: load, update, revision, layout and frame. They load the templates the way
/// affrescoui_launcher does, into hidden windows that only get the events a workload sends.

#include "Bench.hpp"
#include "CodeEditor.hpp"
#include "Headless.hpp"
#include "Templates.hpp"

#define GLFW_INCLUDE_NONE // only the window hint: no OpenGL header (Linux may have none installed)
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::bench {

/* Context */

Context::Context(std::filesystem::path examples, std::filesystem::path edits)
    : examples_(std::move(examples)), edits_(std::move(edits)) {}

std::filesystem::path Context::templateFolder(const std::string& name) const {
	return templateDirectory(examples_, name);
}

Application& Context::application() {
	if(!application_) application_.emplace();
	return *application_;
}

void Context::configure(Screen& screen) {
	screen.setPlaceholderTypes(true);     // as affrescoui_launcher does
	examples::registerCodeEditor(screen); // and its application-layer "code" type
	// Channels find nobody, so no workload waits on a host
	screen.setSocketDirectory(std::filesystem::temp_directory_path() / "affrescoui-bench-no-host");
}

Window& Context::openWindow(const json& document) {
	Application& app = application();
	const json settings = document.value("window", json::object());
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // never on screen
	Window& window = app.createWindow(settings.value("title", "AffrescoUI"), settings.value("width", 800),
	                                  settings.value("height", 600));
	window.setMinSize(settings.value("minWidth", 0), settings.value("minHeight", 0));
	return window;
}

LoadedScreen Context::load(const json& document, const std::filesystem::path& baseDirectory) {
	Application& app = application();
	auto screen = std::make_unique<Screen>(Screen::fromString(document.dump(), baseDirectory));
	configure(*screen);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	Window& window = screen->createWindow(app);
	loaded_.push_back(std::move(screen));
	return {*loaded_.back(), window};
}

Screen& Context::build(const std::string& text, const std::filesystem::path& baseDirectory, Window& window) {
	Screen& screen = create(text, baseDirectory);
	screen.build(window);
	return screen;
}

Screen& Context::create(const std::string& text, const std::filesystem::path& baseDirectory) {
	auto screen = std::make_unique<Screen>(Screen::fromString(text, baseDirectory));
	configure(*screen);
	loaded_.push_back(std::move(screen));
	return *loaded_.back();
}

/* Inputs */

std::string readFile(const std::filesystem::path& path) {
	const std::ifstream file(path, std::ios::binary);
	if(!file) throw std::runtime_error("could not open " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

json screenDocument(const std::filesystem::path& file) {
	const Screen screen = Screen::fromString(readFile(file), file.parent_path());
	json document = json::parse(screen.documentJson());
	document["state"] = json::parse(screen.stateJson());
	return document;
}

json screenDocument(const json& document, const std::filesystem::path& directory) {
	const Screen screen = Screen::fromString(document.dump(), directory);
	json merged = json::parse(screen.documentJson());
	merged["state"] = json::parse(screen.stateJson());
	return merged;
}

json kanbanWithCards(const std::filesystem::path& directory, int cards) {
	json document = json::parse(readFile(directory / "index.screen.json"));
	json& seed = document["models"]["issues"]["seed"]; // the issues are records: the board shows the launch's
	std::vector<json> originals;
	for(const json& card : seed)
		if(card["board"] == "launch") originals.push_back(card);

	const auto total = static_cast<std::size_t>(cards);
	if(total < originals.size()) throw std::invalid_argument("the Kanban template has more cards than asked for");
	for(std::size_t count = originals.size(); count < total; ++count) { // one card per column in turn
		json card = originals[count % originals.size()];
		card["id"] = card["ref"] = "FLO-P" + std::to_string(count); // unique, as a ref must be
		card["status"] = count % 4;
		card["order"] = count;
		seed.push_back(std::move(card));
	}
	return document;
}

namespace {

/// @brief Runs the updates that state changes have queued, as the end of a routed event does, without drawing: a key
/// release that no panel has the focus for and no shortcut uses.
/// @param window The window.
void runQueuedUpdates(Window& window) { window.routeKey({Key::Unknown, 0, Action::Release, {}}); }

/// @brief Lays the window out, without drawing.
/// @param window The window.
void layOut(Window& window) { static_cast<void>(window.panelAt({})); }

/// @brief The card counts the Kanban workloads fill the template to, the roadmap item's: the template has 12 cards, so
/// even the smallest board has two copies.
constexpr std::array<int, 3> cardCounts = {14, 100, 400};

/// @brief "load/<template>": parsing, validating and building each template into a window opened beforehand.
/// @param context The context.
/// @return One workload per template.
std::vector<Workload> loadWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const std::string& name : templateNames(context.examples())) {
		const std::filesystem::path folder = context.templateFolder(name);
		const auto text = std::make_shared<const std::string>(readFile(folder / "index.screen.json"));
		const auto document = std::make_shared<const json>(json::parse(*text));
		const auto window = std::make_shared<Window*>(nullptr);
		Workload workload{.name = "load/" + name, .unit = "ms", .runs = 5};
		workload.prepare = [&context, document, window] { *window = &context.openWindow(*document); };
		workload.step = [&context, text, folder, window] { static_cast<void>(context.build(*text, folder, **window)); };
		workloads.push_back(std::move(workload));
	}
	return workloads;
}

/// @brief "build/<template>": building each template into a window, its document read and its fonts loaded
/// beforehand. Each run keeps its screen, so `retainedBytes` is the memory of the panels built and what they keep.
/// @param context The context.
/// @return One workload per template.
std::vector<Workload> buildWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const std::string& name : templateNames(context.examples())) {
		const std::filesystem::path folder = context.templateFolder(name);
		const auto text = std::make_shared<const std::string>(readFile(folder / "index.screen.json"));
		const auto document = std::make_shared<const json>(json::parse(*text));
		const auto window = std::make_shared<Window*>(nullptr);
		const auto screen = std::make_shared<Screen*>(nullptr);
		Workload workload{.name = "build/" + name, .unit = "ms", .runs = 5};
		workload.prepare = [&context, text, folder, document, window, screen] {
			*window = &context.openWindow(*document);
			*screen = &context.create(*text, folder);
		};
		workload.step = [window, screen] { (*screen)->build(**window); };
		workloads.push_back(std::move(workload));
	}
	return workloads;
}

/// @brief Renames the board with a state change, then runs the update and lays out, as a click on a button would.
/// @param loaded The Kanban template, with its "renameBoard" action.
/// @param renamed Which name it has; flipped, so the state really changes on every run.
void renameBoard(const LoadedScreen& loaded, bool& renamed) {
	renamed = !renamed;
	const char* event = renamed ? R"({"name": "Launch"})" : R"({"name": "Product launch"})";
	loaded.screen.runAction("renameBoard", loaded.window.root(), event);
	runQueuedUpdates(loaded.window);
	layOut(loaded.window);
}

/// @brief A screen of 50,000 panels: a keyed list of 25,000 rows, each a row and a label showing its item's value,
/// with a "touch" action that changes one item's value.
/// @return The document.
json fiftyThousandPanels() {
	json items = json::array();
	for(int i = 0; i < 25'000; ++i) items.push_back({{"id", i}, {"v", i}});
	json document = json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"actions": { "touch": [{ "set": "state.items[event.i].v", "to": "{{event.v}}" }] },
		"root": { "type": "panel", "children": [{ "type": "column", "children": [
			{ "type": "row", "repeat": { "for": "item", "in": "state.items", "key": "{{item.id}}" }, "children": [
				{ "type": "label", "text": "{{item.v}}" }
			] }
		] }] }
	})json");
	document["state"]["items"] = std::move(items);
	return document;
}

/// @brief "update/panels-50000": one changed value on a screen of 50,000 panels (fiftyThousandPanels()), its update
/// and layout; "update/panels-50000-without-layout": the update alone. The update visits only what reads the changed
/// value.
/// @param context The context.
/// @param layout Whether the window is laid out after the update.
/// @return The workload.
Workload fiftyThousandPanelsWorkload(Context& context, bool layout) {
	const LoadedScreen loaded = context.load(fiftyThousandPanels(), context.templateFolder("kanban"));
	const auto run = std::make_shared<int>(0);
	Workload workload{.name = layout ? "update/panels-50000" : "update/panels-50000-without-layout", .runs = 15};
	workload.step = [loaded, run, layout] {
		++*run;
		const json event = {{"i", (*run * 7919) % 25'000}, {"v", -*run}};
		loaded.screen.runAction("touch", loaded.window.root(), event.dump());
		runQueuedUpdates(loaded.window);
		if(layout) layOut(loaded.window);
	};
	return workload;
}

/// @brief "update/<cards>": one state change on the Kanban template (renaming the board), its reconciliation and
/// layout; and the two "update/panels-50000" workloads.
/// @param context The context.
/// @return One workload per card count.
std::vector<Workload> updateWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const int cards : cardCounts) {
		json document = kanbanWithCards(context.templateFolder("kanban"), cards);
		document["actions"]["renameBoard"] = json::parse(R"([{"set": "state.board", "to": "{{event.name}}"}])");
		const LoadedScreen loaded = context.load(document, context.templateFolder("kanban"));
		const auto renamed = std::make_shared<bool>(false);
		Workload workload{.name = "update/" + std::to_string(cards), .runs = 15};
		workload.step = [loaded, renamed] { renameBoard(loaded, *renamed); };
		workloads.push_back(std::move(workload));
	}
	workloads.push_back(fiftyThousandPanelsWorkload(context, true));
	workloads.push_back(fiftyThousandPanelsWorkload(context, false));
	return workloads;
}

/// @brief Whether an edit changes the document, not only the live state: then undo() can take it back.
/// @param edit Compact edits or a JSON Patch.
/// @return true if one of its paths is outside "/state".
bool changesDocument(const json& edit) {
	const auto outsideState = [](const std::string& path) { return path != "/state" && !path.starts_with("/state/"); };
	for(const char* operation : {"set", "add", "move"})
		if(edit.contains(operation))
			for(const auto& [path, value] : edit[operation].items()) {
				if(outsideState(path)) return true;
				if(operation == std::string_view("move") && outsideState(value.get<std::string>())) return true; // to
			}
	for(const char* list : {"remove", "patch"})
		if(edit.contains(list))
			for(const json& item : edit[list])
				if(outsideState(item.is_string() ? item.get<std::string>() : item.value("path", ""))) return true;
	return false;
}

/// @brief One edit on its own screen (so no other edit's history weighs on it), and how to take it back between runs.
struct EditedScreen {
	LoadedScreen loaded;          ///< The screen.
	std::string revision;         ///< The edit, as a revision.
	std::string resetState;       ///< A revision that puts the live state back as it was loaded.
	bool documentChanged = false; ///< The edit changes the document too, so taking it back needs an undo().
	bool applied = false;         ///< The edit is applied and not taken back yet.
};

/// @brief Takes the edit back, if it is applied: undo() for the document, a revision for the live state.
/// @param screen The screen.
/// @throws std::runtime_error If that fails.
void takeBack(EditedScreen& screen) {
	if(!screen.applied) return;
	std::string error;
	if(screen.documentChanged && !screen.loaded.screen.undo(&error)) throw std::runtime_error("undo: " + error);
	if(!screen.loaded.screen.applyRevision(screen.resetState, &error))
		throw std::runtime_error("resetting the state: " + error);
	screen.applied = false;
}

/// @brief Applies the edit, then lays the window out.
/// @param screen The screen.
/// @throws std::runtime_error If the revision is refused.
void applyEdit(EditedScreen& screen) {
	std::string error;
	if(!screen.loaded.screen.applyRevision(screen.revision, &error)) throw std::runtime_error(error);
	screen.applied = true;
	layOut(screen.loaded.window);
}

/// @brief "revision/<template>/<edit>": each canonical agent edit of tests/perf/edits applied as a revision (a commit,
/// then reconciliation and layout). Between runs it is taken back, unmeasured. Taking it back commits too, so the
/// history, and with it the cost, grows from run to run (tests/perf/README.md).
/// @param context The context.
/// @return One workload per edit.
std::vector<Workload> revisionWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const std::string& name : templateNames(context.examples())) {
		const std::filesystem::path file = context.edits() / (name + ".json");
		if(!std::filesystem::exists(file)) continue;
		const std::filesystem::path folder = context.templateFolder(name);
		const json document = json::parse(readFile(folder / "index.screen.json"));
		const json edits = json::parse(readFile(file)); // named: items() refers to it during the loop
		for(const auto& [editName, edit] : edits.items()) {
			const LoadedScreen loaded = context.load(document, folder);
			json revision = edit;
			revision["author"] = "bench";
			const json reset = {{"author", "bench"},
			                    {"message", "Put the state back"},
			                    {"set", {{"/state", json::parse(loaded.screen.stateJson())}}}};
			const auto screen = std::make_shared<EditedScreen>(
			    EditedScreen{loaded, revision.dump(), reset.dump(), changesDocument(edit)});
			Workload workload{.name = std::format("revision/{}/{}", name, editName), .runs = 15};
			workload.prepare = [screen] { takeBack(*screen); };
			workload.step = [screen] { applyEdit(*screen); };
			workloads.push_back(std::move(workload));
		}
	}
	return workloads;
}

/// @brief Resizes the window, then lays it out.
/// @param window The window.
/// @param smaller Which size it has; flipped, so the size really changes on every run.
void resize(Window& window, bool& smaller) {
	smaller = !smaller;
	window.setSize(smaller ? 1180 : 1280, smaller ? 760 : 800);
	layOut(window);
}

/// @brief How many panels a tree has.
/// @param panel The tree's root.
/// @return The panels, the root included.
std::size_t panelCount(const Panel& panel) {
	std::size_t count = 1;
	for(const auto& child : panel.children()) count += panelCount(*child);
	return count;
}

/// @brief The cards that fill the Kanban template to at least 5,000 panels (checked when the workload is set up).
constexpr int cardsForFiveThousandPanels = 770;

/// @brief "layout/400": laying out the largest template screen, the Kanban template with 400 cards, after a resize;
/// "layout/5000": the same with enough cards for 5,000 panels.
/// @param context The context.
/// @return The workloads.
std::vector<Workload> layoutWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const int cards : {cardCounts.back(), cardsForFiveThousandPanels}) {
		const LoadedScreen loaded =
		    context.load(kanbanWithCards(context.templateFolder("kanban"), cards), context.templateFolder("kanban"));
		layOut(loaded.window);
		const std::size_t panels = panelCount(loaded.window.root());
		if(cards == cardsForFiveThousandPanels && panels < 5000)
			throw std::logic_error(std::format("layout/5000 has only {} panels", panels));
		const auto smaller = std::make_shared<bool>(false);
		Workload workload{.name =
		                      cards == cardsForFiveThousandPanels ? "layout/5000" : "layout/" + std::to_string(cards),
		                  .runs = 15};
		workload.step = [&window = loaded.window, smaller] { resize(window, *smaller); };
		workloads.push_back(std::move(workload));
		if(cards != cardsForFiveThousandPanels) continue;

		// One card's title changes: only what that moves is laid out again
		auto* title = dynamic_cast<Label*>(loaded.window.root().findById("cardTitle"));
		if(!title) throw std::logic_error("the Kanban template has no cardTitle label");
		Workload change{.name = "layout/5000/change", .runs = 15};
		change.step = [&window = loaded.window, title, smaller] {
			*smaller = !*smaller;
			title->setText(*smaller ? "A title that changed" : "Another title");
			layOut(window);
		};
		workloads.push_back(std::move(change));
	}
	return workloads;
}

/// @brief The first text field of a tree, depth first.
/// @param panel The tree.
/// @return The field, or nullptr.
TextArea* firstTextArea(Panel& panel) {
	if(auto* field = dynamic_cast<TextArea*>(&panel)) return field;
	for(const auto& child : panel.children())
		if(TextArea* field = firstTextArea(*child)) return field;
	return nullptr;
}

/// @brief "frame/<cards>": laying out and drawing a full frame of the Kanban template, in the main thread's CPU time
/// (the GPU's work isn't counted).
/// @param context The context.
/// @return One workload per card count, the smallest and the largest.
std::vector<Workload> frameWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const int cards : {cardCounts.front(), cardCounts.back()}) {
		const LoadedScreen loaded =
		    context.load(kanbanWithCards(context.templateFolder("kanban"), cards), context.templateFolder("kanban"));
		Workload workload{.name = "frame/" + std::to_string(cards), .runs = 15, .cpuTime = true};
		workload.step = [&window = loaded.window] {
			window.invalidate(); // a full frame: layout and drawing
			window.render();
		};
		workloads.push_back(std::move(workload));
	}

	// The caret of the board's text field appears and disappears, as a blink does: one small region drawn again
	const LoadedScreen busy = context.load(kanbanWithCards(context.templateFolder("kanban"), cardCounts.back()),
	                                       context.templateFolder("kanban"));
	TextArea* field = firstTextArea(busy.window.root());
	if(!field) throw std::logic_error("the Kanban template has no text field");
	busy.window.render();
	Workload blink{.name = "frame/blink/" + std::to_string(cardCounts.back()), .runs = 15, .cpuTime = true};
	blink.step = [&window = busy.window, field] {
		window.setFocus(window.focusedPanel() == field ? nullptr : field);
		window.render();
	};
	workloads.push_back(std::move(blink));

	// The same, without presenting the frame: what it costs the library, the windowing system's part left out
	Workload drawn{.name = "frame/blink/" + std::to_string(cardCounts.back()) + "/drawn", .runs = 15, .cpuTime = true};
	drawn.step = [&window = busy.window, field] {
		window.setFocus(window.focusedPanel() == field ? nullptr : field);
		detail::renderWithoutPresenting(window);
	};
	workloads.push_back(std::move(drawn));
	return workloads;
}

} // namespace

std::vector<Group> screenGroups() {
	return {{"load", loadWorkloads},
	        {"build", buildWorkloads},
	        {"update", updateWorkloads},
	        {"revision", revisionWorkloads},
	        {"layout", layoutWorkloads},
	        {"frame", frameWorkloads},
	        {"editor", editorWorkloads},
	        {"forge", forgeWorkloads},
	        {"budgets", budgetWorkloads}};
}

} // namespace affresco::bench
