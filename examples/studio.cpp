// sglgui_studio: a client for a running sglgui_host. Ask an AI agent for changes in plain words, apply JSON Patches
// yourself, review the agent's proposals (preview them live in the host), switch auto-accept, browse and restore the
// history.
//
//   sglgui_studio [--socket DIR]      (default /tmp/sglgui-<uid>, where sglgui_host listens)
//
// The Studio is itself an sGLGUI screen (assets/screens/studio.screen.json), layout and behaviour included. This
// program only plugs it into the host: every message from the host runs the screen's "host:<type>" action (the
// message is its `event`), and the screen's "send" events go to the host.

#include "SocketDir.hpp"

#include <sgl/sgl.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
	std::filesystem::path socketDir = defaultSocketDir();
	for(int i = 1; i < argc; ++i) {
		if(std::string_view(argv[i]) == "--socket" && i + 1 < argc) socketDir = argv[++i];
		else {
			std::cerr << "usage: sglgui_studio [--socket DIR]\n";
			return 2;
		}
	}

	try {
		sgl::Application app;
		std::unique_ptr<sgl::channel::SocketChannel> host; // declared before the screen, so it outlives it

		sgl::Screen studio = sgl::Screen::fromFile(SGLGUI_EXAMPLES_ASSETS "/screens/studio.screen.json");
		studio.onUnhandledAction([](const std::string& action, sgl::Panel&) {
			if(!action.starts_with("host:")) std::cerr << "sglgui_studio: no action '" << action << "'\n";
		});
		sgl::Window& window = studio.createWindow(app);

		auto run = [&](const std::string& action, const sgl::json& event) {
			studio.runAction(action, window.root(), event.dump());
		};

		auto connect = [&] {
			try {
				host = sgl::channel::SocketChannel::connect(app, socketDir / "studio.sock");
			} catch(const std::exception& error) {
				run("host:closed", {{"message", error.what()}});
				return;
			}
			host->onMessage.connect([&](const std::string& line) {
				const sgl::json message = sgl::json::parse(line, nullptr, false);
				if(message.is_object()) run("host:" + message.value("type", std::string()), message);
			});
			host->onClosed.connect([&] {
				run("host:closed", {{"message", "The host closed the connection"}});
				app.post([&] { host.reset(); }); // not from inside its own signal
			});
		};

		studio.onEvent([&](const std::string& eventJson) {
			const sgl::json event = sgl::json::parse(eventJson);
			const std::string name = event.value("name", "");
			const sgl::json& data = event["data"];

			if(name == "reconnect") {
				if(!host) connect();
			} else if(!host) {
				run("local:error", {{"message", "Not connected to the host"}});
			} else if(name == "send") {
				host->send(data.dump());
			} else if(name == "sendPatch") {
				// Typed as text: it has to be a JSON Patch array before it can be a revision
				const sgl::json patch = sgl::json::parse(data.value("text", ""), nullptr, false);
				if(!patch.is_array()) {
					run("local:error", {{"message", "Not a JSON Patch: expected a list like [{\"op\": ...}]"}});
					return;
				}
				host->send(
				    sgl::json{{"type", "revision"}, {"message", "Direct change from the Studio"}, {"patch", patch}}
				        .dump());
			}
		});

		connect();
		return app.run();
	} catch(const std::exception& error) {
		std::cerr << "sglgui_studio: " << error.what() << '\n';
		return 1;
	}
}
