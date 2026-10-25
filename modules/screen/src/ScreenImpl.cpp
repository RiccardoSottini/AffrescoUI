/// @file
/// @brief affresco::Screen::Impl: how the screen's modules are put together, its "persist" state file and its peers.

#include "ScreenImpl.hpp"
#include "StateFile.hpp"
#include "features/FeaturesService.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace affresco {

Screen::Impl::Impl(json document, std::filesystem::path baseDirectory)
    : repository(rules, std::move(document)), runtime(repository, std::move(baseDirectory)) {
	// A commit's "run" steps (a merged proposal's), once, on the live screen; a restore or a revert carries none
	repository.onCommit.connect([this](const CommitInfo& commit) {
		if(commit.run.is_array() && !commit.run.empty() && !images.rendering()) runtime.runOnce(commit.run, commit.id);
	});
	runtime.onEvent.connect([this](const json& event) {
		if(images.rendering()) return; // from a proposal drawn for a picture: it didn't happen
		broadcast(event);
		events(event.dump());
	});
	runtime.onError.connect([this](const std::string& message) {
		broadcast({{"type", "error"}, {"message", message}, {"revision", repository.head().number}});
	});
	runtime.onSend.connect([this](const std::string& name, const json& message) { send(name, message); });
	runtime.onConnect.connect([this](const std::string& name) {
		if(const auto it = channels.find(name); it != channels.end()) {
			if(!it->second.socket && !it->second.remote) open(it->second);
		} else {
			runtime.onError("AffrescoUI screen: connect: no channel '" + name + "' in \"channels\"");
		}
	});
	runtime.onUpdated.connect([this] { save(); });
	runtime.onAsk.connect([this](const json& request) { // the person asks the agents, from inside the app
		if(images.rendering()) return;
		if(request.value("type", "") != "request") { // a cancel: for whoever is connected
			broadcast(request);
			return;
		}
		if(peers.empty()) {
			(void)runtime.receiveReply({{"request", request["id"]}, {"error", "no agent is connected"}});
			return;
		}
		// One agent answers: the last connected (an MCP server the person started wins over the host's own); every
		// other peer, a client, still sees it
		const auto agent = std::ranges::find_if(peers.rbegin(), peers.rend(),
		                                        [](const auto& peer) { return peer->session->author() == "agent"; });
		for(const auto& peer : peers)
			if(agent == peers.rend() || peer->session->author() != "agent" || peer == *agent)
				peer->session->send(request);
	});
	repairs.onMessage.connect([this](const json& message) {
		for(const auto& peer : peers) sendRepairs(*peer, message);
	});
	rules.setServices(&services);
	rules.setData(&runtime.dataStore()); // a change to the models dry-runs on a copy of the records first
	runtime.setServices(&services);
	services.add(detail::makeFeaturesProvider(features, repository, alive));

	socketDirectory = Screen::defaultSocketDirectory();
}

Screen::Impl::~Impl() {
	*alive = false;
	servers.clear();
	peerServers.clear();
	if(Window* window = runtime.window()) // the reconnect timers point at the screen
		for(const auto& [name, channel] : channels)
			if(channel.retry) window->cancelTimer(channel.retry);
	channels.clear();
}

void Screen::Impl::warnOfFindings() const {
	for(const std::string& finding : rules.findings(repository.document(), repository.state()))
		affresco::log(LogLevel::Warning, "AffrescoUI screen: " + finding);
}

/* Persistence */

std::vector<json::json_pointer> Screen::Impl::persisted() const {
	std::vector<json::json_pointer> pointers;
	const json& document = repository.document();
	if(!document.contains("persist")) return pointers;
	for(const json& path : document["persist"]) {
		std::string pointer;
		const std::string rest = path.get<std::string>().substr(6); // after "state."
		for(std::size_t start = 0; start <= rest.size();) {
			const std::size_t dot = std::min(rest.find('.', start), rest.size());
			pointer += "/" + rest.substr(start, dot - start);
			start = dot + 1;
		}
		pointers.emplace_back(pointer);
	}
	return pointers;
}

json Screen::Impl::persistedState() {
	json result = json::object();
	for(const auto& pointer : persisted())
		if(repository.state().contains(pointer)) result[pointer.to_string()] = repository.state()[pointer];
	return result;
}

