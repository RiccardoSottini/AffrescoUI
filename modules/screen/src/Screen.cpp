/// @file
/// @brief affresco::Screen, the composition root: puts the format's rules, the repository, the runtime and the peers
/// together with the services, and adds includes, declared channels and the "persist" state file.

#include "affresco/Screen.hpp"
#include "OffscreenImages.hpp"

#include "affresco/channel/Session.hpp"
#include "affresco/channel/Sockets.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/format/Rules.hpp"
#include "affresco/format/Runtime.hpp"
#include "affresco/net/RemotePeers.hpp"
#include "affresco/vcs/Repository.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace affresco {

namespace {

/// @brief A program connected to the screen.
struct Peer {
	std::unique_ptr<Channel> owned;            ///< Its channel, if it came through a socket.
	Channel* channel = nullptr;                ///< Its channel.
	std::unique_ptr<channel::Session> session; ///< Its protocol session.
};

/// @brief A channel the file declares: a program the screen talks to over a local socket.
struct DeclaredChannel {
	std::string name;                               ///< Its name in the file: actions "<name>:<type>".
	std::filesystem::path path;                     ///< The socket.
	double reconnectMs = 0.0;                       ///< Wait before connecting again (0: never).
	std::unique_ptr<channel::SocketChannel> socket; ///< While connected.
	std::uint64_t retry = 0;                        ///< The reconnect timer, if waiting.
};

} // namespace

/// @brief The modules and the peers.
///
/// Declaration order is construction order: the services, the rules, then the repository that uses them, then the
/// runtime and the peers that use the repository (destroyed the other way round; servers stop before their peers go,
/// and the runtime lets go of the services before they end).
/// @brief The executor of the Screen's network: it forwards to the Screen's current executor, which the host may set
/// after the network is made. Threads: post() from any, set() from the main thread.
class RelayExecutor final : public Executor {
public:
	/// @brief Forwards a task to the current executor, or drops it if there is none.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		if(target_) target_->post(std::move(task));
	}
	/// @brief Sets the executor tasks go to.
	/// @param target The executor, or nullptr.
	void set(Executor* target) noexcept {
		const std::scoped_lock lock(mutex_);
		target_ = target;
	}

private:
	std::mutex mutex_;          ///< Guards target_.
	Executor* target_ = nullptr; ///< See set().
};

struct Screen::Impl {
	services::ServiceRegistry services;                          ///< What "services" may use (RFC 0002).
	RelayExecutor relay;                                         ///< The network's executor.
	std::unique_ptr<net::Client> network;                        ///< See Screen::network(); outlives the runtime.
	format::Rules rules;                                         ///< The format's rules.
	vcs::Repository repository;                                  ///< Document, state, history, review.
	format::Runtime runtime;                                     ///< Panels and actions.
	detail::OffscreenImages images{repository, runtime};         ///< Pictures for peers ("render").
	std::vector<std::unique_ptr<Peer>> peers;                    ///< Connected programs.
	std::vector<std::unique_ptr<channel::SocketServer>> servers; ///< From listen().
	std::vector<std::unique_ptr<net::PeerServer>> peerServers;   ///< From listenWebSocket().
	Signal<const std::string&> events;                           ///< "emit" events, as JSON text.
	std::shared_ptr<bool> alive = std::make_shared<bool>(true);  ///< Guards removals still queued after destruction.

	Executor* executor = nullptr;                    ///< For channels (set by startChannels()).
	std::filesystem::path socketDirectory;           ///< Where relative channel sockets are.
	std::map<std::string, DeclaredChannel> channels; ///< From "channels".
	std::filesystem::path stateFile;                 ///< Where "persist" paths are kept.
	json saved;                                      ///< What was last loaded or written there.

