/// @file
/// @brief Forge's language servers, native side (Lsp.hpp).

#include "features/lsp/Lsp.hpp"

#include "features/lsp/LspCall.hpp"
#include "features/lsp/LspProtocol.hpp"
#include "features/palette/FileIndex.hpp"

#include <affresco/core/Json.hpp>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace forge {

namespace {

using affresco::json;
using lsp::codePointsOf;
using lsp::positionArgument;
using lsp::ServerCall;
using lsp::textArgument;
using lsp::utf16Units;

constexpr std::size_t mostWaiting = 64;    ///< Requests kept while a server initializes.
constexpr std::size_t mostChanges = 1000;  ///< Changes kept before a document is sent whole instead.
constexpr std::size_t longestName = 256;   ///< Bytes of a rename's new name, or a symbol query.
constexpr std::size_t largestFile = std::size_t{8} << 20; ///< A file forge.lsp.apply edits on disk, at most 8 MiB.

/// @brief What a request kind asks a server.
struct Kind {
	std::string_view name;       ///< As forge.lsp.request names it.
	std::string_view method;     ///< The LSP method.
	std::string_view capability; ///< The server capability that offers it.
	bool positioned;             ///< Whether it sends the caret's position.
};

/// @brief The request kinds forge.lsp.request knows.
constexpr Kind kinds[] = {
    {"hover", "textDocument/hover", "hoverProvider", true},
    {"completion", "textDocument/completion", "completionProvider", true},
    {"signature", "textDocument/signatureHelp", "signatureHelpProvider", true},
    {"definition", "textDocument/definition", "definitionProvider", true},
    {"references", "textDocument/references", "referencesProvider", true},
    {"rename", "textDocument/rename", "renameProvider", true},
    {"format", "textDocument/formatting", "documentFormattingProvider", false},
    {"symbols", "textDocument/documentSymbol", "documentSymbolProvider", false},
    {"workspaceSymbols", "workspace/symbol", "workspaceSymbolProvider", false},
    {"tokens", "textDocument/semanticTokens/full", "semanticTokensProvider", false},
};

/// @brief The semantic token types Forge asks servers for (those styleOfToken() knows, and the common rest).
const json tokenTypes = {"namespace", "type",     "class",    "enum",      "interface", "struct",   "typeParameter",
                         "parameter", "variable", "property", "enumMember", "event",    "function", "method",
                         "macro",     "keyword",  "modifier", "comment",   "string",    "number",   "regexp",
                         "operator",  "decorator", "concept"};

/// @brief The language servers of one screen, and what each knows of the open documents.
class Client {
public:
	/// @brief A client with no server started.
	/// @param screen The screen.
	/// @param workspace The open folder.
	/// @param buffers The open files' texts.
	/// @param executor The main thread, or nullptr.
	Client(affresco::Screen& screen, std::shared_ptr<Workspace> workspace, std::shared_ptr<Buffers> buffers,
	       affresco::Executor* executor)
	    : screen_(screen), workspace_(std::move(workspace)), buffers_(std::move(buffers)), executor_(executor) {}
	/// @brief Disconnects from the documents, which editors may still hold.
	~Client() {
		for(auto& [name, server] : servers_) reset(server);
	}
	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	/// @brief forge.lsp.request: asks the language's server.
	/// @param with The call's arguments.
	/// @return `{ "pending" }`.
	/// @throws std::runtime_error For arguments that aren't a request.
	json request(const json& with) {
		const std::string kindName = textArgument(with, "kind", 64);
		const Kind* kind = nullptr;
		for(const Kind& one : kinds)
			if(one.name == kindName) kind = &one;
		if(!kind) throw std::runtime_error("no request kind '" + kindName + "'");
		const json server = lsp::serverFor(textArgument(with, "language", 64));
		if(server.is_null()) return {{"pending", false}};
		const Request request{*kind,
		                      textArgument(with, "id", 4096),
		                      server["languageId"].get<std::string>(),
		                      positionArgument(with, "line") - 1,
		                      positionArgument(with, "column") - 1,
		                      with.is_object() && with.contains("request") ? with["request"] : json(0),
		                      textArgument(with, "newName", longestName),
		                      textArgument(with, "query", longestName)};
		if(request.id.empty() && kind->name != "workspaceSymbols") throw std::runtime_error("expected { \"id\": file }");
		if(kind->name == "rename" && request.newName.empty()) throw std::runtime_error("expected a newName");
		if(workspace_->root.empty()) {
			show(request, std::nullopt, "no folder is open");
			return {{"pending", true}};
		}
		Server& entry = this->server(server["service"].get<std::string>());
		whenReady(entry, [this, &entry, request] { send(entry, request); },
		          [this, request](const std::string& error) { show(request, std::nullopt, error); });
		return {{"pending", true}};
	}

