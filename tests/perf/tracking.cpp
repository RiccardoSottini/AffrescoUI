/// @file
/// @brief The workloads that dependency tracking is judged on (tests/perf/PROFILE.md): what an update, a revision and
/// an idle second cost when a change touches little of a large screen. Each one sets its screen up on its first run, so
/// selecting one workload loads only its own screen, and each step leaves the screen as it found it, or the next run's
/// prepare puts it back, unmeasured.

#include "Bench.hpp"

#include <affresco/lisp/Document.hpp>

#include <array>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

/// @brief The card counts the Kanban workloads fill the template to, as "update/<cards>" does.
constexpr std::array<int, 3> cardCounts = {14, 100, 400};

/// @brief A screen that a workload loads on its first run, shared by its prepare, step and report.
using LazyScreen = std::shared_ptr<std::optional<LoadedScreen>>;

/// @brief Loads the screen if this is the first run.
/// @param screen Where it is kept.
/// @param load Loads it.
/// @return The screen.
const LoadedScreen& loadOnce(std::optional<LoadedScreen>& screen, const std::function<LoadedScreen()>& load) {
	if(!screen) screen.emplace(load());
	return *screen;
}

/// @brief Adds a node to a document as an overlay of its own, where a workload can read or change it. An overlay, not a
/// child of the root: a template written as a program builds its root, and a document may not write it differently.
/// @param document The document.
/// @param name The overlay's name, new to the document.
/// @param node The node.
/// @throws std::invalid_argument If the document has an overlay by that name.
void addOverlay(json& document, const std::string& name, json node) {
	json& overlays = document["overlays"];
	if(overlays.contains(name)) throw std::invalid_argument("the document already has an overlay " + name);
	overlays[name] = std::move(node);
}

/// @brief Runs an action with an event, then the updates it queued, and lays out if asked.
/// @param loaded The screen.
/// @param action The action.
/// @param event The event.
/// @param layout Whether the window is laid out after.
void runAndUpdate(const LoadedScreen& loaded, const char* action, const json& event, bool layout = true) {
	loaded.screen.runAction(action, loaded.window.root(), event.dump());
	runQueuedUpdates(loaded.window);
	if(layout) layOut(loaded.window);
}

/// @brief A workload whose step runs one action on a screen loaded on its first run, with an event that changes on
/// every run, so that the state really changes. Its result tells the screen's panels ("panels").
/// @param name The workload's name.
/// @param load Loads the screen.
/// @param action The action.
/// @param event The event of a run, from its number.
/// @param layout Whether the window is laid out after the update.
/// @return The workload.
Workload actionWorkload(std::string name, std::function<LoadedScreen()> load, const char* action,
                        std::function<json(int)> event, bool layout = true) {
	const LazyScreen screen = std::make_shared<std::optional<LoadedScreen>>();
	const auto run = std::make_shared<int>(0);
	Workload workload{.name = std::move(name), .runs = 15};
	workload.prepare = [screen, load = std::move(load)] { static_cast<void>(loadOnce(*screen, load)); };
	workload.step = [screen, run, action, event = std::move(event), layout] {
		runAndUpdate(**screen, action, event(++*run), layout);
	};
	workload.report = [screen](json& result) {
		if(*screen) result["panels"] = panelCount((*screen)->window.root());
	};
	return workload;
}

/* A state change one label reads, a record written, an animation's frame: on the Kanban template */

