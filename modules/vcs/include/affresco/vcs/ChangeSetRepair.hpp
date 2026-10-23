#pragma once

/// @file
/// @brief vcs::ChangeSetRepairs: change sets that carry their request, base and proof, checked when they are installed,
/// and repaired by an agent through review when the template under them changed (VERSION_CONTROL.md, "Self-repairing
/// change sets").

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/vcs/Repository.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs {

/// @brief Where an installed change set stands.
enum class RepairStatus {
	Applied,     ///< On main, as recorded.
	NeedsRepair, ///< Its patch doesn't apply, or its proof fails: left off, waiting for a repair.
	Repairing,   ///< A repair was asked for (and may be waiting for review).
	Repaired,    ///< Its repair was accepted: on main, re-recorded on the new base.
	GaveUp       ///< Its repairs failed maxAttempts times: left off until a reviewer retries.
};

/// @brief A status's name in messages.
/// @param status The status.
/// @return "applied", "needsRepair", "repairing", "repaired" or "gaveUp".
[[nodiscard]] std::string_view toString(RepairStatus status) noexcept;

/// @brief A change set this screen was asked to install, and how it stands.
struct InstalledChangeSet {
	json changes;                                ///< As installed, or as re-recorded by its repair.
	RepairStatus status = RepairStatus::Applied; ///< See RepairStatus.
	std::string reason;                          ///< Why it needs repair; empty when applied.
	int attempts = 0;                            ///< Repairs asked for since it last applied or was retried.
	std::string request;                         ///< The open repair request's id, while repairing.
	int proposal = 0;                            ///< The repair's proposal, once the agent sent one.
	json proposed;                               ///< The change set the proposal would record (with repairedFrom).
};

/// @brief The change sets installed on one Repository, and their repairs.
///
/// A change set may carry, besides its revisions, the request in words that made it ("request"), the template it was
/// recorded on ("template": name, commit and tree) and its proof ("proof": scenarios). install() applies it only if
/// its patch applies and its proof passes on the result; otherwise it marks it NeedsRepair, with the reason, and
/// leaves the screen as it was. It then asks the peers for a repair (a "request" of kind "repair", see request()), one
/// at a time and at most maxAttempts times per change set. The agent's answer (handle()) only ever opens a proposal
/// (Repository::propose()): a reviewer accepts it, and the change set is re-recorded on the new base with a
/// "repairedFrom" link.
class ChangeSetRepairs {
public:
	/// @brief Checks a change set's proof on a document.
	/// @param document The document with the live state in "state".
	/// @param proof The change set's "proof".
	/// @return Why it fails, or std::nullopt if it passes.
	using ProofCheck = std::function<std::optional<std::string>(const json& document, const json& proof)>;

	static constexpr int maxAttempts = 3;             ///< Repairs asked for per change set before giving up.
	static constexpr std::size_t maxChangeSets = 64;  ///< Change sets one screen installs.
	static constexpr std::size_t maxRequestBytes = 4096; ///< Of a change set's request in words.

	/// @brief Starts with no change set installed.
	/// @param repository The document; must outlive this.
	/// @param templateName The template's name, recorded as the base of change sets (e.g. "flow/index.screen.json").
	/// The repository's document is taken as the template, so construct this right after loading it.
	ChangeSetRepairs(Repository& repository, std::string templateName);
	/// @brief Disconnects from the repository.
	~ChangeSetRepairs();
	ChangeSetRepairs(const ChangeSetRepairs&) = delete;
	ChangeSetRepairs& operator=(const ChangeSetRepairs&) = delete;

	/// @brief Names the template, as change sets record their base.
	/// @param name Its file's name, e.g. "index.screen.json".
	void setTemplateName(std::string name) { templateName_ = std::move(name); }

	/// @brief Sets how proofs are checked; without one, a change set with a proof needs repair ("no proof check").
	/// @param check The check.
	void setProofCheck(ProofCheck check) { check_ = std::move(check); }

	/// @brief Installs a change set: applies it if it applies and its proof passes, or marks it NeedsRepair and asks
	/// for a repair. One already in the history counts as applied.
	/// @param changes The change set ("affrescoui-changes"; its signature, if any, checked by the caller).
	/// @return Where it stands.
	/// @throws DocumentError If it is malformed, or too many are installed.
	RepairStatus install(const json& changes);

	/// @brief Handles a "repair" message from a peer: the agent's revision for an open request
	/// (`{"type": "repair", "request": id, "revision": {...}}`), a reviewer's retry (`{"retry": name}`) or, with
	/// neither, a question for the status.
	/// @param message The message, with "from" set by the channel to its author.
	/// @param answer Sends an answer to that peer.
	void handle(const json& message, const std::function<void(const json&)>& answer);

	/// @brief The open repair request, for a peer that connects after it was sent.
	/// @return It, or std::nullopt.
	[[nodiscard]] std::optional<json> openRequest() const;

	/// @brief What every installed change set stands at: `{"type": "repairs", "changeSets": [{name, status, reason,
	/// attempts, proposal, request}]}`.
	[[nodiscard]] json status() const;

	/// @brief The installed change sets.
	[[nodiscard]] const std::vector<InstalledChangeSet>& changeSets() const noexcept { return installed_; }

	/// @brief The template this screen runs, as change sets record their base: `{name, commit, tree}`.
	[[nodiscard]] json currentBase() const;

	/// @brief Adds the repair fields to a change set (exportChanges()): what it was made for, its base and its proof.
	/// @param changes The change set.
	/// @param request The request in words (may be empty).
	/// @param base Its base (currentBase() when it was exported).
	/// @param proof Its proof, `{"scenarios": [...]}` (may be null: none).
	/// @return The change set with "request", "template" and "proof".
	[[nodiscard]] static json describe(json changes, std::string_view request, const json& base, const json& proof);

	Signal<const json&> onMessage; ///< A repair request or the status, for every peer.

private:
	/// @brief Why a change set can't be applied as it is.
	/// @param changes The change set.
	/// @return The reason, or std::nullopt if its patch applies and its proof passes.
	[[nodiscard]] std::optional<std::string> problemWith(const json& changes) const;
	/// @brief Checks a change set's proof on the document it would make.
	/// @param changes The change set.
	/// @param document The document with the live state in "state".
	/// @return Why it fails, or std::nullopt.
	[[nodiscard]] std::optional<std::string> checkProof(const json& changes, const json& document) const;
	/// @brief Asks for the next repair, if none is open: the first change set that needs one, unless it gave up.
	void askNext();
	/// @brief The request for a change set's repair.
	/// @param installed The change set.
	/// @return `{"type": "request", "kind": "repair", "id", "from": "host", "text", "repair": {...}}`.
	[[nodiscard]] json request(const InstalledChangeSet& installed) const;
	/// @brief Opens the agent's repair as a proposal, after checking it applies and passes the proof.
	/// @param message The "repair" message with "request" and "revision".
	/// @param answer Sends the answer to the agent.
	void propose(const json& message, const std::function<void(const json&)>& answer);
	/// @brief Retries a change set at a reviewer's request: installs it again, with its attempts reset.
	/// @param message The "repair" message with "retry".
	/// @param answer Sends the answer to the reviewer.
	void retry(const json& message, const std::function<void(const json&)>& answer);
	/// @brief Follows a reviewer's decision on a repair's proposal.
	/// @param decision The decision.
	void decided(const Decision& decision);
	/// @brief Marks a change set as needing repair, and asks for the next one.
	/// @param installed The change set.
	/// @param reason Why.
	void needsRepair(InstalledChangeSet& installed, std::string reason);
	/// @brief Finds an installed change set by name.
	/// @param name Its name.
	/// @return It, or nullptr.
	[[nodiscard]] InstalledChangeSet* find(std::string_view name);

	Repository& repository_;                  ///< The document.
	std::string templateName_;                ///< See currentBase().
	std::string templateTree_;                ///< The SHA-256 of the template as loaded, with its state.
	std::string templateCommit_;              ///< Revision 0's commit.
	ProofCheck check_;                        ///< See setProofCheck().
	std::vector<InstalledChangeSet> installed_; ///< In the order installed.
	Connection decisions_ = 0;                ///< To onDecision, disconnected on destruction.
};

} // namespace affresco::vcs
