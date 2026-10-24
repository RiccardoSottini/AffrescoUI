/// @file
/// @brief channel::Session: the live protocol for one peer. Reads its messages within its limits, passes each to its
/// group (changes, review, queries, blame) and answers it; relays requests and replies.

#include "affresco/channel/Session.hpp"
#include "affresco/core/Log.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace affresco::channel {

namespace {

/// @brief Encodes bytes in base64 (RFC 4648, with padding), so binary data fits in a JSON string.
/// @param bytes The bytes.
/// @return The text.
std::string toBase64(std::string_view bytes) {
	static constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string text;
	text.reserve((bytes.size() + 2) / 3 * 4);
	for(std::size_t i = 0; i < bytes.size(); i += 3) {
		const std::size_t left = bytes.size() - i;
		std::uint32_t group = std::uint32_t{static_cast<std::uint8_t>(bytes[i])} << 16;
		if(left > 1) group |= std::uint32_t{static_cast<std::uint8_t>(bytes[i + 1])} << 8;
		if(left > 2) group |= static_cast<std::uint8_t>(bytes[i + 2]);
		text += alphabet[(group >> 18) & 63];
		text += alphabet[(group >> 12) & 63];
		text += left > 1 ? alphabet[(group >> 6) & 63] : '=';
		text += left > 2 ? alphabet[group & 63] : '=';
	}
	return text;
}

/// @brief Tells whether a read shows the document whole, which a peer whose data rules limit its reads may not make.
/// @param type The message's type.
/// @return true for diff, exportChanges, render, getEvidence and blame.
bool showsWhole(std::string_view type) noexcept {
	static constexpr std::array<std::string_view, 5> reads = {"diff", "exportChanges", "render", "getEvidence",
	                                                          "blame"};
	return std::ranges::find(reads, type) != reads.end();
}

/// @brief A message's "type", read without throwing.
/// @param message The message, an object.
/// @return It, or "" if it has none or it isn't text.
std::string typeOf(const json& message) {
	return message.contains("type") && message["type"].is_string() ? message["type"].get<std::string>() : "";
}

/// @brief Tells whether a message is relayed to the other peers as it is: a request, a reply or a repair.
/// @param type The message's type.
/// @return true if it is.
bool relayed(std::string_view type) noexcept { return type == "request" || type == "reply" || type == "repair"; }

/// @brief Who asks for a lease or a role: the session, its channel and its author.
struct Asker {
	VersionControl& versions;   ///< The document's version control.
	const void* connection;     ///< The session asking, which its leases name.
	const std::string& channel; ///< The session's channel ("agent"): who may review.
	const std::string& author;  ///< Who asks, as the session names it ("agent/claude-code").
};

/// @brief A refusal.
/// @param error Why.
/// @return The answer, without its "id".
json refused(const std::string& error) { return {{"type", "rejected"}, {"error", error}}; }

/// @brief Answers lease and release: a peer leases and releases parts for itself.
/// @param asker Who asks.
/// @param type "lease" or "release".
/// @param target The part.
/// @param message The message.
/// @return The answer, without its "id".
json ownLease(const Asker& asker, const std::string& type, const std::string& target, const json& message) {
	if(type == "release") {
		if(asker.versions.release(asker.connection, target)) return {{"type", "released"}, {"target", target}};
		return refused("you hold no lease on " + target);
	}
	const json seconds = message.value("seconds", json(300));
	const int duration = seconds.is_number_integer()
	                         ? static_cast<int>(std::clamp<std::int64_t>(seconds.get<std::int64_t>(), 1, 3600))
	                         : 300;
	if(const auto error = asker.versions.lease(asker.connection, asker.author, target, duration))
		return refused(*error);
	return {{"type", "leased"}, {"target", target}, {"seconds", duration}};
}

/// @brief Answers lease, release, endLease and setRole (VERSION_CONTROL.md, "Leases" and "Roles"): a peer leases and
/// releases parts for itself; only a reviewer ends another's lease or sets a role, and nobody sets their own.
/// @param asker Who asks.
/// @param type The message's type.
/// @param message The message.
/// @return The answer, without its "id", or std::nullopt if it is none of those.
std::optional<json> coordinate(const Asker& asker, const std::string& type, const json& message) {
	if(type != "lease" && type != "release" && type != "endLease" && type != "setRole") return std::nullopt;
	const std::string target =
	    type == "setRole" ? "" : expect::string(message.value("target", json()), type + ".target");
	if(type == "release" || type == "lease") return ownLease(asker, type, target, message);
	if(!asker.versions.mayReview(asker.channel))
		return refused("permission denied: '" + asker.channel + "' may not review changes");
	const std::optional<std::string> error =
	    type == "endLease"
	        ? asker.versions.endLease(target, asker.author)
	        : asker.versions.setRole(expect::string(message.value("author", json()), "setRole.author"),
	                                 expect::string(message.value("role", json()), "setRole.role"), asker.author);
	return error ? refused(*error) : json{{"type", "done"}};
}

/// @brief Answers "getUsage" and "clearUsage" (VERSION_CONTROL.md, "Usage"): for a reviewer only, while the host
/// has them on.
/// @param usage The summaries (may be empty: off).
/// @param versions The document's version control.
/// @param channel The session's channel: who may review.
/// @param type The message's type.
/// @return The answer, without its "id", or std::nullopt if it is neither.
std::optional<json> usageAnswer(const Session::UsageSummary& usage, const VersionControl& versions,
                                const std::string& channel, const std::string& type) {
	if(type != "getUsage" && type != "clearUsage") return std::nullopt;
	if(!versions.mayReview(channel))
		return json{{"type", "rejected"}, {"error", "permission denied: only a reviewer reads usage summaries"}};
	const std::optional<json> summary = usage ? usage(type == "clearUsage") : std::nullopt;
	if(!summary)
		return json{{"type", "rejected"},
		            {"error", "usage summaries are off: the host switches them on, with the person's consent"}};
	return json{{"type", "usage"}, {"usage", *summary}};
}

/// @brief Answers the messages about leases, roles and usage summaries (coordinate(), usageAnswer()).
/// @param asker Who asks.
/// @param usage The usage summaries (may be empty: off).
/// @param type The message's type.
/// @param message The message.
/// @return The answer, without its "id", or std::nullopt if it is none of those.
std::optional<json> coordinationAnswer(const Asker& asker, const Session::UsageSummary& usage, const std::string& type,
                                       const json& message) {
	if(std::optional<json> answer = coordinate(asker, type, message)) return answer;
	return usageAnswer(usage, asker.versions, asker.channel, type);
}

/// @brief Tells whether a peer asks for a read it may not make: one showing the document whole, while its data rules
/// limit its reads.
/// @param versions The document's version control.
/// @param channel The session's channel, whose data rules apply.
/// @param type The message's type.
/// @return true if it does.
bool readsTooMuch(const VersionControl& versions, const std::string& channel, std::string_view type) {
	return showsWhole(type) && versions.readable(channel);
}

/// @brief The newest commits on main a "getHistory" asks for (like `git log -n`), oldest first.
/// @param versions The document's version control.
/// @param message The message: its "limit", or all without one.
/// @return The entries.
json historyOf(const VersionControl& versions, const json& message) {
	const std::vector<CommitInfo> log = versions.log();
	// Read as a double: a peer's 1e20 converted to an int would be undefined behaviour
	const double limit = message.value("limit", 0.0);
	const std::size_t skip =
	    limit >= 1 && limit < static_cast<double>(log.size()) ? log.size() - static_cast<std::size_t>(limit) : 0;
	json entries = json::array();
	for(std::size_t i = skip; i < log.size(); ++i) entries.push_back(toJson(log[i]));
	return entries;
}

/// @brief What a "diff" asks for: a proposal against main's head, or any two commits (default: the previous one
/// against HEAD).
/// @param versions The document's version control.
/// @param message The message.
/// @return The patch.
/// @throws DocumentError If it names neither a proposal nor commits there are.
json diffOf(const VersionControl& versions, const json& message) {
	return message.contains("proposal")
	           ? versions.proposalDiff(expect::integer(message["proposal"], "diff.proposal"))
			   : versions.diff(message.value("from", json("HEAD~1")), message.value("to", json("HEAD")));
}

/// @brief The impact of what a "diff" asks for (VersionControl::impact()): a proposal's, or between two commits.
/// @param versions The document's version control.
/// @param message The message.
/// @return The impact, or null.
json impactOf(const VersionControl& versions, const json& message) {
	if(!message.contains("proposal"))
		return versions.impact(message.value("from", json("HEAD~1")), message.value("to", json("HEAD")));
	for(const ProposalInfo& p : versions.proposals())
		if(json(p.id) == message["proposal"]) return p.impact;
	return nullptr;
}

} // namespace

