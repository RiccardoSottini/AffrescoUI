/// @file
/// @brief host::ChangesWindow: the host's window on a change.

#include "ChangesWindow.hpp"
#include "HostScreens.hpp"

#include <affresco/core/Log.hpp>
#include <affresco/format/Locale.hpp>
#include <affresco/vcs/Repository.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace host {

using affresco::json;

namespace {

/// How many applied changes the window lists, newest first.
constexpr std::size_t mostCommits = 50;

/// The longest place an app may ask the Source page for: "#id", a JSON Pointer or a definition's address.
constexpr std::size_t mostSourceBytes = 512;

/// The highest line an app may ask the Source page to mark (a definition's text is at most 16 KiB).
constexpr std::int64_t mostSourceLine = 1'000'000;

/// The longest request the window may send an app's assistant: a plan names at most 64 later changes, each with its
/// message, far below this.
constexpr std::size_t mostAskBytes = 32 * 1024;

/// The action an app's assistant starts a request with (examples/shared/addons/assistant): what the host's `--ask`
/// runs too.
constexpr const char* assistantAction = "assistant.start";

/// Who the window undoes as: the person's own channel, whose changes to a program's definitions are the person's.
constexpr std::string_view undoAuthor = affresco::vcs::Repository::personChannel;

/// @brief An error for the window to show.
/// @param message What went wrong.
/// @return `{"error": ...}` as JSON text.
[[nodiscard]] std::string errorAnswer(const std::string& message) { return json({{"error", message}}).dump(); }

/// @brief The call's arguments, an object even if they are malformed.
/// @param text Their JSON text.
/// @return The object.
[[nodiscard]] json argumentsOf(const std::string& text) {
	json arguments = json::parse(text, nullptr, false);
	return arguments.is_object() ? arguments : json::object();
}

/// @brief The line an app asked for, if it is a whole number from 1.
/// @param arguments The call's arguments.
/// @return `line`, or null.
[[nodiscard]] json lineOf(const json& arguments) {
	const auto line = arguments.find("line");
	if(line == arguments.end() || !line->is_number_integer()) return nullptr;
	const std::int64_t number = line->get<std::int64_t>();
	return number >= 1 && number <= mostSourceLine ? json(number) : json();
}

/// @brief The place an app asked the Source page for, if it is a text of at most mostSourceBytes.
/// @param arguments The call's arguments.
/// @param key Where it is: `source` from an app, `at` from the window.
/// @return The place, or std::nullopt.
[[nodiscard]] std::optional<std::string> placeOf(const json& arguments, const char* key) {
	const auto place = arguments.find(key);
	if(place == arguments.end() || !place->is_string() || place->get_ref<const std::string&>().size() > mostSourceBytes)
		return std::nullopt;
	return place->get<std::string>();
}

/// @brief The commit that last changed what a source names, as the Source page shows it.
/// @param versions The app's history.
/// @param address The source's address: a definition's, "#id" or a JSON Pointer.
/// @return `{number, id, author, message}`, or null if the history can't say (a history that keeps none, or a
/// definition only a previewed proposal has).
[[nodiscard]] json blameOf(const affresco::VersionControl& versions, const std::string& address) {
	try {
		const affresco::CommitInfo commit = versions.blame(address);
		return {{"number", commit.number}, {"id", commit.id}, {"author", commit.author}, {"message", commit.message}};
	} catch(const affresco::DocumentError&) {
		return nullptr; // the page shows the source without it
	}
}

/// @brief The choices the window sends with an undo or its plan: only "drop", "keep" and "head" (a JSON Patch to
/// apply after it, "then", is an agent's rebuild, never the window's).
/// @param arguments The call's arguments.
/// @param where Where they are, for errors ("changes.undo").
/// @return The options; empty if it names none.
/// @throws DocumentError If one is malformed, naming it.
[[nodiscard]] affresco::UndoOptions choicesOf(const json& arguments, std::string_view where) {
	json choices = json::object();
	for(const char* key : {"drop", "keep", "head"})
		if(const auto value = arguments.find(key); value != arguments.end()) choices[key] = *value;
	return affresco::undoOptionsFrom(choices, where);
}

/// @brief The request an app's assistant is asked, if it is a text of 1 to mostAskBytes.
/// @param arguments The call's arguments.
/// @return `text`, or std::nullopt.
[[nodiscard]] std::optional<std::string> askedTextOf(const json& arguments) {
	const auto text = arguments.find("text");
	if(text == arguments.end() || !text->is_string()) return std::nullopt;
	const std::string& value = text->get_ref<const std::string&>();
	if(value.empty() || value.size() > mostAskBytes) return std::nullopt;
	return value;
}

} // namespace

