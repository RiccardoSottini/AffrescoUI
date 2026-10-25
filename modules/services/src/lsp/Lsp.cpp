/// @file
/// @brief The `lsp` provider: a language server the host granted, spoken to in the Language Server Protocol through a
/// `jsonrpc` service. It initializes the server at the first call, keeps the documents a screen syncs (whole texts),
/// asks at positions counted as a screen counts them (from 0, columns in code points), and gives back the server's
/// results normalised and bounded (LspProtocol.hpp); the server's diagnostics are a stream. The requests servers
/// commonly send (their configuration, registrations, progress) are answered here; any other goes to the screen.

#include "lsp/LspProtocol.hpp"
#include "Providers.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::services {

namespace {

namespace fs = std::filesystem;

constexpr std::size_t mostWaiting = 64;                     ///< Calls kept while the server initializes.
constexpr std::size_t mostDocuments = 256;                  ///< Documents open at once; then the least recent closes.
constexpr std::size_t mostSubscriptions = 256;              ///< Subscriptions to the diagnostics.
constexpr std::size_t largestText = std::size_t{8} << 20;   ///< Bytes of a document's text, at most 8 MiB.
constexpr std::size_t longestName = 256;                    ///< Bytes of a rename's new name, or a symbol query.
constexpr std::size_t mostConfigurationItems = 256;         ///< Sections a `workspace/configuration` may ask for.
constexpr long long largestPosition = std::int64_t{1} << 30; ///< A line or a column, at most.

/// @brief A request a screen may make, and what it asks the server.
struct Kind {
	std::string_view name;       ///< The method, as a screen invokes it.
	std::string_view method;     ///< The LSP method.
	std::string_view capability; ///< The server capability that offers it.
	bool positioned;             ///< Whether it takes a position.
	std::string_view about;      ///< For the catalog.
};

/// @brief The requests the service knows.
constexpr std::array<Kind, 10> kinds = {{
    {"hover", "textDocument/hover", "hoverProvider", true, "what is at a position, as markdown"},
    {"completion", "textDocument/completion", "completionProvider", true, "what may be typed at a position"},
    {"signature", "textDocument/signatureHelp", "signatureHelpProvider", true, "the signature of the call around a position"},
    {"definition", "textDocument/definition", "definitionProvider", true, "where what is at a position is defined"},
    {"references", "textDocument/references", "referencesProvider", true, "where what is at a position is used"},
    {"rename", "textDocument/rename", "renameProvider", true, "the edits that rename what is at a position"},
    {"format", "textDocument/formatting", "documentFormattingProvider", false, "the edits that format a file"},
    {"symbols", "textDocument/documentSymbol", "documentSymbolProvider", false, "the symbols of a file, nested"},
    {"workspaceSymbols", "workspace/symbol", "workspaceSymbolProvider", false, "the folder's symbols that match a query"},
    {"tokens", "textDocument/semanticTokens/full", "semanticTokensProvider", false, "a file's semantic tokens, styled"},
}};

/// @brief The semantic token types asked for (those styleOfToken() knows, and the common rest).
/// @return Them; a function, since a json built at static initialization could throw where nothing can catch it.
json tokenTypes() {
	return {"namespace", "type",     "class",      "enum",   "interface", "struct",   "typeParameter", "parameter",
	        "variable",  "property", "enumMember", "event",  "function",  "method",   "macro",         "keyword",
	        "modifier",  "comment",  "string",     "number", "regexp",    "operator", "decorator",     "concept"};
}

/// @brief What the client tells a server it can do.
/// @return The ClientCapabilities.
json clientCapabilities() {
	return {{"general", {{"positionEncodings", {"utf-32", "utf-16"}}}},
	        {"workspace", {{"workspaceFolders", true}, {"configuration", true}, {"symbol", json::object()}}},
	        {"textDocument",
	         {{"synchronization", {{"didSave", false}}},
	          {"hover", {{"contentFormat", {"markdown", "plaintext"}}}},
	          {"completion", {{"completionItem", {{"snippetSupport", true}}}}},
	          {"signatureHelp", {{"signatureInformation", {{"parameterInformation", {{"labelOffsetSupport", true}}}}}}},
	          {"definition", json::object()},
	          {"references", json::object()},
	          {"rename", json::object()},
	          {"formatting", json::object()},
	          {"documentSymbol", {{"hierarchicalDocumentSymbolSupport", true}}},
	          {"publishDiagnostics", json::object()},
	          {"semanticTokens",
	           {{"requests", {{"full", true}}},
	            {"tokenTypes", tokenTypes()},
	            {"tokenModifiers", json::array()},
	            {"formats", {"relative"}}}}}}};
}

/// @brief A method of the jsonrpc service, as the client invokes it.
/// @param method The LSP method.
/// @return Its spec (the jsonrpc service takes any name).
MethodSpec methodOf(std::string_view method) {
	return {std::string(method), Effect::Run, false, false, nullptr, {}};
}

/// @brief A value of a JSON object, or null.
/// @param object The object.
/// @param key The key.
/// @return It, or a null value.
const json& member(const json& object, std::string_view key) {
	static const json none;
	if(!object.is_object()) return none;
	const auto found = object.find(key);
	return found == object.end() ? none : *found;
}

/// @brief A section of the settings a server asks for, by its dotted name.
/// @param settings The service's `settings`.
/// @param section E.g. "python.analysis"; "" for all of them.
/// @return It, or null.
json section(const json& settings, std::string_view section) {
	const json* at = &settings;
	while(!section.empty() && at->is_object()) {
		const std::size_t dot = section.find('.');
		at = &member(*at, section.substr(0, dot));
		section = dot == std::string_view::npos ? std::string_view() : section.substr(dot + 1);
	}
	return section.empty() ? *at : json();
}

/// @brief A call the service makes to its jsonrpc service, answered on the main thread.
class InnerCall final : public Call {
public:
	/// @brief Gets the result, or no result and the error's code and message. Main thread.
	using Answer = std::function<void(std::optional<json> result, std::string code, std::string message)>;
	/// @brief Gets one event of a subscription. Main thread.
	using Events = std::function<void(json event)>;