	/// @brief forge.lsp.apply: applies edits, all or none.
	/// @param with `{ "edits" }`.
	/// @return `{ "files", "edits" }`.
	/// @throws std::runtime_error With the reason nothing was applied.
	json apply(const json& with) {
		const json& edits = with.is_object() && with.contains("edits") ? with["edits"] : json();
		if(!edits.is_array() || edits.size() > lsp::Limits{}.edits) throw std::runtime_error("expected { \"edits\": [...] }");
		const std::filesystem::path& root = workspace_->root;
		if(root.empty()) throw std::runtime_error("no folder is open");
		std::map<std::string, json> byFile;
		for(const json& edit : edits) {
			const std::string id = textArgument(edit, "id", 4096);
			// the id comes from the document: it must name a file inside the folder
			if(id.empty() || lsp::idOfUri(root, lsp::uriOf(root, id)) != id)
				throw std::runtime_error("an edit names a file outside the open folder");
			byFile[id].push_back(edit);
		}
		std::map<std::string, std::string> texts; // checked first, so a bad edit changes nothing
		for(auto& [id, list] : byFile) {
			std::string text;
			if(const auto model = buffers_->find(id)) text = model->text();
			else {
				std::error_code error;
				if(std::filesystem::file_size(root / id, error) > largestFile || error)
					throw std::runtime_error(id + " can't be read, or is too large");
				std::ifstream in(root / id, std::ios::binary);
				text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			}
			if(auto problem = lsp::applyEdits(text, list)) throw std::runtime_error(id + ": " + *problem);
			texts.emplace(id, std::move(text));
		}
		for(auto& [id, text] : texts) {
			if(const auto model = buffers_->find(id)) {
				replace(*model, text);
				continue;
			}
			// A fresh name, created exclusively: a link planted at a fixed name would send the text elsewhere
			if(auto failed = writeInFolder(root / id, text)) throw std::runtime_error(id + ": " + *failed);
		}
		return {{"files", texts.size()}, {"edits", edits.size()}};
	}

	/// @brief forge.lsp.status: what the client knows of each server.
	/// @return `{ service: { "state", "initialized", "documents" } }`.
	json status() const {
		const json services = screen_.services().status();
		json out = json::object();
		for(const auto& [name, server] : servers_)
			out[name] = {{"state", services.contains(name) ? services[name].value("state", "") : "undeclared"},
			             {"initialized", server.state == Server::State::Ready},
			             {"documents", server.documents.size()}};
		return out;
	}

private:
	/// @brief One request of forge.lsp.request.
	struct Request {
		Kind kind;              ///< What it asks.
		std::string id;         ///< The file.
		std::string languageId; ///< Its LSP language.
		std::size_t line;       ///< The caret's line, from 0.
		std::size_t column;     ///< Its column, from 0, in code points.
		json request;           ///< The screen's number for it, given back.
		std::string newName;    ///< For "rename".
		std::string query;      ///< For "workspaceSymbols".
	};