Session::Session(Channel& channel, VersionControl& versions, std::string author, std::function<json()> catalog,
                 ScreenImages* images, const PeerLimits& limits)
    : channel_(channel), versions_(versions), author_(std::move(author)), catalog_(std::move(catalog)), images_(images),
      limits_(limits), messageRate_(limits.messagesPerSecond, limits.messageBurst, TokenBucket::Clock::now()),
      revisionRate_(limits.revisionsPerMinute / 60.0, limits.revisionBurst, TokenBucket::Clock::now()),
      pictureRate_(limits.revisionsPerMinute / 60.0, limits.revisionBurst, TokenBucket::Clock::now()) {
	messages_ = channel_.onMessage.connect([this](const std::string& line) { handle(line); });

	decisions_ = versions_.onDecision.connect([this](const Decision& decision) {
		if(decision.error)
			send({{"type", "rejected"},
			      {"proposal", decision.proposal},
			      {"error", *decision.error},
			      {"revision", decision.revision}});
		else
			send({{"type", "applied"},
			      {"proposal", decision.proposal},
			      {"revision", decision.revision},
			      {"commit", decision.commit}});
	});
	commits_ = versions_.onCommit.connect([this](const CommitInfo& commit) {
		json message = toJson(commit);
		message["type"] = "committed";
		message["revision"] = commit.number;
		send(message);
	});
	reviews_ = versions_.onReviewChanged.connect([this] {
		// A picture of a proposal shows it for a moment, then puts back what was shown: nothing a peer needs to hear
		if(!images_ || !images_->rendering()) send(reviewState());
	});

	// Introduce ourselves: whoever is on the other end gets the full picture to work from (the part its data rules let
	// it read, VERSION_CONTROL.md "Data rules")
	const std::optional<json> readable = versions_.readable(author_);
	send({{"type", "hello"},
	      {"format", "affrescoui-screen"},
	      {"revision", versions_.head().number},
	      {"head", toJson(versions_.head())},
	      {"document", readable ? *readable : versions_.document()},
	      {"state", readable ? readable->value("state", json::object()) : versions_.state()},
	      {"mode", toString(versions_.mode())},
	      {"you", author_},
	      {"canReview", versions_.mayReview(author_)}});
	versions_.join(this, author_); // after hello: joining tells every peer, this one included
}

