/// @file
/// @brief affrescoui: one command-line tool for screen files, for people, agents and CI.
//
//   affrescoui <command> [arguments]     affrescoui help     affrescoui <command> --help
//
// The commands are listed in commands() below, and documented with their exit codes in tools/affrescoui/README.md.

#include "Cli.hpp"

#include <affresco/affresco.hpp>

#include <array>
#include <iostream>
#include <vector>

#ifndef AFFRESCOUI_VERSION
#define AFFRESCOUI_VERSION "unknown"
#endif

namespace affresco::cli {

std::span<const Command> commands() {
	static constexpr std::array<Command, 8> list = {{
	    {"run", "<file> [--socket DIR] [--data DIR] [--mode default|acceptEdits|plan] [--plugin changes.json]...",
		 "Show a screen and let peers change it, like affrescoui_host", runScreen},
	    {"check", "<file> [--json]", "Check a screen: the schema, the format's rules and the checker", checkScreen},
	    {"test", "<scenarios.json>... [--json] [--only NAME] [--goldens DIR] [--update] [--out DIR] [--document FILE]",
		 "Play a screen's scenarios and report what failed", testScreen},
	    {"render", "<file> -o out.png [--scale 1|2] [--size WIDTHxHEIGHT]",
		 "Draw a screen's first frame offscreen, to a PNG", renderScreen},
	    {"fmt", "<file> [--check]", "Write a file the way AffrescoUI files are written; --check only says if it is",
		 formatScreen},
	    {"ids", "<file> [--check]",
		 "Give readable ids to the nodes that have none; --check only says if some have none", nameScreenNodes},
	    {"export", "<file> [--from REVISION] [--name NAME] [-o changes.json] [--data DIR]",
		 "Write the revisions after one, from the screen's history on disk, as a change set", exportChanges},
	    {"import", "<file> <changes.json> [--data DIR]", "Apply a change set to the screen's history on disk",
		 importChanges},
	}};
	return list;
}

} // namespace affresco::cli

namespace {

/// @brief Prints what the tool does and its commands.
/// @param out Where.
void printHelp(std::ostream& out) {
	out << "affrescoui " AFFRESCOUI_VERSION ": a tool for AffrescoUI screen files\n\n"
	       "usage: affrescoui <command> [arguments]\n\ncommands:\n";
	for(const affresco::cli::Command& command : affresco::cli::commands())
		out << "  " << command.name << ' ' << command.usage << "\n      " << command.summary << '\n';
	out << "\nexit codes: 0 done, 1 the file has problems, 2 a wrong command line, 3 it couldn't be done\n"
	       "more: tools/affrescoui/README.md\n";
}

/// @brief Runs a command, turning what it throws into a message and an exit code.
/// @param command The command.
/// @param args What follows its name.
/// @return How it ended.
affresco::cli::Exit run(const affresco::cli::Command& command, std::span<const std::string_view> args) {
	using affresco::cli::Exit;
	if(!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
		std::cout << "usage: affrescoui " << command.name << ' ' << command.usage << '\n' << command.summary << '\n';
		return Exit::Success;
	}
	try {
		return command.run(args);
	} catch(const affresco::cli::UsageError& error) {
		std::cerr << "affrescoui " << command.name << ": " << error.what() << "\nusage: affrescoui " << command.name
		          << ' ' << command.usage << '\n';
		return Exit::Usage;
	} catch(const affresco::cli::FileProblem& error) {
		std::cerr << "affrescoui " << command.name << ": " << error.what() << '\n';
		return Exit::Problems;
	} catch(const std::exception& error) {
		std::cerr << "affrescoui " << command.name << ": " << error.what() << '\n';
		return Exit::Failure;
	}
}

/// @brief Runs the tool.
/// @param args The command line, without the program's name.
/// @return How it ended.
affresco::cli::Exit runTool(const std::vector<std::string_view>& args) {
	using affresco::cli::Exit;
	if(args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
		printHelp(args.empty() ? std::cerr : std::cout);
		return args.empty() ? Exit::Usage : Exit::Success;
	}
	if(args[0] == "--version") {
		std::cout << "affrescoui " AFFRESCOUI_VERSION "\n";
		return Exit::Success;
	}
	for(const affresco::cli::Command& command : affresco::cli::commands())
		if(command.name == args[0]) return run(command, std::span(args).subspan(1));
	std::cerr << "affrescoui: unknown command '" << args[0] << "'\n\n";
	printHelp(std::cerr);
	return Exit::Usage;
}

} // namespace

int main(int argc, char** argv) {
	try {
		return static_cast<int>(runTool(std::vector<std::string_view>(argv + 1, argv + argc)));
	} catch(...) { // run() reports what commands throw; only running out of memory gets here
		return static_cast<int>(affresco::cli::Exit::Failure);
	}
}
