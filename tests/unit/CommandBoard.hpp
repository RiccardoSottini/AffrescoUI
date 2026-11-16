/// @file
/// @brief The board app of the commands-as-tools tests (command_e2e.cpp, command_attacks.cpp), in JSON and in Lisp, built
/// in a headless window with two peers connected over in-memory channels: an agent and a reviewer, "studio".

#pragma once

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/Screen.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Panel.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace commandboard {

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::test::MemoryChannel;

/// The board app: six commands, each meeting another rule.
constexpr const char* boardJson = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "cards": [ { "id": "c1", "title": "Write", "column": "todo" },
	                      { "id": "c2", "title": "Test", "column": "doing" } ], "note": "" },
	"services": { "files": { "provider": "fs", "root": "work" } },
	"permissions": { "allow": ["studio"], "reviewers": ["studio"],
		"run": { "allow": ["agent(moveCard)"], "deny": ["*(clearBoard)"] } },
	"actions": {
		"moveCard": { "title": "Move card", "description": "Moves a card to another column.",
			"arguments": { "card": { "type": "string" }, "to": { "enum": ["todo", "doing", "done"] } },
			"do": [{ "set": "state.cards", "to": "{{map(state.cards, it.id == card ? merge(it, { column: to }) : it)}}" }] },
		"addCard": { "title": "Add card", "description": "Adds a card to do.",
			"arguments": { "title": { "type": "string" } },
			"do": [{ "push": "state.cards", "value": { "id": "new", "title": "{{title}}", "column": "todo" } }] },
		"clearBoard": { "title": "Clear board", "do": [{ "set": "state.cards", "to": [] }] },
		"exportBoard": { "title": "Export board", "do": [{ "call": "exportBoard" }] },
		"saveBoard": { "title": "Save board", "arguments": { "name": { "type": "string" } },
			"do": [{ "invoke": "files.write", "with": { "path": "{{name}}", "text": "board" } }] },
		"acceptAll": { "title": "Accept all", "do": [{ "accept": "{{proposals[0].id}}", "head": "{{proposals[0].head}}" }] },
		"allowLast": [{ "allowRun": "{{runs[length(runs) - 1].id}}" }],
		"denyLast": [{ "denyRun": "{{runs[length(runs) - 1].id}}", "reason": "not today" }],
		"touch": [{ "set": "state.cards", "to": "{{map(state.cards, merge(it, { column: 'todo' }))}}" }]
	},
	"keys": { "f7": "allowLast", "f8": "denyLast" },
	"root": { "type": "label", "id": "title", "text": "{{length(state.cards)}} cards" }
})json";

/// The same app in Lisp.
constexpr const char* boardLisp = R"lisp(
(defcommand moveCard "Moves a card to another column." :title "Move card"
  :arguments {:card {:type string} :to {:enum [todo doing done]}}
  [card to]
  (set "state.cards" (map (fn [c] (if (= (:id c) card) (merge c {:column to}) c)) state.cards)))
(defcommand addCard "Adds a card to do." :title "Add card" :arguments {:title {:type string}} [title]
  (push "state.cards" {:id "new" :title title :column "todo"}))
(defcommand clearBoard "Clears the board." :title "Clear board" [] (set "state.cards" []))
(defcommand exportBoard "Exports the board." :title "Export board" [] (call "exportBoard"))
(defcommand saveBoard "Saves the board." :title "Save board" :arguments {:name {:type string}} [name]
  (invoke "files.write" :with {:path name :text "board"}))
(defcomputed firstProposal "The oldest proposal." (first proposals))
(defcomputed lastRun "The last run a peer asked for." (:id (last runs)))
(defcommand acceptAll "Accepts the first proposal." :title "Accept all" []
  (accept (:id computed.firstProposal) :head (:head computed.firstProposal)))
(defn allowLast "The person allows the last run." [] (allowRun computed.lastRun))
(defn denyLast "The person refuses the last run." [] (denyRun computed.lastRun :reason "not today"))
(defkey "f7" allowLast)
(defkey "f8" denyLast)
(defn touch "The person moves every card back." []
  (set "state.cards" (map (fn [c] (merge c {:column "todo"})) state.cards)))
(defscreen main
  :state {:cards [{:id "c1" :title "Write" :column "todo"} {:id "c2" :title "Test" :column "doing"}] :note ""}
  :services {:files {:provider fs :root work}}
  :permissions {:allow [studio] :reviewers [studio]
                :run {:allow ["agent(moveCard)"] :deny ["*(clearBoard)"]}}
  (label#title :text ~(str (count state.cards) " cards")))
)lisp";

/// @brief A folder of the test's own, emptied first and removed after.
struct Folder {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "affrescoui-command-e2e-test"; ///< It.
	/// @brief Makes it, with "work" inside.
	Folder() {
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path / "work");
	}
	/// @brief Removes it.
	~Folder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	Folder(const Folder&) = delete;
	Folder& operator=(const Folder&) = delete;
	Folder(Folder&&) = delete;
	Folder& operator=(Folder&&) = delete;
};

