#pragma once

/// @file
/// @brief affresco::Screen::Impl, the screen's modules and peers, shared by the files that make up affresco::Screen.

#include "affresco/Screen.hpp"
#include "pictures/OffscreenImages.hpp"

#include "affresco/channel/Session.hpp"
#include "affresco/channel/Sockets.hpp"
#include "affresco/data/Store.hpp"
#include "affresco/format/Rules.hpp"
#include "affresco/format/Runtime.hpp"
#include "affresco/net/RemotePeers.hpp"
#include "affresco/vcs/ChangeSetRepair.hpp"
#include "affresco/vcs/Repository.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace affresco {

namespace detail {

/// @brief The executor of the Screen's network: it forwards to the Screen's current executor, which the host may set
/// after the network is made. Threads: post() from any, set() from the main thread.
class RelayExecutor final : public Executor {
public:
	/// @brief Forwards a task to the current executor, or drops it if there is none.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		if(target_) target_->post(std::move(task));
	}
	/// @brief Sets the executor tasks go to.
	/// @param target The executor, or nullptr.
	void set(Executor* target) noexcept {
		const std::scoped_lock lock(mutex_);
		target_ = target;
	}

private:
	std::mutex mutex_;           ///< Guards target_.
	Executor* target_ = nullptr; ///< See set().
};

} // namespace detail

/// @brief The modules and the peers.
///
/// Declaration order is construction order: the services, the rules, then the repository that uses them, then the
/// runtime and the peers that use the repository (destroyed the other way round; servers stop before their peers go,
/// and the runtime lets go of the services before they end).
struct Screen::Impl {
	/// @brief A program connected to the screen.
	struct Peer {
		std::unique_ptr<Channel> owned;            ///< Its channel, if it came through a socket.
		Channel* channel = nullptr;                ///< Its channel.
		std::unique_ptr<channel::Session> session; ///< Its protocol session.
	};

	/// @brief A channel the file declares: a program the screen talks to over a local socket, or a remote peer.
	struct DeclaredChannel {
		std::string name;                               ///< Its name in the file: actions "<name>:<type>".
		std::filesystem::path path;                     ///< The socket.
		double reconnectMs = 0.0;                       ///< Wait before connecting again (0: never).
		std::unique_ptr<channel::SocketChannel> socket; ///< While connected.
		std::uint64_t retry = 0;                        ///< The reconnect timer, if waiting.
		std::string url;                 ///< A remote peer's ws:// or wss:// address instead of the socket, or "".
		std::string token;               ///< The token for url, from the host (never from the document).
		std::unique_ptr<Channel> remote; ///< While connected to url.
	};

	format::Features features;                           ///< Catalogues and publishers (RFC 0004); outlives the rest.
	std::optional<format::AppManifest> app;              ///< The app it was loaded from (Screen::fromApp()).
	services::ServiceRegistry services;                  ///< What "services" may use (RFC 0002).
	detail::RelayExecutor relay;                         ///< The network's executor.
	std::unique_ptr<net::Client> network;                ///< See Screen::network(); outlives the runtime.
	std::unique_ptr<net::SignIn> signIn;                 ///< The connections' "auth"; outlives the runtime.
	std::shared_ptr<audio::Engine> audio;                ///< See Screen::audio(); posts to relay.
	std::unique_ptr<data::Store> store;                  ///< The records' file (setDataFile()); outlives the runtime.
	format::Rules rules;                                 ///< The format's rules.
	vcs::Repository repository;                          ///< Document, state, history, review.
	vcs::ChangeSetRepairs repairs{repository, "screen"}; ///< Installed change sets and their repairs.
	format::Runtime runtime;                             ///< Panels and actions.
	detail::OffscreenImages images{repository, runtime}; ///< Pictures for peers ("render").
	std::vector<std::unique_ptr<Peer>> peers;            ///< Connected programs.
	std::vector<std::unique_ptr<channel::SocketServer>> servers; ///< From listen().
	std::vector<std::unique_ptr<net::PeerServer>> peerServers;   ///< From listenWebSocket().
	Signal<const std::string&> events;                           ///< "emit" events, as JSON text.
	std::shared_ptr<bool> alive = std::make_shared<bool>(true);  ///< Guards removals still queued after destruction.

	Executor* executor = nullptr;                    ///< For channels (set by startChannels()).
	std::filesystem::path socketDirectory;           ///< Where relative channel sockets are.
	std::map<std::string, DeclaredChannel> channels; ///< From "channels".
	std::filesystem::path stateFile;                 ///< Where "persist" paths are kept.
	json saved;                                      ///< What was last loaded or written there.

