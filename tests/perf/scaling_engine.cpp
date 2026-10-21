/// @file
/// @brief The scaling areas that need no window (tests/perf/README.md, "Scaling"): the checker, the history, list
/// expressions, text, the editor (in a headless window) and a peer's messages, each at sizes that double.

#include "Bench.hpp"
#include "Expression.hpp"
#include "Headless.hpp"

#include <affresco/channel/Limits.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/render/Font.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/vcs/Repository.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::bench {
namespace {

/// @brief "scaling/checker/<cards>": the document checker's findings and validation of the Kanban template.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> checkerWorkloads(Context& context, bool large) {
	const auto rules = std::make_shared<const format::Rules>();
	return perSize(scalingSizes(16, large), [&context, rules](int cards) {
		const auto document = std::make_shared<const json>(kanbanWithCards(context.screens(), cards));
		Workload workload{.name = scalingName("checker", cards), .runs = 5};
		workload.step = [rules, document] {
			rules->validate(*document);
			if(!rules->findings(*document, document->value("state", json::object())).empty())
				throw std::logic_error("checker: the Kanban template has findings");
		};
		return workload;
	});
}

/// @brief A small commit: the logo's text, a different one each time (as history.cpp makes).
/// @param number Which one.
/// @return The revision.
json rename(int number) {
	return {{"author", "bench"}, {"message", "Rename the logo"}, {"set", {{"#logo/text", "Flow " + std::to_string(number)}}}};
}

/// @brief A Kanban history of n renames, and the next rename's number.
struct History {
	format::Rules rules;                      ///< The format's rules.
	std::unique_ptr<vcs::Repository> history; ///< The history.
	int next = 0;                             ///< The next rename's number.

	/// @brief Makes the history.
	/// @param document The Kanban template.
	/// @param commits n.
	/// @throws std::runtime_error If a commit is refused.
	History(const json& document, int commits) : history(std::make_unique<vcs::Repository>(rules, document)) {
		commitRenames(commits);
	}

	/// @brief Commits more renames.
	/// @param commits How many.
	/// @throws std::runtime_error If one is refused.
	void commitRenames(int commits) {
		for(int i = 0; i < commits; ++i)
			if(const auto error = history->commit(rename(next++))) throw std::runtime_error("history: " + *error);
	}
};

/// @brief The Kanban template, as read from its file.
/// @param context The context.
/// @return The document.
json kanban(const Context& context) { return json::parse(readFile(context.screens() / "kanban.screen.json")); }

/// @brief "scaling/history/commit/<n>": 16 more commits on a history of n, per commit; retainedBytes is what one keeps.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> commitWorkloads(Context& context, bool large) {
	const json document = kanban(context);
	return perSize(scalingSizes(64, large), [&document](int commits) {
		const auto history = std::make_shared<History>(document, commits);
		Workload workload{.name = scalingName("history/commit", commits), .runs = 5, .operations = 16};
		workload.step = [history] { history->commitRenames(16); };
		return workload;
	});
}

/// @brief What one read of a history does: given the history and n.
using HistoryRead = void (*)(const vcs::Repository& history, int commits);

/// @brief One area of reads on a history of n commits, at each size.
/// @param context The context.
/// @param large Nightly sizes.
/// @param area E.g. "history/restore".
/// @param read The read.
/// @return One workload per size.
std::vector<Workload> historyReads(const Context& context, bool large, const char* area, HistoryRead read) {
	const json document = kanban(context);
	return perSize(scalingSizes(64, large), [&document, area, read](int commits) {
		// Renames, but commit n/2 sets a state key instead, so reverting it meets no conflict
		const auto history = std::make_shared<History>(document, commits / 2 - 1);
		if(const auto error = history->history->commit({{"author", "bench"}, {"set", {{"/state/bench", commits}}}}))
			throw std::runtime_error("history: " + *error);
		history->commitRenames(commits - commits / 2);
		Workload workload{.name = scalingName(area, commits), .runs = 5};
		workload.step = [history, read, commits] { read(*history->history, commits); };
		return workload;
	});
}

