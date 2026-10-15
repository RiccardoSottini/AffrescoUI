#include "sgl/Screen.hpp"

#include "sgl/channel/Session.hpp"
#include "sgl/channel/Sockets.hpp"
#include "sgl/format/Rules.hpp"
#include "sgl/format/Runtime.hpp"
#include "sgl/vcs/Repository.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace sgl {

/// @brief A program connected to the screen.
struct Peer {
	std::unique_ptr<Channel> owned;            ///< Its channel, if it came through a socket.
	Channel* channel = nullptr;                ///< Its channel.
	std::unique_ptr<channel::Session> session; ///< Its protocol session.
};

/// @brief The modules and the peers.
///
/// Declaration order is construction order: rules, then the repository that uses them, then the runtime and the
/// peers that use the repository (destroyed the other way round; servers stop before their peers go).
struct Screen::Impl {
	format::Rules rules;                                         ///< The format's rules.
	vcs::Repository repository;                                  ///< Document, state, history, review.
	format::Runtime runtime;                                     ///< Panels and actions.
	std::vector<std::unique_ptr<Peer>> peers;                    ///< Connected programs.
	std::vector<std::unique_ptr<channel::SocketServer>> servers; ///< From listen().
	Signal<const std::string&> events;                           ///< "emit" events, as JSON text.
	std::shared_ptr<bool> alive = std::make_shared<bool>(true); ///< Guards removals still queued after destruction.

	/// @brief Builds the modules and forwards runtime events and errors to every peer.
	/// @param document The screen.
	/// @param baseDirectory Where relative paths are resolved from.
	/// @throws DocumentError If the screen is invalid.
	Impl(json document, std::filesystem::path baseDirectory)
	    : repository(rules, std::move(document)), runtime(repository, std::move(baseDirectory)) {
		runtime.onEvent.connect([this](const json& event) {
			broadcast(event);
			events(event.dump());
		});
		runtime.onError.connect([this](const std::string& message) {
			broadcast({{"type", "error"}, {"message", message}, {"revision", repository.head().number}});
		});
	}

	~Impl() {
		*alive = false;
		servers.clear();
	}

	/// @brief Sends a message to every peer.
	/// @param message The message.
	/// @param except A peer to skip (may be nullptr).
	void broadcast(const json& message, const Peer* except = nullptr) {
		for(const auto& peer : peers)
			if(peer.get() != except) peer->session->send(message);
	}

	/// @brief Adds a peer and relays its "request"/"reply" messages to the others.
	/// @param channel Its channel.
	/// @param owned Its channel, if the screen owns it (may be null).
	/// @param author The name it is authored as.
	/// @return The peer.
	Peer& add(Channel& channel, std::unique_ptr<Channel> owned, std::string author) {
		auto peer = std::make_unique<Peer>();
		peer->owned = std::move(owned);
		peer->channel = &channel;
		peer->session = std::make_unique<channel::Session>(channel, repository, std::move(author));

		// A request in plain words from one peer (e.g. the user's client) is for the others (e.g. an AI agent)
		peer->session->onRelay.connect([this, sender = peer.get()](const json& message) {
			if(peers.size() < 2) {
				sender->session->send({{"type", "error"},
				                       {"message", "nobody else is connected to receive this " +
				                                       message.value("type", std::string("message"))},
				                       {"id", message.value("id", json())}});
				return;
			}
			broadcast(message, sender);
		});
		peers.push_back(std::move(peer));
		return *peers.back();
	}

	/// @brief Removes a peer that went away.
	/// @param peer The peer.
	void remove(const Peer* peer) {
		std::erase_if(peers, [peer](const auto& p) { return p.get() == peer; });
	}
};

namespace {

/// @brief Parses JSON text.
/// @param text The text.
/// @param what What it is, for the error.
/// @param error Receives the error, if any (may be nullptr).
/// @return The value, or std::nullopt.
std::optional<json> parseJson(std::string_view text, const char* what, std::string* error) {
	try {
		return json::parse(text);
	} catch(const json::parse_error& parseError) {
		if(error) *error = std::string("sGLGUI screen: invalid ") + what + " JSON: " + parseError.what();
		return std::nullopt;
	}
}

/// @brief Turns an optional error into a bool.
/// @param failure The error, if any.
/// @param error Receives it (may be nullptr).
/// @return true if there was no error.
bool succeeded(const std::optional<std::string>& failure, std::string* error) {
	if(failure && error) *error = *failure;
	return !failure;
}

} // namespace

Screen::Screen(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Screen::Screen(Screen&&) noexcept = default;
Screen& Screen::operator=(Screen&&) noexcept = default;
Screen::~Screen() = default;

Screen Screen::fromFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	if(!file) {
		throw ScreenError(
		    "sGLGUI screen: could not open " + std::filesystem::absolute(path).string() +
		    " (relative paths start from the current folder: " + std::filesystem::current_path().string() + ")");
	}

	std::stringstream content;
	content << file.rdbuf();
	return fromString(content.str(), path.parent_path());
}

Screen Screen::fromString(std::string_view content, std::filesystem::path baseDirectory) {
	json document;
	try {
		document = json::parse(content);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("sGLGUI screen: invalid JSON: ") + error.what());
	}
	return Screen(std::make_unique<Impl>(std::move(document), std::move(baseDirectory)));
}

