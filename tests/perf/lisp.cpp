/// @file
/// @brief The "lisp" workloads (RFC 0005, "Performance"): what a program's function costs in an event and in a binding,
/// each beside the same thing written as steps or as an expression, a reload when a definition changes, making an
/// interpreter, building the largest example's program, with and without recording where its nodes come from, and
/// finding where each of its nodes is written; and opening Orrery with a feature written as a program installed.

#include "Bench.hpp"
#include "Headless.hpp"

#include <affresco/lisp/Document.hpp>
#include <affresco/lisp/Interpreter.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace affresco::bench {
namespace {

/// How many labels the bindings' screens have.
constexpr int labels = 100;

/// @brief A program: what events and bindings run, and a screen of labels bound to a function.
/// @return Its text.
std::string programWithBindings() {
	std::string text = R"lisp(
(defn shown "What a label shows." [n] (str "n=" n))
(defn bump "One more." [] (set "state.n" (inc state.n)))
(defscreen main :window {:width 400 :height 300} :state {:n 0}
  :actions {:bump-steps [{:set "state.n" :to "{{state.n + 1}}"}]}
  (column#list
)lisp";
	for(int at = 0; at < labels; ++at)
		text += "    (label :id \"l" + std::to_string(at) + "\" :text ~(shown state.n))\n";
	return text + "))\n";
}

/// @brief The same screen with its labels bound to an expression.
/// @return The document.
json documentWithExpressions() {
	json children = json::array();
	for(int at = 0; at < labels; ++at)
		children.push_back({{"type", "label"}, {"id", "l" + std::to_string(at)}, {"text", "{{'n=' + state.n}}"}});
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"window", {{"width", 400}, {"height", 300}}},
	        {"state", {{"n", 0}}},
	        {"actions", {{"bump-steps", json::array({{{"set", "state.n"}, {"to", "{{state.n + 1}}"}}})}}},
	        {"root", {{"type", "column"}, {"id", "list"}, {"children", std::move(children)}}}};
}

