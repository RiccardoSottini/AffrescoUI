/// @file
/// @brief RuntimeConsole (ProgramConsole.hpp): the screen's program, as an editor connected to the app sees it.

#include "ProgramConsole.hpp"

#include "Engine.hpp"

#include <affresco/render/Window.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

namespace affresco::format {

namespace {

/// @brief A place in an evaluated text, as one in the editor's buffer the text starts in.
/// @param line The line in the text, from 1; 0 for none.
/// @param column The column in the text, from 1.
/// @param request Where the text starts.
/// @return The line and the column in the buffer; 0 and 0 for none.
std::pair<std::size_t, std::size_t> inBuffer(std::size_t line, std::size_t column, const ConsoleRequest& request) {
	if(line == 0) return {0, 0};
	const std::size_t startLine = std::max<std::size_t>(request.line, 1U);
	const std::size_t startColumn = std::max<std::size_t>(request.column, 1U);
	return {startLine - 1 + line, line == 1 ? startColumn - 1 + column : column};
}

/// @brief An argument list as written: `[a b]`.
/// @param parameters Its names.
/// @return It.
std::string argumentList(const json& parameters) {
	std::string out = "[";
	for(const json& parameter : parameters) {
		if(!parameter.is_string()) continue;
		if(out.size() > 1) out += ' ';
		out += parameter.get<std::string>();
	}
	return out + "]";
}

/// @brief The line a program's entry starts on in its file on disk, if the file holds that entry's text.
/// @param path The file.
/// @param text The entry's text.
/// @return The line, from 1; 0 if the file isn't there or doesn't hold it.
std::size_t lineOnDisk(const std::filesystem::path& path, const std::string& text) {
	std::ifstream in(path, std::ios::binary);
	if(!in || text.empty()) return 0;
	const std::string content{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	const std::size_t at = content.find(text);
	if(at == std::string::npos) return 0;
	return 1U + static_cast<std::size_t>(
	                std::count(content.begin(), content.begin() + static_cast<std::ptrdiff_t>(at), '\n'));
}

} // namespace

bool RuntimeConsole::hasProgram() const { return engine_.programs.hasProgram(); }

std::string RuntimeConsole::fileFor(const ConsoleRequest& request) const {
	const json& document = engine_.versions.sourceDocument();
	const auto section = document.find("program");
	const auto has = [&](const std::string& file) {
		return !file.empty() && section != document.end() && section->is_object() && section->contains(file);
	};
	if(has(request.file)) return request.file;
	if(!request.space.empty() && has(request.space + ".lisp")) return request.space + ".lisp";
	return std::string(Programs::defaultFile);
}

ConsoleResult RuntimeConsole::evaluate(const ConsoleRequest& request) {
	ConsoleResult result;
	if(const std::optional<std::string> why = engine_.programs.refusalToEvaluate()) {
		result.error = *why;
		return result;
	}
	Window* window = engine_.window;
	if(!window) {
		result.error = "the screen isn't built yet, so there is nothing to evaluate in";
		return result;
	}
	if(request.text.size() > Programs::maxEvaluatedBytes) {
		result.error = "the text is longer than a program's file may be";
		return result;
	}
	const std::string file = fileFor(request);
	Programs::Evaluated ran;
	{
		// As the answer to the step's question runs it (ProgramEvaluate.cpp): the person's, no program's authority
		const AuthorityScope holder(engine_, nullptr);
		const DocumentContext::BudgetScope budget(engine_.document);
		const json event = json::object();
		const Scope locals;
		ran = request.wholeFile
		          ? engine_.programs.evaluateFile(request.text, file, window->root(), event, locals, request.origin)
		          : engine_.programs.evaluateConfirmed(request.text, file, window->root(), event, locals,
		                                               request.origin, true);
	}
	engine_.reconciler.requestStateUpdate();
	result.values = std::move(ran.values);
	result.error = std::move(ran.error);
	result.proposal = ran.proposal;
	std::tie(result.line, result.column) = inBuffer(ran.line, ran.column, request);
	return result;
}

std::optional<ConsoleName> RuntimeConsole::nameOf(const std::string& name, bool callable) const {
	lisp::Interpreter& interpreter = *engine_.programs.live_;
	const json described = engine_.programs.describeName(interpreter, name, {});
	if(described.is_null()) {
		if(!callable) return std::nullopt;
		return ConsoleName{.name = name, .kind = "builtin", .doc = {}, .file = {}, .line = 0, .arguments = {}};
	}
	ConsoleName out{.name = name,
	                .kind = described.value("kind", std::string()),
	                .doc = described.value("doc", std::string()),
	                .file = described.value("file", std::string()),
	                .line = 0,
	                .arguments = {}};
	if(const auto arguments = described.find("arguments"); arguments != described.end() && arguments->is_array())
		out.arguments.push_back(argumentList(*arguments));
	// A place only for what the file on disk still holds as it is: a revision's text is in no file
	const json authors = engine_.versions.programAuthors();
	if(!out.file.empty() && authors.value(out.file, json::object()).value(name, std::string()) == "file") {
		// The definition's line counts from its file's top where the program was read from the file, else from its own
		std::size_t entryLine = 1;
		if(engine_.programs.program_)
			for(const lisp::Entry& entry : engine_.programs.program_->entries())
				if(entry.file == out.file && entry.name == name) entryLine = entry.line;
		const std::size_t line = described.value("line", std::size_t{1});
		const std::size_t inEntry = line >= entryLine ? line - entryLine + 1 : line;
		const json entry = engine_.versions.sourceDocument()
		                       .value("program", json::object())
		                       .value(out.file, json::object())
		                       .value(name, json());
		const std::size_t onDisk =
		    entry.is_string() ? lineOnDisk(engine_.document.baseDirectory() / out.file, entry.get<std::string>()) : 0;
		out.line = onDisk == 0 ? inEntry : onDisk - 1 + inEntry;
	}
	return out;
}

std::vector<ConsoleName> RuntimeConsole::names(std::string_view prefix, std::size_t most) {
	if(!hasProgram() || !engine_.programs.live_) return {};
	const std::vector<std::string> callable = engine_.programs.live_->callableNames();
	std::vector<std::string> found = callable;
	const json& document = engine_.versions.document();
	if(const auto actions = document.find("actions"); actions != document.end() && actions->is_object())
		for(const auto& [name, action] : actions->items()) found.push_back(name);
	std::erase_if(found, [&](const std::string& name) { return !name.starts_with(prefix); });
	std::ranges::sort(found);
	found.erase(std::ranges::unique(found).begin(), found.end());
	std::vector<ConsoleName> out;
	for(const std::string& name : found) {
		if(out.size() >= most) break;
		if(std::optional<ConsoleName> described = nameOf(name, std::ranges::binary_search(callable, name)))
			out.push_back(std::move(*described));
	}
	return out;
}

std::optional<ConsoleName> RuntimeConsole::describe(std::string_view name) {
	if(!hasProgram() || !engine_.programs.live_) return std::nullopt;
	const std::vector<std::string> callable = engine_.programs.live_->callableNames();
	return nameOf(std::string(name), std::ranges::binary_search(callable, name));
}

std::vector<std::string> RuntimeConsole::files() const {
	std::vector<std::string> out;
	const json section = engine_.versions.sourceDocument().value("program", json::object());
	if(section.is_object())
		for(const auto& [file, entries] : section.items()) out.push_back(file);
	return out;
}

std::optional<std::string> RuntimeConsole::unshowable(std::string_view text) const {
	// Too long to evaluate is too long to show: the person isn't asked to read a text that would be refused anyway
	if(text.size() > Programs::maxEvaluatedBytes) return "the text is longer than a program's file may be";
	return hiddenCharacter(text);
}

} // namespace affresco::format