	/// @brief A call.
	/// @param arguments Its params.
	/// @param stop Its stop token: the screen's call's, or the service's own.
	/// @param executor The main thread.
	/// @param answer Gets the answer, once; may be empty.
	/// @param events Gets the events, for a subscription; may be empty.
	InnerCall(json arguments, std::stop_token stop, Executor& executor, Answer answer, Events events = {})
	    : arguments_(std::move(arguments)), stop_(std::move(stop)), executor_(executor), answer_(std::move(answer)),
	      events_(std::move(events)) {}

	const json& arguments() const noexcept override { return arguments_; }
	std::stop_token stopToken() const noexcept override { return stop_; }
	void progress(json /*progress*/) override {}
	void event(json event) override {
		if(events_ && !ended_ && !stop_.stop_requested())
			executor_.post([events = events_, event = std::move(event)]() mutable { events(std::move(event)); });
	}
	void finish(json result) override { end(std::move(result), {}, {}); }
	void fail(std::string code, std::string message, json /*data*/) override {
		end(std::nullopt, std::move(code), std::move(message));
	}

private:
	/// @brief Hands the answer to the main thread, once.
	/// @param result The result, or nothing.
	/// @param code The error's code.
	/// @param message The error's message.
	void end(std::optional<json> result, std::string code, std::string message) {
		if(ended_.exchange(true) || !answer_) return;
		executor_.post([answer = std::move(answer_), result = std::move(result), code = std::move(code),
		                message = std::move(message)]() mutable { answer(std::move(result), std::move(code), std::move(message)); });
	}

