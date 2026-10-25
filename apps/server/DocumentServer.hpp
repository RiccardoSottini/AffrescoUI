#pragma once

/// @file
/// @brief DocumentServer: screen documents and their history kept on disk, shared over WebSocket with the people a
/// TokenFile names, with their permissions, their presence and their leases (apps/server/README.md).

#include "TokenFile.hpp"

#include <affresco/channel/Limits.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/net/RemotePeers.hpp>
#include <affresco/vcs/Leases.hpp>
#include <affresco/vcs/Presence.hpp>
#include <affresco/vcs/Repository.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::server {

/// @brief Serves the documents of a folder: `documents/<name>.screen.json`, each with its history in
/// `history/<name>`, and each person's own documents in `people/<person>/`.
///
/// A peer connects with a token, is told the documents (`{"type": "documents"}`), and opens one with
/// `{"type": "open", "document": "kanban"}` (a name starting with '~', e.g. "~settings", is the person's own, made
/// when first opened). From then on it speaks the live protocol (channel::Session) on that document, plus four
/// messages the server answers: "presence" (where it is), "getPresence", "lease" and "release". Everything runs on
/// the executor's thread.
class DocumentServer {
public:
	static constexpr std::size_t maxDocuments = 1024;      ///< Documents open at once, shared and personal.
	static constexpr std::size_t maxPersonalDocuments = 16; ///< One person's own documents ("~name") open at once.
	static constexpr std::size_t maxServerMessageBytes = 4096; ///< The longest open, presence or lease message.

	/// @brief Loads the documents and listens.
	/// @param executor The main thread; outlives the server.
	/// @param folder The folder (documents/, history/, people/).
	/// @param tokens Who may connect.
	/// @param address Where to listen, e.g. "127.0.0.1:7420".
	/// @param options The listener's limits and origins.
	/// @throws std::runtime_error If a document can't be read or listening fails.
	DocumentServer(Executor& executor, std::filesystem::path folder, const std::shared_ptr<TokenFile>& tokens,
	               std::string_view address, net::PeerServerOptions options = {});
	/// @brief Loads the documents and listens, admitting whom a verifier admits (e.g. a ChainVerifier of a TokenFile
	/// and a UserInfoVerifier).
	/// @param executor The main thread; outlives the server.
	/// @param folder The folder (documents/, history/, people/).
	/// @param verifier Who may connect (safe on any thread).
	/// @param people Everyone the verifier may admit, for the permission rules (called on the executor's thread).
	/// @param address Where to listen, e.g. "127.0.0.1:7420".
	/// @param options The listener's limits and origins.
	/// @throws std::runtime_error If a document can't be read or listening fails.
	DocumentServer(Executor& executor, std::filesystem::path folder, std::shared_ptr<net::TokenVerifier> verifier,
	               std::function<std::vector<std::string>()> people, std::string_view address,
	               net::PeerServerOptions options = {});
	/// @brief Stops listening and closes every peer.
	~DocumentServer();
	DocumentServer(const DocumentServer&) = delete;
	DocumentServer& operator=(const DocumentServer&) = delete;

	/// @brief The port it listens on.
	/// @return The port.
	[[nodiscard]] std::uint16_t port() const noexcept { return listener_->port(); }
	/// @brief The shared documents' names.
	/// @return The names, sorted.
	[[nodiscard]] std::vector<std::string> documents() const;
	/// @brief How many peers are connected.
	/// @return The count.
	[[nodiscard]] std::size_t peers() const noexcept { return peers_.size(); }

private:
	/// @brief A document being served.
	struct Document {
		/// @brief Sets up the rules; the repository comes after.
		/// @param base The format's rules.
		/// @param people Everyone who may connect.
		Document(const DocumentRules& base, std::function<std::vector<std::string>()> people)
		    : leases(base), rules(base, leases, std::move(people)) {}
		vcs::Leases leases;                          ///< Its leases.
		vcs::LeasedRules rules;                      ///< The format's rules plus the leases.
		vcs::Presence presence;                      ///< Who is on it.
		std::unique_ptr<vcs::Repository> repository; ///< It and its history (last: uses the rules).
	};