/* Runtime */

void Screen::registerType(std::string name, Factory factory) {
	impl_->runtime.registerType(std::move(name), std::move(factory));
}

void Screen::setPlaceholderTypes(bool enabled) { impl_->runtime.setPlaceholderTypes(enabled); }

Window& Screen::createWindow(Application& app) { return impl_->runtime.createWindow(app); }

void Screen::build(Window& window) { impl_->runtime.build(window); }

Panel& Screen::instantiate(std::string_view name, Panel& parent) { return impl_->runtime.instantiate(name, parent); }

void Screen::onAction(std::string name, Action action) { impl_->runtime.onAction(std::move(name), std::move(action)); }

void Screen::onUnhandledAction(UnhandledAction fallback) { impl_->runtime.onUnhandledAction(std::move(fallback)); }

void Screen::runAction(std::string_view name, Panel& sender, std::string_view eventJson) {
	json event;
	try {
		event = json::parse(eventJson);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("sGLGUI screen: runAction: invalid event JSON: ") + error.what());
	}
	impl_->runtime.runAction(name, sender, event);
}

Panel* Screen::find(std::string_view id) noexcept { return impl_->runtime.find(id); }

Color Screen::color(std::string_view name) const { return impl_->runtime.color(name); }

std::shared_ptr<const Font> Screen::font(std::string_view name) const { return impl_->runtime.font(name); }

Connection Screen::onEvent(std::function<void(const std::string&)> listener) {
	return impl_->events.connect(std::move(listener));
}

/* Version control */

bool Screen::applyRevision(std::string_view revisionJson, std::string* error) {
	const auto revision = parseJson(revisionJson, "revision", error);
	return revision && succeeded(impl_->repository.commit(*revision), error);
}

RevisionResult Screen::submitRevision(std::string_view revisionJson) {
	std::string error;
	const auto parsed = parseJson(revisionJson, "revision", &error);
	if(!parsed) return {RevisionStatus::Rejected, revision(), 0, error};
	return impl_->repository.submit(*parsed);
}

std::vector<ProposalInfo> Screen::proposals() const { return impl_->repository.proposals(); }

bool Screen::acceptProposal(int id, std::string* error) { return succeeded(impl_->repository.accept(id), error); }

bool Screen::rejectProposal(int id, std::string_view reason) { return impl_->repository.reject(id, reason); }

bool Screen::previewProposal(int id, std::string* error) { return succeeded(impl_->repository.preview(id), error); }

void Screen::endPreview() { impl_->repository.endPreview(); }

std::optional<int> Screen::previewedProposal() const noexcept { return impl_->repository.previewing(); }

std::vector<HistoryEntry> Screen::history() const { return impl_->repository.history(); }

bool Screen::restore(int revision, std::string* error) {
	try {
		return succeeded(impl_->repository.commit(impl_->repository.restoreRevision(revision, "user")), error);
	} catch(const DocumentError& failure) {
		return succeeded(failure.what(), error);
	}
}

RevisionResult Screen::submitRestore(int revision, std::string_view author) {
	try {
		return impl_->repository.submit(impl_->repository.restoreRevision(revision, author));
	} catch(const DocumentError& failure) {
		return {RevisionStatus::Rejected, this->revision(), 0, failure.what()};
	}
}

bool Screen::undo(std::string* error) {
	const auto entries = history();
	if(entries.size() < 2) return succeeded("sGLGUI screen: nothing to undo", error);
	return restore(entries[entries.size() - 2].number, error);
}

int Screen::revision() const noexcept { return impl_->repository.head().number; }

std::string Screen::exportChanges(int fromRevision, std::string_view name) const {
	return impl_->repository.exportChanges(fromRevision, name).dump(2);
}

bool Screen::importChanges(std::string_view changesJson, std::string* error) {
	const auto changes = parseJson(changesJson, "changes", error);
	return changes && succeeded(impl_->repository.importChanges(*changes), error);
}

std::string Screen::documentJson() const { return impl_->repository.document().dump(); }

std::string Screen::stateJson() const { return impl_->repository.state().dump(); }

VersionControl& Screen::versions() noexcept { return impl_->repository; }

/* Channel */

void Screen::connect(Channel& channel, std::string author) { impl_->add(channel, nullptr, std::move(author)); }

void Screen::listen(Executor& executor, const std::filesystem::path& path, std::string author) {
	Impl* impl = impl_.get();
	impl->servers.push_back(std::make_unique<channel::SocketServer>(
	    executor, path, [impl, &executor, author](std::unique_ptr<Channel> connection) {
		    Channel& channel = *connection;
		    Peer& peer = impl->add(channel, std::move(connection), author);
		    // Gone: removed after the signal that says so has finished
		    channel.onClosed.connect([impl, &executor, peer = &peer] {
			    executor.post([impl, peer, alive = impl->alive] {
				    if(*alive) impl->remove(peer);
			    });
		    });
	    }));
}

void Screen::setAutoAccept(bool enabled) { impl_->repository.setAutoAccept(enabled); }

} // namespace sgl