void Screen::Impl::load() {
	if(persisted().empty()) return;
	const std::ifstream in(stateFile);
	std::stringstream text;
	text << in.rdbuf();
	json file;
	try {
		file = parseUntrusted(text.str());
		// NOLINTNEXTLINE(bugprone-empty-catch): the screen keeps its own state, as the comment below says
	} catch(const std::exception&) { // json::parse_error, or DocumentError if it nests too deeply
		// Not a state file this screen wrote: the screen keeps its own state, as before
	}
	json rejected = json::object();
	if(file.is_object())
		for(const auto& pointer : persisted()) {
			const std::string key = pointer.to_string();
			if(!file.contains(key)) continue;
			if(std::optional<std::string> problem = persistedMismatch(pointer, file[key])) {
				rejected[key] = {{"value", file[key]}, {"problem", std::move(*problem)}};
				continue; // the document's own value stays: the side file keeps this one
			}
			repository.state()[pointer] = file[key];
		}
	if(!rejected.empty()) keepRejected(rejected);
	saved = persistedState();
}

void Screen::Impl::save() {
	if(stateFile.empty() || persisted().empty()) return; // no state file: a screen from fromString()
	if(images.rendering()) return;                       // a proposal's state, shown only for a picture
	json now = persistedState();
	if(now == saved) return;
	std::error_code error;
	std::filesystem::create_directories(stateFile.parent_path(), error);
	if(!detail::writeStateFile(stateFile, now.dump(2) + '\n')) {
		std::ostringstream message; // a path prints quoted, as it always has
		message << "AffrescoUI screen: could not write " << stateFile;
		affresco::log(LogLevel::Error, message.str());
		return;
	}
	saved = std::move(now);
}

/* Peers */

void Screen::Impl::broadcast(const json& message, const Peer* except) {
	for(const auto& peer : peers)
		if(peer.get() != except) peer->session->send(message);
}

void Screen::Impl::sendRepairs(const Peer& peer, json message) const {
	if(repository.readable(peer.session->author())) {
		if(message.contains("repair"))
			for(const char* key : {"revisions", "proof", "failed"}) message["repair"].erase(key);
		if(message.contains("changeSets"))
			for(json& row : message["changeSets"]) row.erase("reason");
	}
	peer.session->send(message);
}

Screen::Impl::Peer& Screen::Impl::add(Channel& channel, std::unique_ptr<Channel> owned, std::string author) {
	auto peer = std::make_unique<Peer>();
	peer->owned = std::move(owned);
	peer->channel = &channel;
	peer->session = std::make_unique<channel::Session>(
	    channel, repository, std::move(author), [this] { return runtime.catalog(); }, &images);
	peer->session->setUsage([this](bool clear) -> std::optional<json> {
		if(!runtime.usage.enabled()) return std::nullopt;
		if(clear) runtime.usage.clear();
		return runtime.usage.summary(repository.document(), format::Usage::now());
	});

	// A request in plain words from one peer (e.g. the user's client) is for the others (e.g. an AI agent)
	peer->session->onRelay.connect([this, sender = peer.get()](const json& message) {
		if(message.value("type", std::string()) == "repair")
			return repairs.handle(message, [this, sender](const json& answer) { sendRepairs(*sender, answer); });
		if(message.value("type", std::string()) == "reply" && runtime.receiveReply(message)) return; // the screen's
		if(peers.size() < 2) {
			sender->session->send({{"type", "error"},
			                       {"message", "nobody else is connected to receive this " +
			                                       message.value("type", std::string("message"))},
			                       {"id", message.value("id", json())}});
			return;
		}
		broadcast(message, sender);
	});
	if(const std::optional<json> open = repairs.openRequest()) sendRepairs(*peer, *open);
	peers.push_back(std::move(peer));
	return *peers.back();
}

void Screen::Impl::adopt(Executor& mainThread, std::unique_ptr<Channel> connection, std::string name) {
	Channel& channel = *connection;
	Peer& peer = add(channel, std::move(connection), std::move(name));
	channel.onClosed.connect([this, &mainThread, peer = &peer] {
		mainThread.post([this, peer, guard = alive] {
			if(*guard) remove(peer);
		});
	});
}

void Screen::Impl::remove(const Peer* peer) {
	std::erase_if(peers, [peer](const auto& p) { return p.get() == peer; });
}

} // namespace affresco