/// @brief A screen of labels bound to an `if-let`, which a binding writes as an expression, or to the same as a call
/// of the program's function.
/// @param function Whether the labels call the function.
/// @return Its text.
std::string programWithLet(bool function) {
	std::string text = R"lisp(
(defn mode-text "What a label shows." [mode] (if-let [m mode] (str/upper-case m) "none"))
(defscreen main :window {:width 400 :height 300} :state {:mode nil}
  :actions {:flip [{:set "state.mode" :to "{{state.mode ? null : 'edit'}}"}]}
  (column#list
)lisp";
	const std::string bound =
	    function ? "~(mode-text state.mode)" : "~(if-let [m state.mode] (str/upper-case m) \"none\")";
	for(int at = 0; at < labels; ++at) text += "    (label :id \"l" + std::to_string(at) + "\" :text " + bound + ")\n";
	return text + "))\n";
}

/// @brief A program of many definitions, for a reload.
/// @return Its text.
std::string programOfMany() {
	std::string text;
	for(int at = 0; at < 200; ++at) {
		const std::string number = std::to_string(at);
		text.append("(defn f").append(number).append(" \"One of many.\" [x] (let [y (inc x)] (if (> y ").append(number);
		text.append(") (str y) nil)))\n");
	}
	return text + "(defscreen main (panel#root))\n";
}

/// How many interpreters "lisp/start" makes in a step.
constexpr int starts = 1000;

/// @brief "lisp/start": making an interpreter, the language's own functions with it, as every build and every host
/// that runs a program does first.
/// @param workloads Gets it.
void addStartWorkload(std::vector<Workload>& workloads) {
	Workload start{.name = "lisp/start", .runs = 15, .operations = starts};
	start.step = [] {
		for(int at = 0; at < starts; ++at) {
			const lisp::Interpreter interpreter;
			static_cast<void>(interpreter);
		}
	};
	workloads.push_back(std::move(start));
}

/// The examples written as programs, each opened by "lisp/open-<app>" (and "lisp/open-<app>-json" beside its JSON
/// version, where it has one).
constexpr std::array openedApps{"arcadia", "calamaio", "code-editor", "flow",  "forge", "haven",  "launcher",
                                "modello", "orrery",   "sign-in",     "sinopia", "streak", "tempera", "tempo"};

/// @brief "lisp/open-<app>" and "lisp/open-<app>-json": opening each example written as a program and as JSON, as a
/// scenario does each time (read, built, judged, its program loaded, the screen built into a window): what an app
/// written as a program costs beside its JSON version, once the process has opened it before. An app with no json/
/// folder (Calamaio) has the first only.
/// @param context The context.
/// @param workloads Gets them.
void addOpenWorkloads(Context& context, std::vector<Workload>& workloads) {
	for(const char* app : openedApps)
		for(const char* notation : {"lisp", "json"}) {
			if(!std::filesystem::is_directory(context.examples() / app / notation)) continue;
			// Shared, so copying a workload's function copies no path, and can't throw
			const auto folder = std::make_shared<const std::filesystem::path>(context.examples() / app / notation);
			const auto window = std::make_shared<Window*>(nullptr);
			Workload open{.name = std::string("lisp/open-") + app + (std::string(notation) == "json" ? "-json" : ""),
			              .unit = "ms",
			              .runs = 5};
			open.prepare = [&context, folder, window] {
				*window = &context.openWindow(
				    json::parse(Screen::fromApp(*folder, {}, Screen::History::InMemory).documentJson()));
			};
			open.step = [folder, window] {
				Screen screen = Screen::fromApp(*folder, {}, Screen::History::InMemory);
				screen.build(**window);
			};
			workloads.push_back(std::move(open));
		}
}

/// @brief "lisp/open-orrery-sorter" and "lisp/open-orrery-sorter-parts": opening the Lisp Orrery with Sinopia's slide
/// sorter installed, as the sorter written as a program (its record holds `provides.program`, built when the screen
/// is) and as the record of its JSON parts the app held before (master's), the rest of the document the same.
/// @param context The context.
/// @param workloads Gets them.
void addFeatureWorkloads(Context& context, std::vector<Workload>& workloads) {
	const std::filesystem::path folder = context.examples() / "orrery" / "lisp";
	const json program = Screen::fromApp(folder, {}, Screen::History::InMemory).versions().sourceDocument();
	if(!program["features"]["slideSorter"]["provides"].contains("program"))
		throw std::runtime_error("lisp: Orrery's slide sorter is not written as a program");
	json parts = program;
	const std::filesystem::path manifest = context.examples() / "sinopia" / "json" / "features" / "slide-sorter";
	parts["features"]["slideSorter"]["provides"] = json::parse(readFile(manifest / "feature.json"))["provides"];
	for(const auto& [name, document] : {std::pair{"", program}, std::pair{"-parts", parts}}) {
		// Shared, so copying a workload's function copies no text, and can't throw
		const auto source = std::make_shared<const json>(document);
		const auto text = std::make_shared<const std::string>(document.dump());
		const auto base = std::make_shared<const std::filesystem::path>(folder);
		const auto window = std::make_shared<Window*>(nullptr);
		Workload open{.name = std::string("lisp/open-orrery-sorter") + name, .unit = "ms", .runs = 5};
		open.prepare = [&context, source, window] { *window = &context.openWindow(*source); };
		open.step = [text, base, window] {
			Screen screen = Screen::fromString(*text, *base);
			screen.build(**window);
		};
		workloads.push_back(std::move(open));
	}
}

/// @brief The example whose program is the largest, as the host loads it: its entry screen, with the program files it
/// includes.
/// @param context The context.
/// @return Its folder (with "lisp" and "json" inside), or an empty path if no example is written as a program.
std::filesystem::path largestApp(const Context& context) {
	std::filesystem::path largest;
	std::size_t size = 0;
	for(const auto& folder : std::filesystem::directory_iterator(context.examples())) {
		if(!std::filesystem::is_regular_file(folder.path() / "lisp" / "app.lisp")) continue;
		Screen screen = Screen::fromApp(folder.path() / "lisp", {}, Screen::History::InMemory);
		const json& document = screen.versions().sourceDocument();
		if(!document.contains("program") || document["program"].dump().size() <= size) continue;
		size = document["program"].dump().size();
		largest = folder.path();
	}
	return largest;
}

/// @brief The `program` section of the largest example written as a program (largestApp()).
/// @param context The context.
/// @return The section, or an empty object.
json largestProgram(const Context& context) {
	const std::filesystem::path app = largestApp(context);
	if(app.empty()) return json::object();
	Screen screen = Screen::fromApp(app / "lisp", {}, Screen::History::InMemory);
	return screen.versions().sourceDocument()["program"];
}

/// @brief The ids of the panels in a window whose node is written in the document (Screen::source() finds it).
/// @param loaded The screen.
/// @param panel Where to look.
/// @param ids Gets them, each once.
void writtenIds(const LoadedScreen& loaded, const Panel& panel, std::vector<std::string>& ids) {
	const std::string& id = panel.id();
	if(!id.empty() && std::ranges::find(ids, id) == ids.end()) {
		bool written = true;
		try {
			static_cast<void>(loaded.screen.source("#" + id));
		} catch(const DocumentError&) {
			written = false; // made by a binding, not written
		}
		if(written) ids.push_back(id);
	}
	for(const auto& child : panel.children()) writtenIds(loaded, *child, ids);
}

/// @brief "lisp/source-largest" and "lisp/source-largest-json": where each node of the largest example the window
/// shows is written (Screen::source()), what a pick reads to describe it, once the program's origins are kept (the
/// first pick after a change builds them: "lisp/origins-largest"). One operation is one node.
/// @param context The context.
/// @param workloads Gets them.
void addSourceWorkloads(Context& context, std::vector<Workload>& workloads) {
	const std::filesystem::path app = largestApp(context);
	if(app.empty()) throw std::runtime_error("lisp: no example is written as a program");
	for(const char* notation : {"lisp", "json"}) {
		Screen opened = Screen::fromApp(app / notation, {}, Screen::History::InMemory);
		const LoadedScreen loaded = context.load(opened.versions().sourceDocument(), app / notation);
		const auto ids = std::make_shared<std::vector<std::string>>();
		writtenIds(loaded, loaded.window.root(), *ids);
		if(ids->empty()) throw std::runtime_error("lisp: the largest example shows no written node");
		Workload source{.name = std::string("lisp/source-largest") + (std::string(notation) == "json" ? "-json" : ""),
		                .runs = 15,
		                .operations = static_cast<int>(ids->size())};
		source.step = [loaded, ids] {
			for(const std::string& id : *ids) static_cast<void>(loaded.screen.source("#" + id));
		};
		workloads.push_back(std::move(source));
	}
}

/// @brief "lisp/build-largest" and "lisp/origins-largest": building the largest example's program (builtProgram()),
/// and finding where its nodes come from (nodeOrigins()), its second, recording build, which should cost about one
/// build. Each step changes a comment in one definition, so that neither finds the result it kept.
/// @param context The context.
/// @param workloads Gets them.
void addOriginWorkloads(const Context& context, std::vector<Workload>& workloads) {
	const auto section = std::make_shared<json>(largestProgram(context));
	if(section->empty()) throw std::runtime_error("lisp: no example is written as a program");
	const auto turn = std::make_shared<int>(0);
	const auto changed = [section, turn] {
		json copy = *section;
		json& entries = copy.begin().value();
		json& text = entries.back();
		text = text.get<std::string>() + "\n; " + std::to_string(++*turn);
		return copy;
	};
	Workload build{.name = "lisp/build-largest", .unit = "ms", .runs = 15};
	build.step = [changed] { static_cast<void>(lisp::builtProgram(changed())); };
	workloads.push_back(std::move(build));
	Workload origins{.name = "lisp/origins-largest", .unit = "ms", .runs = 15};
	origins.step = [changed] {
		if(lisp::nodeOrigins(changed())->empty()) throw std::runtime_error("lisp: the largest example has no origins");
	};
	workloads.push_back(std::move(origins));
}

} // namespace