	/// @brief Builds the modules and forwards runtime events and errors to every peer.
	/// @param document The screen.
	/// @param baseDirectory Where relative paths are resolved from.
	/// @throws DocumentError If the screen is invalid.
	Impl(json document, std::filesystem::path baseDirectory);

	Impl(const Impl&) = delete;            ///< Callbacks hold `this`: it never copies.
	Impl& operator=(const Impl&) = delete; ///< Callbacks hold `this`: it never copies.
	Impl(Impl&&) = delete;                 ///< Callbacks hold `this`: it never moves (the Screen moves its pointer).
	Impl& operator=(Impl&&) = delete;      ///< Callbacks hold `this`: it never moves.

	/// @brief Stops the servers and the reconnect timers before the peers and modules go.
	~Impl();

	/// @brief Logs what the screen refers to but doesn't define, as warnings: the screen still opens. Called when it
	/// is first built, once the application has registered its actions.
	void warnOfFindings() const;

	/* Channels (ScreenChannels.cpp) */

	/// @brief Runs a channel's action if the file has it ("host:committed" for a "committed" message on "host").
	/// @param action The action.
	/// @param event Its event.
	void run(const std::string& action, const json& event);

	/// @brief Connects a declared channel; on failure runs "<name>:closed" and tries again later if it should.
	/// @param channel The channel.
	void open(DeclaredChannel& channel);

	/// @brief Connects a declared channel to its socket or remote peer; on failure runs "<name>:closed".
	/// @param channel The channel, with no connection.
	/// @return Whether it connected.
	bool tryConnect(DeclaredChannel& channel);

	/// @brief Runs a newly connected channel's actions: "<name>:open" now, "<name>:<type>" for each message, and
	/// "<name>:closed" (then a reconnect, if it should) when it closes.
	/// @param channel The channel, connected.
	void watch(DeclaredChannel& channel);

	/// @brief Schedules a reconnect, if the channel has "reconnect".
	/// @param channel The channel.
	void retry(DeclaredChannel& channel);

	/// @brief Sends a message on a declared channel ("send" steps); if it isn't connected, runs "<name>:error".
	/// @param name The channel.
	/// @param message The message.
	void send(const std::string& name, const json& message);

	/* Persistence */

	/// @brief The "persist" paths as JSON Pointers into the state: "state.habits" is "/habits".
	/// @return The pointers.
	std::vector<json::json_pointer> persisted() const;

	/// @brief The persisted part of the live state.
	/// @return {pointer: value}.
	json persistedState();

	/// @brief Reads the state file into the live state (the paths it has).
	void load();

	/// @brief Writes the persisted part of the state if it changed since it was loaded or last written.
	void save();

	/// @brief Why a value kept on disk no longer fits the document's "stateTypes" (ScreenData.cpp).
	/// @param pointer Its place in the state.
	/// @param value The value.
	/// @return The problem, or std::nullopt if it fits (or has no declared type).
	[[nodiscard]] std::optional<std::string> persistedMismatch(const json::json_pointer& pointer,
	                                                           const json& value) const;

	/// @brief Keeps the values kept on disk that no longer fit in `<name>.state.rejected.json`, beside the state
	/// file, so none is lost: the screen opens with its defaults for them (ScreenData.cpp).
	/// @param rejected `{pointer: {"value", "problem"}}`.
	void keepRejected(const json& rejected) const;

	/* Peers */

	/// @brief Sends a message to every peer.
	/// @param message The message.
	/// @param except A peer to skip (may be nullptr).
	void broadcast(const json& message, const Peer* except = nullptr);

	/// @brief Sends a peer a message about change set repairs. One whose data rules let it read only some paths gets it
	/// without the revisions, proof and failures, which can show the values hidden from it.
	/// @param peer The peer.
	/// @param message A repair request, or the repairs' status.
	void sendRepairs(const Peer& peer, json message) const;

	/// @brief Adds a peer and relays its "request"/"reply" messages to the others.
	/// @param channel Its channel.
	/// @param owned Its channel, if the screen owns it (may be null).
	/// @param author The name it is authored as.
	/// @return The peer.
	Peer& add(Channel& channel, std::unique_ptr<Channel> owned, std::string author);

	/// @brief Adds a peer whose channel the screen owns, and removes it once its channel closes.
	/// @param mainThread Runs the removal after the signal that says so has finished.
	/// @param connection The channel.
	/// @param name The name it is authored as.
	void adopt(Executor& mainThread, std::unique_ptr<Channel> connection, std::string name);

	/// @brief Removes a peer that went away.
	/// @param peer The peer.
	void remove(const Peer* peer);
};

} // namespace affresco