/// @brief Writes a text file. @param path Where. @param text What.
inline void writeText(const std::filesystem::path& path, const std::string& text) {
	std::FILE* file = std::fopen(path.string().c_str(), "wb");
	REQUIRE(file != nullptr);
	std::fputs(text.c_str(), file);
	std::fclose(file);
}

/// The time a question must be shown before a press says yes to it, in milliseconds, and a little more.
constexpr double answerDelayMs = 600.0;

/// @brief A panel of the window's question that is a button with a name.
/// @param panel Where to look.
/// @param name The name.
/// @return It, or nullptr.
inline affresco::Panel* button(affresco::Panel* panel, const std::string& name) {
	if(!panel) return nullptr;
	if(panel->accessibleRole() == "button" && panel->accessibleLabel() == name) return panel;
	for(const auto& child : panel->children())
		if(affresco::Panel* found = button(child.get(), name)) return found;
	return nullptr;
}

/// @brief The texts the window's question shows, joined.
/// @param panel Where to look.
/// @return Each label's and text box's text, a line each.
inline std::string shownTexts(affresco::Panel* panel) {
	if(!panel) return "";
	std::string texts;
	if(const auto* label = dynamic_cast<affresco::Label*>(panel)) texts = label->text() + "\n";
	if(const auto* editor = dynamic_cast<affresco::TextEditor*>(panel)) texts = editor->text() + "\n";
	for(const auto& child : panel->children()) texts += shownTexts(child.get());
	return texts;
}

/// @brief An executor whose posted tasks the test runs on the main thread; services post to it from their own.
class PumpedExecutor final : public affresco::Executor {
public:
	/// @brief Keeps a task. @param task It.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}

	/// @brief Runs the tasks posted, and those they post, until a condition holds.
	/// @param done The condition, checked after each batch; none to run only what is posted already.
	/// @return Whether it held within 10 s.
	bool pumpUntil(const std::function<bool()>& done = {}) {
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while(!done || !done()) {
			std::vector<std::function<void()>> tasks;
			{
				std::unique_lock lock(mutex_);
				if(!done && tasks_.empty()) return true;
				if(!posted_.wait_until(lock, end, [this] { return !tasks_.empty(); })) return done();
				tasks.swap(tasks_);
			}
			for(const auto& task : tasks) task();
		}
		return true;
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::condition_variable posted_;           ///< Told when a task comes.
	std::vector<std::function<void()>> tasks_; ///< The tasks not run yet.
};

/// @brief Loads the board app, JSON or Lisp, or another document.
/// @param folder Where a Lisp program is written.
/// @param lisp Whether to load the Lisp one.
/// @param document Another document instead of the board, JSON or Lisp (starting with "("), or empty.
/// @return The screen, not built.
inline affresco::Screen load(const Folder& folder, bool lisp, const std::string& document) {
	if(!document.empty() && !document.starts_with('(')) return affresco::Screen::fromString(document);
	if(!lisp && document.empty()) return affresco::Screen::fromString(boardJson);
	writeText(folder.path / "board.screen.lisp", document.empty() ? boardLisp : document.c_str());
	return affresco::Screen::fromFile(folder.path / "board.screen.lisp", folder.path / "data",
	                                  affresco::Screen::History::InMemory);
}

/// @brief The board app built in a headless window, an agent and a reviewer connected.
struct Board {
	Folder folder;                                  ///< Its files ("work") and data.
	MemoryChannel agent;                            ///< The agent's channel; outlives the screen.
	MemoryChannel studio;                           ///< The reviewer's channel; outlives the screen.
	PumpedExecutor executor;                        ///< The services' answers.
	ManualClock clock;                              ///< The window's time.
	std::unique_ptr<affresco::Window> window;       ///< The window; outlives the screen.
	affresco::Screen screen;                        ///< The app.
	int exports = 0;                                ///< How often the native exportBoard ran.
	std::vector<std::string> errors;                ///< What the screen reported.
	int nextId = 0;                                 ///< The last message id sent.

	/// @brief Loads, builds and connects it.
	/// @param lisp Whether to load the Lisp app.
	/// @param document Another document instead of the board, JSON or Lisp (starting with "("), or empty.
	explicit Board(bool lisp = false, const std::string& document = {})
	    : window(createHeadlessWindow(400, 300, clock)), screen(load(folder, lisp, document)) {
		screen.onCall("exportBoard", [this](const std::string&) {
			++exports;
			return std::string("null");
		});
		screen.onError([this](const std::string& message) { errors.push_back(message); });
		screen.services().addRoot("work", folder.path / "work");
		screen.setExecutor(&executor);
		screen.build(*window);
		window->render();
		screen.connect(agent, "agent");
		screen.connect(studio, "studio");
	}