std::vector<Workload> lispWorkloads(Context& context) {
	std::vector<Workload> workloads;
	addStartWorkload(workloads);
	addOpenWorkloads(context, workloads);
	addFeatureWorkloads(context, workloads);
	addOriginWorkloads(context, workloads);
	addSourceWorkloads(context, workloads);
	const json bound = lisp::sourceDocument(programWithBindings(), "bench.lisp");
	const LoadedScreen lisp = context.load(bound, {});
	const LoadedScreen plain = context.load(documentWithExpressions(), {});
	Workload event{.name = "lisp/event", .runs = 15, .operations = 1000};
	event.step = [lisp] {
		for(int at = 0; at < 1000; ++at) lisp.screen.runAction("bump", lisp.window.root());
	};
	workloads.push_back(std::move(event));
	Workload steps{.name = "lisp/event-as-steps", .runs = 15, .operations = 1000};
	steps.step = [plain] {
		for(int at = 0; at < 1000; ++at) plain.screen.runAction("bump-steps", plain.window.root());
	};
	workloads.push_back(std::move(steps));
	Workload binding{.name = "lisp/binding", .runs = 15, .operations = labels};
	binding.step = [lisp] {
		lisp.screen.runAction("bump", lisp.window.root());
		detail::renderWithoutPresenting(lisp.window);
	};
	workloads.push_back(std::move(binding));
	Workload expression{.name = "lisp/binding-as-expression", .runs = 15, .operations = labels};
	expression.step = [plain] {
		plain.screen.runAction("bump-steps", plain.window.root());
		detail::renderWithoutPresenting(plain.window);
	};
	workloads.push_back(std::move(expression));
	for(const bool function : {false, true}) {
		const LoadedScreen let = context.load(lisp::sourceDocument(programWithLet(function), "bench.lisp"), {});
		Workload workload{
		    .name = function ? "lisp/binding-let-as-function" : "lisp/binding-let", .runs = 15, .operations = labels};
		workload.step = [let] {
			let.screen.runAction("flip", let.window.root());
			detail::renderWithoutPresenting(let.window);
		};
		workloads.push_back(std::move(workload));
	}
	const json many = lisp::sourceDocument(programOfMany(), "bench.lisp");
	const LoadedScreen reloading = context.load(many, {});
	const auto turn = std::make_shared<int>(0);
	Workload reload{.name = "lisp/reload-200", .runs = 15};
	reload.step = [reloading, turn] {
		const std::string text = "(defn f0 \"One of many.\" [x] " + std::to_string(++*turn) + ")";
		const json revision = {{"author", "user"}, {"message", "m"}, {"set", {{"/program/bench.lisp/f0", text}}}};
		if(!reloading.screen.applyRevision(revision.dump())) throw std::runtime_error("lisp: the revision was refused");
	};
	workloads.push_back(std::move(reload));
	return workloads;
}

} // namespace affresco::bench