/// @brief Throws if a computed revision is empty.
/// @param revision What restore or revert made.
/// @param what The area.
void expectChange(const json& revision, const char* what) {
	if(!revision.is_object() || revision.empty()) throw std::logic_error(std::string(what) + ": no revision");
}

/// @brief "scaling/history/restore/<n>": the revision that restores the first commit.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> restoreWorkloads(Context& context, bool large) {
	return historyReads(context, large, "history/restore", [](const vcs::Repository& history, int /*commits*/) {
		expectChange(history.restoreRevision(1, "bench"), "restore");
	});
}

/// @brief "scaling/history/revert/<n>": the revision that reverts the commit in the middle (a state change).
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> revertWorkloads(Context& context, bool large) {
	return historyReads(context, large, "history/revert", [](const vcs::Repository& history, int commits) {
		expectChange(history.revertRevision(commits / 2, "bench"), "revert");
	});
}

/// @brief "scaling/history/diff/<n>": the diff from the first commit to the last.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> diffWorkloads(Context& context, bool large) {
	return historyReads(context, large, "history/diff", [](const vcs::Repository& history, int commits) {
		if(history.diff(1, commits).empty()) throw std::logic_error("history/diff: no change");
	});
}

/// @brief "scaling/history/blame/<n>": blame of a node no commit changed, so the whole history is searched.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> blameWorkloads(Context& context, bool large) {
	return historyReads(context, large, "history/blame", [](const vcs::Repository& history, int /*commits*/) {
		static_cast<void>(history.blame("#board"));
	});
}

