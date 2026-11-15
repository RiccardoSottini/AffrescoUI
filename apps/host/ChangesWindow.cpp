/// @file
/// @brief host::ChangesWindow: the host's window on a change.

#include "ChangesWindow.hpp"
#include "HostScreens.hpp"

#include <affresco/core/Log.hpp>
#include <affresco/format/Locale.hpp>

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
	/// @return `{"selection", "selectionCount", "source", "sourceCount", "previewing", "proposals", "history"}`.
	[[nodiscard]] json list() const {
		if(mainWindow.shouldClose() && window != nullptr) window->setShouldClose(true); // it goes with its app
		json proposals = json::array();
		for(const affresco::ProposalInfo& p : screen.proposals())
			proposals.push_back({{"id", p.id}, {"head", p.head}, {"author", p.author}, {"message", p.message}});
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
		        {"history", std::move(history)}};
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

	/// @brief Undoes an applied change, keeping everything after it, as a new commit by the person.
	/// @param arguments `commit`.
	/// @return Why not, or nothing.
	[[nodiscard]] std::string undo(const json& arguments) const {
		affresco::VersionControl& versions = screen.versions();
		const json revision = versions.revertRevision(arguments.value("commit", json()), "person");
		const std::optional<std::string> refused = versions.commit(revision);
		return refused.value_or("");
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
		own->onCall("changes.undo", [this](const std::string& text) {
			try {
				const std::string refused = undo(argumentsOf(text));
				return refused.empty() ? json::object().dump() : errorAnswer(refused);
			} catch(const affresco::DocumentError& error) {
				return errorAnswer(error.what());
			}
		});
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
