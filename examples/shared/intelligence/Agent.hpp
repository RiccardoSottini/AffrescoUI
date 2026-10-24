#pragma once

/// @file
/// @brief ProviderAgent: an agent inside the host, connected to its screen as a peer (author "agent"), that answers
/// the requests of the app's own chat (the `ask` step, the assistant add-on) with the provider of intelligence the
/// person set up in the AffrescoUI Launcher. Its changes are revisions like any agent's: proposals that wait for the
/// person's review, never accepted by it.

#include "intelligence/ClaudeCode.hpp"
#include "intelligence/Http.hpp"
#include "intelligence/Settings.hpp"

#include <affresco/core/Channel.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/core/SecretStore.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace examples::intelligence {

/// @brief Runs the claude-code provider once: (model, system prompt, input) to its answer.
using ClaudeRunner = std::function<ClaudeAnswer(const std::string& model, const std::string& system,
                                                const std::string& input, const std::function<bool()>& stopped)>;

/// @brief The bounds and the parts an agent works with.
struct AgentOptions {
	std::filesystem::path dataDir;                    ///< Where intelligence.json is (read at each request).
	Catalog catalog;                                  ///< The providers.
	std::string guide;                                ///< SCREEN_FORMAT.md's text (readFormatGuide()).
	std::shared_ptr<affresco::SecretStore> keys;      ///< The keys (systemKeys()).
	NetworkFactory network = systemNetwork();         ///< Makes the HTTP client of a request.
	ClaudeRunner claude;                              ///< Runs claude-code; empty: the installed command line.
	int maxSteps = 12;                                ///< The most answers of the model per request.
	int maxRevisions = 6;                             ///< The most revisions per request.
	long long maxOutputTokens = 60000;                ///< The most tokens the model writes per request.
	std::chrono::milliseconds budget{240000};         ///< The longest a request may take.
	std::chrono::milliseconds screenTimeout{30000};   ///< The longest the screen may take to answer the agent.
	std::size_t inlineDocument = 32 * 1024;           ///< A screen this large or smaller is sent whole, else its outline.
};

/// @brief The agent: a Channel whose other end is the screen (Screen::connect(agent, "agent")).
///
/// Requests from the screen's own chat (`request` messages with `"source": "screen"`) are queued and worked on one at
/// a time on a thread of its own, so the window never waits: it reads the screen and submits revisions through the
/// live protocol (posted to the main thread), and calls the provider (an HTTP client allowing its origin only, or
/// the claude command line with no tools). Errors come back as the reply's `error`, without secrets.
class ProviderAgent final : public affresco::Channel {
public:
	/// @brief Starts the thread.
	/// @param executor The main thread (the Application), where messages to the screen are delivered; it outlives
	/// the agent.
	/// @param options The bounds and parts.
	ProviderAgent(affresco::Executor& executor, AgentOptions options);
	/// @brief Stops the request in progress and the thread.
	~ProviderAgent() override;
	ProviderAgent(const ProviderAgent&) = delete;
	ProviderAgent& operator=(const ProviderAgent&) = delete;
	ProviderAgent(ProviderAgent&&) = delete;
	ProviderAgent& operator=(ProviderAgent&&) = delete;

	/// @brief A message from the screen: a request is queued (at most 8 wait; more are answered "busy"), an answer to
	/// the agent's own question is handed to it, anything else is ignored. Main thread.
	/// @param message One JSON object.
	void send(std::string_view message) override;

	/// @brief The most requests that wait.
	static constexpr std::size_t maxQueued = 8;

private:
	/// @brief A request from the chat.
	struct Job {
		std::string id;   ///< The request's id, which the reply names.
		std::string text; ///< What the person asked.
	};

	/// @brief The thread: works on the jobs, one at a time.
	void work();
	/// @brief Works on one request and replies to it.
	/// @param job The request.
	void answer(const Job& job);
	/// @brief Asks the screen something and waits for its answer (the message with the same id). Worker thread.
	/// @param message The message (an id is added).
	/// @return The answer, or nullopt if none came in time or the agent stopped.
	std::optional<affresco::json> ask(affresco::json message);
	/// @brief Sends a message to the screen, on the main thread.
	/// @param message The message.
	void deliver(const affresco::json& message);
	/// @brief Whether the agent is stopping. Any thread.
	/// @return Whether it is.
	[[nodiscard]] bool stopping() const noexcept { return stop_; }

	affresco::Executor& executor_;                                 ///< The main thread.
	AgentOptions options_;                                         ///< See the constructor.
	std::mutex mutex_;                                             ///< Guards jobs_, answers_ and waiting_.
	std::condition_variable wake_;                                 ///< Told of a job, an answer or the stop.
	std::deque<Job> jobs_;                                         ///< Waiting.
	std::map<std::string, affresco::json> answers_;                ///< The screen's answers, by id.
	std::map<std::string, bool> waiting_;                          ///< The ids the worker waits for.
	std::atomic<bool> stop_ = false;                               ///< Set by the destructor.
	std::atomic<unsigned long> asked_ = 0;                         ///< Numbers the questions.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   ///< Guards deliveries queued after destruction.
	std::thread worker_;                                           ///< Works on the jobs.
	friend class AgentRun;
};

} // namespace examples::intelligence
