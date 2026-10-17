/// @file
/// @brief affrescoui_studio: a client for a running affrescoui_host.
//
// Ask an AI agent for changes in plain words, apply JSON Patches yourself, review the agent's proposals (preview them
// live in the host, accept, "always allow", reject), pick the mode, revoke grants, browse, revert and restore the
// history.
//
//   affrescoui_studio [--socket DIR]      (default /tmp/affrescoui-<uid>, where affrescoui_host listens)
//
// The Studio is itself an AffrescoUI screen (assets/screens/studio.screen.json), layout and behaviour included. This
// program only plugs it into the host: every message from the host runs the screen's "host:<type>" action (the
// message is its `event`), and the screen's "send" events go to the host.

#include "SocketDir.hpp"

#include <affresco/affresco.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
	std::filesystem::path socketDir = defaultSocketDir();
	for(int i = 1; i < argc; ++i) {
		if(std::string_view(argv[i]) == "--socket" && i + 1 < argc) socketDir = argv[++i];
		else {
			std::cerr << "usage: affrescoui_studio [--socket DIR]\n";
			return 2;
		}
	}

	try {
		affresco::Application app;
		std::unique_ptr<affresco::channel::SocketChannel> host; // declared before the screen, so it outlives it

		affresco::Screen studio = affresco::Screen::fromFile(AFFRESCOUI_EXAMPLES_ASSETS "/screens/studio.screen.json");
		studio.onUnhandledAction([](const std::string& action, affresco::Panel&) {
			if(!action.starts_with("host:")) std::cerr << "affrescoui_studio: no action '" << action << "'\n";
		});
		affresco::Window& window = studio.createWindow(app);

		auto run = [&](const std::string& action, const affresco::json& event) {
			studio.runAction(action, window.root(), event.dump());
		};

		auto connect = [&] {
			try {
				host = affresco::channel::SocketChannel::connect(app, socketDir / "studio.sock");
			} catch(const std::exception& error) {
				run("host:closed", {{"message", error.what()}});
				return;
			}
			host->onMessage.connect([&](const std::string& line) {
				const affresco::json message = affresco::json::parse(line, nullptr, false);
				if(message.is_object()) run("host:" + message.value("type", std::string()), message);
			});
			host->onClosed.connect([&] {
				run("host:closed", {{"message", "The host closed the connection"}});
				app.post([&] { host.reset(); }); // not from inside its own signal
			});
		};

		studio.onEvent([&](const std::string& eventJson) {
			const affresco::json event = affresco::json::parse(eventJson);
			const std::string name = event.value("name", "");
			const affresco::json& data = event["data"];

			if(name == "reconnect") {
				if(!host) connect();
			} else if(!host) {
				run("local:error", {{"message", "Not connected to the host"}});
			} else if(name == "send") {
				host->send(data.dump());
			} else if(name == "sendPatch") {
				// Typed as text: it has to be a JSON Patch array before it can be a revision
				const affresco::json patch = affresco::json::parse(data.value("text", ""), nullptr, false);
				if(!patch.is_array()) {
					run("local:error", {{"message", "Not a JSON Patch: expected a list like [{\"op\": ...}]"}});
					return;
				}
				host->send(
				    affresco::json{{"type", "revision"}, {"message", "Direct change from the Studio"}, {"patch", patch}}
				        .dump());
			}
		});

		connect();
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_studio: " << error.what() << '\n';
		return 1;
	}
}