/// @brief A folder in the system's temporary folder, removed with what it holds when the last workload goes.
class TemporaryFolder {
public:
	/// @brief Makes the folder.
	/// @param name What it holds, in its name.
	explicit TemporaryFolder(const std::string& name)
	    : path_(std::filesystem::temp_directory_path() /
	            ("affrescoui-bench-" + name + "-" +
	             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
		std::filesystem::create_directories(path_);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	/// @brief Removes it.
	~TemporaryFolder() {
		try {
			std::filesystem::remove_all(path_);
		} catch(...) { // a destructor mustn't throw: say what stays behind
			std::fputs("affrescoui_bench: a history folder in the temporary folder couldn't be removed\n", stderr);
		}
	}
	/// @brief The folder.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< The folder.
};

/// @brief Writes a history of n renames to a new folder, as one change set.
/// @param document The Kanban template.
/// @param commits n.
/// @return The folder.
/// @throws std::runtime_error If the change set is refused.
std::shared_ptr<const TemporaryFolder> writeHistory(const json& document, int commits) {
	auto folder = std::make_shared<const TemporaryFolder>("history-" + std::to_string(commits));
	json revisions = json::array();
	for(int i = 0; i < commits; ++i) revisions.push_back(rename(i));
	const format::Rules rules;
	vcs::Repository writer(rules, document);
	writer.openHistory(folder->path() / "kanban.history");
	const json changes = {{"format", "affrescoui-changes"}, {"version", 1}, {"name", "renames"}, {"revisions", revisions}};
	if(const auto error = writer.importChanges(changes)) throw std::runtime_error("history/load: " + *error);
	return folder; // the writer goes first, releasing the folder's lock
}

/// @brief "scaling/history/load/<n>": opening a folder that holds a history of n commits.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> loadWorkloads(Context& context, bool large) {
	const auto document = std::make_shared<const json>(kanban(context));
	const auto rules = std::make_shared<const format::Rules>();
	return perSize(scalingSizes(64, large), [document, rules](int commits) {
		const auto folder = writeHistory(*document, commits);
		Workload workload{.name = scalingName("history/load", commits), .unit = "ms", .runs = 5};
		workload.step = [folder, rules, document, commits] {
			vcs::Repository reader(*rules, *document);
			if(!reader.openHistory(folder->path() / "kanban.history").loaded || reader.head().number != commits)
				throw std::logic_error("history/load: not loaded whole");
		};
		return workload;
	});
}

/// @brief What a list expression sees: a state of n numbers.
struct ListInputs {
	json state;                           ///< {"items": [0 .. n-1]}
	json window = json::object();         ///< window
	json revision = 0;                    ///< revision
	json event = json::object();          ///< event
	json globals = {{"mode", "default"}}; ///< globals
	format::Scope locals;                 ///< No local variables.
};

/// @brief "scaling/expressions/{list,sort}/<n>": a filter, map and sum, and a sort, over a list of n numbers.
/// @param large Nightly sizes.
/// @return Two workloads per size.
std::vector<Workload> expressionWorkloads(Context& /*context*/, bool large) {
	const std::array<std::pair<const char*, const char*>, 2> expressions{
	    {{"expressions/list", "{{sum(map(filter(state.items, it % 2 == 0), it * 3))}}"},
	     {"expressions/sort", "{{sort(state.items, it, 'desc')}}"}}};
	std::vector<Workload> workloads;
	for(const int count : scalingSizes(256, large)) {
		const auto inputs = std::make_shared<ListInputs>();
		inputs->state["items"] = json::array();
		for(int i = 0; i < count; ++i) inputs->state["items"].push_back((i * 7919) % count); // not in order
		for(const auto& [area, text] : expressions) {
			const auto expression = std::make_shared<const json>(text);
			Workload workload{.name = scalingName(area, count)};
			workload.step = [inputs, expression] {
				const format::Context scope{inputs->state, inputs->window, inputs->revision,
				                            inputs->event, inputs->locals, inputs->globals};
				if(format::evaluate(*expression, scope, "bench").is_null()) throw std::logic_error("expressions: null");
			};
			workloads.push_back(std::move(workload));
		}
	}
	return workloads;
}

/// @brief The template font with the icon font as its fallback, as the templates load it.
/// @param context The context.
/// @return The font.
std::shared_ptr<const Font> textFont(const Context& context) {
	const std::filesystem::path fonts = context.screens().parent_path() / "fonts";
	const std::array<std::filesystem::path, 1> fallbacks{fonts / "Lucide.ttf"};
	return Font::load(fonts / "Roboto-Regular.ttf", 15.0f, fallbacks);
}

/// @brief An icon of the fallback font.
/// @param font The font.
/// @return Its character.
/// @throws std::logic_error If the icon font has no "star".
char32_t starIcon(const Font& font) {
	const char32_t star = font.codepointNamed("star");
	if(star == 0) throw std::logic_error("text: the icon font has no \"star\"");
	return star;
}

/// @brief n characters of words in three scripts, with an icon in every word.
/// @param font The font.
/// @param count n.
/// @return The text.
std::u32string mixedText(const Font& font, int count) {
	const std::array<std::u32string_view, 3> words{U"layout", U"δέντρο",
	                                               U"дерево"};
	std::u32string text;
	for(std::size_t word = 0; text.size() < static_cast<std::size_t>(count); ++word) {
		text += words[word % words.size()];
		text += starIcon(font);
		text += U' ';
	}
	text.resize(static_cast<std::size_t>(count));
	return text;
}

/// @brief What a text workload does with its text.
using TextUse = void (*)(const Font& font, std::u32string_view text);

/// @brief A workload that measures, wraps or shapes a text whose first characters change every step, so neither the
/// line cache nor the word cache answers it.
/// @param name The workload's name.
/// @param font The font.
/// @param text The text, with room at its start for the counter.
/// @param use What is done with it.
/// @return The workload.
Workload textWorkload(std::string name, std::shared_ptr<const Font> font, std::u32string text, TextUse use) {
	const auto shared = std::make_shared<std::u32string>(std::move(text));
	const auto next = std::make_shared<unsigned>(0);
	Workload workload{.name = std::move(name)};
	workload.step = [font = std::move(font), shared, next, use] {
		unsigned counter = (*next)++;
		for(std::size_t i = 0; i < 6 && i < shared->size(); ++i, counter /= 10)
			(*shared)[i] = static_cast<char32_t>(U'0' + counter % 10);
		use(*font, *shared);
	};
	return workload;
}

/// @brief Measures a text. @param font The font. @param text The text.
void measureText(const Font& font, std::u32string_view text) { static_cast<void>(font.measure(text)); }
/// @brief Wraps a text at 240 pixels. @param font The font. @param text The text.
void wrapText(const Font& font, std::u32string_view text) { static_cast<void>(font.wrap(text, 240.0f)); }
/// @brief Shapes a text. @param font The font. @param text The text.
void shapeText(const Font& font, std::u32string_view text) { static_cast<void>(font.shape(text)); }

/// @brief "scaling/text/{measure,wrap,shape}/<n>": n characters of Latin, Greek and Cyrillic words with icons.
/// @param context The context.
/// @param large Nightly sizes.
/// @return Three workloads per size.
std::vector<Workload> textWorkloads(Context& context, bool large) {
	const auto font = textFont(context);
	std::vector<Workload> workloads;
	for(const int count : scalingSizes(64, large)) {
		const std::u32string text = mixedText(*font, count);
		workloads.push_back(textWorkload(scalingName("text/measure", count), font, text, measureText));
		workloads.push_back(textWorkload(scalingName("text/wrap", count), font, text, wrapText));
		workloads.push_back(textWorkload(scalingName("text/shape", count), font, text, shapeText));
	}
	return workloads;
}

/// @brief "scaling/adversarial/line/<n>": wrapping one word of n characters, with no space to break at.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> lineWorkloads(Context& context, bool large) {
	const auto font = textFont(context);
	return perSize(scalingSizes(64, large), [&font](int count) {
		return textWorkload(scalingName("adversarial/line", count), font,
		                    std::u32string(static_cast<std::size_t>(count), U'm'), wrapText);
	});
}