	/// @brief A document a server has open.
	struct Document {
		std::weak_ptr<affresco::TextModel> model; ///< The buffer's text.
		long long version = 1;                    ///< The last version sent.
		affresco::Connection connection = 0;      ///< To model->onChanged.
		json changes = json::array();             ///< Changes not sent yet.
		bool whole = false;                       ///< Send the whole text instead of changes.
		bool posted = false;                      ///< A flush is posted.
	};

	/// @brief One language server.
	struct Server {
		/// @brief How far it is.
		enum class State { Down, Starting, Ready } state = State::Down;
		std::string service;                         ///< Its service's name.
		json capabilities = json::object();          ///< What it said it offers.
		bool utf16 = true;                           ///< Its columns are UTF-16 units (else code points).
		bool incremental = false;                    ///< It takes changes (else whole texts).
		long long restarts = 0;                      ///< The service's restarts when it was initialized.
		std::filesystem::path root;                  ///< The folder it was initialized with.
		std::map<std::string, Document, std::less<>> documents; ///< By id.
		std::vector<std::pair<std::function<void()>, std::function<void(const std::string&)>>> waiting; ///< Until ready.
		std::shared_ptr<ServerCall> diagnostics;     ///< Its publishDiagnostics subscription.
	};

	/// @brief A server's entry, made on first use.
	/// @param service Its service.
	/// @return It.
	Server& server(const std::string& service) {
		Server& entry = servers_[service];
		entry.service = service;
		return entry;
	}

	/// @brief Forgets what a server knew (it crashed, or the client ends).
	/// @param server The server.
	void reset(Server& server) {
		for(auto& [id, document] : server.documents)
			if(const auto model = document.model.lock()) model->onChanged.disconnect(document.connection);
		server.documents.clear();
		if(server.diagnostics) server.diagnostics->stop();
		server.diagnostics.reset();
		server.state = Server::State::Down;
	}

	/// @brief Runs a task once a server is initialized, initializing it first; a server that crashed and restarted is
	/// initialized again.
	/// @param server The server.
	/// @param task The task.
	/// @param failed Gets the error if it can't be initialized.
	void whenReady(Server& server, std::function<void()> task, std::function<void(const std::string&)> failed) {
		const json status = screen_.services().status();
		const json entry = status.contains(server.service) ? status[server.service] : json::object();
		if(server.state == Server::State::Ready &&
		   (entry.value("state", "") == "failed" || entry.value("restarts", 0LL) != server.restarts))
			reset(server);
		if(server.state == Server::State::Ready && server.root != workspace_->root) changeFolder(server);
		if(server.state == Server::State::Ready) return task();
		if(server.waiting.size() >= mostWaiting) return failed("the language server is still starting");
		server.waiting.emplace_back(std::move(task), std::move(failed));
		if(server.state == Server::State::Down) initialize(server);
	}

