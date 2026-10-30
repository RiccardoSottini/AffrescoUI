#pragma once

/// @file
/// @brief channel::Session: the live-document protocol over a Channel.

#include "affresco/channel/Limits.hpp"
#include "affresco/core/Channel.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/ScreenImages.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"

#include <functional>
#include <optional>
#include <string>

namespace affresco::channel {

/// @brief The live-document protocol (SCREEN_FORMAT.md, "Live screens") over a Channel.
///
/// The peer on the other end reads the document and its history, sends revisions and, if the document lets it
/// review, decides on proposals, sets the mode and revokes grants. It is told about every commit and every change
/// to the proposals.
/// Everything it sends is authored by `author` (followed by the name it gives itself): a channel can't claim to be
/// someone it isn't. "request" and "reply" messages are not interpreted: they come out of onRelay, stamped with
/// "from", for whoever connects peers to each other. The peer is held to PeerLimits: the size of a message, and how
/// many messages and changes it sends in a given time.
class Session {
public:
	/// @brief Starts the protocol and sends "hello".
	/// @param channel The transport; must outlive the session.
	/// @param versions The document; must outlive the session.
	/// @param author The name the document's permissions know this peer by.
	/// @param catalog What the format offers, for "getCatalog" (may be empty: answered with an empty catalog).
	/// @param images Draws pictures of the screen, for "render" (may be nullptr: such a host has no pictures); must
	/// outlive the session.
	/// @param limits What the peer may send (see PeerLimits for the defaults).
	Session(Channel& channel, VersionControl& versions, std::string author, std::function<json()> catalog = {},
	        ScreenImages* images = nullptr, const PeerLimits& limits = {});
	~Session();

	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	/// @brief Sends a message to the peer, as one line of JSON.
	/// @param message One protocol message. Bytes in its strings that aren't UTF-8 are sent as U+FFFD.
	void send(const json& message);
	/// @brief The name this peer is authored as.
	[[nodiscard]] const std::string& author() const noexcept { return author_; }
	/// @brief The usage summaries (VERSION_CONTROL.md, "Usage"): given whether to clear them first, the summary, or
	/// std::nullopt while they are off.
	using UsageSummary = std::function<std::optional<json>(bool clear)>;
	/// @brief Answers "getUsage" and "clearUsage" from a reviewer (by default they are off).
	/// @param summary The summaries; must not outlive what it reads.
	void setUsage(UsageSummary summary) { usage_ = std::move(summary); }

	Signal<const json&> onRelay; ///< A "request" or "reply" for other peers, with "from" set.

private:
	/// @brief A message from the peer being handled.
	struct Incoming {
		const json& message; ///< The message, a JSON object.
		std::string type;    ///< Its "type" ("" if it has none, or one that isn't a string).
		json id;             ///< Its "id", echoed in the answer (null if none).
		std::string author;  ///< Who it is authored as (see authorOf()).
	};