	/// @brief Sends a message as a peer, and gives the first answer to it.
	/// @param peer Who sends it.
	/// @param message It, without an id.
	/// @return The answer with its id; a "none" type if there was none.
	json ask(MemoryChannel& peer, json message) {
		message["id"] = ++nextId;
		peer.receive(message.dump());
		executor.pumpUntil();
		return answer(peer, nextId);
	}

	/// @brief The first answer a peer was sent to a message, after an earlier one if given.
	/// @param peer Its channel.
	/// @param id The message's id.
	/// @param after How many answers to that id to pass over.
	/// @return It; a "none" type if there is none.
	static json answer(const MemoryChannel& peer, int id, std::size_t after = 0) {
		for(const json& sent : peer.sent)
			if(sent.value("id", json()) == id && after-- == 0) return sent;
		return {{"type", "none"}};
	}

	/// @brief Asks the agent to run a command, the arguments given.
	/// @param command Its name.
	/// @param arguments Its arguments.
	/// @param author A full name's part after "agent/", or empty.
	/// @return The first answer.
	json run(const std::string& command, const json& arguments = json::object(), const std::string& author = {}) {
		json message = {{"type", "runCommand"}, {"command", command}, {"arguments", arguments}};
		if(!author.empty()) message["author"] = author;
		return ask(agent, std::move(message));
	}

	/// @brief The command entries a peer lists.
	/// @param peer Who asks.
	/// @return name → entry.
	std::map<std::string, json> commands(MemoryChannel& peer) {
		std::map<std::string, json> found;
		for(const json& entry : ask(peer, {{"type", "getCommands"}, {"all", true}}).value("commands", json::array()))
			found[entry["name"].get<std::string>()] = entry;
		return found;
	}

	/// @brief The runs the last review message a peer heard lists.
	/// @param peer Its channel.
	/// @return They, oldest first; none if it heard none.
	static json runs(const MemoryChannel& peer) {
		for(const json& sent : std::ranges::reverse_view(peer.sent))
			if(sent.value("type", "") == "review") return sent.value("runs", json::array());
		return json::array();
	}

	/// @brief A state value.
	/// @param key Its key in `state`.
	/// @return It.
	[[nodiscard]] json state(const std::string& key) const { return json::parse(screen.stateJson()).at(key); }

	/// @brief A card's column.
	/// @param id The card.
	/// @return Its column; "" if there is no such card.
	[[nodiscard]] std::string column(const std::string& id) const {
		for(const json& card : state("cards"))
			if(card["id"] == id) return card["column"].get<std::string>();
		return "";
	}

	/// @brief Clicks one of the window question's buttons with the person's own mouse, once it has been shown long
	/// enough for a press to count.
	/// @param name The button's name.
	void press(const std::string& name) {
		clock.advance(answerDelayMs);
		window->render();
		affresco::Panel* found = button(window->confirmation(), name);
		REQUIRE(found != nullptr);
		const affresco::Rect& frame = found->frame();
		const affresco::Vec2 at{(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
		window->routeMouseMove(at);
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		window->render();
		executor.pumpUntil();
	}

	/// @brief Runs one of the app's own actions or functions, as the person's input would.
	/// @param name It.
	void act(const std::string& name) {
		screen.runAction(name, window->root(), "{}");
		window->render();
		executor.pumpUntil();
	}

	/// @brief Presses and lets go of a key, the person's own input.
	/// @param key It.
	void key(affresco::Key key) {
		window->routeKey({key, 0, affresco::Action::Press, affresco::Mods{}});
		window->routeKey({key, 0, affresco::Action::Release, affresco::Mods{}});
		window->render();
		executor.pumpUntil();
	}

	/// @brief A reviewer's decision on a run.
	/// @param type "allowRun" or "denyRun".
	/// @param run The run.
	/// @param always For allowRun: whether to add a grant.
	/// @return The answer.
	json decide(MemoryChannel& peer, const std::string& type, const json& run, bool always = false) {
		json message = {{"type", type}, {"run", run}};
		if(type == "allowRun") message["always"] = always;
		else message["reason"] = "not today";
		return ask(peer, std::move(message));
	}

	/// @brief Lets time pass on the window's clock, a frame a second.
	/// @param seconds How long.
	void wait(int seconds) {
		for(int second = 0; second < seconds; ++second) {
			clock.advance(1000.0);
			window->render();
		}
	}
};

} // namespace commandboard