	/// @brief Builds the modules and forwards runtime events and errors to every peer.
	/// @param document The screen.
	/// @param baseDirectory Where relative paths are resolved from.
	/// @throws DocumentError If the screen is invalid.
	Impl(json document, std::filesystem::path baseDirectory)
	    : repository(rules, std::move(document)), runtime(repository, std::move(baseDirectory)) {
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
				if(!it->second.socket) open(it->second);
			} else {
				runtime.onError("AffrescoUI screen: connect: no channel '" + name + "' in \"channels\"");
			}
		});
		runtime.onUpdated.connect([this] { save(); });
		rules.setServices(&services);
		runtime.setServices(&services);

		socketDirectory = Screen::defaultSocketDirectory();
	}

	Impl(const Impl&) = delete;            ///< Callbacks hold `this`: it never copies.
	Impl& operator=(const Impl&) = delete; ///< Callbacks hold `this`: it never copies.
	Impl(Impl&&) = delete;                 ///< Callbacks hold `this`: it never moves (the Screen moves its pointer).
	Impl& operator=(Impl&&) = delete;      ///< Callbacks hold `this`: it never moves.

	/// @brief Stops the servers and the reconnect timers before the peers and modules go.
	~Impl() {
		*alive = false;
		servers.clear();
		peerServers.clear();
		if(Window* window = runtime.window()) // the reconnect timers point at the screen
			for(const auto& [name, channel] : channels)
				if(channel.retry) window->cancelTimer(channel.retry);
		channels.clear();
	}

	/// @brief Logs what the screen refers to but doesn't define, as warnings: the screen still opens. Called when it
	/// is first built, once the application has registered its actions.
	void warnOfFindings() const {
		for(const std::string& finding : rules.findings(repository.document(), repository.state()))
			affresco::log(LogLevel::Warning, "AffrescoUI screen: " + finding);
	}

	/* Channels */

	/// @brief Runs a channel's action if the file has it ("host:committed" for a "committed" message on "host").
	/// @param action The action.
	/// @param event Its event.
	void run(const std::string& action, const json& event) {
		if(Window* window = runtime.window(); window && runtime.hasAction(action))
			runtime.runAction(action, window->root(), event);
	}

	/// @brief Connects a declared channel; on failure runs "<name>:closed" and tries again later if it should.
	/// @param channel The channel.
	void open(DeclaredChannel& channel) {
		if(!executor) return;
		if(Window* window = runtime.window(); window && channel.retry)
			window->cancelTimer(std::exchange(channel.retry, 0));
		try {
			channel.socket = channel::SocketChannel::connect(*executor, channel.path);
		} catch(const std::exception& error) {
			run(channel.name + ":closed", {{"type", "closed"}, {"message", error.what()}});
			retry(channel);
			return;
		}
		const std::string name = channel.name;
		channel.socket->onMessage.connect([this, name](const std::string& line) {
			json message;
			try {
				message = parseUntrusted(line);
			} catch(const std::exception&) {
				return; // not a message: nothing to run
			}
			if(message.is_object() && message.contains("type") && message["type"].is_string())
				run(name + ":" + message["type"].get<std::string>(), message);
		});
		channel.socket->onClosed.connect([this, name, guard = alive] {
			// Dropped after the signal that says so has finished
			executor->post([this, name, guard] {
				if(!*guard) return;
				DeclaredChannel& c = channels.at(name);
				c.socket.reset();
				run(name + ":closed", {{"type", "closed"}, {"message", "The other end closed the connection"}});
				retry(c);
			});
		});
		run(name + ":open", {{"type", "open"}});
	}

	/// @brief Schedules a reconnect, if the channel has "reconnect".
	/// @param channel The channel.
	void retry(DeclaredChannel& channel) {
		Window* window = runtime.window();
		if(!window || channel.reconnectMs <= 0.0 || channel.retry) return;
		channel.retry = window->setTimer(channel.reconnectMs, [this, name = channel.name] {
			DeclaredChannel& c = channels.at(name);
			c.retry = 0;
			if(!c.socket) open(c);
		});
	}

	/// @brief Sends a message on a declared channel ("send" steps); if it isn't connected, runs "<name>:error".
	/// @param name The channel.
	/// @param message The message.
	void send(const std::string& name, const json& message) {
		const auto it = channels.find(name);
		if(it == channels.end()) {
			runtime.onError("AffrescoUI screen: send: no channel '" + name + "' in \"channels\"");
			return;
		}
		if(!it->second.socket || !it->second.socket->isOpen()) {
			run(name + ":error", {{"type", "error"}, {"message", "Not connected to '" + name + "'"}});
			return;
		}
		it->second.socket->send(message.dump());
	}

	/* Persistence */

	/// @brief The "persist" paths as JSON Pointers into the state: "state.habits" is "/habits".
	/// @return The pointers.
	std::vector<json::json_pointer> persisted() const {
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

	/// @brief The persisted part of the live state.
	/// @return {pointer: value}.
	json persistedState() {
		json result = json::object();
		for(const auto& pointer : persisted())
			if(repository.state().contains(pointer)) result[pointer.to_string()] = repository.state()[pointer];
		return result;
	}

	/// @brief Reads the state file into the live state (the paths it has).
	void load() {
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
		if(file.is_object())
			for(const auto& pointer : persisted())
				if(file.contains(pointer.to_string())) repository.state()[pointer] = file[pointer.to_string()];
		saved = persistedState();
	}

	/// @brief Writes the persisted part of the state if it changed since it was loaded or last written.
	void save() {
		if(stateFile.empty() || persisted().empty()) return; // no state file: a screen from fromString()
		if(images.rendering()) return;                       // a proposal's state, shown only for a picture
		json now = persistedState();
		if(now == saved) return;
		std::error_code error;
		std::filesystem::create_directories(stateFile.parent_path(), error);
		std::ofstream out(stateFile);
		if(!out) {
			std::ostringstream message; // a path prints quoted, as it always has
			message << "AffrescoUI screen: could not write " << stateFile;
			affresco::log(LogLevel::Error, message.str());
			return;
		}
		out << now.dump(2) << '\n';
		saved = std::move(now);
	}

	/// @brief Sends a message to every peer.
	/// @param message The message.
	/// @param except A peer to skip (may be nullptr).
	void broadcast(const json& message, const Peer* except = nullptr) {
		for(const auto& peer : peers)
			if(peer.get() != except) peer->session->send(message);
	}

	/// @brief Adds a peer and relays its "request"/"reply" messages to the others.
	/// @param channel Its channel.
	/// @param owned Its channel, if the screen owns it (may be null).
	/// @param author The name it is authored as.
	/// @return The peer.
	Peer& add(Channel& channel, std::unique_ptr<Channel> owned, std::string author) {
		auto peer = std::make_unique<Peer>();
		peer->owned = std::move(owned);
		peer->channel = &channel;
		peer->session = std::make_unique<channel::Session>(
		    channel, repository, std::move(author), [this] { return runtime.catalog(); }, &images);

		// A request in plain words from one peer (e.g. the user's client) is for the others (e.g. an AI agent)
		peer->session->onRelay.connect([this, sender = peer.get()](const json& message) {
			if(peers.size() < 2) {
				sender->session->send({{"type", "error"},
				                       {"message", "nobody else is connected to receive this " +
				                                       message.value("type", std::string("message"))},
				                       {"id", message.value("id", json())}});
				return;
			}
			broadcast(message, sender);
		});
		peers.push_back(std::move(peer));
		return *peers.back();
	}

	/// @brief Adds a peer whose channel the screen owns, and removes it once its channel closes.
	/// @param mainThread Runs the removal after the signal that says so has finished.
	/// @param connection The channel.
	/// @param name The name it is authored as.
	void adopt(Executor& mainThread, std::unique_ptr<Channel> connection, std::string name) {
		Channel& channel = *connection;
		Peer& peer = add(channel, std::move(connection), std::move(name));
		channel.onClosed.connect([this, &mainThread, peer = &peer] {
			mainThread.post([this, peer, guard = alive] {
				if(*guard) remove(peer);
			});
		});
	}

	/// @brief Removes a peer that went away.
	/// @param peer The peer.
	void remove(const Peer* peer) {
		std::erase_if(peers, [peer](const auto& p) { return p.get() == peer; });
	}
};