	/// @brief Handles one message from the peer; malformed or refused ones get an "error" or "rejected" answer.
	/// @param line The message.
	void handle(const std::string& line);
	/// @brief Holds a message to the peer's limits before it is parsed: one over the message rate or the size gets an
	/// "error" (without an "id": it isn't read), and costs the app no more.
	/// @param line The message.
	/// @return Whether to handle it.
	bool admit(const std::string& line);
	/// @brief Notes that the peer went over a limit: logged once, until a message is admitted again.
	/// @param what The limit, e.g. "too many messages: at most 100 a second".
	void overLimit(const std::string& what);
	/// @brief Parses a line from the peer, answering "error" if it isn't a JSON object.
	/// @param line The line.
	/// @return The message, or std::nullopt.
	std::optional<json> read(const std::string& line);
	/// @brief The author a message is recorded under: this channel's, plus the name the peer gives itself after '/'.
	/// @param message The message.
	/// @return E.g. "agent" or "agent/claude-code".
	[[nodiscard]] std::string authorOf(const json& message) const;
	/// @brief Answers a change: "applied", "proposed" or "rejected".
	/// @param in The message.
	/// @param result What happened to it.
	void reply(const Incoming& in, const RevisionResult& result);
	/// @brief Handles revision, restore, revert and undo.
	/// @param in The message.
	/// @return false if it is none of these.
	/// @throws DocumentError If it is malformed.
	bool handleChange(const Incoming& in);
	/// @brief Handles accept, reject, preview, endPreview, setMode and revoke (reviewers only).
	/// @param in The message.
	/// @return false if it is none of these.
	/// @throws DocumentError If it is malformed.
	bool handleReview(const Incoming& in);
	/// @brief Carries out a reviewer's decision (handleReview(), once the peer may review).
	/// @param in The message: accept, reject, preview, endPreview, setMode or revoke.
	/// @return Why it was refused, or std::nullopt.
	/// @throws DocumentError If it is malformed.
	std::optional<std::string> decide(const Incoming& in);
	/// @brief Turns the host's inspector on or off (handleReview(), once the peer may review).
	/// @param in The message: `{"type": "inspect", "on": bool}`.
	/// @return Why it was refused, or std::nullopt.
	/// @throws DocumentError If it is malformed.
	std::optional<std::string> inspect(const Incoming& in);
	/// @brief The "proposal" a review message names.
	/// @param in The message.
	/// @return The proposal's id.
	/// @throws DocumentError If it has none, or it isn't a whole number.
	static int proposalOf(const Incoming& in);
	/// @brief Handles getDocument, getCatalog, getHistory, diff, exportChanges and render.
	/// @param in The message.
	/// @return false if it is none of these.
	/// @throws DocumentError If it is malformed.
	bool handleQuery(const Incoming& in);
	/// @brief Answers "render" with a picture of the screen: `{"type": "image", "format": "png", "data": base64, ...}`.
	/// @param in The message: `proposal` (optional) and `scale` (1 by default, at most ScreenImages::maxScale).
	/// @throws DocumentError If it is malformed, over the rate of pictures, or the picture can't be drawn.
	void sendImage(const Incoming& in);
	/// @brief Answers "getEvidence" with what a proposal does, gathered by the host (VERSION_CONTROL.md, "Evidence"):
	/// `{"type": "evidence", "proposal", "evidence": {"before", "after", "region", "behaviour", "scenarios"}}`.
	/// @param in The message: `proposal`, and `scale` as for render.
	/// @throws DocumentError If it is malformed, over the rate of pictures, or the evidence can't be gathered.
	void sendEvidence(const Incoming& in);
	/// @brief Handles blame: `{"type": "blame", "target": "#board"}` gives the commit on main that last changed it.
	/// @param in The message.
	/// @return false if it isn't one.
	/// @throws DocumentError If it is malformed, or the target isn't there.
	bool handleBlame(const Incoming& in);
	/// @brief Handles convert (Convert.cpp): `{"type": "convert", "notation": "lisp", "text": "..."}` answers
	/// `{"type": "converted", "value": ...}`, and with `value` instead of `text`, the value's `text` in the notation.
	/// @param in The message.
	/// @return false if it isn't one.
	/// @throws DocumentError If it is malformed, or the text isn't a value in the notation (a NotationError, with the
	/// line and column).
	bool handleConvert(const Incoming& in);
	/// @brief The "review" message: proposals, preview, mode and grants.
	[[nodiscard]] json reviewState() const;

	Channel& channel_;                                                    ///< The transport.
	VersionControl& versions_;                                            ///< The document.
	std::string author_;                                                  ///< See author().
	std::function<json()> catalog_;                                       ///< For "getCatalog".
	ScreenImages* images_ = nullptr;                                      ///< For "render" (may be nullptr).
	PeerLimits limits_;                                                   ///< What the peer may send.
	TokenBucket messageRate_;                                             ///< Messages of any kind.
	TokenBucket revisionRate_;                                            ///< Changes.
	TokenBucket pictureRate_; ///< Pictures ("render"), at the rate of changes: each draws the whole screen.
	bool limited_ = false;                                                ///< Over a limit now (see overLimit()).
	Connection messages_ = 0, decisions_ = 0, commits_ = 0, reviews_ = 0; ///< Disconnected on destruction.
	UsageSummary usage_; ///< For "getUsage" and "clearUsage" (empty: off).
};

} // namespace affresco::channel
