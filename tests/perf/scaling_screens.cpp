/// @file
/// @brief The scaling areas that need a window (tests/perf/README.md, "Scaling"): each workload of a group at sizes
/// that double, so scaling.py can fit how its cost grows with n. The Kanban template grows by its cards; the
/// adversarial screens grow by what a document controls: nesting, siblings and state keys.

#include "Bench.hpp"
#include "Headless.hpp"

#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

/// @brief Runs the updates that state changes have queued, without drawing (as screens.cpp does).
/// @param window The window.
void runQueuedUpdates(Window& window) { window.routeKey({Key::Unknown, 0, Action::Release, {}}); }

/// @brief The first text field in a tree, depth first.
/// @param panel Where to start.
/// @return The field, or nullptr.
TextArea* firstTextArea(Panel& panel) {
	if(auto* field = dynamic_cast<TextArea*>(&panel)) return field;
	for(const auto& child : panel.children())
		if(TextArea* field = firstTextArea(*child)) return field;
	return nullptr;
}

/// @brief Lays the window out, without drawing.
/// @param window The window.
void layOut(Window& window) { static_cast<void>(window.panelAt({})); }

/// @brief The Kanban template with n cards and a "renameBoard" action, loaded into a hidden window.
/// @param context The context.
/// @param cards n.
/// @return The screen.
LoadedScreen loadKanban(Context& context, int cards) {
	json document = kanbanWithCards(context.templateFolder("kanban"), cards);
	document["actions"]["renameBoard"] = json::parse(R"([{"set": "state.board", "to": "{{event.name}}"}])");
	return context.load(document, context.templateFolder("kanban"));
}

/// @brief "scaling/update/<cards>": one state change (renaming the board), its reconciliation and layout.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> updateWorkloads(Context& context, bool large) {
	return perSize(scalingSizes(16, large), [&context](int cards) {
		const LoadedScreen loaded = loadKanban(context, cards);
		const auto renamed = std::make_shared<bool>(false);
		Workload workload{.name = scalingName("update", cards)};
		workload.step = [loaded, renamed] {
			*renamed = !*renamed;
			loaded.screen.runAction("renameBoard", loaded.window.root(),
			                        *renamed ? R"({"name": "Launch"})" : R"({"name": "Product launch"})");
			runQueuedUpdates(loaded.window);
			layOut(loaded.window);
		};
		return workload;
	});
}

/// @brief "scaling/layout/<cards>": a full layout after a resize.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> layoutWorkloads(Context& context, bool large) {
	return perSize(scalingSizes(16, large), [&context](int cards) {
		const LoadedScreen loaded = loadKanban(context, cards);
		layOut(loaded.window);
		const auto smaller = std::make_shared<bool>(false);
		Workload workload{.name = scalingName("layout", cards)};
		workload.step = [&window = loaded.window, smaller] {
			*smaller = !*smaller;
			window.setSize(*smaller ? 1180 : 1280, *smaller ? 760 : 800);
			layOut(window);
		};
		return workload;
	});
}

/// @brief "scaling/frame/<cards>": a full frame, laid out and drawn, in the main thread's CPU time; and
/// "scaling/frame/damage/<cards>": the caret of a text field shown and hidden, one small region drawn again without
/// presenting it (about O(1): the damage, not the screen, sets its cost).
/// @param context The context.
/// @param large Nightly sizes.
/// @return Two workloads per size.
std::vector<Workload> frameWorkloads(Context& context, bool large) {
	std::vector<Workload> workloads;
	for(const int cards : scalingSizes(16, large)) {
		const LoadedScreen loaded = loadKanban(context, cards);
		Workload full{.name = scalingName("frame", cards), .cpuTime = true};
		full.step = [&window = loaded.window] {
			window.invalidate();
			window.render();
		};
		workloads.push_back(std::move(full));

		const LoadedScreen busy = loadKanban(context, cards);
		TextArea* field = firstTextArea(busy.window.root());
		if(!field) throw std::logic_error("the Kanban template has no text field");
		busy.window.render();
		Workload damage{.name = scalingName("frame/damage", cards), .cpuTime = true};
		damage.step = [&window = busy.window, field] {
			window.setFocus(window.focusedPanel() == field ? nullptr : field);
			detail::renderWithoutPresenting(window);
		};
		workloads.push_back(std::move(damage));
	}
	return workloads;
}

/// @brief "scaling/load/<cards>": parsing, validating and building into a window opened beforehand.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> loadWorkloads(Context& context, bool large) {
	return perSize(scalingSizes(16, large), [&context](int cards) {
		const auto document = std::make_shared<const json>(kanbanWithCards(context.templateFolder("kanban"), cards));
		const auto text = std::make_shared<const std::string>(document->dump());
		const auto window = std::make_shared<Window*>(nullptr);
		Workload workload{.name = scalingName("load", cards), .unit = "ms", .runs = 3};
		workload.prepare = [&context, document, window] { *window = &context.openWindow(*document); };
		workload.step = [&context, text, window] {
			static_cast<void>(context.build(*text, context.templateFolder("kanban"), **window));
		};
		return workload;
	});
}

/// @brief "scaling/build/<cards>": building only, the document read beforehand.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> buildWorkloads(Context& context, bool large) {
	return perSize(scalingSizes(16, large), [&context](int cards) {
		const auto document = std::make_shared<const json>(kanbanWithCards(context.templateFolder("kanban"), cards));
		const auto text = std::make_shared<const std::string>(document->dump());
		const auto window = std::make_shared<Window*>(nullptr);
		const auto screen = std::make_shared<Screen*>(nullptr);
		Workload workload{.name = scalingName("build", cards), .unit = "ms", .runs = 3};
		workload.prepare = [&context, text, document, window, screen] {
			*window = &context.openWindow(*document);
			*screen = &context.create(*text, context.templateFolder("kanban"));
		};
		workload.step = [window, screen] { (*screen)->build(**window); };
		return workload;
	});
}