Session::~Session() {
	channel_.onMessage.disconnect(messages_);
	versions_.onDecision.disconnect(decisions_);
	versions_.onCommit.disconnect(commits_);
	versions_.onReviewChanged.disconnect(reviews_);
	versions_.leave(this); // its leases end with it
}

// Error texts can quote what the peer sent, bytes that aren't UTF-8 included (a parse error shows the bytes it read),
// and dump() throws on those by default: they are replaced with U+FFFD instead, so answering never fails
void Session::send(const json& message) {
	channel_.send(message.dump(-1, ' ', false, json::error_handler_t::replace));
}

// Open proposals, the one being previewed, the mode and the "don't ask again" grants
json Session::reviewState() const {
	json proposals = json::array(), grants = json::array();
	for(const ProposalInfo& p : versions_.proposals()) proposals.push_back(toJson(p));
	// What a proposal changes can show values a peer's data rules hide from it
	if(versions_.readable(author_))
		for(json& p : proposals)
			for(const char* key : {"changes", "touches", "conflicts", "behaviour", "impact"}) p.erase(key);
	for(const Grant& g : versions_.grants()) grants.push_back(toJson(g));
	const auto previewing = versions_.previewing();
	return {{"type", "review"},
	        {"proposals", proposals},
	        {"previewing", previewing ? json(*previewing) : json()},
	        {"mode", toString(versions_.mode())},
	        {"grants", grants},
	        {"coordination", versions_.coordination()},
	        {"revision", versions_.head().number}};
}