struct ChangesWindow::Impl {
	affresco::Application& app;          ///< The application.
	affresco::Screen& screen;            ///< The app's screen.
	affresco::Window& mainWindow;        ///< The app's window.
	std::filesystem::path file;          ///< The window's screen file.
	std::optional<affresco::Screen> own; ///< The window's screen, once opened.
	affresco::Window* window = nullptr;  ///< The window, once opened.
	json selection = nullptr;            ///< The change asked for last.
	int selectionCount = 0;              ///< How many times one was asked for: the window reads it to follow.
	json source = nullptr;               ///< The source asked for last: `{"source": at, "line"?: n}`.
	int sourceCount = 0;                 ///< How many times one was asked for: the window reads it to follow.
	affresco::Connection keys = 0;       ///< The shortcut on the app's window.

	/// @brief Opens the window, or brings it back, on a change or on the Source page.
	/// @param arguments `source` (a text of at most mostSourceBytes) and `line`: the Source page for that place;
	/// otherwise `proposal`, if the proposal is open, or else the newest applied change.
	void open(const json& arguments) {
		if(const std::optional<std::string> place = placeOf(arguments, "source")) {
			source = {{"source", *place}};
			if(const json line = lineOf(arguments); !line.is_null()) source["line"] = line;
			++sourceCount;
		} else {
			select(arguments);
		}
		if(window != nullptr) {
			window->setShouldClose(false);
			window->setVisible(true);
			return;
		}
		own.emplace(readHostScreen(file));
		own->setLocale(affresco::format::systemLocale());
		registerCalls();
		window = &own->createWindow(app, mainWindow);
	}

	/// @brief Asks the window for a change.
	/// @param arguments `proposal`, if the proposal is open; otherwise the newest applied change.
	void select(const json& arguments) {
		const std::vector<affresco::ProposalInfo> proposals = screen.proposals();
		const auto isOpen = [&](const affresco::ProposalInfo& p) {
			return json(p.id) == arguments.value("proposal", json());
		};
		if(std::ranges::any_of(proposals, isOpen)) selection = {{"proposal", arguments["proposal"]}};
		else if(!proposals.empty()) selection = {{"proposal", proposals.back().id}};
		else selection = {{"to", "HEAD"}};
		++selectionCount;
	}

	/// @brief What the window lists, and what it should show.
	/// @return `{"selection", "selectionCount", "source", "sourceCount", "previewing", "proposals", "history",
	/// "canAsk"}`, each proposal with what a merge left out of it (`leftOut`, the part's two values shown as text).
	[[nodiscard]] json list() const {
		if(mainWindow.shouldClose() && window != nullptr) window->setShouldClose(true); // it goes with its app
		json proposals = json::array();
		for(const affresco::ProposalInfo& p : screen.proposals())
			proposals.push_back({{"id", p.id},
			                     {"head", p.head},
			                     {"author", p.author},
			                     {"message", p.message},
			                     {"leftOut", leftOutOf(p)}});
		const std::vector<affresco::CommitInfo> log = screen.versions().log();
		json history = json::array();
		for(auto commit = log.rbegin(); commit != log.rend() && history.size() < mostCommits; ++commit)
			history.push_back({{"number", commit->number},
			                   {"id", commit->id},
			                   {"author", commit->author},
			                   {"message", commit->message}});
		const std::optional<int> previewed = screen.previewedProposal();
		return {{"selection", selection},
		        {"selectionCount", selectionCount},
		        {"source", source},
		        {"sourceCount", sourceCount},
		        {"previewing", previewed ? json(*previewed) : json()},
		        {"proposals", std::move(proposals)},
		        {"history", std::move(history)},
		        {"canAsk", canAsk()}};
	}

