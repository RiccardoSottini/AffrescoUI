/// @file
/// @brief The nREPL operations' replies (EditorOps.hpp), in the shapes CIDER, Calva and Conjure read.

#include "EditorOps.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace affresco::net::nrepl {

using bencode::Value;

namespace {

/// @brief The twelve operations.
constexpr std::array<std::string_view, 12> operations{"clone",    "close",     "describe",  "ls-sessions",
                                                      "eval",     "load-file", "interrupt", "completions",
                                                      "complete", "lookup",    "info",      "eldoc"};

/// @brief The exception class an error is reported as: one for every error the language gives.
constexpr std::string_view errorClass = "LispError";

/// @brief A byte string value. @param text Its bytes. @return It.
Value text(std::string_view text) { return Value(std::string(text)); }

/// @brief How a client names what a name is, for completion and documentation.
enum class NameType {
	Macro,    ///< A macro.
	Function, ///< Something code calls.
	Variable  ///< Anything else: something code reads.
};

/// @brief Classifies a name by ConsoleName::kind. @param kind Its kind. @return What a client calls it.
NameType typeOf(std::string_view kind) noexcept {
	if(kind == "macro" || kind == "defmacro") return NameType::Macro;
	constexpr std::array<std::string_view, 7> called{"defn",    "defcommand", "function", "special form",
	                                                 "builtin", "fn",         "command"};
	return std::ranges::find(called, kind) != called.end() ? NameType::Function : NameType::Variable;
}

/// @brief Splits an argument list as written (`[text & more]`) into its names. @param list It. @return The names.
Value argumentNames(std::string_view list) {
	Value::List names;
	std::size_t at = 0;
	while(at < list.size()) {
		const std::size_t start = list.find_first_not_of(" \t\r\n,[]()", at);
		if(start == std::string_view::npos) break;
		const std::size_t end = std::min(list.find_first_of(" \t\r\n,[]()", start), list.size());
		names.push_back(text(list.substr(start, end - start)));
		at = end;
	}
	return Value(std::move(names));
}

/// @brief Joins a name's argument lists, one a line, as `arglists-str` reads. @param name It. @return The text.
std::string joinedArguments(const ConsoleName& name) {
	std::string joined;
	for(const std::string& list : name.arguments) {
		if(!joined.empty()) joined += '\n';
		joined += list;
	}
	return joined;
}

/// @brief What `lookup` and `info` tell of a name.
/// @param name It. @param host Its pathOf(). @param space The session's namespace name.
/// @return The entries.
Value::Dictionary infoOf(const ConsoleName& name, const EditorHost& host, std::string_view space) {
	Value::Dictionary info{{"name", text(name.name)},
	                       {"ns", text(space)},
	                       {"doc", text(name.doc)},
	                       {"arglists-str", text(joinedArguments(name))}};
	std::string path = !name.file.empty() && host.pathOf ? host.pathOf(name.file) : std::string();
	if(!path.empty()) {
		info.emplace("file", Value(std::move(path)));
		info.emplace("line", Value(static_cast<std::int64_t>(name.line)));
	}
	return info;
}

/// @brief Answers `eldoc` for a name found.
/// @param link Where it goes. @param request The request. @param name It. @param space The namespace name.
void sendEldoc(EditorLink& link, const Value& request, const ConsoleName& name, std::string_view space) {
	Value::Dictionary reply = replyTo(request);
	Value::List lists;
	for(const std::string& list : name.arguments) lists.push_back(argumentNames(list));
	reply.emplace("name", text(name.name));
	reply.emplace("ns", text(space));
	reply.emplace("type", text(typeOf(name.kind) == NameType::Variable ? "variable" : "function"));
	reply.emplace("eldoc", Value(std::move(lists)));
	reply.emplace("docstring", text(name.doc));
	reply.emplace("status", statusOf({"done"}));
	link.send(Value(std::move(reply)));
}

} // namespace

std::span<const std::string_view> operationNames() noexcept { return operations; }

std::string_view textOf(const Value& request, std::string_view key) noexcept {
	const Value* found = request.find(key);
	return found != nullptr && found->isString() ? std::string_view(found->string()) : std::string_view();
}

std::string_view textOf(const Value& request, std::string_view first, std::string_view second) noexcept {
	const std::string_view found = textOf(request, first);
	return found.empty() ? textOf(request, second) : found;
}

Value::Dictionary replyTo(const Value& request) {
	Value::Dictionary reply;
	for(const std::string_view key : {std::string_view("id"), std::string_view("session")})
		if(const Value* found = request.find(key); found != nullptr) reply.emplace(std::string(key), *found);
	return reply;
}

Value statusOf(std::initializer_list<std::string_view> words) {
	Value::List list;
	list.reserve(words.size());
	for(const std::string_view word : words)
		if(!word.empty()) list.push_back(text(word));
	return Value(std::move(list));
}

void sendStatus(EditorLink& link, const Value& request, std::initializer_list<std::string_view> words) {
	Value::Dictionary reply = replyTo(request);
	reply.emplace("status", statusOf(words));
	link.send(Value(std::move(reply)));
}