// One JSON object per line
void Session::handle(const std::string& line) {
	if(!admit(line)) return;
	const std::optional<json> message = read(line);
	if(!message) return;

	// Read without throwing: a peer's malformed message must not take the app down
	const json& m = *message;
	const Incoming in{m, typeOf(m), m.value("id", json()), authorOf(m)};

	try {
		if(relayed(in.type)) {
			json relayed = m;
			relayed["from"] = in.author;
			relayed.erase("source"); // only the screen says a request comes from inside the app
			onRelay(relayed);
		} else if(readsTooMuch(versions_, author_, in.type)) {
			expect::fail(in.type, "permission denied: the data rules let '" + author_ + "' read only some paths");
		} else if(const auto answer = coordinationAnswer({versions_, this, author_, in.author}, usage_, in.type, m)) {
			json reply = *answer;
			reply["revision"] = versions_.head().number;
			reply["id"] = in.id;
			send(reply);
		} else if(!handleChange(in) && !handleReview(in) && !handleQuery(in) && !handleBlame(in)) {
			send({{"type", "error"}, {"message", "unknown message type '" + in.type + "'"}, {"id", in.id}});
		}
	} catch(const DocumentError& error) {
		send({{"type", "rejected"}, {"error", error.what()}, {"revision", versions_.head().number}, {"id", in.id}});
	} catch(const json::exception& error) {
		send({{"type", "rejected"},
		      {"error", std::string("AffrescoUI screen: ") + error.what()},
		      {"revision", versions_.head().number},
		      {"id", in.id}});
	}
}

bool Session::admit(const std::string& line) {
	// The rate first: every message costs a token, a refused one too, so a flood of large messages is cut off as fast
	if(!messageRate_.take(TokenBucket::Clock::now())) {
		const std::string what = "too many messages: at most " + toText(limits_.messagesPerSecond) + " a second";
		overLimit(what);
		send({{"type", "error"}, {"message", what}});
		return false;
	}
	if(line.size() > limits_.maxMessageBytes) {
		const std::string what = "message too large: " + std::to_string(line.size()) + " bytes, at most " +
		                         std::to_string(limits_.maxMessageBytes);
		overLimit(what);
		send({{"type", "error"}, {"message", what}});
		return false;
	}
	limited_ = false;
	return true;
}

void Session::overLimit(const std::string& what) {
	if(!limited_) log(LogLevel::Warning, "AffrescoUI: peer '" + author_ + "' is over its limits: " + what);
	limited_ = true;
}

std::optional<json> Session::read(const std::string& line) {
	try {
		json message = parseUntrusted(line);
		if(message.is_object()) return message;
		send({{"type", "error"}, {"message", "expected a JSON object"}});
	} catch(const json::parse_error& error) {
		send({{"type", "error"}, {"message", std::string("invalid JSON: ") + error.what()}});
	} catch(const DocumentError& error) {
		send({{"type", "error"}, {"message", error.what()}});
	}
	return std::nullopt;
}

// The channel decides who the author is; the name the sender gives itself is kept after a '/'
std::string Session::authorOf(const json& message) const {
	const std::string declared =
	    message.contains("author") && message["author"].is_string() ? message["author"].get<std::string>() : "";
	return declared.empty() || declared == author_ ? author_ : author_ + "/" + declared;
}

void Session::reply(const Incoming& in, const RevisionResult& result) {
	if(result.status == RevisionStatus::Applied) {
		send({{"type", "applied"}, {"revision", result.revision}, {"commit", result.commit}, {"id", in.id}});
	} else if(result.status == RevisionStatus::Proposed) {
		send({{"type", "proposed"},
		      {"proposal", result.proposal},
		      {"commit", result.commit},
		      {"revision", result.revision},
		      {"id", in.id}});
	} else {
		log(LogLevel::Warning, result.error);
		send({{"type", "rejected"}, {"error", result.error}, {"revision", result.revision}, {"id", in.id}});
	}
}

// revision, restore, revert, undo: changes, which follow the author's permissions like any other, and rebuild the
// screen, so they have a rate of their own
bool Session::handleChange(const Incoming& in) {
	if(in.type != "revision" && in.type != "restore" && in.type != "revert" && in.type != "undo") return false;
	if(!revisionRate_.take(TokenBucket::Clock::now())) {
		const std::string what = "too many changes: at most " + toText(limits_.revisionsPerMinute) + " a minute";
		overLimit(what);
		send({{"type", "rejected"}, {"error", what}, {"revision", versions_.head().number}, {"id", in.id}});
		return true;
	}

	const json& m = in.message;
	const auto revision = [&] {
		if(!m.contains("revision")) expect::fail(in.type + ".revision", "expected a revision number or a commit");
		return m["revision"];
	};

	if(in.type == "revision") {
		json submitted = m;
		submitted["author"] = in.author;
		submitted.erase("type");
		submitted.erase("id");
		reply(in, versions_.submitFrom(this, submitted));
	} else if(in.type == "restore") {
		reply(in, versions_.submitFrom(this, versions_.restoreRevision(revision(), in.author)));
	} else if(in.type == "revert") {
		reply(in, versions_.submitFrom(this, versions_.revertRevision(revision(), in.author)));
	} else { // undo
		reply(in, versions_.submitFrom(this, versions_.revertRevision("HEAD", in.author)));
	}
	return true;
}