/// @brief "scaling/revision/<cards>": the same small revision (the logo's text) applied to a growing document: a
/// commit, then reconciliation and layout.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> revisionWorkloads(Context& context, bool large) {
	return perSize(scalingSizes(16, large), [&context](int cards) {
		const LoadedScreen loaded = loadKanban(context, cards);
		const auto next = std::make_shared<int>(0);
		Workload workload{.name = scalingName("revision", cards)};
		workload.step = [loaded, next] {
			const json revision = {{"author", "bench"},
			                       {"message", "Rename the logo"},
			                       {"set", {{"#logo/text", "Flow " + std::to_string((*next)++)}}}};
			std::string error;
			if(!loaded.screen.applyRevision(revision.dump(), &error)) throw std::runtime_error(error);
			layOut(loaded.window);
		};
		return workload;
	});
}

/// @brief A screen document around a root node and a state.
/// @param root The root node.
/// @param state The state.
/// @return The document.
json screenOf(json root, json state = json::object()) {
	return {{"format", "affrescoui-screen"}, {"version", 2}, {"state", std::move(state)}, {"root", std::move(root)}};
}

/// @brief Panels nested n deep around a label: two JSON levels per panel, so 96 stays under the parser's limit.
/// @param depth n.
/// @return The document.
json nestedScreen(int depth) {
	json node = {{"type", "label"}, {"id", "leaf"}, {"text", "Deep"}};
	for(int level = depth; level > 0; --level)
		node = {{"type", "panel"}, {"id", std::format("p{}", level)}, {"children", json::array({std::move(node)})}};
	return screenOf(std::move(node));
}

/// @brief A stack of n labels.
/// @param count n.
/// @return The document.
json siblingsScreen(int count) {
	json children = json::array();
	for(int i = 0; i < count; ++i) children.push_back({{"type", "label"}, {"id", std::format("l{}", i)}, {"text", "Item"}});
	return screenOf({{"type", "stack"}, {"id", "list"}, {"children", std::move(children)}});
}

/// @brief Building and laying out a document in a window opened beforehand.
/// @param context The context.
/// @param name The workload's name.
/// @param document The document.
/// @return The workload.
Workload buildAndLayOut(Context& context, std::string name, const json& document) {
	const auto text = std::make_shared<const std::string>(document.dump());
	const auto shared = std::make_shared<const json>(document);
	const auto window = std::make_shared<Window*>(nullptr);
	Workload workload{.name = std::move(name), .runs = 5};
	workload.prepare = [&context, shared, window] { *window = &context.openWindow(*shared); };
	workload.step = [&context, text, window] {
		static_cast<void>(context.build(*text, context.templateFolder("kanban"), **window));
		layOut(**window);
	};
	return workload;
}

/// @brief "scaling/adversarial/deep/<n>": building and laying out panels nested n deep.
/// @param context The context.
/// @return One workload per size: the same sizes at night, as nesting stays under the parser's limit.
std::vector<Workload> deepWorkloads(Context& context, bool /*large*/) {
	return perSize(scalingSizes(6, false), [&context](int depth) {
		return buildAndLayOut(context, scalingName("adversarial/deep", depth), nestedScreen(depth));
	});
}

/// @brief "scaling/adversarial/siblings/<n>": building and laying out a stack of n labels.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> siblingWorkloads(Context& context, bool large) {
	return perSize(scalingSizes(64, large), [&context](int count) {
		return buildAndLayOut(context, scalingName("adversarial/siblings", count), siblingsScreen(count));
	});
}

/// @brief "scaling/adversarial/keys/<n>": one state change in a screen whose state has n keys.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> keyWorkloads(Context& context, bool large) {
	return perSize(scalingSizes(64, large), [&context](int count) {
		json state = json::object();
		for(int i = 0; i < count; ++i) state[std::format("k{}", i)] = i;
		json document = screenOf({{"type", "label"}, {"id", "value"}, {"text", "{{state.k0}}"}}, std::move(state));
		document["actions"]["bump"] = json::parse(R"([{"set": "state.k0", "to": "{{event.value}}"}])");
		const LoadedScreen loaded = context.load(document, context.templateFolder("kanban"));
		const auto flip = std::make_shared<bool>(false);
		Workload workload{.name = scalingName("adversarial/keys", count)};
		workload.step = [loaded, flip] {
			*flip = !*flip;
			loaded.screen.runAction("bump", loaded.window.root(), *flip ? R"({"value": 1})" : R"({"value": 2})");
			runQueuedUpdates(loaded.window);
			layOut(loaded.window);
		};
		return workload;
	});
}

} // namespace

std::vector<Group> scalingScreenGroups(bool large) {
	const auto with = [large](std::vector<Workload> (*create)(Context&, bool)) {
		return [large, create](Context& context) { return create(context, large); };
	};
	return {{"scaling/update", with(updateWorkloads)},
	        {"scaling/layout", with(layoutWorkloads)},
	        {"scaling/frame", with(frameWorkloads)},
	        {"scaling/load", with(loadWorkloads)},
	        {"scaling/build", with(buildWorkloads)},
	        {"scaling/revision", with(revisionWorkloads)},
	        {"scaling/adversarial/deep", with(deepWorkloads)},
	        {"scaling/adversarial/siblings", with(siblingWorkloads)},
	        {"scaling/adversarial/keys", with(keyWorkloads)}};
}

} // namespace affresco::bench