	/// @brief What a merge left out of a proposal, as the window shows it: each part's path, and main's and the
	/// proposal's values as text, side by side.
	/// @param proposal The proposal.
	/// @return `[{"path", "mine", "theirs"}]`; empty if nothing was left out.
	[[nodiscard]] static json leftOutOf(const affresco::ProposalInfo& proposal) {
		json parts = json::array();
		if(!proposal.leftOut.is_array()) return parts;
		const auto text = [](const json& value) {
			if(value.is_string()) return value.get<std::string>();
			return value.is_null() ? std::string("(none)") : value.dump(2, ' ', false, json::error_handler_t::replace);
		};
		for(const json& part : proposal.leftOut)
			if(part.is_object() && part.contains("path"))
				parts.push_back({{"path", part["path"]},
				                 {"mine", text(part.value("main", json()))},
				                 {"theirs", text(part.value("proposal", json()))}});
		return parts;
	}

	/// @brief Tells whether the app has an assistant to ask: an action assistantAction in its document, where the
	/// app's own runAction finds it.
	/// @return true if so.
	[[nodiscard]] bool canAsk() const {
		const json& document = screen.versions().document();
		const auto actions = document.find("actions");
		return actions != document.end() && actions->is_object() && actions->contains(assistantAction);
	}

	/// @brief Where a node is written, for the Source page: the app's document as it runs (a previewed proposal's,
	/// while one is previewed), never a file on disk.
	/// @param arguments `at` (a text of at most mostSourceBytes) and `line`, the line to mark.
	/// @return Screen::source() with `blame` (left out if the history can't say) and `marked` (`line`, else the
	/// source's own, else left out); or `{"error"}`.
	[[nodiscard]] std::string sourceAt(const json& arguments) const {
		const std::optional<std::string> at = placeOf(arguments, "at");
		if(!at) return errorAnswer("expected a place to show: \"#id\", a JSON Pointer or a definition's address");
		try {
			json answer = screen.source(*at);
			if(json blame = blameOf(screen.versions(), answer.value("address", std::string())); !blame.is_null())
				answer["blame"] = std::move(blame);
			const json line = lineOf(arguments);
			if(!line.is_null()) answer["marked"] = line;
			else if(answer.contains("line")) answer["marked"] = answer["line"];
			return answer.dump(-1, ' ', false, json::error_handler_t::replace);
		} catch(const affresco::DocumentError& error) {
			return errorAnswer(error.what());
		}
	}

	/// @brief What undoing an applied change would do, as the person, worked out without changing anything.
	/// @param arguments `commit`, and the choices made so far: `drop` and `keep` (choicesOf()).
	/// @return The plan (toJson(UndoPlan)), or `{"error"}` for an unknown commit or a malformed choice.
	[[nodiscard]] std::string planUndo(const json& arguments) const {
		try {
			const affresco::UndoOptions options = choicesOf(arguments, "changes.undoPlan");
			const affresco::UndoPlan plan =
			    screen.versions().undoPlan(arguments.value("commit", json()), undoAuthor, options);
			return affresco::toJson(plan).dump(-1, ' ', false, json::error_handler_t::replace);
		} catch(const affresco::DocumentError& error) {
			return errorAnswer(error.what());
		}
	}

	/// @brief Undoes an applied change, as a new commit by the person, keeping every later change but those the
	/// choices take out.
	/// @param arguments `commit`, and the choices made on its plan: `drop`, `keep` and `head` (choicesOf()). Without
	/// them, a plain undo that keeps everything after it.
	/// @return Why not, or nothing.
	/// @throws DocumentError If it can't be undone as asked, or a choice is malformed.
	[[nodiscard]] std::string undo(const json& arguments) const {
		affresco::VersionControl& versions = screen.versions();
		const affresco::UndoOptions options = choicesOf(arguments, "changes.undo");
		const json revision = versions.undoRevision(arguments.value("commit", json()), undoAuthor, options);
		const std::optional<std::string> refused = versions.commit(revision);
		return refused.value_or("");
	}

	/// @brief Asks the app's own assistant to undo a change another way, in the person's name: the agents answer
	/// with a proposal, which the person reviews like any other. Nothing is decided here.
	/// @param arguments `text`, the request (askedTextOf()).
	/// @return `{}`, or `{"error"}` without a text or without an assistant.
	[[nodiscard]] std::string askAssistant(const json& arguments) const {
		const std::optional<std::string> text = askedTextOf(arguments);
		if(!text)
			return errorAnswer("expected a request of 1 to " + std::to_string(mostAskBytes) +
			                   " bytes for the assistant");
		if(!canAsk()) return errorAnswer("this app has no assistant");
		screen.runAction(assistantAction, mainWindow.root(), json{{"text", *text}}.dump());
		return json::object().dump();
	}

