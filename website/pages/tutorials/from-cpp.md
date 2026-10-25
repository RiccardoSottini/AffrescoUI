# From C++

The tools above need no code. An application embeds the same engine to add what a screen file can't hold: native
actions, custom node types, functions, images it computes, and the grants for services and the network.

```cpp
#include <affresco/affresco.hpp>

int main() {
	affresco::Application app;
	affresco::Screen screen = affresco::Screen::fromFile("app.screen.json");
	screen.createWindow(app);

	screen.onAction("save", [&](affresco::Panel&) { /* native code, if needed */ });
	screen.listen(app, "/tmp/my-app/agent.sock", "agent"); // optional: let other programs change it live
	return app.run();
}
```

```cmake
find_package(AffrescoUI REQUIRED)   # or add_subdirectory(AffrescoUI) / FetchContent
target_link_libraries(my_app PRIVATE affrescoui::affrescoui)
```

## What code adds

| Call | What it does | In the screen |
|---|---|---|
| `onAction(name, fn)` | A native action | `"onClick": "save"` |
| `onCall(name, fn)` | A function returning JSON | `{ "call": "pickFile", "into": "state.path" }` |
| `registerType(name, factory)` | A custom node type, before `createWindow` | `{ "type": "chart" }` |
| `setImage(name, w, h, rgba)` | An image the app draws; call again to change it | `{ "type": "image", "source": "chart" }` |
| `allowImageDirectory(dir)` | Image files from another folder | `"source": "photo.png"` |
| `services().addRoot(name, dir)` | A folder the `fs` service may use | `"services"` |
| `network().allow(origin)` | An origin `connections` may reach | `"connections"` |
| `features().trustPublisher(key)` | A publisher features may come from | `"features"` |
| `listen(app, socket, author)` | Peers on that socket change the screen as `author` | `permissions` rules |

The document decides how a panel looks: a change code makes to a built panel directly lasts until the document shows
something new there. To keep a change, make it in the document (state or a revision) or in a registered type.

The libraries are also available one by one (`affrescoui::render`, `affrescoui::vcs`, ...). The classes and functions
are in [the C++ API reference](../reference/cpp-api.md). The stock host, `affrescoui_launcher`, is a program like this
one (`apps/host`); the example apps in `examples/` have no C++ of their own and run on it, and
`modules/capi/example` puts a screen inside a program's own OpenGL scene through the C API.

Next: [other languages](bindings.md).