void sendFailure(EditorLink& link, const Value& request, std::string_view why, std::string_view more) {
	Value::Dictionary message = replyTo(request);
	message.emplace("err", Value(std::string(why) + "\n"));
	link.send(Value(std::move(message)));
	sendStatus(link, request, {"error", more, "done"});
}

void answerDescribe(EditorLink& link, const Value& request, const std::string& version) {
	Value::Dictionary ops;
	for(const std::string_view name : operations) ops.emplace(std::string(name), Value(Value::Dictionary{}));
	Value::Dictionary versions{{"affrescoui", Value(Value::Dictionary{{"version-string", Value(version)}})}};
	Value::Dictionary reply = replyTo(request);
	reply.emplace("ops", Value(std::move(ops)));
	reply.emplace("versions", Value(std::move(versions)));
	reply.emplace("aux", Value(Value::Dictionary{}));
	reply.emplace("status", statusOf({"done"}));
	link.send(Value(std::move(reply)));
}

void answerCompletions(EditorLink& link, const Value& request, ProgramConsole& console) {
	const std::string_view prefix = textOf(request, "prefix", "symbol");
	Value::List candidates;
	for(const ConsoleName& name : console.names(prefix, maxCompletions)) {
		const NameType type = typeOf(name.kind);
		const std::string_view word = type == NameType::Macro      ? "macro"
		                              : type == NameType::Function ? "function"
		                                                           : "var";
		candidates.emplace_back(Value::Dictionary{{"candidate", text(name.name)}, {"type", text(word)}});
		if(candidates.size() == maxCompletions) break; // a console that gives more than it was asked for
	}
	Value::Dictionary reply = replyTo(request);
	reply.emplace("completions", Value(std::move(candidates)));
	reply.emplace("status", statusOf({"done"}));
	link.send(Value(std::move(reply)));
}

void answerLookup(EditorLink& link, const Value& request, ProgramConsole* console, const EditorHost& host,
                  std::string_view space) {
	const std::string_view op = textOf(request, "op");
	const std::string_view symbol = op == "lookup" ? textOf(request, "sym") : textOf(request, "sym", "symbol");
	std::optional<ConsoleName> name;
	if(!symbol.empty() && console != nullptr && console->hasProgram()) name = console->describe(symbol);
	if(op == "eldoc") {
		if(name) sendEldoc(link, request, *name, space);
		else sendStatus(link, request, {"done", "no-eldoc"});
		return;
	}
	Value::Dictionary reply = replyTo(request);
	if(op == "lookup") { // nested under `info`, `{}` when nothing is known
		reply.emplace("info", Value(name ? infoOf(*name, host, space) : Value::Dictionary{}));
	} else if(name) { // `info`: at the top
		for(auto& entry : infoOf(*name, host, space)) reply.insert(std::move(entry));
	}
	reply.emplace("status", name ? statusOf({"done"}) : statusOf({"done", "no-info"}));
	link.send(Value(std::move(reply)));
}

void answerEvaluation(EditorLink& link, const Value& request, const ConsoleResult& result, EditorSession& session) {
	for(const std::string& value : result.values) {
		Value::Dictionary message = replyTo(request);
		message.emplace("value", Value(value));
		message.emplace("ns", Value(session.space));
		link.send(Value(std::move(message)));
		session.rememberValue(value);
	}
	if(result.proposal) { // not applied as it is: say where it went, so the editor isn't left guessing
		Value::Dictionary message = replyTo(request);
		message.emplace("out", Value("became proposal " + std::to_string(*result.proposal) + ", for review\n"));
		link.send(Value(std::move(message)));
	}
	if(result.error) {
		session.lastError = *result.error;
		Value::Dictionary message = replyTo(request);
		std::string error = *result.error;
		if(result.line > 0) { // where, in the editor's buffer, as a Clojure error says it (CIDER jumps to it)
			const std::string_view file = textOf(request, "file");
			error += " (" + std::string(file.empty() ? "REPL" : file) + ":" + std::to_string(result.line) + ":" +
			         std::to_string(result.column) + ")";
		}
		message.emplace("err", Value(error + "\n"));
		link.send(Value(std::move(message)));
		Value::Dictionary exception = replyTo(request);
		exception.emplace("ex", text(errorClass));
		exception.emplace("root-ex", text(errorClass));
		exception.emplace("status", statusOf({"eval-error"}));
		link.send(Value(std::move(exception)));
	}
	sendStatus(link, request, {"done"});
}

std::string lispString(std::string_view raw) {
	std::string quoted = "\"";
	quoted.reserve(raw.size() + 2U);
	constexpr std::string_view hex = "0123456789abcdef";
	for(const char character : raw) {
		const auto code = static_cast<unsigned char>(character);
		if(character == '"' || character == '\\') quoted += {'\\', character};
		else if(character == '\n') quoted += "\\n";
		else if(character == '\t') quoted += "\\t";
		else if(code < 0x20U) quoted += {'\\', 'u', '0', '0', hex[code >> 4U], hex[code & 0x0fU]};
		else quoted += character;
	}
	quoted += '"';
	return quoted;
}

} // namespace affresco::net::nrepl
