/// @file
/// @brief The workloads that need a window: load, update, revision, layout and frame. They load the templates the way
/// affrescoui_host does, into hidden windows that only get the events a workload sends.

#include "Bench.hpp"
#include "CodeEditor.hpp"

#define GLFW_INCLUDE_NONE // only the window hint: no OpenGL header (Linux may have none installed)
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
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

Context::Context(std::filesystem::path screens, std::filesystem::path edits)
    : screens_(std::move(screens)), edits_(std::move(edits)) {}

Application& Context::application() {
	if(!application_) application_.emplace();
	return *application_;
}

void Context::configure(Screen& screen) const {
	screen.setPlaceholderTypes(true);     // as affrescoui_host does
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
	auto screen = std::make_unique<Screen>(Screen::fromString(text, baseDirectory));
	configure(*screen);
	screen->build(window);
	loaded_.push_back(std::move(screen));
	return *loaded_.back();
}

/* Inputs */

std::string readFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if(!file) throw std::runtime_error("could not open " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

std::vector<std::string> templateNames(const std::filesystem::path& directory) {
	constexpr std::string_view suffix = ".screen.json";
	std::vector<std::string> names;
	for(const auto& entry : std::filesystem::directory_iterator(directory)) {
		const std::string file = entry.path().filename().string();
		if(file.ends_with(suffix)) names.push_back(file.substr(0, file.size() - suffix.size()));
	}
	std::ranges::sort(names);
	return names;
}

json kanbanWithCards(const std::filesystem::path& directory, int cards) {
	json document = json::parse(readFile(directory / "kanban.screen.json"));
	json& columns = document["state"]["columns"];
	std::vector<json> originals;
	for(const json& column : columns)
		for(const json& card : column["cards"]) originals.push_back(card);

	const auto total = static_cast<std::size_t>(cards);
	if(total < originals.size()) throw std::invalid_argument("the Kanban template has more cards than asked for");
	for(std::size_t count = originals.size(); count < total; ++count) // one card per column in turn
		columns[count % columns.size()]["cards"].push_back(originals[count % originals.size()]);
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

/// @brief The card counts the Kanban workloads fill the template to: as it is, and two larger boards.
constexpr std::array<int, 3> cardCounts = {14, 100, 400};

/// @brief "load/<template>": parsing, validating and building each template into a window opened beforehand.
/// @param context The context.
/// @return One workload per template.
std::vector<Workload> loadWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const std::string& name : templateNames(context.screens())) {
		auto text = std::make_shared<const std::string>(readFile(context.screens() / (name + ".screen.json")));
		auto document = std::make_shared<const json>(json::parse(*text));
		auto window = std::make_shared<Window*>(nullptr);
		Workload workload{.name = "load/" + name, .unit = "ms", .runs = 5};
		workload.prepare = [&context, document, window] { *window = &context.openWindow(*document); };
		workload.step = [&context, text, window] {
			static_cast<void>(context.build(*text, context.screens(), **window));
		};
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

/// @brief "update/<cards>": one state change on the Kanban template (renaming the board), its reconciliation and
/// layout.
/// @param context The context.
/// @return One workload per card count.
std::vector<Workload> updateWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const int cards : cardCounts) {
		json document = kanbanWithCards(context.screens(), cards);
		document["actions"]["renameBoard"] = json::parse(R"([{"set": "state.board", "to": "{{event.name}}"}])");
		const LoadedScreen loaded = context.load(document, context.screens());
		auto renamed = std::make_shared<bool>(false);
		Workload workload{.name = "update/" + std::to_string(cards), .runs = 15};
		workload.step = [loaded, renamed] { renameBoard(loaded, *renamed); };
		workloads.push_back(std::move(workload));
	}
	return workloads;
}

/// @brief Whether an edit changes the document, not only the live state: then undo() can take it back.
/// @param edit Compact edits or a JSON Patch.
/// @return true if one of its paths is outside "/state".
bool changesDocument(const json& edit) {
	auto outsideState = [](const std::string& path) { return path != "/state" && !path.starts_with("/state/"); };
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
/// then reconciliation and layout). Between runs it is taken back, unmeasured.
/// @param context The context.
/// @return One workload per edit.
std::vector<Workload> revisionWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const std::string& name : templateNames(context.screens())) {
		const std::filesystem::path file = context.edits() / (name + ".json");
		if(!std::filesystem::exists(file)) continue;
		const json document = json::parse(readFile(context.screens() / (name + ".screen.json")));
		const json edits = json::parse(readFile(file)); // named: items() refers to it during the loop
		for(const auto& [editName, edit] : edits.items()) {
			const LoadedScreen loaded = context.load(document, context.screens());
			json revision = edit;
			revision["author"] = "bench";
			const json reset = {{"author", "bench"},
			                    {"message", "Put the state back"},
			                    {"set", {{"/state", json::parse(loaded.screen.stateJson())}}}};
			auto screen = std::make_shared<EditedScreen>(
			    EditedScreen{loaded, revision.dump(), reset.dump(), changesDocument(edit)});
			Workload workload{.name = "revision/" + name + "/" + editName, .runs = 15};
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

/// @brief "layout/400": laying out the largest screen, the Kanban template with 400 cards, after a resize.
/// @param context The context.
/// @return The workload.
std::vector<Workload> layoutWorkloads(Context& context) {
	const LoadedScreen loaded = context.load(kanbanWithCards(context.screens(), cardCounts.back()), context.screens());
	auto smaller = std::make_shared<bool>(false);
	Workload workload{.name = "layout/" + std::to_string(cardCounts.back()), .runs = 15};
	workload.step = [&window = loaded.window, smaller] { resize(window, *smaller); };
	return single(std::move(workload));
}

/// @brief "frame/<cards>": laying out and drawing a full frame of the Kanban template, in the main thread's CPU time
/// (the GPU's work isn't counted).
/// @param context The context.
/// @return One workload per card count, the smallest and the largest.
std::vector<Workload> frameWorkloads(Context& context) {
	std::vector<Workload> workloads;
	for(const int cards : {cardCounts.front(), cardCounts.back()}) {
		const LoadedScreen loaded = context.load(kanbanWithCards(context.screens(), cards), context.screens());
		Workload workload{.name = "frame/" + std::to_string(cards), .runs = 15, .cpuTime = true};
		workload.step = [&window = loaded.window] {
			window.invalidate(); // a full frame: layout and drawing
			window.render();
		};
		workloads.push_back(std::move(workload));
	}
	return workloads;
}

} // namespace

std::vector<Group> screenGroups() {
	return {{"load", loadWorkloads},
	        {"update", updateWorkloads},
	        {"revision", revisionWorkloads},
	        {"layout", layoutWorkloads},
	        {"frame", frameWorkloads}};
}

} // namespace affresco::bench