	/// @brief Sends initialize, then initialized, and runs the waiting tasks.
	/// @param server The server.
	void initialize(Server& server) {
		server.state = Server::State::Starting;
		server.root = workspace_->root;
		const std::string rootUri = lsp::uriOf(server.root, "");
		const json capabilities = {
		    {"general", {{"positionEncodings", {"utf-32", "utf-16"}}}},
		    {"workspace", {{"workspaceFolders", true}, {"symbol", json::object()}}},
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
		        {"tokenTypes", tokenTypes},
		        {"tokenModifiers", json::array()},
		        {"formats", {"relative"}}}}}}};
		const json params = {{"processId", nullptr},
		                     {"clientInfo", {{"name", "Forge"}}},
		                     {"rootUri", rootUri},
		                     {"workspaceFolders", {{{"uri", rootUri}, {"name", server.root.filename().string()}}}},
		                     {"capabilities", capabilities}};
		call(server.service, "initialize", params, [this, &server](std::optional<json> result, const std::string& error) {
			if(server.state != Server::State::Starting) return;
			std::vector<std::pair<std::function<void()>, std::function<void(const std::string&)>>> waiting;
			waiting.swap(server.waiting);
			if(!result || !result->is_object()) {
				server.state = Server::State::Down;
				for(auto& [task, failed] : waiting) failed(error.empty() ? "the language server didn't initialize" : error);
				return;
			}
			ready(server, *result);
			for(auto& [task, failed] : waiting) task();
		});
	}

	/// @brief Takes a server's initialize result, says initialized, and subscribes to its diagnostics.
	/// @param server The server.
	/// @param result Its result.
	void ready(Server& server, const json& result) {
		server.capabilities = result.contains("capabilities") && result["capabilities"].is_object() ? result["capabilities"]
		                                                                                            : json::object();
		server.utf16 = server.capabilities.value("positionEncoding", "utf-16") != "utf-32";
		const json& sync = server.capabilities.contains("textDocumentSync") ? server.capabilities["textDocumentSync"] : json();
		server.incremental = sync == 2 || (sync.is_object() && sync.value("change", 0) == 2);
		const json status = screen_.services().status();
		server.restarts = status.contains(server.service) ? status[server.service].value("restarts", 0LL) : 0;
		server.state = Server::State::Ready;
		notify(server.service, "initialized", json::object());
		server.diagnostics = std::make_shared<ServerCall>(
		    json::object(), executor_, alive_, ServerCall::Done(),
		    [this, &server](const json& params) { publishDiagnostics(server, params); });
		if(screen_.services().subscribe(server.service, "textDocument/publishDiagnostics", server.diagnostics))
			server.diagnostics.reset(); // a declaration without the stream: no diagnostics
	}

	/// @brief Tells a server the open folder changed, and closes its documents.
	/// @param server The server, ready.
	void changeFolder(Server& server) {
		for(const auto& [id, document] : server.documents) notify(server.service, "textDocument/didClose",
		                                                          {{"textDocument", {{"uri", lsp::uriOf(server.root, id)}}}});
		const std::filesystem::path old = server.root;
		const State state = server.state;
		reset(server);
		server.state = state;
		server.root = workspace_->root;
		const auto folder = [](const std::filesystem::path& root) {
			return json{{{"uri", lsp::uriOf(root, "")}, {"name", root.filename().string()}}};
		};
		notify(server.service, "workspace/didChangeWorkspaceFolders",
		       {{"event", {{"added", folder(server.root)}, {"removed", folder(old)}}}});
		subscribeAgain(server);
	}
	using State = Server::State;

	/// @brief Subscribes to a server's diagnostics again after reset() (the folder changed).
	/// @param server The server, ready.
	void subscribeAgain(Server& server) {
		server.diagnostics = std::make_shared<ServerCall>(
		    json::object(), executor_, alive_, ServerCall::Done(),
		    [this, &server](const json& params) { publishDiagnostics(server, params); });
		if(screen_.services().subscribe(server.service, "textDocument/publishDiagnostics", server.diagnostics))
			server.diagnostics.reset();
	}

	/// @brief Sends one request, once the server is ready.
	/// @param server The server.
	/// @param request The request.
	void send(Server& server, const Request& request) {
		const json& offered = server.capabilities.contains(request.kind.capability) ? server.capabilities[request.kind.capability]
		                                                                            : json();
		if(offered.is_null() || offered == false) return show(request, std::nullopt, "the language server doesn't offer that");
		json params = json::object();
		std::shared_ptr<affresco::TextModel> model;
		if(!request.id.empty() && request.kind.name != "workspaceSymbols") {
			model = open(server, request.id, request.languageId);
			if(!model) return show(request, std::nullopt, request.id + " isn't open");
			params["textDocument"] = {{"uri", lsp::uriOf(server.root, request.id)}};
		}
		if(request.kind.positioned) {
			const std::size_t line = std::min(request.line, model->lineCount() - 1);
			params["position"] = {{"line", line}, {"character", serverColumn(server, *model, line, request.column)}};
		}
		if(request.kind.name == "completion") params["context"] = {{"triggerKind", 1}};
		if(request.kind.name == "references") params["context"] = {{"includeDeclaration", true}};
		if(request.kind.name == "rename") params["newName"] = request.newName;
		if(request.kind.name == "format") params["options"] = {{"tabSize", 4}, {"insertSpaces", false}};
		if(request.kind.name == "workspaceSymbols") params = {{"query", request.query}};
		call(server.service, std::string(request.kind.method), params,
		     [this, &server, request](std::optional<json> result, const std::string& error) {
			     if(!result) return show(request, std::nullopt, error);
			     if(request.kind.name == "tokens" && result->is_object()) {
				     const json& provider = server.capabilities["semanticTokensProvider"];
				     (*result)["legend"] = provider.is_object() && provider.contains("legend") ? provider["legend"] : json();
			     }
			     try {
				     show(request, lsp::normalize(request.kind.name, *result, server.root, request.id, {}, mapper(server)), {});
			     } catch(const std::exception& failure) {
				     show(request, std::nullopt, failure.what());
			     }
		     });
	}

	/// @brief Opens a document with a server (didOpen) if it isn't yet, closing those whose buffers are gone, and
	/// sends its pending changes.
	/// @param server The server, ready.
	/// @param id The file.
	/// @param languageId Its LSP language.
	/// @return Its text, or nullptr if it has no buffer.
	std::shared_ptr<affresco::TextModel> open(Server& server, const std::string& id, const std::string& languageId) {
		std::erase_if(server.documents, [&](auto& entry) {
			const auto model = entry.second.model.lock();
			if(model && buffers_->find(entry.first) == model) return false;
			if(model) model->onChanged.disconnect(entry.second.connection);
			notify(server.service, "textDocument/didClose", {{"textDocument", {{"uri", lsp::uriOf(server.root, entry.first)}}}});
			return true;
		});
		const std::shared_ptr<affresco::TextModel> model = buffers_->find(id);
		if(!model) return nullptr;
		if(const auto found = server.documents.find(id); found != server.documents.end()) {
			flush(server, id);
			return model;
		}
		Document& document = server.documents[id];
		document.model = model;
		document.connection = model->onChanged.connect(
		    [this, &server, id](const affresco::TextChange& change) { changed(server, id, change); });
		notify(server.service, "textDocument/didOpen",
		       {{"textDocument",
		         {{"uri", lsp::uriOf(server.root, id)}, {"languageId", languageId}, {"version", 1}, {"text", model->text()}}}});
		return model;
	}

	/// @brief Keeps an edit of an open document, and posts a flush (so a burst of edits is one didChange).
	/// @param server The server.
	/// @param id The document.
	/// @param change The edit.
	void changed(Server& server, const std::string& id, const affresco::TextChange& change) {
		const auto found = server.documents.find(id);
		if(found == server.documents.end()) return;
		Document& document = found->second;
		const auto model = document.model.lock();
		// UTF-16 columns of the replaced text can't be counted once it is gone: such servers get the whole text
		if(!model || !server.incremental || server.utf16 || document.changes.size() >= mostChanges) document.whole = true;
		else
			document.changes.push_back({{"range",
			                             {{"start", {{"line", change.from.line}, {"character", change.from.column}}},
			                              {"end", {{"line", change.oldEnd.line}, {"character", change.oldEnd.column}}}}},
			                            {"text", model->textIn(change.from, change.newEnd)}});
		if(document.posted || !executor_) return;
		document.posted = true;
		executor_->post([this, alive = std::weak_ptr<bool>(alive_), &server, id] {
			if(alive.lock()) flush(server, id);
		});
	}

	/// @brief Sends a document's pending changes (didChange).
	/// @param server The server.
	/// @param id The document.
	void flush(Server& server, const std::string& id) {
		const auto found = server.documents.find(id);
		if(found == server.documents.end()) return;
		Document& document = found->second;
		document.posted = false;
		const auto model = document.model.lock();
		if(!model || (!document.whole && document.changes.empty())) return;
		const json changes = document.whole ? json::array({{{"text", model->text()}}}) : document.changes;
		document.changes = json::array();
		document.whole = false;
		notify(server.service, "textDocument/didChange",
		       {{"textDocument", {{"uri", lsp::uriOf(server.root, id)}, {"version", ++document.version}}},
		        {"contentChanges", changes}});
	}

	/// @brief A server's column for an editor's.
	/// @param server The server.
	/// @param model The text.
	/// @param line The line, from 0.
	/// @param column The column, from 0, in code points.
	/// @return It in the server's encoding.
	static std::size_t serverColumn(const Server& server, const affresco::TextModel& model, std::size_t line,
	                                std::size_t column) {
		if(!server.utf16) return column;
		std::u32string text;
		model.line(line, text);
		return utf16Units(text, column);
	}

	/// @brief Maps a server's columns to the editor's: through the buffer's line for UTF-16 servers (a file that
	/// isn't open keeps the server's column, exact for ASCII lines).
	/// @param server The server.
	/// @return The mapper, or empty for code points.
	lsp::ColumnMapper mapper(const Server& server) const {
		if(!server.utf16) return {};
		return [buffers = buffers_](const std::string& id, std::size_t line, std::size_t column) {
			const auto model = buffers->find(id);
			if(!model || line >= model->lineCount()) return column;
			std::u32string text;
			model->line(line, text);
			return codePointsOf(text, column);
		};
	}

	/// @brief A server's diagnostics for a file: runs `lspDiagnostics`.
	/// @param server The server.
	/// @param params publishDiagnostics' params.
	void publishDiagnostics(const Server& server, const json& params) {
		json shown = lsp::normalize("diagnostics", params, server.root, {}, {}, mapper(server));
		if(shown.contains("error")) return;
		shown["service"] = server.service;
		run("lspDiagnostics", shown);
	}

	/// @brief Gives the screen a request's answer: runs `lspShow`.
	/// @param request The request.
	/// @param result The normalized result, or std::nullopt.
	/// @param error Why there is none.
	void show(const Request& request, std::optional<json> result, const std::string& error) {
		json event = {{"kind", request.kind.name}, {"id", request.id}, {"request", request.request}};
		if(result) event["result"] = std::move(*result);
		else event["error"] = error.empty() ? "the language server failed" : error;
		run("lspShow", event);
	}

	/// @brief Runs an action of the screen from its root, if it is built.
	/// @param action The action.
	/// @param event Its event.
	void run(std::string_view action, const json& event) {
		if(affresco::Panel* root = screen_.find("root")) screen_.runAction(action, *root, event.dump());
	}

	/// @brief Sends a request to a service.
	/// @param service The service.
	/// @param method The method.
	/// @param params Its params.
	/// @param done Gets the answer, on the main thread (or now, if it can't be sent).
	void call(const std::string& service, const std::string& method, json params, ServerCall::Done done) {
		auto pending = std::make_shared<ServerCall>(std::move(params), executor_, alive_, done);
		if(auto problem = screen_.services().invoke(service, method, std::move(pending))) done(std::nullopt, *problem);
	}

	/// @brief Sends a notification to a service (declared in its `notifications`), ignoring a failure.
	/// @param service The service.
	/// @param method The method.
	/// @param params Its params.
	void notify(const std::string& service, const std::string& method, json params) {
		auto pending = std::make_shared<ServerCall>(std::move(params), executor_, alive_, ServerCall::Done());
		(void)screen_.services().invoke(service, method, std::move(pending));
	}

	/// @brief Replaces a buffer's text as one undo step, changing only the lines between the first and last that differ.
	/// @param model The buffer.
	/// @param text The new text.
	static void replace(affresco::TextModel& model, std::string_view text) {
		const std::string old = model.text();
		std::size_t prefix = 0;
		while(prefix < old.size() && prefix < text.size() && old[prefix] == text[prefix]) ++prefix;
		std::size_t suffix = 0;
		while(suffix < old.size() - prefix && suffix < text.size() - prefix &&
		      old[old.size() - 1 - suffix] == text[text.size() - 1 - suffix])
			++suffix;
		while(prefix > 0 && (static_cast<unsigned char>(old[prefix]) & 0xC0) == 0x80) --prefix;
		while(suffix > 0 && (static_cast<unsigned char>(old[old.size() - suffix]) & 0xC0) == 0x80) --suffix;
		const affresco::TextPosition from = model.positionOf(prefix), to = model.positionOf(old.size() - suffix);
		model.beginStep(false, from, from);
		model.erase(from, to);
		model.insert(from, text.substr(prefix, text.size() - prefix - suffix));
		model.endStep();
	}

	affresco::Screen& screen_;                   ///< See the constructor.
	std::shared_ptr<Workspace> workspace_;       ///< See the constructor.
	std::shared_ptr<Buffers> buffers_;           ///< See the constructor.
	affresco::Executor* executor_;               ///< See the constructor.
	std::map<std::string, Server, std::less<>> servers_; ///< By service; entries never move.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards what is posted to the main thread.
};

} // namespace

