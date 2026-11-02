/// @file
/// @brief `affrescoui repl`, and what `affrescoui run` does for a Lisp file it is asked to print or that is a program
/// (modules/lisp, RFC 0005): nothing here opens a window or reaches outside the program.

#include "Cli.hpp"
#include "JsonText.hpp"

#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>

#include <iostream>
#include <optional>
#include <string>

namespace affresco::cli {

std::optional<Exit> runLisp(const std::filesystem::path& file, bool print) {
	if(file.extension() != ".lisp") {
		if(print) throw UsageError("--print: only a .lisp file is built");
		return std::nullopt;
	}
	const std::string text = readFile(file);
	try {
		const bool isProgram = lisp::isProgram(text);
		if(!print && !isProgram) return std::nullopt; // a data file opens in a window, as any screen
		lisp::Program program;
		if(isProgram) program.addFile(text, file.string());
		const json tree = isProgram ? program.build() : lisp::buildText(text, file.string());
		std::cout << printText(parseText(tree.dump())) << std::flush;
		for(const lisp::UndefinedName& name : program.undefinedNames())
			std::cerr << file.string() << ": line " << name.line << ": \"" << name.name << "\" is not defined, in "
			          << name.definition << '\n';
		return Exit::Success;
	} catch(const DocumentError& error) {
		throw FileProblem(error.what());
	}
}

Exit lispRepl(std::span<const std::string_view> args) {
	const Arguments arguments(args, {}, {});
	if(!arguments.positional().empty()) throw UsageError("repl takes nothing");
	lisp::Interpreter interpreter;
	std::string pending;
	std::string line;
	const auto prompt = [&] { std::cout << (pending.empty() ? "> " : "  ") << std::flush; };
	prompt();
	while(std::getline(std::cin, line)) {
		pending += line + "\n";
		try {
			interpreter.setBudget({});
			const lisp::Value value = interpreter.eval(pending, "repl", "repl");
			std::cout << interpreter.print(value) << '\n';
			pending.clear();
		} catch(const lisp::LispError& error) {
			const bool unfinished =
			    error.message().starts_with("expected ')'") || error.message().starts_with("expected ']'") ||
			    error.message().starts_with("expected '}'") || error.message().starts_with("a string that never ends");
			if(!unfinished) {
				std::cout << error.what() << '\n';
				pending.clear();
			}
		}
		prompt();
	}
	std::cout << '\n';
	return Exit::Success;
}

} // namespace affresco::cli
