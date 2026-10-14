// A chat app whose layout *and behaviour* live in assets/screens/chat.screen.json (sGLGUI Screen Format):
// threads, messages, sending and selection are state + actions in the file. This program only adds the drag handle
// (a native widget) and, optionally, a "brain": a program that receives what you type and sends back revisions
// that change the app while it runs.
//
//   chatbot_demo                                       the chat on its own
//   chatbot_demo --brain "python3 examples/brains/rules_brain.py"   type "dark mode", "add a counter", "undo", ...
//   chatbot_demo --stdio                               driven by the MCP server (mcp/sglgui_mcp.py starts it)
//   chatbot_demo --plugin my-changes.json              starts with a saved change set applied

#include "AppOptions.hpp"

#include <sgl/sgl.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

// Thin vertical strip on the sidebar's right edge, like the splitter next to VS Code's file explorer:
// drag it to resize the sidebar. Shows a line while hovered or dragged. Used in the screen as "type": "resizeHandle".
class ResizeHandle : public sgl::Panel {
public:
	sgl::Signal<float> onDrag; // requested x of the split line, in window coordinates

	explicit ResizeHandle(sgl::Color lineColor) : lineColor_(lineColor) {
		setColor(sgl::colors::transparent);
		setCursor(sgl::Cursor::ResizeHorizontal);

		onHover.connect([this](bool hovered) {
			hovered_ = hovered;
			invalidate();
		});

		// The window keeps sending mouse events here until the button is released, even outside the strip
		onMouseButton.connect([this](const sgl::MouseButtonEvent& event) {
			if(event.button != sgl::MouseButton::Left) return;
			dragging_ = event.action == sgl::Action::Press;
			grabOffset_ = event.position.x - splitX(); // keeps the line where it was grabbed, no jump
			invalidate();
		});
		onMouseMove.connect([this](const sgl::MouseMoveEvent& event) {
			if(dragging_) onDrag(event.position.x - grabOffset_);
		});
	}

protected:
	// Drawn rather than added as a child panel: a child would be hit-tested first and steal the clicks
	void paint(sgl::Painter& painter) const override {
		if(!hovered_ && !dragging_) return;
		const float x = splitX();
		painter.fillRect({x - 1.0f, frame().top, x + 1.0f, frame().bottom}, lineColor_);
	}

private:
	[[nodiscard]] float splitX() const noexcept { return frame().left + frame().width() / 2.0f; }

	sgl::Color lineColor_;
	bool hovered_ = false;
	bool dragging_ = false;
	float grabOffset_ = 0.0f;
};

} // namespace

int main(int argc, char** argv) {
	try {
		const AppOptions options = AppOptions::parse(argc, argv);
		sgl::Application app;
		std::unique_ptr<sgl::Channel> channel; // declared before the screen, so it outlives it

		sgl::Screen screen = sgl::Screen::fromFile(SGLGUI_EXAMPLES_ASSETS "/screens/chat.screen.json");
		screen.registerType("resizeHandle", [&screen] {
			auto handle = std::make_unique<ResizeHandle>(screen.color("accent"));
			handle->onDrag.connect([&screen, sender = handle.get()](float x) {
				screen.runAction("resizeSidebar", *sender, "{\"x\": " + std::to_string(x) + "}");
			});
			return handle;
		});

		sgl::Window& window = screen.createWindow(app);

		// Whatever was clicked, typing keeps going to the input box
		window.onMouseButton.connect([&](const sgl::MouseButtonEvent& event) {
			if(event.action == sgl::Action::Press) window.setFocus(screen.find("input"));
		});

		options.connect(app, screen, channel);

		return app.run();
	} catch(const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