namespace {

/// @brief Reads an environment variable that holds a path.
/// @param name The variable's name, in ASCII.
/// @return Its value, or nothing if it is not set or empty.
std::optional<std::filesystem::path> environmentPath(std::string_view name) {
#ifdef _WIN32
	// Read in UTF-16: the ANSI code page can't hold every user name, and profile paths contain it.
	std::wstring wideName;
	for(const char letter : name) wideName.push_back(static_cast<wchar_t>(letter));
	wchar_t* value = nullptr;
	std::size_t length = 0;
	if(_wdupenv_s(&value, &length, wideName.c_str()) != 0 || !value) return std::nullopt;
	const std::unique_ptr<wchar_t, void (*)(void*)> freer(value, [](void* memory) { std::free(memory); });
#else
	const char* value = std::getenv(std::string(name).c_str());
	if(!value) return std::nullopt;
#endif
	if(*value == 0) return std::nullopt;
	return std::filesystem::path(value);
}

/// @brief The folder for application data of this platform, where screens' data is kept by default.
/// @return %APPDATA%\\affrescoui on Windows, ~/.affrescoui elsewhere (in the current folder without a home).
std::filesystem::path platformDataDirectory() {
#ifdef _WIN32
	return environmentPath("APPDATA").value_or(".") / "affrescoui";
#else
	return environmentPath("HOME").value_or(".") / ".affrescoui";
#endif
}

/// @brief Reads an included file.
/// @param path The file.
/// @return Its JSON object.
/// @throws ScreenError If it can't be read, isn't a JSON object, or nests too deeply.
json readInclude(const std::filesystem::path& path) {
	const std::ifstream file(path);
	if(!file) throw ScreenError("AffrescoUI screen: include: could not open " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	json part;
	try {
		part = parseUntrusted(text.str());
	} catch(const json::parse_error&) { // NOLINT(bugprone-empty-catch): reported below, as not a JSON object
	} catch(const DocumentError& deep) {
		throw ScreenError("AffrescoUI screen: include: " + path.string() + ": " + deep.what());
	}
	if(!part.is_object()) throw ScreenError("AffrescoUI screen: include: " + path.string() + " is not a JSON object");
	return part;
}

/// @brief Merges two objects key by key: `part`'s entries come first and `document`'s win.
/// @param document The including file's object, which receives the merge.
/// @param part The included file's.
void mergeObject(json& document, const json& part) {
	json merged = part;
	merged.update(document);
	document = std::move(merged);
}

/// @brief Merges one included file into a screen: each of its sections comes first and the screen's entries win;
/// the screen's own root and window (and format and version) are kept, and an include's "$schema" is dropped. Menus
/// merge one level deeper, menu by menu, so several files can add items to the same menu.
/// @param document The screen.
/// @param part The included file, its own includes already merged.
void mergeInclude(json& document, const json& part) {
	static constexpr std::array<std::string_view, 5> screensOwn = {"$schema", "format", "version", "root", "window"};
	for(const auto& [key, value] : part.items()) {
		if(std::ranges::find(screensOwn, key) != screensOwn.end()) {
			if(!document.contains(key) && key != "$schema") document[key] = value;
		} else if(!document.contains(key)) {
			document[key] = value;
		} else if(value.is_object() && document[key].is_object()) {
			json& section = document[key];
			if(key == "menus")
				for(const auto& [menu, items] : value.items())
					if(items.is_object() && section.contains(menu) && section[menu].is_object())
						mergeObject(section[menu], items);
			mergeObject(section, value);
		}
	}
}

/// @brief Merges a screen's "include" files into it (recursively, at most 8 deep): each section of an included file
/// comes first and the including file's entries win; root and window come from the including file.
/// @param document The screen.
/// @param baseDirectory Where its include paths are resolved from.
/// @param depth Includes inside includes so far.
/// @throws ScreenError If an include can't be read.
void resolveIncludes(json& document, const std::filesystem::path& baseDirectory, int depth = 0) {
	if(!document.is_object() || !document.contains("include")) return;
	if(depth > 8) throw ScreenError("AffrescoUI screen: include: nested too deeply (a file includes itself?)");
	const json includes = document["include"].is_string() ? json::array({document["include"]}) : document["include"];
	document.erase("include");
	if(!includes.is_array()) throw ScreenError("AffrescoUI screen: include: expected a list of files");

	for(const json& name : includes) {
		if(!name.is_string()) throw ScreenError("AffrescoUI screen: include: expected file names");
		const std::filesystem::path path = baseDirectory / name.get<std::string>();
		json part = readInclude(path);
		resolveIncludes(part, path.parent_path(), depth + 1);
		mergeInclude(document, part);
	}
}

/// @brief Where a screen file's data on disk is kept: its "persist" state and its history.
/// @param dataDirectory The data folder (Screen::defaultDataDirectory()).
/// @param screenFile The screen file.
/// @param suffix What is kept: ".state.json" or ".history".
/// @return <data folder>/<the file's name without ".screen.json"><suffix>: "habits.screen.json" -> "habits.history".
std::filesystem::path stateLocation(const std::filesystem::path& dataDirectory, const std::filesystem::path& screenFile,
                                    std::string_view suffix) {
	std::string name = screenFile.filename().string();
	for(const char* extension : {".json", ".screen"})
		if(name.ends_with(extension)) name.resize(name.size() - std::string_view(extension).size());
	return dataDirectory / (name + std::string(suffix));
}

/// @brief Parses JSON text.
/// @param text The text.
/// @param what What it is, for the error.
/// @param error Receives the error, if any (may be nullptr).
/// @return The value, or std::nullopt.
std::optional<json> parseJson(std::string_view text, const char* what, std::string* error) {
	try {
		return parseUntrusted(text);
	} catch(const json::parse_error& parseError) {
		if(error) *error = std::string("AffrescoUI screen: invalid ") + what + " JSON: " + parseError.what();
	} catch(const DocumentError& deep) {
		if(error) *error = deep.what();
	}
	return std::nullopt;
}

/// @brief Turns an optional error into a bool.
/// @param failure The error, if any.
/// @param error Receives it (may be nullptr).
/// @return true if there was no error.
bool succeeded(const std::optional<std::string>& failure, std::string* error) {
	if(failure && error) *error = *failure;
	return !failure;
}

} // namespace

Screen::Screen(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Screen::Screen(Screen&&) noexcept = default;
Screen& Screen::operator=(Screen&&) noexcept = default;
Screen::~Screen() = default;

std::filesystem::path Screen::defaultDataDirectory() {
	return environmentPath("AFFRESCOUI_DATA_DIR").value_or(platformDataDirectory());
}

Screen Screen::fromFile(const std::filesystem::path& path) { return fromFile(path, defaultDataDirectory()); }

Screen Screen::fromFile(const std::filesystem::path& path, const std::filesystem::path& dataDirectory,
                        History history) {
	const std::ifstream file(path);
	if(!file) {
		throw ScreenError(
		    "AffrescoUI screen: could not open " + std::filesystem::absolute(path).string() +
		    " (relative paths start from the current folder: " + std::filesystem::current_path().string() + ")");
	}

	std::stringstream content;
	content << file.rdbuf();
	Screen screen = fromString(content.str(), path.parent_path());
	// The history first: it sets the live state that the "persist" paths are then loaded into
	if(history == History::OnDisk) screen.setHistoryDirectory(stateLocation(dataDirectory, path, ".history"));
	screen.setStateFile(stateLocation(dataDirectory, path, ".state.json"));
	return screen;
}

Screen Screen::fromString(std::string_view content, std::filesystem::path baseDirectory) {
	json document;
	try {
		document = parseUntrusted(content); // a ScreenError if it nests too deeply
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("AffrescoUI screen: invalid JSON: ") + error.what());
	}
	resolveIncludes(document, baseDirectory);
	return Screen(std::make_unique<Impl>(std::move(document), std::move(baseDirectory)));
}

/* Runtime */

void Screen::registerType(std::string name, Factory factory, std::vector<std::string> properties,
                          ApplyProperties apply) {
	format::Runtime::ApplyProperties forward;
	if(apply) forward = [apply = std::move(apply)](Panel& panel, const json& own) { apply(panel, own.dump()); };
	impl_->runtime.registerType(std::move(name), std::move(factory), std::move(properties), std::move(forward));
}

void Screen::setPlaceholderTypes(bool enabled) { impl_->runtime.setPlaceholderTypes(enabled); }

Window& Screen::createWindow(Application& app) {
	impl_->warnOfFindings();
	Window& window = impl_->runtime.createWindow(app);
	startChannels(app);
	return window;
}

std::filesystem::path Screen::defaultSocketDirectory() {
	if(auto directory = environmentPath("AFFRESCOUI_SOCKET_DIR")) return *std::move(directory);
#ifdef _WIN32
	// Inside the user's profile, which other users can't write to, so nobody else can make the folder first.
	if(auto local = environmentPath("LOCALAPPDATA")) return *std::move(local) / "affrescoui" / "sockets";
	return std::filesystem::temp_directory_path() / "affrescoui" / "sockets"; // per user too: %LOCALAPPDATA%\Temp
#else
	// In /tmp, so that socket paths stay within their limit of about 100 bytes. The uid keeps users apart, and the
	// examples refuse the folder if another user made it first (examples/SocketDir.hpp).
	return std::filesystem::path("/tmp") / ("affrescoui-" + std::to_string(getuid()));
#endif
}

void Screen::setSocketDirectory(std::filesystem::path directory) { impl_->socketDirectory = std::move(directory); }

void Screen::startChannels(Executor& executor) {
	Impl& impl = *impl_;
	impl.executor = &executor;
	impl.services.setExecutor(&executor);
	impl.relay.set(&executor);
	const json& document = impl.repository.document();
	if(!document.contains("channels")) return;
	for(const auto& [name, spec] : document["channels"].items()) {
		if(impl.channels.contains(name)) continue;
		DeclaredChannel& channel = impl.channels[name];
		channel.name = name;
		const std::filesystem::path socket = spec["socket"].get<std::string>();
		channel.path = socket.is_absolute() ? socket : impl.socketDirectory / socket;
		const json& reconnect = spec.value("reconnect", json(false));
		channel.reconnectMs = reconnect.is_object()   ? reconnect.value("after", 2000.0)
		                      : reconnect == true     ? 2000.0
		                      : reconnect.is_number() ? reconnect.get<double>()
		                                              : 0.0;
		impl.open(channel);
	}
}

void Screen::setHistoryDirectory(const std::filesystem::path& directory) {
	const vcs::Repository::OpenedHistory opened = impl_->repository.openHistory(directory);
	if(!opened.setAside.empty())
		affresco::log(LogLevel::Warning, "AffrescoUI history: the screen file changed since " + directory.string() +
		                                     " was written: a new history starts there, and the old one is kept in " +
		                                     opened.setAside.string());
	if(!opened.memoryOnly.empty())
		affresco::log(LogLevel::Warning, "AffrescoUI history: " + directory.string() + ": " + opened.memoryOnly +
		                                     "; changes to this screen are kept in memory only");
}

void Screen::setStateFile(std::filesystem::path file) {
	impl_->stateFile = std::move(file);
	impl_->load();
}

std::string Screen::catalogJson() const { return impl_->runtime.catalog().dump(); }

json Screen::schema() const { return impl_->runtime.schema(); }

std::string Screen::referenceTables(std::string_view markdown) const {
	return impl_->runtime.referenceTables(markdown);
}

void Screen::onCall(std::string name, Call call) {
	impl_->runtime.onCall(std::move(name), [call = std::move(call)](const json& arguments) {
		const std::string result = call(arguments.dump());
		return result.empty() ? json() : json::parse(result);
	});
}

void Screen::onAsyncCall(std::string name, AsyncCall call) {
	impl_->runtime.onAsyncCall(std::move(name),
	                           [call = std::move(call)](const json& arguments, CallProgress& progress) {
		                           const std::string result = call(arguments.dump(), progress);
		                           return result.empty() ? json() : json::parse(result);
	                           });
}

void Screen::setExecutor(Executor* executor) noexcept {
	impl_->runtime.setExecutor(executor);
	impl_->services.setExecutor(executor);
	impl_->relay.set(executor);
}

services::ServiceRegistry& Screen::services() noexcept { return impl_->services; }

net::Client& Screen::network() {
	Impl& impl = *impl_;
	if(!impl.network) {
		impl.network = std::make_unique<net::Client>(impl.relay);
		impl.runtime.setNetwork(impl.network.get());
	}
	return *impl.network;
}

void Screen::setModel(std::string name, std::shared_ptr<DataModel> model) {
	impl_->runtime.setModel(std::move(name), std::move(model));
}

void Screen::build(Window& window) {
	impl_->warnOfFindings();
	impl_->runtime.build(window);
}

Panel& Screen::instantiate(std::string_view name, Panel& parent) { return impl_->runtime.instantiate(name, parent); }

void Screen::onAction(std::string name, Action action) {
	impl_->rules.addNativeAction(name);
	impl_->runtime.onAction(std::move(name), std::move(action));
}

void Screen::onUnhandledAction(UnhandledAction fallback) { impl_->runtime.onUnhandledAction(std::move(fallback)); }

void Screen::runAction(std::string_view name, Panel& sender, std::string_view eventJson) {
	json event;
	try {
		event = json::parse(eventJson);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("AffrescoUI screen: runAction: invalid event JSON: ") + error.what());
	}
	impl_->runtime.runAction(name, sender, event);
}

