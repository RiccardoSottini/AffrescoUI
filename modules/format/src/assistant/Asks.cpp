/// @file
/// @brief format::Asks: the requests a screen's own person sends its agents, and their replies.

#include "Asks.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <chrono>

namespace affresco::format {

namespace {

/// @brief Now, for the ledger.
/// @return Milliseconds since 1970 (UTC), as `$now()` reads them.
std::int64_t millisecondsNow() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

/// @brief Reads a text a peer sent, cut to a size at a character boundary.
/// @param value The value: anything but a string reads as "".
/// @param most The most bytes.
/// @return The text.
std::string bounded(const json& value, std::size_t most) {
	if(!value.is_string()) return {};
	std::string text = value.get<std::string>();
	if(text.size() <= most) return text;
	std::size_t end = most;
	while(end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) --end; // not inside a character
	text.resize(end);
	return text;
}

/// @brief Sets a flag for as long as it lives.
class Raised {
public:
	/// @brief Sets the flag.
	/// @param flag The flag; outlives this object.
	explicit Raised(bool& flag) noexcept : flag_(flag) { flag_ = true; }
	Raised(const Raised&) = delete;
	Raised& operator=(const Raised&) = delete;
	Raised(Raised&&) = delete;
	Raised& operator=(Raised&&) = delete;
	/// @brief Clears the flag.
	~Raised() { flag_ = false; }

private:
	bool& flag_; ///< The flag.
};

} // namespace

std::string Asks::send(const std::string& text, const std::string& where, const json& options,
                       const json& attachments) {
	if(text.find_first_not_of(" \t\r\n") == std::string::npos) fail(where + ".ask", "a request needs some text");
	if(text.size() > maxAsk)
		fail(where + ".ask",
		     "a request is at most " + std::to_string(maxAsk) + " bytes, not " + std::to_string(text.size()));
	if(!options.is_null() && !options.is_object()) fail(where + ".options", "expected an object");
	if(!options.is_null() && options.dump().size() > maxOptions)
		fail(where + ".options", "a request's options are at most " + std::to_string(maxOptions) + " bytes");
	makeRoom();
	Request request;
	request.time = millisecondsNow();
	request.id = "ask-" + std::to_string(request.time) + "-" + std::to_string(++sent_);
	request.text = text;
	for(const ProposalInfo& proposal : engine_.versions.proposals()) request.heads[proposal.id] = proposal.head;
	requests_.push_back(std::move(request));
	engine_.document.globalsChanged();

	json message = {
	    {"type", "request"}, {"id", requests_.back().id}, {"text", text}, {"from", "user"}, {"source", "screen"}};
	if(options.is_object() && !options.empty()) message["options"] = options; // for the agents, passed on as is
	if(attachments.is_array() && !attachments.empty()) message["attachments"] = attachments;
	if(!engine_.window) {
		engine_.owner.onAsk(message);
	} else {
		engine_.scheduler.defer([&engine = engine_, message = std::move(message)] { engine.owner.onAsk(message); });
	}
	return requests_.back().id;
}

bool Asks::receive(const json& reply) {
	if(!reply.is_object() || !reply.contains("request") || !reply["request"].is_string()) return false;
	const auto request = std::ranges::find(requests_, reply["request"].get<std::string>(), &Request::id);
	if(request == requests_.end()) return false;

	const std::string from = bounded(reply.value("from", json()), maxFrom);
	const bool done = !reply.contains("done") || !reply["done"].is_boolean() || reply["done"].get<bool>();
	const std::string error = bounded(reply.value("error", json()), maxText);
	const std::string text = bounded(reply.value("text", json()), maxText);
	if(!done && error.empty() && text.empty()) { // "working on it", with nothing to say yet
		request->state = "working";
		engine_.document.globalsChanged();
		return true;
	}
	json entry = {{"from", from}, {"text", text}, {"time", millisecondsNow()}, {"done", done}};
	if(!error.empty()) entry["error"] = error;
	if(request->replies.size() >= maxReplies) request->replies.erase(request->replies.begin());
	request->replies.push_back(entry);
	request->state = !error.empty() ? "failed" : done ? "answered" : "working";

	json event = entry;
	event["request"] = request->id;
	event["error"] = error;
	event["proposals"] = newProposals(*request, from);
	for(const json& id : event["proposals"])
		if(std::ranges::find(request->proposals, id) == request->proposals.end()) request->proposals.push_back(id);
	engine_.document.globalsChanged();
	engine_.reconciler.requestReconcile();

	if(engine_.window && engine_.owner.hasAction(action)) {
		const Raised replying(replying_);
		engine_.owner.runAction(action, engine_.window->root(), event);
	}
	return true;
}

json Asks::list() const {
	json list = json::array();
	for(const Request& request : requests_)
		list.push_back({{"id", request.id},
		                {"text", request.text},
		                {"time", request.time},
		                {"state", request.state},
		                {"replies", request.replies},
		                {"proposals", request.proposals}});
	return list;
}

json Asks::newProposals(Request& request, std::string_view from) const {
	const std::string_view channel = from.substr(0, from.find('/'));
	json found = json::array();
	for(const ProposalInfo& proposal : engine_.versions.proposals()) {
		const std::string_view author = std::string_view(proposal.author).substr(0, proposal.author.find('/'));
		if(channel.empty() || author != channel) continue;
		if(const auto before = request.heads.find(proposal.id);
		   before != request.heads.end() && before->second == proposal.head)
			continue; // as it was when the person asked
		std::string& reported = request.reported[proposal.id];
		if(reported == proposal.head) continue;
		reported = proposal.head;
		found.push_back(proposal.id);
	}
	return found;
}

void Asks::makeRoom() {
	if(requests_.size() < maxRequests) return;
	const auto finished = std::ranges::find_if(
	    requests_, [](const Request& request) { return request.state == "answered" || request.state == "failed"; });
	requests_.erase(finished != requests_.end() ? finished : requests_.begin());
}

} // namespace affresco::format