// accept, reject, preview, endPreview, setMode, revoke: decisions, for reviewers only (an agent must not approve
// its own changes); inspect too, which shows how the screen is built
bool Session::handleReview(const Incoming& in) {
	static constexpr std::array<std::string_view, 7> decisions = {"accept",  "reject", "preview", "endPreview",
	                                                              "setMode", "revoke", "inspect"};
	if(std::ranges::find(decisions, in.type) == decisions.end()) return false;

	const int at = versions_.head().number;
	if(!versions_.mayReview(author_)) {
		send({{"type", "rejected"},
		      {"error", "permission denied: '" + author_ + "' may not review changes"},
		      {"revision", at},
		      {"id", in.id}});
		return true;
	}
	if(const std::optional<std::string> error = in.type == "inspect" ? inspect(in) : decide(in))
		send({{"type", "rejected"}, {"error", *error}, {"revision", versions_.head().number}, {"id", in.id}});
	else send({{"type", "done"}, {"revision", versions_.head().number}, {"id", in.id}});
	return true;
}

std::optional<std::string> Session::decide(const Incoming& in) {
	const json& m = in.message;
	if(in.type == "accept") {
		// The peer says which version it approves: the branch may move while the reviewer looks at it
		if(!m.contains("head") || !m["head"].is_string())
			expect::fail("accept.head", "expected the proposal's head commit: the version you reviewed");
		const std::string duration = m.contains("for") ? expect::string(m["for"], "accept.for") : "";
		return versions_.accept(proposalOf(in),
		                        {in.author, m["head"].get<std::string>(), m.value("dontAskAgain", false), duration});
	}
	if(in.type == "reject") {
		const int proposal = proposalOf(in);
		if(versions_.reject(proposal, toText(m.value("reason", json(""))))) return std::nullopt;
		return "AffrescoUI screen: no proposal " + std::to_string(proposal);
	}
	if(in.type == "preview") return versions_.preview(proposalOf(in));
	if(in.type == "endPreview") {
		versions_.endPreview();
		return std::nullopt;
	}
	if(in.type == "setMode") {
		const auto mode = modeFromString(expect::string(m.value("mode", json()), "setMode.mode"));
		if(!mode) return R"(AffrescoUI screen: setMode.mode: expected "default", "acceptEdits" or "plan")";
		versions_.setMode(*mode);
		return std::nullopt;
	}
	const std::string rule = expect::string(m.value("rule", json()), "revoke.rule"); // revoke
	if(versions_.revokeGrant(rule)) return std::nullopt;
	return "AffrescoUI screen: no grant '" + rule + "'";
}

std::optional<std::string> Session::inspect(const Incoming& in) {
	const json on = in.message.value("on", json());
	if(!on.is_boolean()) expect::fail("inspect.on", "expected true or false");
	if(!images_) return "AffrescoUI screen: this host has no inspector";
	return images_->inspect(on.get<bool>());
}

int Session::proposalOf(const Incoming& in) {
	return expect::integer(in.message.value("proposal", json()), in.type + ".proposal");
}

// getDocument, getCatalog, getHistory, diff, exportChanges, render: reads, which change nothing
bool Session::handleQuery(const Incoming& in) {
	const json& m = in.message;

	if(in.type == "getDocument") {
		const json state = reviewState();
		const std::optional<json> readable = versions_.readable(author_);
		send({{"type", "document"},
		      {"revision", versions_.head().number},
		      {"head", toJson(versions_.head())},
		      {"document", readable ? *readable : versions_.document()},
		      {"state", readable ? readable->value("state", json::object()) : versions_.state()},
		      {"proposals", state["proposals"]},
		      {"previewing", state["previewing"]},
		      {"mode", state["mode"]},
		      {"grants", state["grants"]},
		      {"you", author_},
		      {"canReview", versions_.mayReview(author_)},
		      {"id", in.id}});
	} else if(in.type == "getCatalog") {
		send({{"type", "catalog"}, {"catalog", catalog_ ? catalog_() : json::object()}, {"id", in.id}});
	} else if(in.type == "getHistory") {
		const json entries = historyOf(versions_, m);
		send({{"type", "history"}, {"entries", entries}, {"revision", versions_.head().number}, {"id", in.id}});
	} else if(in.type == "diff") {
		const json patch = diffOf(versions_, m);
		json reply = {{"type", "diff"}, {"patch", patch}, {"revision", versions_.head().number}, {"id", in.id}};
		if(!versions_.readable(author_)) reply["impact"] = impactOf(versions_, m); // the layers it touches
		send(reply);
	} else if(in.type == "exportChanges") {
		send({{"type", "changes"},
		      {"changes", versions_.exportChanges(m.value("from", json(0)), m.value("name", "changes"))},
		      {"id", in.id}});
	} else if(in.type == "render") {
		sendImage(in);
	} else if(in.type == "getEvidence") {
		sendEvidence(in);
	} else {
		return false;
	}
	return true;
}