/// @brief The Kanban template filled to a number of cards, with a label that reads a state key nothing else reads, and
/// the actions the workloads below run: "benchRead" sets that key, "benchRecord" writes one card's title through the
/// records' own "update" step, and "benchSpin" sets the angle of a panel that turns (as an animate step does each
/// frame).
/// @param context The context.
/// @param cards The cards.
/// @return The screen.
LoadedScreen loadProbedKanban(Context& context, int cards) {
	json document = kanbanWithCards(context.templateFolder("kanban"), cards);
	std::string card;
	for(const json& record : document["models"]["issues"]["seed"])
		if(record["board"] == "launch") {
			card = record["id"].get<std::string>(); // one the board shows
			break;
		}
	document["state"]["benchRead"] = 0;
	document["state"]["benchSpin"] = 0;
	document["actions"]["benchRead"] = json::parse(R"([{"set": "state.benchRead", "to": "{{event.v}}"}])");
	document["actions"]["benchSpin"] = json::parse(R"([{"set": "state.benchSpin", "to": "{{event.v}}"}])");
	document["actions"]["benchRecord"] =
	    json::array({{{"update", "issues"}, {"id", card}, {"fields", {{"title", "{{event.title}}"}}}}});
	addOverlay(document, "benchRead", {{"type", "label"}, {"id", "benchReadLabel"}, {"text", "{{state.benchRead}}"}});
	addOverlay(
	    document, "benchSpin",
	    {{"type", "panel"}, {"id", "benchSpinner"}, {"width", 16}, {"height", 16}, {"rotate", "{{state.benchSpin}}"}});
	return context.load(document, context.templateFolder("kanban"));
}

/// @brief "update/read/<cards>", "update/record/<cards>" and "update/animation/400".
/// @param context The context.
/// @param workloads Gets them.
void addKanbanWorkloads(Context& context, std::vector<Workload>& workloads) {
	for(const int cards : cardCounts) {
		const auto load = [&context, cards] { return loadProbedKanban(context, cards); };
		workloads.push_back(actionWorkload(std::format("update/read/{}", cards), load, "benchRead",
		                                   [](int run) { return json{{"v", run}}; }));
		workloads.push_back(actionWorkload(std::format("update/record/{}", cards), load, "benchRecord", [](int run) {
			return json{{"title", run % 2 ? "A title that changed" : "Another title"}};
		}));
	}
	workloads.push_back(actionWorkload(
	    "update/animation/400", [&context] { return loadProbedKanban(context, cardCounts.back()); }, "benchSpin",
	    [](int run) { return json{{"v", (run * 6) % 360}}; })); // a 60-frame turn
}

/* A screen of 50,000 panels shaped like an app */

/// @brief The sections, groups and rows of "update/app-50000": 100 x 10 x 25 rows of 2 panels.
constexpr int appSections = 100, appGroups = 10, appRows = 25;
/// @brief The state keys its labels read, spread over the sections, each read by one label.
constexpr int appPlaces = 400;

/// @brief One section of the app-shaped screen: a label of its computed value, its share of the state places' labels,
/// and its groups, each a use of the "group" component with its title and rows.
/// @param section Its number.
/// @return The node.
json appSection(int section) {
	json children = json::array();
	children.push_back({{"type", "label"}, {"text", std::format("{{{{computed.total{}}}}}", section)}});
	constexpr int perSection = appPlaces / appSections;
	for(int place = section * perSection; place < (section + 1) * perSection; ++place)
		children.push_back({{"type", "label"}, {"text", std::format("{{{{state.place{}}}}}", place)}});
	for(int group = 0; group < appGroups; ++group)
		children.push_back({{"use", "group"},
		                    {"with",
		                     {{"title", std::format("Group {}.{}", section, group)},
		                      {"rows", std::format("{{{{state.sections[{}].groups[{}].rows}}}}", section, group)}}}});
	return {{"type", "column"}, {"id", std::format("section{}", section)}, {"children", std::move(children)}};
}

