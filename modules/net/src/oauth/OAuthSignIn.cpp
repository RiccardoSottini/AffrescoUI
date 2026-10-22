/// @file
/// @brief SignIn (SignIn.hpp): the providers, their states and tokens, the browser flow and refresh. The device flow
/// is in OAuthDevice.cpp.

#include "oauth/OAuthSignIn.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <utility>

namespace affresco::net {

namespace {

/// @brief The seconds since the epoch of a time.
/// @param time The time.
/// @return The seconds.
long long epochSeconds(std::chrono::system_clock::time_point time) {
	return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

/// @brief A sign-in an earlier run kept.
struct Kept {
	std::string refreshToken; ///< Its refresh token.
	std::string account;      ///< The account it named.
};

/// @brief Reads the sign-in an earlier run kept, if it was with the same provider.
/// @param store Where it was kept.
/// @param key Its key there.
/// @param provider The provider now.
/// @return It, or nullopt if none was kept, it can't be read, or it was with another provider.
std::optional<Kept> readKept(SecretStore& store, const std::string& key, const identity::Provider& provider) {
	const auto kept = store.read(key);
	const json secret = kept ? json::parse(*kept, nullptr, false) : json();
	if(!secret.is_object() || secret.value("tokenUrl", "") != provider.tokenUrl ||
	   secret.value("clientId", "") != provider.clientId || !secret["refresh"].is_string())
		return std::nullopt;
	return Kept{secret["refresh"].get<std::string>(), secret.value("account", "")};
}

} // namespace

SignIn::SignIn(Executor& executor, network::Network& network, SignInOptions options)
    : executor_(executor), network_(network), options_(std::move(options)),
      timer_(std::make_unique<OAuthTimer>(executor)) {
	if(!options_.store) options_.store = std::make_shared<MemorySecretStore>();
	if(!options_.openBrowser) options_.openBrowser = openInBrowser;
}

SignIn::~SignIn() {
	onStatus_ = nullptr;
	entries_.clear(); // cancels the requests and the listeners before the timer goes
}

/// @brief Declares the providers a document names; a provider that changed signs out (see Identity::declare).
void SignIn::declare(const std::vector<std::pair<std::string, identity::Provider>>& providers) {
	std::map<std::string, std::unique_ptr<Entry>> next;
	for(const auto& [name, provider] : providers) {
		const auto found = entries_.find(name);
		if(found != entries_.end() && found->second->provider == provider) {
			next.emplace(name, std::move(found->second));
			continue;
		}
		if(found != entries_.end()) options_.store->erase(key(name)); // a changed provider signs out
		auto entry = std::make_unique<Entry>();
		entry->name = name;
		entry->provider = provider;
		next.emplace(name, std::move(entry));
	}
	auto removed = std::exchange(entries_, std::move(next));
	for(auto& [name, entry] : removed)
		if(entry) {
			stop(*entry);
			finish(*entry, "the sign-in '" + name + "' was removed");
		}
	for(auto& [name, entry] : entries_) {
		if(entry->state != "signedOut" || !entry->refreshToken.empty()) continue;
		auto kept = readKept(*options_.store, key(name), entry->provider);
		if(!kept) continue;
		entry->refreshToken = std::move(kept->refreshToken);
		entry->account = std::move(kept->account);
		entry->state = "signingIn";
		refresh(*entry);
	}
	changed();
}

/// @brief Starts signing in, or resumes a kept sign-in; returns at once (see Identity::signIn).
void SignIn::signIn(const std::string& name, std::function<void(std::optional<std::string> error)> done) {
	const auto found = entries_.find(name);
	if(found == entries_.end()) {
		executor_.post([done = std::move(done), name] {
			if(done) done("there is no sign-in named '" + name + "'");
		});
		return;
	}
	Entry& entry = *found->second;
	if(entry.state == "signedIn" && bearer(name)) {
		executor_.post([done = std::move(done)] {
			if(done) done(std::nullopt);
		});
		return;
	}
	if(done) entry.waiting.push_back(std::move(done));
	if(entry.state == "signingIn") return;
	entry.state = "signingIn";
	entry.error.clear();
	if(!entry.refreshToken.empty()) refresh(entry);
	else if(entry.provider.flow == "device") startDevice(entry);
	else startBrowser(entry);
	changed();
}

/// @brief Forgets the tokens, here and in the SecretStore, and stops a sign-in in progress (see Identity::signOut).
void SignIn::signOut(const std::string& name) {
	const auto found = entries_.find(name);
	if(found == entries_.end()) return;
	Entry& entry = *found->second;
	stop(entry);
	options_.store->erase(key(name));
	entry.accessToken.clear();
	entry.refreshToken.clear();
	entry.account.clear();
	entry.error.clear();
	entry.state = "signedOut";
	log(LogLevel::Info, "AffrescoUI sign-in: '" + name + "' signed out");
	finish(entry, "signed out");
}

/// @brief The access token to send, for the transport only: never shown or logged (see Identity::bearer).
std::optional<std::string> SignIn::bearer(const std::string& name) const {
	const auto found = entries_.find(name);
	if(found == entries_.end() || found->second->accessToken.empty()) return std::nullopt;
	if(found->second->expires <= std::chrono::system_clock::now()) return std::nullopt;
	return found->second->accessToken;
}

void SignIn::refused(const std::string& name) {
	const auto found = entries_.find(name);
	if(found == entries_.end() || found->second->state != "signedIn") return;
	Entry& entry = *found->second;
	if(entry.refreshToken.empty() || entry.refusedOnce) {
		fail(entry, "the server refused the sign-in");
		return;
	}
	entry.refusedOnce = true; // refreshes once; refused again, it signs out
	entry.accessToken.clear();
	entry.state = "signingIn";
	refresh(entry);
	changed();
}

json SignIn::status() const {
	json status = json::object();
	for(const auto& [name, entry] : entries_) {
		json one = {{"state", entry->state}, {"account", entry->account}, {"expires", nullptr},
		            {"error", entry->error}, {"code", entry->userCode}, {"verificationUrl", entry->verificationUrl}};
		if(!entry->accessToken.empty()) one["expires"] = epochSeconds(entry->expires);
		status[name] = std::move(one);
	}
	return status;
}

/// @brief Sets what runs, on the main thread, after a sign-in's state changes (see Identity::onStatus).
void SignIn::onStatus(std::function<void()> callback) { onStatus_ = std::move(callback); }

void SignIn::startBrowser(Entry& entry) {
	if(const auto refused = network_.check(entry.provider.tokenUrl)) {
		fail(entry, *refused);
		return;
	}
	entry.pending = {oauth::randomText(32), oauth::randomText(16), oauth::randomText(16), {}};
	std::string error;
	entry.redirect = LoopbackRedirect::open(
	    executor_, entry.pending.state,
	    [this, &entry](std::optional<std::string> code, std::string why) {
		    entry.redirect.reset(); // this callback's owner: it is not called again
		    if(!code) {
			    fail(entry, why);
			    return;
		    }
		    post(entry, entry.provider.tokenUrl,
		         {{"grant_type", "authorization_code"},
		          {"code", *code},
		          {"redirect_uri", entry.pending.redirectUri},
		          {"client_id", entry.provider.clientId},
		          {"code_verifier", entry.pending.verifier}},
		         [this, &entry](std::optional<std::string> body, const std::string&, const std::string& failed) {
			         if(body) accept(entry, *body, entry.pending.nonce);
			         else fail(entry, failed);
		         });
	    },
	    error);
	if(!entry.redirect) {
		fail(entry, "couldn't wait for the browser: " + error);
		return;
	}
	entry.pending.redirectUri = entry.redirect->uri();
	options_.openBrowser(oauth::authorizationUrl(entry.provider, entry.pending));
}

void SignIn::refresh(Entry& entry) {
	post(entry, entry.provider.tokenUrl,
	     {{"grant_type", "refresh_token"}, {"refresh_token", entry.refreshToken}, {"client_id", entry.provider.clientId}},
	     [this, &entry](std::optional<std::string> body, const std::string&, const std::string& error) {
		     if(body) accept(entry, *body, {});
		     else fail(entry, "couldn't refresh the sign-in: " + error); // signs out cleanly
	     });
}

void SignIn::post(Entry& entry, const std::string& url, const std::vector<std::pair<std::string, std::string>>& fields,
                  std::function<void(std::optional<std::string>, std::string, std::string)> done) {
	network::Request request;
	request.method = "POST";
	request.url = url;
	request.headers = {{"Content-Type", "application/x-www-form-urlencoded"}, {"Accept", "application/json"}};
	request.body = oauth::form(fields);
	request.maxBodyBytes = oauth::maxResponseBytes;
	const auto shared = std::make_shared<decltype(done)>(std::move(done));
	// The entry owns the transfer: destroying it (stop(), signOut(), the entry going) cancels the handlers
	entry.transfer = network_.start(
	    std::move(request),
	    {.done = [shared](network::Response response) { (*shared)(std::move(response.body), {}, {}); },
	     .failed =
	         [shared](network::Failure failure) {
		         const std::string why = failure.code == "status" ? oauth::errorCode(failure.body) : std::string();
		         (*shared)(std::nullopt, std::move(failure.body),
		                   why.empty() ? failure.message : "the provider answered " + why);
	         },
	     .event = {}});
}

void SignIn::accept(Entry& entry, const std::string& body, const std::string& nonce) {
	auto read = oauth::readTokens(body, entry.provider, nonce, std::chrono::system_clock::now());
	if(const auto* error = std::get_if<std::string>(&read)) {
		fail(entry, *error);
		return;
	}
	auto& tokens = std::get<oauth::Tokens>(read);
	stop(entry);
	entry.accessToken = std::move(tokens.access);
	if(!tokens.refresh.empty()) entry.refreshToken = std::move(tokens.refresh);
	if(!tokens.account.empty()) entry.account = std::move(tokens.account);
	entry.expires = tokens.expires;
	entry.state = "signedIn";
	entry.error.clear();
	entry.userCode.clear();
	entry.verificationUrl.clear();
	keep(entry);
	if(!entry.refreshToken.empty()) {
		// Refreshed a minute before it expires, so requests never carry an expired token
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
		    entry.expires - std::chrono::system_clock::now() - std::chrono::minutes{1});
		entry.timer = timer_->after(std::max(left, std::chrono::milliseconds{1000}), [this, &entry] {
			entry.timer = 0;
			entry.refusedOnce = false;
			refresh(entry);
		});
	}
	log(LogLevel::Info, "AffrescoUI sign-in: '" + entry.name + "' signed in");
	finish(entry, std::nullopt);
}

void SignIn::fail(Entry& entry, const std::string& error) {
	stop(entry);
	options_.store->erase(key(entry.name));
	entry.accessToken.clear();
	entry.refreshToken.clear();
	entry.userCode.clear();
	entry.verificationUrl.clear();
	entry.account.clear();
	entry.state = entry.waiting.empty() ? "signedOut" : "error";
	entry.error = error;
	log(LogLevel::Warning, "AffrescoUI sign-in: '" + entry.name + "' failed: " + error);
	finish(entry, error);
}

void SignIn::stop(Entry& entry) {
	entry.transfer.reset();
	entry.redirect.reset();
	if(entry.timer) timer_->cancel(entry.timer);
	entry.timer = 0;
}

void SignIn::finish(Entry& entry, const std::optional<std::string>& error) {
	const auto waiting = std::move(entry.waiting);
	entry.waiting.clear();
	changed();
	for(const auto& done : waiting) done(error);
}

void SignIn::keep(const Entry& entry) {
	if(entry.refreshToken.empty()) return;
	const json secret = {{"refresh", entry.refreshToken},
	                     {"account", entry.account},
	                     {"tokenUrl", entry.provider.tokenUrl},
	                     {"clientId", entry.provider.clientId}};
	if(!options_.store->write(key(entry.name), secret.dump()))
		log(LogLevel::Warning, "AffrescoUI sign-in: couldn't keep the sign-in of '" + entry.name + "' in the " +
		                           options_.store->name() + " secret store");
}

std::string SignIn::key(const std::string& name) const { return "affrescoui/" + options_.application + "/" + name; }

void SignIn::changed() const {
	if(onStatus_) onStatus_();
}

} // namespace affresco::net