Panel* Screen::find(std::string_view id) noexcept { return impl_->runtime.find(id); }

Color Screen::color(std::string_view name) const { return impl_->runtime.color(name); }

Color Screen::resolveColor(std::string_view value) const {
	return impl_->runtime.resolveColor(json(std::string(value)));
}

std::shared_ptr<const Font> Screen::font(std::string_view name) const { return impl_->runtime.font(name); }

void Screen::setImage(const std::string& name, const std::shared_ptr<const Bitmap>& bitmap) {
	impl_->runtime.setImage(name, bitmap);
}

void Screen::setImage(const std::string& name, int width, int height, std::span<const std::uint8_t> rgba) {
	setImage(name, Bitmap::fromPixels(width, height, rgba, Bitmap::Limits{}));
}

void Screen::allowImageDirectory(const std::filesystem::path& directory) {
	impl_->runtime.allowImageDirectory(directory);
}

void Screen::waitForImages() { impl_->runtime.waitForImages(); }

Connection Screen::onEvent(std::function<void(const std::string&)> listener) {
	return impl_->events.connect(std::move(listener));
}

/* Version control */

bool Screen::applyRevision(std::string_view revisionJson, std::string* error) {
	const auto revision = parseJson(revisionJson, "revision", error);
	return revision && succeeded(impl_->repository.commit(*revision), error);
}

