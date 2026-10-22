/// @file
/// @brief Screen::createWindow(app, sharing): a screen in a second window that shares another's OpenGL context.

#include "affresco/Screen.hpp"
#include "affresco/core/Json.hpp"

#include <string>

namespace affresco {

Window& Screen::createWindow(Application& app, const Window& sharing) {
	// The document was checked when it was loaded: the window's keys have their types
	const json document = json::parse(documentJson());
	const json settings = document.contains("window") ? document["window"] : json::object();
	Window& window = app.createWindow(settings.value("title", std::string("AffrescoUI")), settings.value("width", 800),
	                                  settings.value("height", 600), sharing);
	window.setMinSize(settings.value("minWidth", 0), settings.value("minHeight", 0));
	setExecutor(&app); // slow calls and services post to the Application, as createWindow(app) does
	build(window);
	startChannels(app);
	return window;
}

} // namespace affresco