	json arguments_;              ///< See the constructor.
	std::stop_token stop_;        ///< See the constructor.
	Executor& executor_;          ///< See the constructor.
	Answer answer_;               ///< See the constructor.
	Events events_;               ///< See the constructor.
	std::atomic<bool> ended_{false}; ///< Whether it was answered.
};

/// @brief A document the server has open.
struct Document {
	std::string text;                   ///< Its text, as last sent.
	std::vector<std::size_t> lineStarts; ///< Where each of its lines starts.
	std::string languageId;             ///< Its LSP language.
	long long version = 1;              ///< The last version sent.
	std::uint64_t used = 0;             ///< When it was last used, to close the least recent.
};

/// @brief What the service knows of its server, kept on the main thread; the calls' answers find it by weak pointer.
class Client final : public std::enable_shared_from_this<Client> {
public:
	/// @brief A client of a jsonrpc service.
	/// @param configuration The `lsp` configuration.
	/// @param context The service's context.
	/// @param inner The jsonrpc service.
	Client(const json& configuration, const ServiceContext& context, std::unique_ptr<Service> inner)
	    : executor_(context.executor), root_(context.grant.root), inner_(std::move(inner)),
	      language_(configuration.value("language", std::string())),
	      options_(configuration.contains("initializationOptions") ? configuration["initializationOptions"] : json()) {}
	/// @brief Fails the calls still waiting, and ends the jsonrpc service.
	~Client() {
		stop_.request_stop();
		for(const std::shared_ptr<Call>& call : waiting_) call->fail("failed", "the service ended");
		for(const std::shared_ptr<Call>& call : subscribers_) call->fail("failed", "the service ended");
		inner_.reset();
	}
	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;
	Client(Client&&) = delete;
	Client& operator=(Client&&) = delete;

	/// @brief Starts a call: at once if the server is initialized, else once it is (initializing it first).
	/// @param method The method.
	/// @param call The call.
	/// @return An error if it can't start.
	std::optional<std::string> invoke(const std::string& method, std::shared_ptr<Call> call) {
		if(state_ == State::Ready) {
			perform(method, call);
			return std::nullopt;
		}
		if(waiting_.size() >= mostWaiting) return "the language server is still starting";
		waiting_.push_back(call);
		methods_.push_back(method);
		if(state_ == State::Down) initialize();
		return std::nullopt;
	}

	/// @brief Adds a subscriber to the diagnostics.
	/// @param call The subscriber.
	/// @return An error if there are too many.
	std::optional<std::string> subscribe(std::shared_ptr<Call> call) {
		std::erase_if(subscribers_, [](const auto& one) { return one->stopToken().stop_requested(); });
		if(subscribers_.size() >= mostSubscriptions) return "the service has 256 subscriptions already";
		subscribers_.push_back(std::move(call));
		return std::nullopt;
	}

private:
	/// @brief How far the server is.
	enum class State { Down, Starting, Ready };

	/// @brief Sends `initialize`; its answer runs the waiting calls.
	void initialize() {
		state_ = State::Starting;
		const json rootUri = root_.empty() ? json() : json(lsp::uriOf(root_, ""));
		const json folders =
		    root_.empty() ? json() : json::array({{{"uri", rootUri}, {"name", root_.filename().string()}}});
		json params = {{"processId", nullptr},
		               {"clientInfo", {{"name", "AffrescoUI"}}},
		               {"rootUri", rootUri},
		               {"workspaceFolders", folders},
		               {"capabilities", clientCapabilities()}};
		if(!options_.is_null()) params["initializationOptions"] = options_;
		request("initialize", std::move(params), stop_.get_token(),
		        [](Client& self, std::optional<json> result, const std::string& /*code*/, const std::string& error) {
			        self.initialized(std::move(result), error);
		        });
	}

	/// @brief Takes the server's `initialize` answer: says `initialized`, subscribes to its diagnostics, and runs the
	/// waiting calls (or fails them).
	/// @param result The result, or nothing.
	/// @param error Why there is none.
	void initialized(std::optional<json> result, const std::string& error) {
		std::vector<std::shared_ptr<Call>> waiting;
		std::vector<std::string> methods;
		waiting.swap(waiting_);
		methods.swap(methods_);
		if(!result || !result->is_object()) {
			state_ = State::Down;
			for(const auto& call : waiting)
				call->fail("failed", error.empty() ? "the language server didn't initialize" : error);
			return;
		}
		const json& offered = member(*result, "capabilities");
		capabilities_ = offered.is_object() ? offered : json::object();
		utf16_ = member(capabilities_, "positionEncoding") != "utf-32";
		const json& sync = member(capabilities_, "textDocumentSync");
		sends_ = !(sync == 0 || member(sync, "change") == 0);
		state_ = State::Ready;
		notify("initialized", json::object());
		const std::weak_ptr<Client> weak = weak_from_this();
		const auto diagnostics = std::make_shared<InnerCall>(
		    json(), stop_.get_token(), executor_,
		    [weak](std::optional<json> /*result*/, const std::string& code, const std::string& message) {
			    if(const auto self = weak.lock())
				    for(const auto& call : self->subscribers_) call->fail(code, message);
		    },
		    [weak](json params) {
			    if(const auto self = weak.lock()) self->published(params);
		    });
		(void)inner_->subscribe("textDocument/publishDiagnostics", diagnostics);
		for(std::size_t i = 0; i < waiting.size(); ++i) perform(methods[i], waiting[i]);
	}

