/// @file
/// @brief The "forge" group: Forge (examples/forge), the demo IDE, loaded as the host loads it, with the folder it
/// writes granted and tests/render/forge/folder open. It measures opening the project, typing, switching tabs,
/// searching for a file, the command palette, the find bar, the secondary side bar and the minimap, each with its
/// update and layout.

#include "Bench.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {

namespace {

/// @brief Forge's folder, beside the templates' folders.
/// @param context The context.
/// @return examples/forge.
std::filesystem::path forgeDirectory(const Context& context) { return context.examples() / "forge" / "json"; }

/// @brief Where Forge's services post their results: the workloads pump it until a file they open is read.
/// @return The executor, which outlives every screen (the Context goes first).
PumpedExecutor& forgeExecutor() {
	static PumpedExecutor executor;
	return executor;
}

/// @brief Forge's screen document, with four bench actions merged in for what a person does through a binding (typing
/// into the editor, typing into the quick pick or the find bar) and that no action of Forge's takes from an event.
/// @param context The context.
/// @return The document, as text.
std::string forgeDocument(const Context& context) {
	json document = json::parse(readFile(forgeDirectory(context) / "index.screen.json"));
	document["actions"]["benchType"] = json::parse(R"([{"edit": "type", "in": "#editor", "text": "{{event.text}}"}])");
	document["actions"]["benchQuery"] = json::parse(
	    R"([{"set": "state.quickPick.query", "to": "{{event.query}}"}, {"set": "state.quickPick.open", "to": true}])");
	document["actions"]["benchClose"] = json::parse(R"([{"set": "state.quickPick.open", "to": false}])");
	document["actions"]["benchFind"] = json::parse(
	    R"([{"set": "state.findBar.text", "to": "{{event.text}}"}, {"run": "findStep", "with": {"step": "first"}}])");
	return document.dump();
}

/// @brief Builds Forge into a window, with the fixed folder as its workspace and no program granted (no server, shell
/// or adapter starts), and lays it out.
/// @param context The context.
/// @param text The document (forgeDocument()).
/// @param window An empty window.
/// @return The screen and its window.
LoadedScreen buildForge(Context& context, const std::string& text, Window& window) {
	Screen& screen = context.create(text, forgeDirectory(context));
	screen.setExecutor(&forgeExecutor());
	screen.services().allowChosenRoot("workspace");
	screen.services().addRoot("workspace", AFFRESCOUI_FORGE_FOLDER);
	screen.build(window); // its onLoad indexes the folder
	static_cast<void>(window.panelAt({}));
	return {screen, window};
}

/// @brief Runs one of Forge's actions with an event, then its updates and layout, as a routed event ends.
/// @param loaded Forge.
/// @param action The action.
/// @param event The event.
void runAndLayOut(const LoadedScreen& loaded, const char* action, const json& event) {
	loaded.screen.runAction(action, loaded.window.root(), event.dump());
	loaded.window.routeKey({Key::Unknown, 0, Action::Release, {}}); // the queued updates
	static_cast<void>(loaded.window.panelAt({}));                   // the layout
}

/// @brief Opens a file of the folder in a tab, and waits until Forge has read it.
/// @param loaded Forge.
/// @param path The file, relative to the folder.
/// @throws std::runtime_error If it isn't read within a minute.
void openFile(const LoadedScreen& loaded, const std::string& path) {
	runAndLayOut(loaded, "openExplorerItem", {{"id", path}});
	forgeExecutor().pumpUntil(
	    [&loaded, &path] {
		    const json state = json::parse(loaded.screen.stateJson());
		    return std::ranges::any_of(state.at("docs"),
			                           [&path](const json& doc) { return doc.value("id", "") == path; });
	    },
	    "forge: the file " + path);
	static_cast<void>(loaded.window.panelAt({})); // the layout, with the text
}

/// @brief "forge/open": parsing Forge with its includes, building it, indexing the folder and the first layout.
/// @param context The context.
/// @param text The document.
/// @return The workload.
Workload openWorkload(Context& context, const std::shared_ptr<const std::string>& text) {
	const auto document = std::make_shared<const json>(json::parse(*text));
	const auto window = std::make_shared<Window*>(nullptr);
	Workload workload{.name = "forge/open", .runs = 5}; // in us, as the others: the locks compare them
	workload.prepare = [&context, document, window] { *window = &context.openWindow(*document); };
	workload.step = [&context, text, window] { static_cast<void>(buildForge(context, *text, **window)); };
	return workload;
}

/// @brief "forge/find", "forge/secondary-side-bar" and "forge/minimap", on a Forge with a file open.
/// @param loaded Forge.
/// @param run The runs so far, shared by the workloads.
/// @return The workloads.
std::vector<Workload> workbenchWorkloads(const LoadedScreen& loaded, const std::shared_ptr<int>& run) {
	std::vector<Workload> workloads;
	Workload find{.name = "forge/find", .runs = 15}; // the find bar, counting the matches of a new text
	find.prepare = [loaded] { runAndLayOut(loaded, "openFind", json::object()); };
	find.step = [loaded, run] { runAndLayOut(loaded, "benchFind", {{"text", ++*run % 2 ? "include" : "main"}}); };
	workloads.push_back(std::move(find));

	Workload side{.name = "forge/secondary-side-bar", .runs = 15}; // shown or hidden, with its outline
	side.prepare = [loaded] { runAndLayOut(loaded, "closeFind", json::object()); };
	side.step = [loaded] { runAndLayOut(loaded, "toggleSecondarySidebar", json::object()); };
	workloads.push_back(std::move(side));

	Workload minimap{.name = "forge/minimap", .runs = 15}; // the editor's minimap, shown or hidden
	minimap.step = [loaded] { runAndLayOut(loaded, "toggleMinimap", json::object()); };
	workloads.push_back(std::move(minimap));
	return workloads;
}

/// @brief "forge/type", "forge/switch-tab", "forge/search", "forge/palette" and workbenchWorkloads(), on one Forge with both of the folder's
/// files open.
/// @param context The context.
/// @param text The document.
/// @return The workloads.
std::vector<Workload> sessionWorkloads(Context& context, const std::string& text) {
	const LoadedScreen loaded = buildForge(context, text, context.openWindow(json::parse(text)));
	openFile(loaded, "README.md");
	openFile(loaded, "src/main.cpp");
	const auto run = std::make_shared<int>(0);
	std::vector<Workload> workloads;

	Workload type{.name = "forge/type", .runs = 15}; // one more character in the file shown
	type.step = [loaded, run] {
		runAndLayOut(loaded, "benchType", {{"text", std::string(1, static_cast<char>('a' + (++*run % 26)))}});
	};
	workloads.push_back(std::move(type));

	Workload switchTab{.name = "forge/switch-tab", .runs = 15}; // between the two open files
	switchTab.step = [loaded, run] {
		runAndLayOut(loaded, "openExplorerItem", {{"id", ++*run % 2 ? "README.md" : "src/main.cpp"}});
	};
	workloads.push_back(std::move(switchTab));

	Workload search{.name = "forge/search", .runs = 15}; // quick open, filtered by a file name
	search.prepare = [loaded] { runAndLayOut(loaded, "benchClose", json::object()); };
	search.step = [loaded, run] { runAndLayOut(loaded, "benchQuery", {{"query", ++*run % 2 ? "main" : "read"}}); };
	workloads.push_back(std::move(search));

	Workload palette{.name = "forge/palette", .runs = 15}; // the command palette, opened then filtered
	palette.prepare = [loaded] { runAndLayOut(loaded, "benchClose", json::object()); };
	palette.step = [loaded] {
		runAndLayOut(loaded, "showCommands", json::object());
		runAndLayOut(loaded, "benchQuery", {{"query", ">toggle"}});
	};
	workloads.push_back(std::move(palette));
	std::ranges::move(workbenchWorkloads(loaded, run), std::back_inserter(workloads));
	return workloads;
}

} // namespace

std::vector<Workload> forgeWorkloads(Context& context) {
	const auto text = std::make_shared<const std::string>(forgeDocument(context));
	std::vector<Workload> workloads;
	workloads.push_back(openWorkload(context, text));
	std::ranges::move(sessionWorkloads(context, *text), std::back_inserter(workloads));
	return workloads;
}

} // namespace affresco::bench