/// @brief "scaling/adversarial/fonts/<n>": shaping n characters that alternate between the font and its fallback,
/// one run of one font per character.
/// @param context The context.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> fontWorkloads(Context& context, bool large) {
	const auto font = textFont(context);
	const char32_t star = starIcon(*font);
	return perSize(scalingSizes(64, large), [&font, star](int count) {
		std::u32string text;
		for(int i = 0; i < count; ++i) text += i % 2 == 0 ? U'a' : star;
		return textWorkload(scalingName("adversarial/fonts", count), font, std::move(text), shapeText);
	});
}

/// @brief An editor of n lines in a headless window, focused, its caret on the middle line.
struct EditorWindow {
	detail::ManualClock clock;      ///< The window's clock.
	std::unique_ptr<Window> window; ///< The window.
	TextEditor* editor = nullptr;   ///< The editor.

	/// @brief Opens it.
	/// @param font The font.
	/// @param lines n.
	EditorWindow(std::shared_ptr<const Font> font, int lines) : window(detail::createHeadlessWindow(800, 600, clock)) {
		std::string text;
		for(int i = 0; i < lines; ++i)
			text += "\tconst int value" + std::to_string(i) + " = compute(" + std::to_string(i) + ");\n";
		editor = &window->root().add<TextEditor>();
		editor->setFont(std::move(font));
		editor->setText(text);
		editor->setSize(px(400.0f), px(300.0f));
		window->render();
		window->setFocus(editor);
		for(int i = 0; i < lines / 2; ++i) press(Key::Down);
	}

	/// @brief Presses and releases a key.
	/// @param key The key.
	void press(Key key) const {
		window->routeKey({key, 0, Action::Press, Mods{}});
		window->routeKey({key, 0, Action::Release, Mods{}});
	}
};

/// @brief "scaling/editor/{insert,delete,caret}/<n>": typing a character, deleting one, and moving the caret down and
/// up, in the middle of n lines; each followed by a frame laid out.
/// @param context The context.
/// @param large Nightly sizes.
/// @return Three workloads per size.
std::vector<Workload> editorWorkloads(Context& context, bool large) {
	const std::shared_ptr<const Font> font =
	    Font::load(context.screens().parent_path() / "fonts" / "JetBrainsMono-Regular.ttf", 14.0f);
	std::vector<Workload> workloads;
	for(const int lines : scalingSizes(64, large)) {
		const auto open = std::make_shared<EditorWindow>(font, lines);
		Workload insert{.name = scalingName("editor/insert", lines)};
		insert.step = [open] {
			open->window->routeChar({U'x'});
			open->window->render();
		};
		Workload erase{.name = scalingName("editor/delete", lines)};
		erase.prepare = [open] { open->window->routeChar({U'x'}); };
		erase.step = [open] {
			open->press(Key::Backspace);
			open->window->render();
		};
		const auto down = std::make_shared<bool>(false);
		Workload caret{.name = scalingName("editor/caret", lines)};
		caret.step = [open, down] {
			*down = !*down;
			open->press(*down ? Key::Down : Key::Up);
			open->window->render();
		};
		for(Workload* workload : {&insert, &erase, &caret}) workloads.push_back(std::move(*workload));
	}
	return workloads;
}