/// @brief "update/app-50000"'s screen: 100 sections, each 10 groups of 25 keyed rows of 2 panels; 400 state places
/// read by labels spread over the sections; one computed value per section; each group a component with two
/// parameters. Its "touch" action changes one row's value.
/// @return The document.
json appShapedScreen() {
	json document = json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"window": { "width": 1280, "height": 800 },
		"actions": { "touch": [{ "set": "state.sections[event.s].groups[event.g].rows[event.r].v", "to": "{{event.v}}" }] },
		"components": { "group": { "params": { "title": "", "rows": [] }, "node": {
			"type": "column", "children": [
				{ "type": "label", "text": "{{title}}" },
				{ "type": "row", "repeat": { "for": "row", "in": "rows", "key": "{{row.id}}" }, "children": [
					{ "type": "label", "text": "{{row.v}}" }
				] }
			] } } }
	})json");
	json sections = json::array(), root = json::array();
	for(int section = 0; section < appSections; ++section) {
		json groups = json::array();
		for(int group = 0; group < appGroups; ++group) {
			json rows = json::array();
			for(int row = 0; row < appRows; ++row) rows.push_back({{"id", row}, {"v", row}});
			groups.push_back({{"id", group}, {"rows", std::move(rows)}});
		}
		sections.push_back({{"id", section}, {"groups", std::move(groups)}});
		document["computed"][std::format("total{}", section)] =
		    std::format("{{{{length(state.sections[{0}].groups) + state.place{1}}}}}", section, section * 4);
		root.push_back(appSection(section));
	}
	for(int place = 0; place < appPlaces; ++place) document["state"][std::format("place{}", place)] = place;
	document["state"]["sections"] = std::move(sections);
	json column = {{"type", "column"}, {"children", std::move(root)}};
	document["root"] = {{"type", "panel"}, {"children", json::array({std::move(column)})}};
	return document;
}

/// @brief "update/app-50000" and "update/app-50000-without-layout": one row's value changed on appShapedScreen().
/// @param context The context.
/// @param workloads Gets them.
void addAppWorkloads(Context& context, std::vector<Workload>& workloads) {
	for(const bool layout : {true, false}) {
		const auto load = [&context] {
			const LoadedScreen loaded = context.load(appShapedScreen(), context.templateFolder("kanban"));
			layOut(loaded.window);
			return loaded;
		};
		const auto event = [](int run) { // a different row each run, spread over the sections
			const int row = (run * 7919) % (appSections * appGroups * appRows);
			return json{{"s", row / (appGroups * appRows)},
			            {"g", row / appRows % appGroups},
			            {"r", row % appRows},
			            {"v", -run}};
		};
		workloads.push_back(actionWorkload(layout ? "update/app-50000" : "update/app-50000-without-layout", load,
		                                   "touch", event, layout));
	}
}

/* A list of 25,000 rows edited: a push, an insert and a removal at the front, a reverse */

/// @brief One edit of a list and the steps that take it back.
struct ListEdit {
	const char* name = "";     ///< The workload's name, after "update/list-".
	const char* edit = "";     ///< The steps that edit the list (a JSON list).
	const char* takeBack = ""; ///< The steps that take the edit back (a JSON list).
};

/// @brief The edits, each on fiftyThousandPanels()'s 25,000 items.
constexpr std::array<ListEdit, 4> listEdits = {{
    {"append-25000", R"([{"push": "state.items", "value": {"id": -1, "v": -1}}])",
	 R"([{"remove": "state.items", "at": "{{length(state.items) - 1}}"}])"},
    {"insert-front-25000", R"([{"set": "state.items", "to": "{{concat([{id: -1, v: -1}], state.items)}}"}])",
	 R"([{"remove": "state.items", "at": 0}])"},
    {"remove-front-25000",
	 R"([{"set": "state.removed", "to": "{{state.items[0]}}"}, {"remove": "state.items", "at": 0}])",
	 R"([{"set": "state.items", "to": "{{concat([state.removed], state.items)}}"}])"},
    {"reverse-25000", R"([{"set": "state.items", "to": "{{reverse(state.items)}}"}])",
	 R"([{"set": "state.items", "to": "{{reverse(state.items)}}"}])"},
}};

/// @brief The screen the list workloads share, and the edit applied and not taken back yet, if any.
struct ListScreen {
	std::optional<LoadedScreen> loaded{}; ///< Loaded on the first run of any of them.
	std::string pending{};                ///< The action that takes the last edit back; empty if none is pending.
};

/// @brief Takes back the edit applied last, if any, and lays out, unmeasured.
/// @param screen The screen.
void takeBackPending(ListScreen& screen) {
	if(screen.pending.empty()) return;
	runAndUpdate(*screen.loaded, screen.pending.c_str(), json::object());
	screen.pending.clear();
}

