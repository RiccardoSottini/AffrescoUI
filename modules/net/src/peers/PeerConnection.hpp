#pragma once

/// @file
/// @brief One remote peer's connection on the server side: the upgrade, the token, then the live protocol as text
/// frames, as a core Channel (RFC 0003).

#include "Tcp.hpp"
#include "websocket/WebSocketFrames.hpp"

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/net/RemotePeers.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace affresco::net {

/// @brief A remote peer's connection: its reader thread opens it (upgrade, Origin, token) and then reads frames; its
/// writer thread sends what is queued. Messages and the close reach the main thread through the executor.
///
/// While it opens, only the reader writes (the 101 answer, pongs, a refusal); once open, every write goes through the
/// writer's bounded queue.
class PeerConnection final : public Channel {
public:
	/// Called once on the reader thread when the opening ends: with the author, or nullopt if the peer was refused.
	using Opened = std::function<void(PeerConnection& connection, std::optional<std::string> author)>;

	/// @brief What every connection of a server shares (it may outlive the server: accepted peers do).
	struct Shared {
		Executor& executor;                             ///< Delivers messages; outlives every connection.
		std::shared_ptr<TokenVerifier> verifier;        ///< Checks tokens.
		PeerServerOptions options;                      ///< Limits and origins.
		std::shared_ptr<std::atomic<std::size_t>> open; ///< Connections open; each counts until destroyed.
	};

	/// @brief Starts opening the connection.
	/// @param shared What the server's connections share.
	/// @param connection The accepted connection (already counted in shared->open).
	/// @param opened Called when the opening ends.
	PeerConnection(std::shared_ptr<const Shared> shared, std::unique_ptr<tcp::Connection> connection, Opened opened);
	/// @brief Closes the connection and waits for its threads (on the main thread: nothing it posted runs after).
	~PeerConnection() override;
	PeerConnection(const PeerConnection&) = delete;
	PeerConnection& operator=(const PeerConnection&) = delete;
	PeerConnection(PeerConnection&&) = delete;
	PeerConnection& operator=(PeerConnection&&) = delete;

	/// @brief Sends one message as a text frame (from the main thread); going over maxQueuedBytes closes the peer.
	/// @param message The message.
	void send(std::string_view message) override;

private:
	using Clock = std::chrono::steady_clock; ///< For the authentication deadline.

	/// @brief The reader thread: opens the connection, then reads frames until it ends, then posts onClosed.
	void readLoop();
	/// @brief Reads the upgrade request, checks it, its Origin and a bearer token, and answers 101.
	/// @param deadline When the token must have come.
	/// @return The author, or nullopt (refused, or gone).
	std::optional<std::string> open(Clock::time_point deadline);
	/// @brief Waits for the first message, `{"type": "auth", "token": ...}`, and checks its token.
	/// @param deadline When it must have come.
	/// @return The author, or nullopt (refused, or gone).
	std::optional<std::string> authenticate(Clock::time_point deadline);
	/// @brief Handles one frame while waiting for the "auth" message.
	/// @param event The frame.
	/// @param author Receives the author when the token is accepted.
	/// @return Whether the opening ended (author set, or refused).
	bool authEvent(const FrameEvent& event, std::optional<std::string>& author);
	/// @brief Asks the verifier (an exception or an empty author refuses).
	/// @param token The token.
	/// @return The author, or nullopt.
	std::optional<std::string> verify(const std::string& token);
	/// @brief Refuses the upgrade with an HTTP status, logs why, and closes.
	/// @param status The status.
	/// @param reason Why (sent and logged).
	/// @return nullopt.
	std::optional<std::string> refuse(int status, const std::string& reason);
	/// @brief Refuses after the upgrade with a close frame, logs why, and closes.
	/// @param code The close code.
	/// @param reason Why (sent and logged).
	/// @return nullopt.
	std::optional<std::string> closeWith(std::uint16_t code, const std::string& reason);
	/// @brief Waits for bytes (appended to input_), until a deadline or close.
	/// @param deadline The deadline, or nullopt to wait until the connection ends.
	/// @return Data, TimedOut (the deadline passed), or Closed/Failed.
	tcp::Received receive(std::optional<Clock::time_point> deadline);
	/// @brief Moves the bytes received into the frame reader.
	void feedFrames();
	/// @brief Handles the frames read so far, once open.
	/// @return Whether to go on reading.
	bool handleFrames();
	/// @brief Handles one frame once open.
	/// @param event The frame.
	/// @return Whether to go on reading.
	bool handle(FrameEvent event);
	/// @brief Queues bytes for the writer.
	/// @param bytes The bytes.
	/// @param last Close the socket once they are sent.
	void queue(std::string bytes, bool last = false);
	/// @brief The writer thread: sends what is queued, in order.
	void writeLoop();
	/// @brief Runs a task on the main thread, unless the connection is gone by then.
	/// @param task The task.
	void post(std::function<void(PeerConnection&)> task);

	std::shared_ptr<const Shared> shared_;        ///< See the constructor.
	std::unique_ptr<tcp::Connection> connection_; ///< The socket.
	Opened opened_;                               ///< See the constructor.
	FrameReader frames_;                          ///< Reads the peer's (masked) frames.
	std::string input_;                           ///< Bytes received and not yet read (reader thread).

	std::mutex mutex_;               ///< Guards the queue and the flags below.
	std::condition_variable wake_;   ///< Wakes the writer.
	std::deque<std::string> output_; ///< Bytes to send, in order.
	std::size_t queuedBytes_ = 0;    ///< Their size.
	bool finishing_ = false;         ///< Close the socket once the queue is sent (a close frame is last).
	bool stopping_ = false;          ///< The destructor runs: send nothing more.

	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
	std::thread writer_;                                         ///< Runs writeLoop().
	std::thread reader_;                                         ///< Runs readLoop(); started last.
};

} // namespace affresco::net