	/// @brief Hands a server's diagnostics to the subscribers, normalised; those of a file outside the folder are
	/// dropped.
	/// @param params publishDiagnostics' params.
	void published(const json& params) {
		json event;
		try {
			event = lsp::normalize("diagnostics", params, root_, {}, {}, mapper());
		} catch(const std::exception&) {
			return;
		}
		std::erase_if(subscribers_, [](const auto& one) { return one->stopToken().stop_requested(); });
		for(const auto& call : subscribers_) call->event(event);
	}

	/// @brief Runs a call, the server ready.
	/// @param method The method.
	/// @param call The call.
	void perform(const std::string& method, const std::shared_ptr<Call>& call) {
		try {
			const json& with = call->arguments();
			if(method == "sync") return call->finish({{"version", sync(relative(with), with)}});
			if(method == "close") {
				close(relative(with));
				return call->finish(nullptr);
			}
			const Kind* kind = nullptr;
			for(const Kind& one : kinds)
				if(one.name == method) kind = &one;
			if(!kind) return call->fail("unsupported", "no method '" + method + "'");
			const json& offered = member(capabilities_, kind->capability);
			if(offered.is_null() || offered == false)
				return call->fail("unsupported", "the language server doesn't offer " + std::string(kind->name));
			ask(*kind, call);
		} catch(const std::exception& failure) {
			call->fail("refused", failure.what());
		}
	}

	/// @brief Sends a request's params, and normalises its answer for the call.
	/// @param kind The request.
	/// @param call The screen's call.
	/// @throws std::runtime_error If its file can't be opened.
	void ask(const Kind& kind, const std::shared_ptr<Call>& call) {
		const json& with = call->arguments();
		std::string path;
		json params = json::object();
		if(kind.name == "workspaceSymbols") params["query"] = member(with, "query").is_string() ? with["query"] : json("");
		else {
			path = relative(with);
			if(with.contains("text") || !documents_.contains(path)) (void)sync(path, with);
			params["textDocument"] = {{"uri", lsp::uriOf(root_, path)}};
		}
		if(kind.positioned) params["position"] = position(documents_.at(path), with);
		if(kind.name == "completion") params["context"] = {{"triggerKind", 1}};
		if(kind.name == "references") params["context"] = {{"includeDeclaration", true}};
		if(kind.name == "rename") params["newName"] = with.value("newName", std::string());
		if(kind.name == "format")
			params["options"] = {{"tabSize", with.value("tabSize", 4)}, {"insertSpaces", with.value("insertSpaces", false)}};
		const std::string name(kind.name);
		request(kind.method, std::move(params), call->stopToken(),
		        [call, name, path](Client& self, std::optional<json> result, const std::string& code,
		                           const std::string& error) {
			        if(!result) return call->fail(code.empty() ? "failed" : code, error);
			        if(name == "tokens" && result->is_object())
				        (*result)["legend"] = member(member(self.capabilities_, "semanticTokensProvider"), "legend");
			        try {
				        call->finish(lsp::normalize(name, *result, self.root_, path, {}, self.mapper()));
			        } catch(const std::exception& failure) {
				        call->fail("refused", failure.what());
			        }
		        });
	}

	/// @brief The file a call names, relative to the folder.
	/// @param with The call's arguments.
	/// @return Its path, with `/`, normal.
	/// @throws std::runtime_error If it names none, or one outside the folder.
	std::string relative(const json& with) const {
		const json& path = member(with, "path");
		if(!path.is_string() || path.get_ref<const std::string&>().empty()) throw std::runtime_error("expected a path");
		if(root_.empty() || !underRoot(root_, path.get_ref<const std::string&>()))
			throw std::runtime_error("the path is outside the folder");
		return fs::path(path.get<std::string>()).lexically_normal().generic_string();
	}