	/// @brief Forwards the live protocol between a Session and a peer's socket.
	class Inner final : public Channel {
	public:
		/// @brief Forwards to a socket.
		/// @param socket The socket.
		explicit Inner(Channel& socket) noexcept : socket_(socket) {}
		/// @brief Sends to the socket.
		/// @param message The message.
		void send(std::string_view message) override { socket_.send(message); }

	private:
		Channel& socket_; ///< The peer's socket.
	};

	/// @brief A connected peer. Its members go in reverse: the session, its channel, then the socket.
	struct Peer {
		int id = 0;                               ///< Its number, for leases and presence.
		std::string author;                       ///< Who it is, as its token says.
		std::unique_ptr<Channel> socket;          ///< The WebSocket.
		channel::TokenBucket rate;                ///< Its server messages (open, presence, lease, release).
		Document* document = nullptr;             ///< The document it opened, or nullptr.
		std::unique_ptr<Inner> inner;             ///< The Session's channel, once opened.
		std::unique_ptr<channel::Session> session; ///< The live protocol, once opened.
	};

	/// @brief Loads the shared documents of folder/documents.
	void loadDocuments();
	/// @brief Puts a document under version control, with its history in a folder.
	/// @param key Its key in documents_ ("kanban", or "~ada/settings").
	/// @param file Its screen file.
	/// @param history Its history's folder.
	/// @return The document.
	/// @throws std::exception If it can't be read or isn't valid.
	Document& load(const std::string& key, const std::filesystem::path& file, const std::filesystem::path& history);
	/// @brief Takes an authenticated peer.
	/// @param socket Its channel.
	/// @param author Who it is.
	void adopt(std::unique_ptr<Channel> socket, std::string author);
	/// @brief Handles a line from a peer: the server's own messages here, the rest to its Session.
	/// @param peer The peer.
	/// @param line The line.
	void handle(Peer& peer, const std::string& line);
	/// @brief Passes a line that isn't the server's own to the peer's Session, or answers that none is open yet.
	/// @param peer The peer.
	/// @param line The line.
	static void forward(Peer& peer, const std::string& line);
	/// @brief Handles a server message (presence, getPresence, lease, release) on an opened document.
	/// @param peer The peer.
	/// @param message The message.
	/// @param type Its type.
	void handleServerMessage(Peer& peer, const json& message, const std::string& type);
	/// @brief Opens a document for a peer.
	/// @param peer The peer.
	/// @param message `{"type": "open", "document": name}`.
	void open(Peer& peer, const json& message);
	/// @brief Finds or loads the document a name means for a person.
	/// @param name The name ("kanban", or "~settings" for the person's own).
	/// @param author The person.
	/// @return The document, or nullptr if there is none by that name.
	Document* find(const std::string& name, const std::string& author);
	/// @brief Takes or ends a lease.
	/// @param peer The peer.
	/// @param message `{"type": "lease", "target", "seconds"}` or `{"type": "release", "target"}`.
	/// @param type "lease" or "release".
	void lease(Peer& peer, const json& message, const std::string& type);
	/// @brief Sends every peer on a document its presence or leases.
	/// @param document The document.
	/// @param what "presence" or "leases".
	void broadcast(const Document& document, const std::string& what);
	/// @brief The presence or leases message of a document.
	/// @param document The document.
	/// @param what "presence" or "leases".
	/// @return The message.
	[[nodiscard]] static json state(const Document& document, const std::string& what);
	/// @brief Removes a peer that went away: its presence and leases end, and the others hear of it.
	/// @param peer The peer.
	void remove(const Peer* peer);

	Executor& executor_;                                      ///< The main thread.
	std::filesystem::path folder_;                            ///< See the constructor.
	std::function<std::vector<std::string>()> people_;        ///< Everyone who may connect.
	format::Rules rules_;                                     ///< The format's rules.
	std::map<std::string, std::unique_ptr<Document>> documents_; ///< By key.
	std::vector<std::unique_ptr<Peer>> peers_;                ///< The connected peers.
	int nextPeer_ = 1;                                        ///< The next peer's id.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< false once destroyed, for posted tasks.
	std::unique_ptr<net::PeerServer> listener_;               ///< Last: started once the rest is ready.
};

} // namespace affresco::server
