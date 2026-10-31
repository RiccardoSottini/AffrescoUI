/// @file
/// @brief host::ChangesWindow: the host's window on a change.

#include "ChangesWindow.hpp"

#include <affresco/core/Log.hpp>
#include <affresco/format/Locale.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace host {

using affresco::json;

namespace {

/// How many applied changes the window lists, newest first.
constexpr std::size_t mostCommits = 50;

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
	affresco::Connection keys = 0;       ///< The shortcut on the app's window.

	/// @brief Opens the window, or brings it back, on a change.
	/// @param arguments `proposal`, if the proposal is open; otherwise the newest applied change.
	void open(const json& arguments) {
		const std::vector<affresco::ProposalInfo> proposals = screen.proposals();
		const auto isOpen = [&](const affresco::ProposalInfo& p) {
			return json(p.id) == arguments.value("proposal", json());
		};
		if(std::ranges::any_of(proposals, isOpen)) selection = {{"proposal", arguments["proposal"]}};
		else if(!proposals.empty()) selection = {{"proposal", proposals.back().id}};
		else selection = {{"to", "HEAD"}};
		++selectionCount;
		if(window != nullptr) {
			window->setShouldClose(false);
			window->setVisible(true);
			return;
		}
		std::ifstream in(file, std::ios::binary);
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if(!in) throw std::runtime_error("could not open " + file.string());
		own.emplace(affresco::Screen::fromString(text, file.parent_path()));
		own->setLocale(affresco::format::systemLocale());
		registerCalls();
		window = &own->createWindow(app, mainWindow);
	}

	/// @brief What the window lists, and what it should show.
	/// @return `{"selection", "selectionCount", "previewing", "proposals", "history"}`.
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
		        {"previewing", previewed ? json(*previewed) : json()},
		        {"proposals", std::move(proposals)},
		        {"history", std::move(history)}};
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
    : impl_(std::make_unique<Impl>(Impl{app, screen, window, std::move(file), {}, nullptr, nullptr, 0, {}})) {
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
