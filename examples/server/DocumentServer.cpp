/// @file
/// @brief DocumentServer: documents on disk, shared with authenticated people.

#include "DocumentServer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace affresco::server {

namespace {

/// @brief What a new personal document is: a screen with nothing on it but its state.
/// @return The document.
json personalDocument() {
	return {{"format", "affrescoui-screen"},
	        {"version", 1},
	        {"state", json::object()},
	        {"root", {{"type", "panel"}, {"id", "main"}}}};
}

/// @brief Tells whether a name may be a document's: the same characters as a person's.
/// @param name The name, without '~'.
/// @return true if it may.
bool validDocumentName(std::string_view name) noexcept { return TokenFile::validName(name); }

/// @brief A string member of a message, or "" if it has none or it isn't a string.
/// @param message The message.
/// @param key The member.
/// @return The value.
std::string text(const json& message, const char* key) {
	const auto value = message.find(key);
	return value != message.end() && value->is_string() ? value->get<std::string>() : std::string();
}

/// @brief An error answer, the way a Session words them.
/// @param what Why.
/// @param id The message's id, echoed (null: none).
/// @return The message.
json error(const std::string& what, const json& id = nullptr) {
	return {{"type", "error"}, {"message", what}, {"id", id}};
}

/// @brief The document as revisions address it: its live state as "state".
/// @param repository The repository.
/// @return The view.
json view(const vcs::Repository& repository) {
	json document = repository.document();
	document["state"] = repository.state();
	return document;
}

} // namespace

DocumentServer::DocumentServer(Executor& executor, std::filesystem::path folder, std::shared_ptr<TokenFile> tokens,
                               std::string_view address, net::PeerServerOptions options)
    : executor_(executor), folder_(std::move(folder)), tokens_(std::move(tokens)) {
	if(!tokens_) throw std::runtime_error("the document server needs a token file");
	loadDocuments();
	listener_ = std::make_unique<net::PeerServer>(
	    executor_, address, tokens_,
	    [this, alive = alive_](std::unique_ptr<Channel> socket, std::string author) {
		    if(*alive) adopt(std::move(socket), std::move(author));
	    },
	    std::move(options));
}

DocumentServer::~DocumentServer() {
	*alive_ = false;
	listener_.reset();
	peers_.clear(); // before the documents their sessions use
}

std::vector<std::string> DocumentServer::documents() const {
	std::vector<std::string> names;
	for(const auto& [key, document] : documents_)
		if(!key.starts_with('~')) names.push_back(key);
	return names;
}

void DocumentServer::loadDocuments() {
	const std::filesystem::path directory = folder_ / "documents";
	std::filesystem::create_directories(directory);
	for(const auto& entry : std::filesystem::directory_iterator(directory)) {
		const std::string file = entry.path().filename().string();
		if(!entry.is_regular_file() || !file.ends_with(".screen.json")) continue;
		const std::string name = file.substr(0, file.size() - std::string_view(".screen.json").size());
		if(!validDocumentName(name) || documents_.size() >= maxDocuments) continue;
		load(name, entry.path(), folder_ / "history" / name);
	}
}

DocumentServer::Document& DocumentServer::load(const std::string& key, const std::filesystem::path& file,
                                               const std::filesystem::path& history) {
	std::ifstream input(file);
	if(!input) throw std::runtime_error("can't read " + file.string());
	json content = json::parse(input);
	auto document = std::make_unique<Document>(rules_, [tokens = tokens_] { return tokens->people(); });
	document->repository = std::make_unique<vcs::Repository>(document->rules, std::move(content));
	const vcs::Repository::OpenedHistory opened = document->repository->openHistory(history);
	if(!opened.memoryOnly.empty())
		throw std::runtime_error(key + ": the history can't be written: " + opened.memoryOnly);
	return *documents_.emplace(key, std::move(document)).first->second;
}

void DocumentServer::adopt(std::unique_ptr<Channel> socket, std::string author) {
	auto owned = std::make_unique<Peer>(Peer{nextPeer_++, std::move(author), std::move(socket),
	                                         channel::TokenBucket(20.0, 40.0, channel::TokenBucket::Clock::now()),
	                                         nullptr, nullptr, nullptr});
	Peer& peer = *owned;
	peers_.push_back(std::move(owned));
	peer.socket->onMessage.connect([this, &peer](const std::string& line) { handle(peer, line); });
	peer.socket->onClosed.connect([this, peer = &peer] {
		executor_.post([this, peer, alive = alive_] {
			if(*alive) remove(peer);
		});
	});
	peer.socket->send(json{{"type", "documents"}, {"documents", documents()}, {"author", peer.author}}.dump());
}

void DocumentServer::handle(Peer& peer, const std::string& line) {
	if(line.size() > maxServerMessageBytes) {
		if(peer.session) peer.inner->onMessage(line); // a revision: the Session holds it to its limits
		else peer.socket->send(error("open a document first").dump());
		return;
	}
	const json message = json::parse(line, nullptr, false);
	const std::string type = message.is_object() ? text(message, "type") : std::string();
	const bool ours = type == "open" || type == "presence" || type == "getPresence" || type == "lease" ||
	                  type == "release";
	if(!ours) {
		if(peer.session) peer.inner->onMessage(line);
		else peer.socket->send(error("open a document first").dump());
		return;
	}
	const json id = message.contains("id") ? message["id"] : json(nullptr);
	if(!peer.rate.take(channel::TokenBucket::Clock::now())) {
		peer.socket->send(error("too many messages: at most 20 a second", id).dump());
		return;
	}
	if(type == "open") open(peer, message);
	else if(!peer.document) peer.socket->send(error("open a document first", id).dump());
	else handleServerMessage(peer, message, type);
}

