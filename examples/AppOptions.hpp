#pragma once

// Command-line options shared by the examples:
//   --brain "command"   run `command` and connect the screen to it (it is the "brain" author)
//   --stdio             the screen's stream is this process's stdin/stdout (used by the MCP server: "mcp" author)
//   --plugin file       import a change set (a "plugin") at start; can be repeated

#include <sgl/sgl.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct AppOptions {
	std::vector<std::string> positional;
	std::string brain;
	bool stdio = false;
	std::vector<std::string> plugins;

	static AppOptions parse(int argc, char** argv) {
		AppOptions options;
		for(int i = 1; i < argc; ++i) {
			const std::string_view arg = argv[i];
			if(arg == "--brain" && i + 1 < argc) options.brain = argv[++i];
			else if(arg == "--plugin" && i + 1 < argc) options.plugins.emplace_back(argv[++i]);
			else if(arg == "--stdio") options.stdio = true;
			else options.positional.emplace_back(arg);
		}
		return options;
	}

	// Imports the plugins, then connects the stream (the channel must outlive the screen: declare it first)
	void connect(sgl::Application& app, sgl::Screen& screen, std::unique_ptr<sgl::Channel>& channel) const {
		for(const std::string& file : plugins) {
			std::ifstream in(file);
			if(!in) throw std::runtime_error("could not open plugin " + file);
			std::stringstream text;
			text << in.rdbuf();
			std::string error;
			if(!screen.importChanges(text.str(), &error)) throw std::runtime_error(file + ": " + error);
			std::cerr << "plugin applied: " << file << " (revision " << screen.revision() << ")\n";
		}

		if(stdio) {
			channel = std::make_unique<sgl::channel::StdioChannel>(app);
			screen.connect(*channel, "mcp");
			channel->onClosed.connect([&app] { app.quit(); }); // the MCP server went away
		} else if(!brain.empty()) {
			channel = std::make_unique<sgl::channel::ProcessChannel>(app, brain);
			screen.connect(*channel, "brain");
		}
	}
};
