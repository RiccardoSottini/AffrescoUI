/// @file
/// @brief The other steps: copy, paste, copyImage, log, stop, note.

#include "Engine.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Bitmap.hpp"
#include "affresco/render/ImageCache.hpp"
#include "affresco/render/Window.hpp"

#include <memory>
#include <string>

namespace affresco::format {

namespace {

/// The most bytes of the clipboard's text "paste" reads.
constexpr std::size_t maxPastedBytes = 1U << 20;

/// @brief "paste": the clipboard's text, or with "image" its image under the name "as" gives, only from the person's
/// own input (a timer, a reply or a peer's message can't read what they copied), written "into" a state path, and given
/// to "onDone" as `result`. Without a window (a check, a preview) nothing happens.
/// @param s The step.
/// @return true.
bool paste(const Running& s) {
	const json kind = s.value("paste");
	if(kind != "text" && kind != "image") fail(s.where + ".paste", R"(expected "text" or "image")");
	const bool image = kind == "image";
	const std::string name = image ? text(s.value("as")) : std::string();
	if(image && name.empty()) fail(s.where + ".as", "expected the name the pasted image gets, e.g. \"clip\"");
	const std::optional<json::json_pointer> into =
	    s.step.contains("into") ? std::optional<json::json_pointer>(s.target("into")) : std::nullopt;
	s.steps.checkControl(s.where); // a revision can't read the person's clipboard for it
	Engine& engine = s.steps.engine();
	// An unaccepted proposal could wire a button to paste into state its author reads: no inbound clipboard then
	if(engine.versions.previewing())
		fail(s.where + ".paste", "doesn't read the clipboard while a proposal is previewed");
	if(!engine.window) return true;
	if(!engine.window->routingOwnPress())
		fail(s.where + ".paste", "reads the clipboard only on the person's own input");
	json pasted;
	if(image) { // the image under its name, for an image node's source; its name and size as the value
		const std::shared_ptr<const Bitmap> bitmap = engine.window->clipboardImage();
		if(bitmap) {
			engine.document.images().setNamed(name, bitmap);
			pasted = {{"image", name}, {"width", bitmap->width()}, {"height", bitmap->height()}};
		}
	} else {
		pasted = engine.window->clipboardText();
		if(pasted.get_ref<const std::string&>().size() > maxPastedBytes)
			fail(s.where + ".paste", "the clipboard holds over 1 MiB of text");
	}
	if(into) {
		statePlace(engine.versions.state(), *into, s.where + ".into") = pasted;
		engine.document.markStateChanged(*into);
	}
	if(s.step.contains("onDone")) {
		const SharedScope locals = snapshot(s.locals);
		const auto outcome =
		    std::make_shared<const Scope>(Scope{locals.get(), locals, json{{"result", std::move(pasted)}}, {}, {}});
		s.steps.later(s.step["onDone"], s.sender, s.event, outcome, s.where + ".onDone")();
	}
	return true;
}

/// @brief Runs copy (to the clipboard), paste (from it), log (a debug message), stop (ends the action) or note (a
/// comment).
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runOtherStep(const Running& s) {
	const json& step = s.step;
	if(step.contains("copy")) {
		if(Window* window = s.steps.engine().window) window->setClipboardText(text(s.value("copy")));
		return true;
	}
	if(step.contains("paste")) return paste(s);
	if(step.contains("copyImage")) { // an image of the screen's (named, a snapshot's or one pasted) to the clipboard
		Engine& engine = s.steps.engine();
		const std::string name = text(s.value("copyImage"));
		const std::shared_ptr<const Bitmap> bitmap = engine.document.images().named(name);
		if(!bitmap) fail(s.where + ".copyImage", "no image named \"" + name + "\"");
		if(engine.window) engine.window->setClipboardImage(bitmap);
		return true;
	}
	if(step.contains("log")) {
		log(LogLevel::Debug, "AffrescoUI screen log: " + text(s.value("log")));
		return true;
	}
	if(step.contains("stop")) return false;
	if(step.contains("note")) return true; // a comment
	return std::nullopt;
}

} // namespace

void registerOtherSteps(StepRegistry& registry) {
	registry.add({"other",
	              {{"copy", {}},
	               {"paste", {"into", "onDone", "as"}, true},
	               {"copyImage", {}},
	               {"log", {}},
	               {"stop", {}},
	               {"note", {}}},
	              runOtherStep});
}

} // namespace affresco::format