void DocumentServer::handleServerMessage(Peer& peer, const json& message, const std::string& type) {
	Document& document = *peer.document;
	if(type == "getPresence") {
		peer.socket->send(state(document, "presence").dump());
		peer.socket->send(state(document, "leases").dump());
	} else if(type == "presence") {
		if(!document.presence.set(peer.id, text(message, "where"), text(message, "doing")))
			peer.socket->send(error("\"where\" is at most 256 bytes, \"doing\" at most 64").dump());
		else broadcast(document, "presence");
	} else lease(peer, message, type);
}

void DocumentServer::open(Peer& peer, const json& message) {
	const json id = message.contains("id") ? message["id"] : json(nullptr);
	if(peer.document) {
		peer.socket->send(error("a connection opens one document; connect again for another", id).dump());
		return;
	}
	Document* document = find(text(message, "document"), peer.author);
	if(!document) {
		peer.socket->send(error("no document is called '" + text(message, "document") + "'", id).dump());
		return;
	}
	if(!document->presence.join(peer.id, peer.author)) {
		peer.socket->send(error("too many people on this document", id).dump());
		return;
	}
	peer.document = document;
	peer.inner = std::make_unique<Inner>(*peer.socket);
	peer.session = std::make_unique<channel::Session>(*peer.inner, *document->repository, peer.author);
	broadcast(*document, "presence");
	peer.socket->send(state(*document, "leases").dump());
}

DocumentServer::Document* DocumentServer::find(const std::string& name, const std::string& author) {
	const bool personal = name.starts_with('~');
	const std::string bare = personal ? name.substr(1) : name;
	if(!validDocumentName(bare)) return nullptr;
	const std::string key = personal ? "~" + author + "/" + bare : bare; // a person reaches only their own
	if(const auto found = documents_.find(key); found != documents_.end()) return found->second.get();
	if(!personal || documents_.size() >= maxDocuments) return nullptr;
	const std::filesystem::path directory = folder_ / "people" / author;
	std::filesystem::create_directories(directory);
	const std::filesystem::path file = directory / (bare + ".screen.json");
	if(!std::filesystem::exists(file)) std::ofstream(file) << personalDocument().dump(1, '\t') << '\n';
	try {
		return &load(key, file, directory / (bare + ".history"));
	} catch(const std::exception&) {
		return nullptr;
	}
}

void DocumentServer::lease(Peer& peer, const json& message, const std::string& type) {
	Document& document = *peer.document;
	const json id = message.contains("id") ? message["id"] : json(nullptr);
	const std::string target = text(message, "target");
	const std::string holder = std::string(peer.author.substr(0, peer.author.find('/')));
	if(type == "release") {
		const bool released = document.leases.release(target, holder);
		peer.socket->send(released ? json{{"type", "released"}, {"target", target}, {"id", id}}.dump()
		                           : error("you hold no lease on " + target, id).dump());
		if(released) broadcast(document, "leases");
		return;
	}
	const auto seconds = message.contains("seconds") && message["seconds"].is_number_integer()
	                         ? message["seconds"].get<std::int64_t>()
	                         : std::int64_t{300};
	auto result = document.leases.acquire(view(*document.repository), target, holder, peer.id,
	                                      std::chrono::seconds{std::clamp<std::int64_t>(seconds, 0, 86400)},
	                                      vcs::Leases::Clock::now());
	if(const auto* refused = std::get_if<std::string>(&result)) {
		peer.socket->send(json{{"type", "rejected"}, {"error", *refused}, {"target", target}, {"id", id}}.dump());
		return;
	}
	peer.socket->send(json{{"type", "leased"}, {"target", target}, {"seconds", seconds}, {"id", id}}.dump());
	broadcast(document, "leases");
}

void DocumentServer::broadcast(const Document& document, const std::string& what) {
	const std::string message = state(document, what).dump();
	for(const auto& peer : peers_)
		if(peer->document == &document) peer->socket->send(message);
}

json DocumentServer::state(const Document& document, const std::string& what) {
	if(what == "presence") return {{"type", "presence"}, {"peers", document.presence.toJson()}};
	return {{"type", "leases"}, {"leases", document.leases.toJson(vcs::Leases::Clock::now())}};
}

void DocumentServer::remove(const Peer* peer) {
	const auto found = std::ranges::find_if(peers_, [peer](const auto& p) { return p.get() == peer; });
	if(found == peers_.end()) return;
	Document* document = (*found)->document;
	const int id = (*found)->id;
	peers_.erase(found); // the session first, then its channels (Peer's member order)
	if(!document) return;
	document->presence.leave(id);
	const bool leased = document->leases.releasePeer(id) > 0;
	broadcast(*document, "presence");
	if(leased) broadcast(*document, "leases");
}

} // namespace affresco::server
