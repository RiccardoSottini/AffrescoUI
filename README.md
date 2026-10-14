# sGLGUI

A small GUI library written from scratch in **C++23** and **OpenGL 4.1 core**.

Started in 2017 as a way to learn OpenGL and C++, revived in 2026.

## Features
- Nested panels that own their children, sized and positioned in pixels or percentages of the parent
- Edge anchors (top/bottom/left/right), minimum sizes, clipping to the parent
- Typed input events (keyboard, text, mouse, scroll) routed to the focused or hovered panel, with lambda signals
- Mouse capture for drags, hover enter/leave events, and per-panel cursor shapes (arrow, I-beam, hand, resize)
- Text: TrueType fonts baked into a glyph atlas, labels with padding, alignment and word wrap, a text input with caret
- Batched renderer: rectangles and text share one shader, so a frame with one font is one draw call,
  and frames are drawn only when something changed
- Alpha blending; fully transparent panels act as free layout containers
- Works on HiDPI (Retina) screens

## Example
```cpp
#include <sgl/sgl.hpp>

using namespace sgl::literals;

int main() {
	sgl::Application app;
	sgl::Window& window = app.createWindow("Hello", 500, 500);

	auto& panel = window.add();
	panel.setPosition(10_pct, 10_pct);
	panel.setSize(80_pct, 80_pct);
	panel.setColor(sgl::Color::hex(0x6767ff));

	window.setDefaultFont(sgl::Font::load("Roboto-Regular.ttf", 16.0f));
	auto& label = panel.add<sgl::Label>("Hello, sGLGUI!");
	label.setAnchor(sgl::Edge::Left, 0_px);
	label.setAnchor(sgl::Edge::Right, 0_px);
	label.setHeight(100_pct);
	label.setTextColor(sgl::colors::white);
	label.setAlignment(sgl::Align::Center, sgl::Align::Center);

	label.onMouseButton.connect([](const sgl::MouseButtonEvent& event) {
		if(event.action == sgl::Action::Press) { /* ... */ }
	});

	return app.run();
}
```
See [examples/chatbot_demo.cpp](examples/chatbot_demo.cpp) for a complete app built from custom widgets
(`Panel` subclasses): a sidebar of threads that you can resize by dragging its edge (like VS Code's explorer),
a chat history and a composer with a text input.

## Building
Requires CMake ≥ 3.25 and a C++23 compiler. GLFW is downloaded automatically.

```sh
cmake -S . -B build
cmake --build build
./build/chatbot_demo
```

To use it from another CMake project:
```cmake
add_subdirectory(sGLGUI)   # or FetchContent
target_link_libraries(my_app PRIVATE sglgui::sglgui)
```

## Project layout
```
include/sgl/      public headers (no OpenGL or GLFW headers exposed)
src/              implementation
src/gl/           all OpenGL code: context, RAII objects, shaders, batched renderer
src/detail/       internal helpers (UTF-8, PPM images)
external/glad/    vendored OpenGL 4.1 core loader
external/stb/     vendored stb_truetype (font rasteriser)
examples/         example programs (examples/assets: Roboto font, Apache 2.0)
```

## License
**Apache License 2.0**: free to use, modify and distribute, commercially or not. Anyone who distributes it must pass
on the [NOTICE](NOTICE) file, which credits the author. Copyright (c) 2017-2026 Riccardo Sottini. See
[LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
