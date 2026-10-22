/// @file
/// @brief The "editor" workloads: a key typed then the frame drawn in files of 1,000 and 100,000 lines, and a frame
/// scrolled then drawn in files of 10,000 and 1,000,000 lines, each frame drawn without being presented (hidden
/// windows: a display is needed, as for the "frame" workloads).
///
/// Each step times its operations one by one. With AFFRESCOUI_PERF_BUDGETS set, a step whose 99th percentile is over
/// its budget fails the run: typing in 100,000 lines within 8 ms key to frame, and scrolling 1,000,000 lines within a
/// 120 Hz frame (8.3 ms). The budgets are wall clock, so they aren't checked by default (callgrind, busy runners).

#include "Bench.hpp"
#include "Headless.hpp"

#include <affresco/render/TextEditor.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco::bench {
namespace {

constexpr int samplesPerStep = 200;            ///< Operations one step times.
constexpr double typingBudget = 8.0;           ///< Key to frame, 99th percentile, in milliseconds.
constexpr double frameBudget = 1000.0 / 120.0; ///< A scrolled frame, 99th percentile, in milliseconds.

/// @brief A hidden window holding one focused editor.
struct EditorView {
	Window* window = nullptr;     ///< 1200 by 900, the Application's.
	TextEditor* editor = nullptr; ///< The editor, filling the window.

	/// @brief Opens the window and gives the editor a file of code-like lines.
	/// @param context Where the fonts are, and the windowing system.
	/// @param lines How many lines.
	EditorView(Context& context, std::size_t lines)
	    : window(&context.openWindow({{"window", {{"width", 1200}, {"height", 900}}}})) {
		editor = &window->root().add<TextEditor>();
		editor->setFont(Font::load(context.examples() / "shared" / "fonts" / "Roboto-Regular.ttf", 14.0f));
		editor->setSize(px(1200), px(900));
		std::string text;
		text.reserve(lines * 40);
		for(std::size_t i = 0; i < lines; ++i)
			text += i % 7 == 0 ? "function f" + std::to_string(i) + "(a, b) {\n" : "    total = total + a[b] * 3;\n";
		editor->setText(text);
		editor->setIndentGuides(true);
		editor->setBracketMatching(true);
		detail::renderWithoutPresenting(*window);
		window->setFocus(editor);
	}
};

/// @brief The 99th percentile of some durations, in milliseconds.
/// @param samples The durations; reordered.
double percentile99(std::vector<double>& samples) {
	const auto at = samples.begin() + static_cast<std::ptrdiff_t>(samples.size() * 99 / 100);
	std::ranges::nth_element(samples, at);
	return *at;
}

/// @brief Times operations one by one and checks their 99th percentile against a budget when budgets are on.
/// @param name The workload, for the error.
/// @param budget The budget in milliseconds.
/// @param operation One operation.
/// @param kept Receives the durations, for the workload's "p99" over all its runs.
void timed(const std::string& name, double budget, const std::function<void()>& operation, std::vector<double>& kept) {
	std::vector<double> samples(samplesPerStep);
	for(double& sample : samples) {
		const auto start = std::chrono::steady_clock::now();
		operation();
		sample = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	}
	kept.insert(kept.end(), samples.begin(), samples.end());
	if(std::getenv("AFFRESCOUI_PERF_BUDGETS") == nullptr) return;
	if(const double p99 = percentile99(samples); p99 > budget)
		throw std::runtime_error(name + ": 99th percentile " + std::to_string(p99) + " ms, over its budget of " +
		                         std::to_string(budget) + " ms");
}

/// @brief Adds "p99", the 99th percentile of every measured operation in the workload's unit, to its result.
/// @param workload The workload; its steps keep their durations in samples.
/// @param samples Where they are kept; the warm-up's first samplesPerStep are left out.
void reportPercentile(Workload& workload, const std::shared_ptr<std::vector<double>>& samples) {
	workload.report = [samples](json& result) {
		std::vector<double> measured(samples->begin() + std::min<std::ptrdiff_t>(samplesPerStep, std::ssize(*samples)),
		                             samples->end());
		if(!measured.empty()) result["p99"] = std::round(percentile99(measured) * 1e3 * 1e3) / 1e3; // ms to us
	};
}

/// @brief A key typed then drawn, in the middle of a file, a word at a time so the text keeps its shape.
/// @param context Where the fonts are.
/// @param lines The file's lines.
/// @param name The workload's name.
Workload typing(Context& context, std::size_t lines, std::string name) {
	const auto view = std::make_shared<EditorView>(context, lines);
	// The caret goes to the middle line's start through find, as a user would get there
	const std::size_t middle = lines / 2 / 7 * 7;
	if(view->editor->setFind("function f" + std::to_string(middle) + "(", {}) || !view->editor->findNext())
		throw std::runtime_error(name + ": no line to type in");
	view->window->routeKey({Key::Right, 0, Action::Press, Mods{0}});
	const auto typed = std::make_shared<int>(0);
	const auto samples = std::make_shared<std::vector<double>>();
	Workload workload{
	    .name = name, .unit = "us", .runs = 5, .operations = samplesPerStep, .step = [view, typed, name, samples] {
		    timed(
		        name, typingBudget,
		        [&] {
			        // Typed then erased, so every run sees the same file
			        const bool erase = ++*typed % 2 == 0;
			        if(erase) view->window->routeKey({Key::Backspace, 0, Action::Press, Mods{0}});
			        else view->window->routeChar({U'x'});
			        detail::renderWithoutPresenting(*view->window);
		        },
		        *samples);
	    }};
	reportPercentile(workload, samples);
	return workload;
}

/// @brief A frame scrolled by a prime number of pixels (a new place every time) then drawn.
/// @param context Where the fonts are.
/// @param lines The file's lines.
/// @param name The workload's name.
Workload scrolling(Context& context, std::size_t lines, std::string name) {
	const auto view = std::make_shared<EditorView>(context, lines);
	const auto offset = std::make_shared<float>(0.0f);
	const float height = static_cast<float>(lines) * 16.0f; // under the text's height: setScroll() clamps
	const auto samples = std::make_shared<std::vector<double>>();
	Workload workload{.name = name,
	                  .unit = "us",
	                  .runs = 5,
	                  .operations = samplesPerStep,
	                  .step = [view, offset, height, name, samples] {
		                  timed(
		                      name, frameBudget,
		                      [&] {
			                      *offset = std::fmod(*offset + 7919.0f, height);
			                      view->editor->setScroll({0.0f, *offset});
			                      detail::renderWithoutPresenting(*view->window);
		                      },
		                      *samples);
	                  }};
	reportPercentile(workload, samples);
	return workload;
}

} // namespace

std::vector<Workload> editorWorkloads(Context& context) {
	std::vector<Workload> workloads;
	workloads.push_back(typing(context, 1'000, "editor/type-1k"));
	workloads.push_back(typing(context, 100'000, "editor/type-100k"));
	workloads.push_back(scrolling(context, 10'000, "editor/scroll-10k"));
	workloads.push_back(scrolling(context, 1'000'000, "editor/scroll-1m"));
	return workloads;
}

} // namespace affresco::bench