	/// @brief Gives the window's screen what it calls. Each runs on the main thread, so it may use the app's screen;
	/// it reads and decides only through version control, which judges it as the person's own.
	void registerCalls() {
		own->onCall("changes.list", [this](const std::string&) { return list().dump(); });
		own->onCall("changes.load", [this](const std::string& text) {
			return screen.fileChanges(argumentsOf(text)).dump(-1, ' ', false, json::error_handler_t::replace);
		});
		own->onCall("changes.source", [this](const std::string& text) { return sourceAt(argumentsOf(text)); });
		own->onCall("changes.preview", [this](const std::string& text) {
			const json arguments = argumentsOf(text);
			const int id = arguments.value("proposal", 0);
			if(screen.previewedProposal() == id) {
				screen.endPreview();
				return json::object().dump();
			}
			std::string error;
			return screen.previewProposal(id, &error) ? json::object().dump() : errorAnswer(error);
		});
		own->onCall("changes.accept", [this](const std::string& text) {
			const json arguments = argumentsOf(text);
			if(screen.previewedProposal()) screen.endPreview();
			std::string error;
			const std::string head = arguments.value("head", std::string());
			return screen.acceptProposal(arguments.value("proposal", 0), head, &error) ? json::object().dump()
			                                                                           : errorAnswer(error);
		});
		own->onCall("changes.reject", [this](const std::string& text) {
			const json arguments = argumentsOf(text);
			if(screen.previewedProposal()) screen.endPreview();
			screen.rejectProposal(arguments.value("proposal", 0), arguments.value("reason", std::string()));
			return json::object().dump();
		});
		own->onCall("changes.resolve", [this](const std::string& text) {
			const json arguments = argumentsOf(text);
			if(screen.previewedProposal()) screen.endPreview();
			const std::optional<std::string> refused =
			    screen.versions().resolveLeftOut(arguments.value("proposal", 0), arguments.value("path", std::string()),
			                                     arguments.value("take", std::string()) == "theirs", undoAuthor);
			return refused ? errorAnswer(*refused) : json::object().dump();
		});
		registerUndoCalls();
	}

	/// @brief Gives the window's screen its undo calls: the plan, the undo as the person chose it, and the request to
	/// the app's assistant. Errors are answers the window shows, never exceptions.
	void registerUndoCalls() {
		own->onCall("changes.undoPlan", [this](const std::string& text) { return planUndo(argumentsOf(text)); });
		own->onCall("changes.undo", [this](const std::string& text) {
			try {
				const std::string refused = undo(argumentsOf(text));
				return refused.empty() ? json::object().dump() : errorAnswer(refused);
			} catch(const affresco::DocumentError& error) {
				return errorAnswer(error.what());
			}
		});
		own->onCall("changes.askUndo", [this](const std::string& text) { return askAssistant(argumentsOf(text)); });
	}
};

ChangesWindow::ChangesWindow(affresco::Application& app, affresco::Screen& screen, affresco::Window& window,
                             std::filesystem::path file)
    : impl_(
          std::make_unique<Impl>(Impl{app, screen, window, std::move(file), {}, nullptr, nullptr, 0, nullptr, 0, {}})) {
	screen.onCall("changes.available", [](const std::string&) { return std::string("true"); });
	Impl* impl = impl_.get();
	screen.onCall("changes.open", [impl](const std::string& text) {
		impl->open(argumentsOf(text));
		return std::string("null");
	});
	impl->keys = window.onKey.connect([impl](const affresco::KeyEvent& event) {
#ifdef __APPLE__
		const bool command = event.mods.super();
#else
		const bool command = event.mods.control();
#endif
		if(event.key == affresco::Key::D && event.action == affresco::Action::Press && command && event.mods.shift())
			impl->open(json::object());
	});
}

ChangesWindow::~ChangesWindow() { impl_->mainWindow.onKey.disconnect(impl_->keys); }

affresco::Screen* ChangesWindow::opened() const noexcept { return impl_->own ? &*impl_->own : nullptr; }

} // namespace host