/// @brief "update/list-<edit>" and "update/list-<edit>-without-layout" for each edit of listEdits, on one shared
/// screen: each run's prepare takes the last edit back, and so does each workload's report, after its runs.
/// @param context The context.
/// @param workloads Gets them.
void addListWorkloads(Context& context, std::vector<Workload>& workloads) {
	const auto screen = std::make_shared<ListScreen>();
	const auto load = [&context] {
		json document = fiftyThousandPanels();
		document["state"]["removed"] = nullptr;
		for(const ListEdit& edit : listEdits) {
			document["actions"][std::string("edit-") + edit.name] = json::parse(edit.edit);
			document["actions"][std::string("takeBack-") + edit.name] = json::parse(edit.takeBack);
		}
		const LoadedScreen loaded = context.load(document, context.templateFolder("kanban"));
		layOut(loaded.window);
		return loaded;
	};
	for(const ListEdit& edit : listEdits)
		for(const bool layout : {true, false}) {
			// 5 runs: an edit that builds the rows again takes seconds (PROFILE.md, H6)
			Workload workload{.name = std::string("update/list-") + edit.name + (layout ? "" : "-without-layout"),
			                  .runs = 5};
			workload.prepare = [screen, load] {
				static_cast<void>(loadOnce(screen->loaded, load));
				takeBackPending(*screen);
			};
			workload.step = [screen, &edit, layout] {
				runAndUpdate(*screen->loaded, (std::string("edit-") + edit.name).c_str(), json::object(), layout);
				screen->pending = std::string("takeBack-") + edit.name;
			};
			workload.report = [screen](json& result) {
				takeBackPending(*screen);
				result["panels"] = panelCount(screen->loaded->window.root());
			};
			workloads.push_back(std::move(workload));
		}
}

/* Computed values, parameters, a program's rows */

/// @brief The size of the computed list and of the parameters' uses.
constexpr int itemCount = 400;

/// @brief "update/computed-unrelated/400"'s screen: a computed list of 400 items, shown and named by a watch, and a
/// state key that neither reads, which its "other" action sets.
/// @return The document.
json computedScreen() {
	json document = json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "other": 0, "seen": 0 },
		"computed": { "doubled": "{{map(state.items, merge(it, { w: it.v * 2 }))}}" },
		"watch": { "computed.doubled": "noticed" },
		"actions": {
			"noticed": [{ "set": "state.seen", "to": "{{length(event.value)}}" }],
			"other": [{ "set": "state.other", "to": "{{event.v}}" }]
		},
		"root": { "type": "panel", "children": [{ "type": "column", "children": [
			{ "type": "label", "repeat": { "for": "item", "in": "computed.doubled", "key": "{{item.id}}" }, "text": "{{item.w}}" }
		] }] }
	})json");
	for(int item = 0; item < itemCount; ++item) document["state"]["items"].push_back({{"id", item}, {"v", item}});
	return document;
}

/// @brief "update/parameter/400"'s screen: 400 uses of one component, each with a parameter read from a state key of
/// its own; its "change" action sets one key.
/// @return The document.
json parameterScreen() {
	json document = json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"components": { "cell": { "params": { "value": 0 }, "node": { "type": "label", "text": "{{value}}" } } },
		"actions": { "change": [{ "set": "state.keys[event.key]", "to": "{{event.v}}" }] },
		"root": { "type": "panel", "children": [{ "type": "column", "children": [] }] }
	})json");
	json& uses = document["root"]["children"][0]["children"];
	for(int use = 0; use < itemCount; ++use) {
		const std::string key = std::format("k{}", use);
		document["state"]["keys"][key] = use;
		uses.push_back({{"use", "cell"}, {"with", {{"value", std::format("{{{{state.keys.{}}}}}", key)}}}});
	}
	return document;
}

/// @brief "update/lisp-row/25000"'s screen: fiftyThousandPanels() written as a program, each label's text a function
/// of its row's item. The items are written in the program too: a program's state is its own.
/// @return The document, as the program's source builds it.
json lispRows() {
	std::string text = R"lisp(
(defn shown "What a row shows." [item] (str "v=" (:v item)))
(defview rows "One row an item, its label a function of it." []
  `(column ~(repeat [item state.items :key item.id]
     `(row (label :text ~(shown item))))))