RevisionResult Screen::submitRevision(std::string_view revisionJson) {
	std::string error;
	const auto parsed = parseJson(revisionJson, "revision", &error);
	if(!parsed) return {RevisionStatus::Rejected, revision(), {}, 0, error};
	return impl_->repository.submit(*parsed);
}

std::vector<ProposalInfo> Screen::proposals() const { return impl_->repository.proposals(); }

bool Screen::acceptProposal(int id, std::string_view head, std::string* error) {
	return succeeded(impl_->repository.accept(id, {"user", std::string(head), false}), error);
}

bool Screen::rejectProposal(int id, std::string_view reason) { return impl_->repository.reject(id, reason); }

bool Screen::previewProposal(int id, std::string* error) { return succeeded(impl_->repository.preview(id), error); }

void Screen::endPreview() { impl_->repository.endPreview(); }

std::optional<int> Screen::previewedProposal() const noexcept { return impl_->repository.previewing(); }

std::vector<CommitInfo> Screen::log() const { return impl_->repository.log(); }

bool Screen::restore(int revision, std::string* error) {
	try {
		return succeeded(impl_->repository.commit(impl_->repository.restoreRevision(revision, "user")), error);
	} catch(const DocumentError& failure) {
		return succeeded(failure.what(), error);
	}
}