/// @brief A channel that keeps nothing: it counts what the session sends.
class CountingChannel final : public Channel {
public:
	/// @brief Counts a message.
	void send(std::string_view /*message*/) override { ++sent; }

	int sent = 0; ///< Messages sent so far.
};

/// @brief A peer's session on a small screen, with limits that never stop the bench.
struct Peer {
	format::Rules rules;      ///< The screen format.
	vcs::Repository history;  ///< The screen.
	CountingChannel channel;  ///< The peer's end.
	channel::Session session; ///< Its session.

	/// @brief Connects the peer as "studio", who may change anything.
	Peer()
	    : history(rules, json::parse(R"({"format": "affrescoui-screen", "version": 1, "state": {"items": []},
	          "permissions": {"allow": ["studio"]}, "root": {"type": "panel", "id": "main"}})")),
	      session(channel, history, "studio", {}, nullptr,
	              channel::PeerLimits{.messagesPerSecond = 1e9, .messageBurst = 1e9, .revisionsPerMinute = 1e9,
	                                  .revisionBurst = 1e9}) {}
};

/// @brief "scaling/peer/<n>": a peer's revision that sets a list of n strings: parsed, checked and committed.
/// @param large Nightly sizes.
/// @return One workload per size.
std::vector<Workload> peerWorkloads(Context& /*context*/, bool large) {
	return perSize(scalingSizes(64, large), [](int count) {
		const auto peer = std::make_shared<Peer>();
		std::array<std::shared_ptr<const std::string>, 2> messages;
		for(std::size_t variant = 0; variant < messages.size(); ++variant) {
			json items = json::array();
			for(int i = 0; i < count; ++i) items.push_back("item " + std::to_string(i) + (variant ? "b" : "a"));
			const json message = {{"type", "revision"}, {"id", variant}, {"set", {{"/state/items", items}}}};
			messages[variant] = std::make_shared<const std::string>(message.dump());
		}
		const auto next = std::make_shared<std::size_t>(0);
		Workload workload{.name = scalingName("peer", count)};
		workload.step = [peer, messages, next] {
			const int before = peer->channel.sent;
			peer->channel.onMessage(*messages[(*next)++ % messages.size()]);
			if(peer->channel.sent == before) throw std::logic_error("peer: no answer");
		};
		return workload;
	});
}

} // namespace

std::vector<Group> scalingEngineGroups(bool large) {
	const auto with = [large](std::vector<Workload> (*create)(Context&, bool)) {
		return [large, create](Context& context) { return create(context, large); };
	};
	return {{"scaling/checker", with(checkerWorkloads)},
	        {"scaling/history/commit", with(commitWorkloads)},
	        {"scaling/history/restore", with(restoreWorkloads)},
	        {"scaling/history/revert", with(revertWorkloads)},
	        {"scaling/history/diff", with(diffWorkloads)},
	        {"scaling/history/blame", with(blameWorkloads)},
	        {"scaling/history/load", with(loadWorkloads)},
	        {"scaling/expressions", with(expressionWorkloads)},
	        {"scaling/text", with(textWorkloads)},
	        {"scaling/adversarial/line", with(lineWorkloads)},
	        {"scaling/adversarial/fonts", with(fontWorkloads)},
	        {"scaling/editor", with(editorWorkloads)},
	        {"scaling/peer", with(peerWorkloads)}};
}

} // namespace affresco::bench