// A read, like the queries: who last changed a node or a value, and when
bool Session::handleBlame(const Incoming& in) {
	if(in.type != "blame") return false;
	const std::string target = expect::string(in.message.value("target", json()), "blame.target");
	send({{"type", "blame"},
	      {"target", target},
	      {"commit", toJson(versions_.blame(target))},
	      {"revision", versions_.head().number},
	      {"id", in.id}});
	return true;
}

void Session::sendImage(const Incoming& in) {
	const json& m = in.message;
	ImageRequest request;
	if(m.contains("proposal") && !m["proposal"].is_null())
		request.proposal = expect::integer(m["proposal"], "render.proposal");
	if(m.contains("scale") && !m["scale"].is_null()) {
		request.scale = expect::integer(m["scale"], "render.scale");
		if(request.scale < 1 || request.scale > ScreenImages::maxScale)
			expect::fail("render.scale", "expected 1 to " + std::to_string(ScreenImages::maxScale));
	}
	if(!images_) expect::fail("render", "this host draws no pictures");
	// A picture takes the main thread for tens of milliseconds (a proposal's is built, then main's again): a read,
	// but one that costs as much as a change, so it has the same rate
	if(!pictureRate_.take(TokenBucket::Clock::now())) {
		const std::string what = "too many pictures: at most " + toText(limits_.revisionsPerMinute) + " a minute";
		overLimit(what);
		throw DocumentError(what);
	}

	Image image;
	if(const std::optional<std::string> error = images_->render(request, image)) throw DocumentError(*error);
	send({{"type", "image"},
	      {"format", "png"},
	      {"data", toBase64(image.png)},
	      {"width", image.width},
	      {"height", image.height},
	      {"scale", request.scale},
	      {"proposal", request.proposal ? json(*request.proposal) : json()},
	      {"revision", versions_.head().number},
	      {"id", in.id}});
}

void Session::sendEvidence(const Incoming& in) {
	const json& m = in.message;
	const int proposal = proposalOf(in);
	const int scale =
	    m.contains("scale") && !m["scale"].is_null() ? expect::integer(m["scale"], "getEvidence.scale") : 1;
	if(scale < 1 || scale > ScreenImages::maxScale)
		expect::fail("getEvidence.scale", "expected 1 to " + std::to_string(ScreenImages::maxScale));
	if(!images_) expect::fail("getEvidence", "this host draws no pictures");
	// Two pictures and the scenarios played: at the rate of pictures
	if(!pictureRate_.take(TokenBucket::Clock::now())) {
		const std::string what = "too many pictures: at most " + toText(limits_.revisionsPerMinute) + " a minute";
		overLimit(what);
		throw DocumentError(what);
	}

	Evidence evidence;
	if(const std::optional<std::string> error = images_->evidence(proposal, scale, evidence))
		throw DocumentError(*error);
	const auto picture = [](const Image& image) {
		return json{{"format", "png"}, {"data", toBase64(image.png)}, {"width", image.width}, {"height", image.height}};
	};
	send({{"type", "evidence"},
	      {"proposal", proposal},
	      {"evidence",
	       {{"before", picture(evidence.before)},
	        {"after", picture(evidence.after)},
	        {"region", evidence.region},
	        {"behaviour", evidence.behaviour},
	        {"scenarios", evidence.scenarios},
	        {"texts", evidence.texts}}},
	      {"scale", scale},
	      {"revision", versions_.head().number},
	      {"id", in.id}});
}

} // namespace affresco::channel