RevisionResult Screen::submitRestore(int revision, std::string_view author) {
	try {
		return impl_->repository.submit(impl_->repository.restoreRevision(revision, author));
	} catch(const DocumentError& failure) {
		return {RevisionStatus::Rejected, this->revision(), {}, 0, failure.what()};
	}
}

bool Screen::revert(int revision, std::string* error) {
	try {
		return succeeded(impl_->repository.commit(impl_->repository.revertRevision(revision, "user")), error);
	} catch(const DocumentError& failure) {
		return succeeded(failure.what(), error);
	}
}

bool Screen::undo(std::string* error) {
	if(revision() == 0) return succeeded("AffrescoUI screen: nothing to undo", error);
	return revert(revision(), error);
}

// NOLINTNEXTLINE(bugprone-exception-escape): head() only throws if memory runs out, which ends the program anyway
int Screen::revision() const noexcept { return impl_->repository.head().number; }

std::string Screen::exportChanges(int fromRevision, std::string_view name) const {
	return impl_->repository.exportChanges(fromRevision, name).dump(2);
}

bool Screen::importChanges(std::string_view changesJson, std::string* error) {
	const auto changes = parseJson(changesJson, "changes", error);
	return changes && succeeded(impl_->repository.importChanges(*changes), error);
}

bool Screen::hasImported(std::string_view changesJson) const {
	const auto changes = parseJson(changesJson, "changes", nullptr);
	return changes && impl_->repository.hasImported(*changes);
}

