#include "affresco/channel/Session.hpp"

#include <iostream>
#include <utility>

namespace affresco::channel {

Session::Session(Channel& channel, VersionControl& versions, std::string author)
    : channel_(channel), versions_(versions), author_(std::move(author)) {
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
	reviews_ = versions_.onReviewChanged.connect([this] { send(reviewState()); });

	// Introduce ourselves: whoever is on the other end gets the full picture to work from
	send({{"type", "hello"},
	      {"format", "affrescoui-screen"},
	      {"revision", versions_.head().number},
	      {"head", toJson(versions_.head())},
	      {"document", versions_.document()},
	      {"state", versions_.state()},
	      {"mode", toString(versions_.mode())},
	      {"you", author_},
	      {"canReview", versions_.mayReview(author_)}});
}

Session::~Session() {
	channel_.onMessage.disconnect(messages_);
	versions_.onDecision.disconnect(decisions_);
	versions_.onCommit.disconnect(commits_);
	versions_.onReviewChanged.disconnect(reviews_);
}

void Session::send(const json& message) { channel_.send(message.dump()); }

// Open proposals, the one being previewed, the mode and the "don't ask again" grants
json Session::reviewState() const {
	json proposals = json::array(), grants = json::array();
	for(const ProposalInfo& p : versions_.proposals()) proposals.push_back(toJson(p));
	for(const Grant& g : versions_.grants()) grants.push_back(toJson(g));
	const auto previewing = versions_.previewing();
	return {{"type", "review"},
	        {"proposals", proposals},
	        {"previewing", previewing ? json(*previewing) : json()},
	        {"mode", toString(versions_.mode())},
	        {"grants", grants},
	        {"revision", versions_.head().number}};
}

// One JSON object per line
void Session::handle(const std::string& line) {
	json message;
	try {
		message = json::parse(line);
	} catch(const json::parse_error& error) {
		send({{"type", "error"}, {"message", std::string("invalid JSON: ") + error.what()}});
		return;
	}
	if(!message.is_object()) {
		send({{"type", "error"}, {"message", "expected a JSON object"}});
		return;
	}

	const std::string type = message.value("type", "");
	const json id = message.value("id", json());

	// The channel decides who the author is; the name the sender gives itself is kept after a '/'
	auto authored = [&](json revision) {
		const std::string declared =
		    message.contains("author") && message["author"].is_string() ? message["author"].get<std::string>() : "";
		revision["author"] = declared.empty() || declared == author_ ? author_ : author_ + "/" + declared;
		return revision;
	};
	const std::string author = authored(json::object())["author"];

	auto reply = [&](const RevisionResult& result) {
		if(result.status == RevisionStatus::Applied)
			send({{"type", "applied"}, {"revision", result.revision}, {"commit", result.commit}, {"id", id}});
		else if(result.status == RevisionStatus::Proposed)
			send({{"type", "proposed"},
			      {"proposal", result.proposal},
			      {"commit", result.commit},
			      {"revision", result.revision},
			      {"id", id}});
		else {
			std::cerr << result.error << '\n';
			send({{"type", "rejected"}, {"error", result.error}, {"revision", result.revision}, {"id", id}});
		}
	};

	// Deciding on proposals is for reviewers only: an agent must not approve its own changes
	auto review = [&](auto&& decide) {
		if(!versions_.mayReview(author_)) {
			send({{"type", "rejected"},
			      {"error", "permission denied: '" + author_ + "' may not review changes"},
			      {"revision", versions_.head().number},
			      {"id", id}});
			return;
		}
		const std::optional<std::string> error = decide();
		if(error) send({{"type", "rejected"}, {"error", *error}, {"revision", versions_.head().number}, {"id", id}});
		else send({{"type", "done"}, {"revision", versions_.head().number}, {"id", id}});
	};
	auto proposal = [&] { return expect::integer(message.value("proposal", json()), type + ".proposal"); };
	auto revision = [&] {
		if(!message.contains("revision")) expect::fail(type + ".revision", "expected a revision number or a commit");
		return message["revision"];
	};

	try {
		if(type == "revision") {
			json submitted = authored(message);
			submitted.erase("type");
			submitted.erase("id");
			reply(versions_.submit(submitted));
		} else if(type == "restore") {
			// Going back is a change like any other: it follows the author's permissions too
			reply(versions_.submit(versions_.restoreRevision(revision(), author)));
		} else if(type == "revert") {
			reply(versions_.submit(versions_.revertRevision(revision(), author)));
		} else if(type == "undo") {
			reply(versions_.submit(versions_.revertRevision("HEAD", author)));
		} else if(type == "accept") {
			// A peer says which version it approves: the branch may move while the reviewer looks at it
			review([&] {
				if(!message.contains("head") || !message["head"].is_string())
					expect::fail("accept.head", "expected the proposal's head commit: the version you reviewed");
				const Approval approval{author, message["head"].get<std::string>(),
				                        message.value("dontAskAgain", false)};
				return versions_.accept(proposal(), approval);
			});
		} else if(type == "reject") {
			review([&]() -> std::optional<std::string> {
				const int p = proposal();
				if(versions_.reject(p, toText(message.value("reason", json(""))))) return std::nullopt;
				return "AffrescoUI screen: no proposal " + std::to_string(p);
			});
		} else if(type == "preview") {
			review([&] { return versions_.preview(proposal()); });
		} else if(type == "endPreview") {
			review([&]() -> std::optional<std::string> {
				versions_.endPreview();
				return std::nullopt;
			});
		} else if(type == "setMode") {
			review([&]() -> std::optional<std::string> {
				const auto mode = modeFromString(expect::string(message.value("mode", json()), "setMode.mode"));
				if(!mode) return "AffrescoUI screen: setMode.mode: expected \"default\", \"acceptEdits\" or \"plan\"";
				versions_.setMode(*mode);
				return std::nullopt;
			});
		} else if(type == "revoke") {
			review([&]() -> std::optional<std::string> {
				const std::string rule = expect::string(message.value("rule", json()), "revoke.rule");
				if(versions_.revokeGrant(rule)) return std::nullopt;
				return "AffrescoUI screen: no grant '" + rule + "'";
			});
		} else if(type == "request" || type == "reply") {
			json relayed = message;
			relayed["from"] = author;
			onRelay(relayed);
		} else if(type == "getDocument") {
			json state = reviewState();
			send({{"type", "document"},
			      {"revision", versions_.head().number},
			      {"head", toJson(versions_.head())},
			      {"document", versions_.document()},
			      {"state", versions_.state()},
			      {"proposals", state["proposals"]},
			      {"previewing", state["previewing"]},
			      {"mode", state["mode"]},
			      {"grants", state["grants"]},
			      {"you", author_},
			      {"canReview", versions_.mayReview(author_)},
			      {"id", id}});
		} else if(type == "getHistory") {
			// The newest "limit" commits on main (like `git log -n`), oldest first; all without one
			const std::vector<CommitInfo> log = versions_.log();
			const int limit = message.value("limit", 0);
			const std::size_t skip = limit > 0 && log.size() > static_cast<std::size_t>(limit)
			                             ? log.size() - static_cast<std::size_t>(limit)
			                             : 0;
			json entries = json::array();
			for(std::size_t i = skip; i < log.size(); ++i) entries.push_back(toJson(log[i]));
			send({{"type", "history"}, {"entries", entries}, {"revision", versions_.head().number}, {"id", id}});
		} else if(type == "diff") {
			// A proposal against main's head, or any two commits (default: the previous one against HEAD)
			const json patch = message.contains("proposal") ? versions_.proposalDiff(proposal())
			                                                : versions_.diff(message.value("from", json("HEAD~1")),
			                                                                 message.value("to", json("HEAD")));
			send({{"type", "diff"}, {"patch", patch}, {"revision", versions_.head().number}, {"id", id}});
		} else if(type == "exportChanges") {
			send(
			    {{"type", "changes"},
				 {"changes", versions_.exportChanges(message.value("from", json(0)), message.value("name", "changes"))},
				 {"id", id}});
		} else {
			send({{"type", "error"}, {"message", "unknown message type '" + type + "'"}, {"id", id}});
		}
	} catch(const DocumentError& error) {
		send({{"type", "rejected"}, {"error", error.what()}, {"revision", versions_.head().number}, {"id", id}});
	} catch(const json::exception& error) {
		send({{"type", "rejected"},
		      {"error", std::string("AffrescoUI screen: ") + error.what()},
		      {"revision", versions_.head().number},
		      {"id", id}});
	}
}

} // namespace affresco::channel