(defscreen main :state {:items [)lisp";
	const json items = fiftyThousandPanels()["state"]["items"]; // held: a temporary's member would go before the loop
	for(const json& item : items) text += std::format("{{:id {} :v {}}} ", item["id"].get<int>(), item["v"].get<int>());
	text += R"lisp(]}
  :actions {:touch [{:set "state.items[event.i].v" :to "{{event.v}}"}]}
  (panel ~(rows)))
)lisp";
	return lisp::sourceDocument(text, "bench.lisp");
}

/// @brief "update/computed-unrelated/400", "update/parameter/400" and "update/lisp-row/25000".
/// @param context The context.
/// @param workloads Gets them.
void addValueWorkloads(Context& context, std::vector<Workload>& workloads) {
	const std::filesystem::path folder = context.templateFolder("kanban");
	workloads.push_back(actionWorkload(
	    "update/computed-unrelated/400", [&context, folder] { return context.load(computedScreen(), folder); }, "other",
	    [](int run) { return json{{"v", run}}; }));
	workloads.push_back(actionWorkload(
	    "update/parameter/400", [&context, folder] { return context.load(parameterScreen(), folder); }, "change",
	    [](int run) { return json{{"key", std::format("k{}", run * 37 % itemCount)}, {"v", -run}}; }));
	workloads.push_back(actionWorkload(
	    "update/lisp-row/25000", [&context] { return context.load(lispRows(), {}); }, "touch",
	    [](int run) { return json{{"i", (run * 7919) % 25'000}, {"v", -run}}; }));
}

/* Revisions */

/// @brief A workload that applies a revision, from the run's number, to the Kanban template filled to some cards, then
/// lays out. Its history grows from run to run, as "scaling/revision" lets it.
/// @param context The context.
/// @param name The workload's name.
/// @param folder The template's folder: the program, or its JSON twin, whose nodes a revision can change by id (a
/// program's document holds the tree it builds, which a revision of the program would then contradict).
/// @param cards The cards.
/// @param revision The revision of a run.
/// @return The workload.
Workload revisionWorkload(Context& context, std::string name, std::filesystem::path folder, int cards,
                          std::function<json(int)> revision) {
	const LazyScreen screen = std::make_shared<std::optional<LoadedScreen>>();
	const auto run = std::make_shared<int>(0);
	Workload workload{.name = std::move(name), .runs = 15};
	workload.prepare = [&context, screen, folder = std::move(folder), cards] {
		static_cast<void>(loadOnce(
		    *screen, [&context, &folder, cards] { return context.load(kanbanWithCards(folder, cards), folder); }));
	};
	workload.step = [screen, run, revision = std::move(revision)] {
		std::string error;
		if(!(*screen)->screen.applyRevision(revision(++*run).dump(), &error)) throw std::runtime_error(error);
		layOut((*screen)->window);
	};
	return workload;
}

} // namespace

std::vector<Workload> trackingUpdateWorkloads(Context& context) {
	std::vector<Workload> workloads;
	addKanbanWorkloads(context, workloads);
	addAppWorkloads(context, workloads);
	addListWorkloads(context, workloads);
	addValueWorkloads(context, workloads);
	return workloads;
}

std::vector<Workload> trackingRevisionWorkloads(Context& context) {
	std::vector<Workload> workloads;
	workloads.push_back(revisionWorkload(
	    context, "revision/state-only/400", context.templateFolder("kanban"), cardCounts.back(), [](int run) {
		    return json{
		        {"author", "bench"}, {"message", "Set one state key"}, {"set", {{"/state/benchRevision", run}}}};
	    }));
	for(const int cards : cardCounts) // the JSON twin: the program builds its logo, which "#logo" can't change
		workloads.push_back(revisionWorkload(context, std::format("revision/one-label/{}", cards),
		                                     context.examples() / "flow" / "json", cards, [](int run) {
			                                     return json{{"author", "bench"},
												             {"message", "Rename the logo"},
												             {"set", {{"#logo/text", "Flow " + std::to_string(run)}}}};
		                                     }));
	return workloads;
}

} // namespace affresco::bench
