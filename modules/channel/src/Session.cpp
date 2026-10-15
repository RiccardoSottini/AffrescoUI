#include "sgl/channel/Session.hpp"

#include <iostream>
#include <utility>

namespace sgl::channel {

Session::Session(Channel& channel, VersionControl& versions, std::string author)
    : channel_(channel), versions_(versions), author_(std::move(author)) {
	messages_ = channel_.onMessage.connect([this](const std::string& line) { handle(line); });

	decisions_ = versions_.onDecision.connect([this](const Decision& decision) {
		if(decision.error)
			send({{"type", "rejected"},
			      {"proposal", decision.proposal},
			      {"error", *decision.error},
			      {"revision", decision.revision}});
		else send({{"type", "applied"}, {"proposal", decision.proposal}, {"revision", decision.revision}});
	});
	commits_ = versions_.onCommit.connect([this](const HistoryEntry& entry) {
		json message = toJson(entry);
		message["type"] = "committed";
		message["revision"] = entry.number;
		send(message);
	});
	reviews_ = versions_.onReviewChanged.connect([this] { send(reviewState()); });

	// Introduce ourselves: whoever is on the other end gets the full picture to work from
	send({{"type", "hello"},
	      {"format", "sglgui-screen"},
	      {"revision", versions_.head().number},
	      {"document", versions_.document()},
	      {"state", versions_.state()},
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

// Proposals waiting for a decision, the one being previewed, and whether auto-accept is on
json Session::reviewState() const {
	json proposals = json::array();
	for(const ProposalInfo& p : versions_.proposals()) proposals.push_back(toJson(p));
	const auto previewing = versions_.previewing();
	return {{"type", "review"},
	        {"proposals", proposals},
	        {"previewing", previewing ? json(*previewing) : json()},
	        {"autoAccept", versions_.autoAccept()},
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

	const std::string type = message.value("type", "");
	const json id = message.value("id", json());

	// The channel decides who the author is; the name the sender gives itself is kept after a '/'
	auto authored = [&](json revision) {
		const std::string declared =
		    message.contains("author") && message["author"].is_string() ? message["author"].get<std::string>() : "";
		revision["author"] = declared.empty() || declared == author_ ? author_ : author_ + "/" + declared;
		return revision;
	};

	auto reply = [&](const RevisionResult& result) {
		if(result.status == RevisionStatus::Applied)
			send({{"type", "applied"}, {"revision", result.revision}, {"id", id}});
		else if(result.status == RevisionStatus::Proposed)
			send({{"type", "proposed"}, {"proposal", result.proposal}, {"revision", result.revision}, {"id", id}});
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

	try {
		if(type == "revision") {
			reply(versions_.submit(authored(message)));
		} else if(type == "restore" || type == "undo") {
			// Going back is a change like any other: it follows the author's policy too
			const std::string author = authored(json::object())["author"];
			const auto history = versions_.history();
			const int target = type == "restore"
			                       ? expect::integer(message.value("revision", json()), "restore.revision")
			                   : history.size() > 1 ? history[history.size() - 2].number
			                                        : history.back().number;
			reply(versions_.submit(versions_.restoreRevision(target, author)));
		} else if(type == "accept") {
			review([&] { return versions_.accept(proposal()); });
		} else if(type == "reject") {
			review([&]() -> std::optional<std::string> {
				const int p = proposal();
				if(versions_.reject(p, toText(message.value("reason", json(""))))) return std::nullopt;
				return "sGLGUI screen: no proposal " + std::to_string(p);
			});
		} else if(type == "preview") {
			review([&] { return versions_.preview(proposal()); });
		} else if(type == "endPreview") {
			review([&]() -> std::optional<std::string> {
				versions_.endPreview();
				return std::nullopt;
			});
		} else if(type == "setAutoAccept") {
			review([&]() -> std::optional<std::string> {
				versions_.setAutoAccept(expect::boolean(message.value("enabled", json()), "setAutoAccept.enabled"));
				return std::nullopt;
			});
		} else if(type == "request" || type == "reply") {
			json relayed = message;
			relayed["from"] = authored(json::object())["author"];
			onRelay(relayed);
		} else if(type == "getDocument") {
			json state = reviewState();
			send({{"type", "document"},
			      {"revision", versions_.head().number},
			      {"document", versions_.document()},
			      {"state", versions_.state()},
			      {"proposals", state["proposals"]},
			      {"previewing", state["previewing"]},
			      {"autoAccept", state["autoAccept"]},
			      {"you", author_},
			      {"canReview", versions_.mayReview(author_)},
			      {"id", id}});
		} else if(type == "getHistory") {
			json entries = json::array();
			for(const HistoryEntry& h : versions_.history()) entries.push_back(toJson(h));
			send({{"type", "history"}, {"entries", entries}, {"revision", versions_.head().number}, {"id", id}});
		} else if(type == "exportChanges") {
			send({{"type", "changes"},
			      {"changes", versions_.exportChanges(message.value("from", 0), message.value("name", "changes"))},
			      {"id", id}});
		} else {
			send({{"type", "error"}, {"message", "unknown message type '" + type + "'"}});
		}
	} catch(const DocumentError& error) {
		send({{"type", "rejected"}, {"error", error.what()}, {"revision", versions_.head().number}, {"id", id}});
	}
}

} // namespace sgl::channel