	/// @brief A position as the server counts it.
	/// @param document The document.
	/// @param with The call's `line` and `column`, from 0 in code points.
	/// @return `{ "line", "character" }`, the line within the document.
	json position(const Document& document, const json& with) const {
		const auto count = [&with](const char* key) {
			const json& value = member(with, key);
			return value.is_number_integer() ? static_cast<std::size_t>(std::clamp(value.get<long long>(), 0LL, largestPosition))
			                                 : std::size_t{0};
		};
		const std::size_t line = std::min(count("line"), document.lineStarts.size() - 1);
		const std::size_t column = count("column");
		return {{"line", line}, {"character", utf16_ ? lsp::utf16Units(lineOf(document, line), column) : column}};
	}

	/// @brief A line of a document.
	/// @param document The document.
	/// @param line The line, from 0, within it.
	/// @return Its text, without its line break.
	static std::string_view lineOf(const Document& document, std::size_t line) {
		const std::size_t start = document.lineStarts[line];
		const std::size_t end = line + 1 < document.lineStarts.size() ? document.lineStarts[line + 1] - 1 : document.text.size();
		return std::string_view(document.text).substr(start, end - start);
	}

	/// @brief Maps the server's columns to code points through the open documents' lines, for a UTF-16 server.
	/// @return The mapper, or none.
	lsp::ColumnMapper mapper() const {
		if(!utf16_) return {};
		return [this](const std::string& path, std::size_t line, std::size_t column) {
			const auto found = documents_.find(path);
			if(found == documents_.end() || line >= found->second.lineStarts.size()) return column;
			return lsp::codePointsOf(lineOf(found->second, line), column);
		};
	}

	/// @brief Opens a document with the server, or sends its new text: the call's `text`, or else the file's.
	/// @param path The file, relative.
	/// @param with The call's arguments: `text` and `language`, each optional.
	/// @return The version sent.
	/// @throws std::runtime_error If the text is too large, isn't UTF-8, or can't be read, or the server is busy.
	long long sync(const std::string& path, const json& with) {
		const json& given = member(with, "text");
		std::string text = given.is_string() ? given.get<std::string>() : read(path);
		if(text.size() > largestText) throw std::runtime_error("the text is over 8 MiB");
		const auto found = documents_.find(path);
		Document* document = found == documents_.end() ? nullptr : &found->second;
		if(document && document->text == text) {
			document->used = ++clock_;
			return document->version;
		}
		const json& language = member(with, "language");
		if(!document) {
			if(documents_.size() >= mostDocuments) closeLeastRecent();
			Document opened{std::move(text), {}, language.is_string() ? language.get<std::string>() : language_, 1, ++clock_};
			if(opened.languageId.empty()) opened.languageId = "plaintext";
			if(sends_)
				notify("textDocument/didOpen", {{"textDocument",
				                                 {{"uri", lsp::uriOf(root_, path)},
				                                  {"languageId", opened.languageId},
				                                  {"version", opened.version},
				                                  {"text", opened.text}}}});
			document = &documents_.emplace(path, std::move(opened)).first->second;
		} else {
			document->text = std::move(text);
			document->used = ++clock_;
			++document->version;
			if(sends_)
				notify("textDocument/didChange",
				       {{"textDocument", {{"uri", lsp::uriOf(root_, path)}, {"version", document->version}}},
				        {"contentChanges", json::array({{{"text", document->text}}})}});
		}
		document->lineStarts.assign(1, 0);
		for(std::size_t i = 0; i < document->text.size(); ++i)
			if(document->text[i] == '\n') document->lineStarts.push_back(i + 1);
		return document->version;
	}