std::filesystem::path findProgram(std::string_view name) {
#ifdef _WIN32
	constexpr char separator = ';';
	const std::string file = std::string(name) + ".exe";
#else
	constexpr char separator = ':';
	const std::string file(name);
#endif
	const char* path = std::getenv("PATH");
	for(std::string_view folders = path ? path : ""; !folders.empty();) {
		const std::size_t end = std::min(folders.find(separator), folders.size());
		const std::filesystem::path folder(folders.substr(0, end));
		folders.remove_prefix(std::min(end + 1, folders.size()));
		std::error_code error;
		if(folder.is_absolute() && std::filesystem::is_regular_file(folder / file, error)) return folder / file;
	}
	return {};
}

void grantLanguageServers(affresco::Screen& screen) {
	const std::pair<const char*, std::vector<std::string>> servers[] = {
	    {"clangd", {}}, {"pyright-langserver", {"--stdio"}}, {"typescript-language-server", {"--stdio"}}};
	for(const auto& [name, arguments] : servers)
		if(const std::filesystem::path program = findProgram(name); !program.empty())
			screen.services().allowProcess(name, program, arguments);
}

void registerLsp(affresco::Screen& screen, std::shared_ptr<Workspace> workspace, std::shared_ptr<Buffers> buffers,
                 affresco::Executor* executor) {
	const auto client = std::make_shared<Client>(screen, workspace, std::move(buffers), executor);
	screen.onCall("forge.lsp.request", [client](const std::string& arguments) {
		return client->request(affresco::parseUntrusted(arguments)).dump();
	});
	screen.onCall("forge.lsp.apply", [client](const std::string& arguments) {
		return client->apply(affresco::parseUntrusted(arguments)).dump();
	});
	screen.onCall("forge.lsp.status", [client](const std::string&) { return client->status().dump(); });
	screen.onCall("forge.lsp.normalize", [workspace](const std::string& arguments) {
		const json with = affresco::parseUntrusted(arguments);
		const std::string kind = textArgument(with, "kind", 64);
		const std::string id = textArgument(with, "id", 4096);
		const json& result = with.is_object() && with.contains("result") ? with["result"] : json();
		// "root": the folder the URIs name, for a result written by hand ("/folder"); only names are compared
		const std::string root = textArgument(with, "root", 4096);
		try {
			return lsp::normalize(kind, result, root.empty() ? workspace->root : std::filesystem::path(root), id).dump();
		} catch(const std::invalid_argument& error) {
			throw std::runtime_error(error.what());
		}
	});
}

} // namespace forge