std::string Screen::documentJson() const { return impl_->repository.document().dump(); }

std::string Screen::stateJson() const { return impl_->repository.state().dump(); }

VersionControl& Screen::versions() noexcept { return impl_->repository; }

/* Channel */

void Screen::connect(Channel& channel, std::string author) { impl_->add(channel, nullptr, std::move(author)); }

void Screen::listen(Executor& executor, const std::filesystem::path& path, std::string author) {
	Impl* impl = impl_.get();
	impl->servers.push_back(std::make_unique<channel::SocketServer>(
	    executor, path, [impl, &executor, author = std::move(author)](std::unique_ptr<Channel> connection) {
		    impl->adopt(executor, std::move(connection), author);
	    }));
}

std::uint16_t Screen::listenWebSocket(Executor& executor, std::string_view address,
                                      std::shared_ptr<net::TokenVerifier> verifier, net::PeerServerOptions options) {
	Impl* impl = impl_.get();
	auto server = std::make_unique<net::PeerServer>(
	    executor, address, std::move(verifier),
	    [impl, &executor](std::unique_ptr<Channel> connection, std::string author) {
		    impl->adopt(executor, std::move(connection), std::move(author));
	    },
	    std::move(options));
	const std::uint16_t port = server->port();
	impl->peerServers.push_back(std::move(server));
	return port;
}

void Screen::setMode(Mode mode) { impl_->repository.setMode(mode); }

void Screen::setModeForThisRun(Mode mode) { impl_->repository.setModeForThisRun(mode); }

} // namespace affresco