	/// @brief A file's text, from the disk.
	/// @param path The file, relative.
	/// @return Its text.
	/// @throws std::runtime_error If it can't be read, is too large or isn't UTF-8.
	std::string read(const std::string& path) const {
		const std::optional<fs::path> file = underRoot(root_, path);
		std::error_code error;
		if(!file || !fs::is_regular_file(*file, error)) throw std::runtime_error(path + " isn't a file");
		if(fs::file_size(*file, error) > largestText || error) throw std::runtime_error(path + " is over 8 MiB");
		std::ifstream in(*file, std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if(!in && !in.eof()) throw std::runtime_error(path + " can't be read");
		if(!isUtf8(text)) throw std::runtime_error(path + " isn't UTF-8 text");
		return text;
	}

	/// @brief Closes a document with the server.
	/// @param path The file, relative.
	void close(const std::string& path) {
		if(documents_.erase(path) && sends_)
			notify("textDocument/didClose", {{"textDocument", {{"uri", lsp::uriOf(root_, path)}}}});
	}

	/// @brief Closes the document used least recently.
	void closeLeastRecent() {
		const auto oldest = std::ranges::min_element(documents_, {}, [](const auto& entry) { return entry.second.used; });
		if(oldest != documents_.end()) close(std::string(oldest->first));
	}

	/// @brief Sends a request to the jsonrpc service; its answer runs on the main thread while the client lives.
	/// @param method The LSP method.
	/// @param params Its params.
	/// @param stop The call's stop token.
	/// @param answered Gets the client and the answer.
	/// @throws std::runtime_error If the request can't be sent.
	void request(std::string_view method, json params, std::stop_token stop,
	             std::function<void(Client&, std::optional<json>, const std::string&, const std::string&)> answered) {
		const std::weak_ptr<Client> weak = weak_from_this();
		const auto call = std::make_shared<InnerCall>(
		    std::move(params), std::move(stop), executor_,
		    [weak, answered = std::move(answered)](std::optional<json> result, std::string code, std::string message) {
			    if(const auto self = weak.lock()) answered(*self, std::move(result), code, message);
		    });
		if(auto problem = inner_->invoke(methodOf(method), call)) {
			if(method == "initialize") return initialized(std::nullopt, *problem);
			throw std::runtime_error(*problem);
		}
	}

	/// @brief Sends a notification to the jsonrpc service (one its configuration lists as such).
	/// @param method The LSP method.
	/// @param params Its params.
	/// @throws std::runtime_error If it can't be sent (the server's input is full, or it ended).
	void notify(std::string_view method, json params) {
		const auto call = std::make_shared<InnerCall>(std::move(params), stop_.get_token(), executor_, InnerCall::Answer());
		if(auto problem = inner_->invoke(methodOf(method), call)) throw std::runtime_error(*problem);
	}

	Executor& executor_;                                      ///< The main thread.
	fs::path root_;                                           ///< The granted folder.
	std::unique_ptr<Service> inner_;                          ///< The jsonrpc service.
	std::string language_;                                    ///< The configuration's `language`.
	json options_;                                            ///< The configuration's `initializationOptions`.
	std::stop_source stop_;                                   ///< Stops the client's own calls when it ends.
	State state_ = State::Down;                               ///< How far the server is.
	json capabilities_ = json::object();                      ///< What it offers.
	bool utf16_ = true;                                       ///< Its columns are UTF-16 units (else code points).
	bool sends_ = true;                                       ///< It takes documents (textDocumentSync isn't None).
	std::map<std::string, Document, std::less<>> documents_;  ///< Open, by path.
	std::uint64_t clock_ = 0;                                 ///< Counts the documents' uses.
	std::vector<std::shared_ptr<Call>> waiting_;              ///< Calls waiting for the server to initialize.
	std::vector<std::string> methods_;                        ///< Their methods.
	std::vector<std::shared_ptr<Call>> subscribers_;          ///< The diagnostics' subscribers.
};

/// @brief The service: its client, which the answers find by weak pointer.
class LspService final : public Service {
public:
	/// @brief A service over a jsonrpc service.
	/// @param configuration The `lsp` configuration.
	/// @param context The service's context.
	/// @param inner The jsonrpc service.
	LspService(const json& configuration, const ServiceContext& context, std::unique_ptr<Service> inner)
	    : client_(std::make_shared<Client>(configuration, context, std::move(inner))) {}
	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		return client_->invoke(method.name, std::move(call));
	}
	std::optional<std::string> subscribe(const std::string& stream, std::shared_ptr<Call> call) override {
		if(stream != "diagnostics") return "no stream '" + stream + "'";
		return client_->subscribe(std::move(call));
	}

private:
	std::shared_ptr<Client> client_; ///< See the constructor.
};

/// @brief The `lsp` provider.
class LspProvider final : public Provider {
public:
	std::string_view name() const override { return "lsp"; }
	std::string_view about() const override {
		return "a language server the host allows, for hover, completion, definitions, references, renames, "
		       "formatting, symbols, semantic tokens and diagnostics";
	}
	json configurationSchema() const override {
		return {{"type", "object"},
		        {"required", {"process", "root"}},
		        {"additionalProperties", false},
		        {"properties",
		         {{"process", {{"type", "string"}}},
		          {"root", {{"type", "string"}}},
		          {"language", {{"type", "string"}, {"maxLength", 64}}},
		          {"initializationOptions", {{"type", "object"}}},
		          {"settings", {{"type", "object"}}}}}};
	}
	std::vector<MethodSpec> methods(const json& /*configuration*/) const override {
		const json path = {{"type", "string"}, {"minLength", 1}, {"maxLength", 4096}};
		const json text = {{"type", "string"}, {"maxLength", largestText}};
		const json language = {{"type", "string"}, {"maxLength", 64}};
		const json count = {{"type", "integer"}, {"minimum", 0}, {"maximum", largestPosition}};
		const json name = {{"type", "string"}, {"maxLength", longestName}};
		const auto object = [](json properties, json required) {
			return json{{"type", "object"},
			            {"additionalProperties", false},
			            {"required", std::move(required)},
			            {"properties", std::move(properties)}};
		};
		std::vector<MethodSpec> specs;
		specs.push_back({"sync", Effect::Read, true, false,
		                 object({{"path", path}, {"text", text}, {"language", language}}, {"path"}),
		                 "opens a file with the server, or sends its new text: `{ version }`"});
		specs.push_back({"close", Effect::Read, true, false, object({{"path", path}}, {"path"}), "closes a file"});
		for(const Kind& kind : kinds) {
			json properties = {{"path", path}, {"text", text}, {"language", language}};
			json required = {"path"};
			if(kind.positioned) {
				properties["line"] = count;
				properties["column"] = count;
				required.push_back("line");
				required.push_back("column");
			}
			if(kind.name == "rename") {
				properties["newName"] = name;
				required.push_back("newName");
			}
			if(kind.name == "format") {
				properties["tabSize"] = {{"type", "integer"}, {"minimum", 1}, {"maximum", 32}};
				properties["insertSpaces"] = {{"type", "boolean"}};
			}
			if(kind.name == "workspaceSymbols") {
				properties = {{"query", name}};
				required = json::array();
			}
			specs.push_back({std::string(kind.name), Effect::Read, true, false,
			                 object(std::move(properties), std::move(required)), std::string(kind.about)});
		}
		specs.push_back({"diagnostics", Effect::Read, true, true, nullptr, "the server's diagnostics, file by file"});
		return specs;
	}
	std::unique_ptr<Service> create(const json& configuration, ServiceContext context) override {
		const json settings = configuration.contains("settings") ? configuration["settings"] : json::object();
		const fs::path root = context.grant.root;
		const auto screen = context.request;
		ServiceContext inner{context.executor, context.grant, context.report,
		                     [settings, root, screen](std::string method, json params, Responder respond) {
			                     if(method == "workspace/configuration") {
				                     json answer = json::array();
				                     const json& items = member(params, "items");
				                     if(items.is_array())
					                     for(const json& item : items) {
						                     if(answer.size() >= mostConfigurationItems) break;
						                     const json& name = member(item, "section");
						                     answer.push_back(
						                         section(settings, name.is_string() ? name.get<std::string>() : ""));
					                     }
				                     return respond(answer, {});
			                     }
			                     if(method == "workspace/workspaceFolders")
				                     return respond(root.empty() ? json()
				                                                 : json::array({{{"uri", lsp::uriOf(root, "")},
				                                                                 {"name", root.filename().string()}}}),
				                                    {});
			                     if(method == "client/registerCapability" || method == "client/unregisterCapability" ||
			                        method == "window/workDoneProgress/create")
				                     return respond(json(), {});
			                     screen(std::move(method), std::move(params), std::move(respond));
		                     },
		                     context.now};
		const json rpc = {{"process", configuration["process"]},
		                  {"root", configuration["root"]},
		                  {"framing", "contentLength"},
		                  {"notifications",
		                   {"initialized", "textDocument/didOpen", "textDocument/didChange", "textDocument/didClose"}}};
		std::unique_ptr<Service> service = makeJsonRpcProvider()->create(rpc, std::move(inner));
		return std::make_unique<LspService>(configuration, context, std::move(service));
	}
};

} // namespace

std::unique_ptr<Provider> makeLspProvider() { return std::make_unique<LspProvider>(); }

} // namespace affresco::services
