/// @file
/// @brief The "lisp" workloads (RFC 0005, "Performance"): what a program's function costs in an event and in a binding,
/// each beside the same thing written as steps or as an expression, and a reload when a definition changes.

#include "Bench.hpp"
#include "Headless.hpp"

#include <affresco/lisp/Document.hpp>

#include <filesystem>
#include <memory>
#include <string>

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

/// @brief "lisp/open-<app>" and "lisp/open-<app>-json": opening each example written as a program and as JSON, as a
/// scenario does each time (read, built, judged, its program loaded, the screen built into a window): what an app
/// written as a program costs beside its JSON version, once the process has opened it before.
/// @param context The context.
/// @param workloads Gets them.
void addOpenWorkloads(Context& context, std::vector<Workload>& workloads) {
	for(const char* app : {"flow", "haven", "arcadia", "orrery", "forge", "sinopia", "code-editor", "launcher"})
		for(const char* notation : {"lisp", "json"}) {
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

} // namespace

std::vector<Workload> lispWorkloads(Context& context) {
	std::vector<Workload> workloads;
	addOpenWorkloads(context, workloads);
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
