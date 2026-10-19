#include "RuntimeImpl.hpp"

namespace affresco::format {

/* Document */

// Throws, changing nothing, if a color or a font is invalid
void Runtime::Impl::readDerived() {
	const json& doc = versions.document();
	decltype(colors) newColors;
	decltype(fonts) newFonts;

	if(doc.contains("colors")) {
		for(const auto& [name, value] : object(doc["colors"], "colors").items()) {
			const std::string path = "colors." + name;
			NamedColor named;
			auto parse = [&](const json& hex, const std::string& at) {
				const auto parsed = Color::parseHex(string(hex, at));
				if(!parsed) fail(at, "expected a hex color like \"#6366f1\"");
				return *parsed;
			};
			if(value.is_object()) { // per theme: {"light": "#ffffff", "dark": "#111827"}
				if(value.empty()) fail(path, "expected at least one theme");
				for(const auto& [themeName, hex] : value.items())
					named.themes.emplace(themeName, parse(hex, path + "." + themeName));
				named.base = named.themes.at(value.begin().key());
			} else {
				named.base = parse(value, path);
			}
			newColors.emplace(name, std::move(named));
		}
	}

	if(doc.contains("fonts")) {
		for(const auto& [name, value] : object(doc["fonts"], "fonts").items()) {
			const std::string path = "fonts." + name;
			object(value, path);
			if(!value.contains("file") || !value.contains("size")) fail(path, "a font needs \"file\" and \"size\"");

			const std::string file = (baseDirectory / string(value["file"], path + ".file")).string();
			const float size = number(value["size"], path + ".size");
			const std::string key = file + "@" + std::to_string(size);

			auto cached = fontCache.find(key);
			if(cached == fontCache.end()) {
				try {
					cached = fontCache.emplace(key, Font::load(file, size)).first;
				} catch(const std::runtime_error& error) {
					fail(path, error.what());
				}
			}
			newFonts.emplace(name, cached->second);
		}
	}

	colors = std::move(newColors);
	fonts = std::move(newFonts);
}

/* Values */

const Runtime::Impl::Globals& Runtime::Impl::globals() const {
	if(!globalsCache) {
		Globals& g = globalsCache.emplace();
		g.window = window ? json{{"width", window->width()}, {"height", window->height()}} : json::object();
		g.revision = toJson(versions.head());

		json list = json::array();
		for(const ProposalInfo& p : versions.proposals()) list.push_back(toJson(p));
		json log = json::array();
		for(const CommitInfo& c : versions.log()) log.push_back(toJson(c));

		const auto previewing = versions.previewing();
		g.other = {{"proposals", list},
		           {"history", log},
		           {"preview", previewing ? json(*previewing) : json()},
		           {"mode", toString(versions.mode())},
		           {"theme", theme},
		           {"screen", screen}};
	}

	return *globalsCache;
}

json Runtime::Impl::evaluate(const json& value, const Scope& locals, const json& event, const std::string& where) const {
	return format::evaluate(value, context(locals, event), where);
}

// "#rrggbb", "@name" (from "colors", in the current theme) or "transparent"
Color Runtime::Impl::color(const json& value, const std::string& path) const {
	const std::string name = string(value, path);
	if(name == "transparent") return colors::transparent;

	if(name.starts_with('@')) {
		const auto it = colors.find(std::string_view(name).substr(1));
		if(it == colors.end()) fail(path, "unknown color '" + name + "'");
		const auto variant = it->second.themes.find(theme);
		return variant != it->second.themes.end() ? variant->second : it->second.base;
	}

	const auto parsed = Color::parseHex(name);
	if(!parsed) fail(path, "expected a color: \"#rrggbb\", \"#rrggbbaa\", \"@name\" or \"transparent\"");
	return *parsed;
}

// The node's properties on top of its styles' (in order: later styles override earlier ones)
json Runtime::Impl::withStyles(const json& node, const std::string& path, int depth) const {
	object(node, path);
	if(!node.contains("style")) return node;
	if(depth > 16) fail(path + ".style", "styles nested too deeply (a style uses itself?)");

	const json& style = node["style"];
	if(isDynamic(style)) return node; // chosen per instance, once its variables are known (scoped())
	const json names = style.is_array() ? style : json::array({style});

	json merged = json::object();
	for(const json& name : names) {
		const std::string key = string(name, path + ".style");
		const json& document = versions.document();
		if(!document.contains("styles") || !document["styles"].contains(key))
			fail(path + ".style", "unknown style '" + key + "'");
		merged.update(withStyles(object(document["styles"][key], "styles." + key), "styles." + key, depth + 1));
	}

	json own = node;
	own.erase("style");
	merged.update(own);
	return merged;
}

/* Runtime */

Runtime::Runtime(VersionControl& versions, std::filesystem::path baseDirectory)
    : impl_(std::make_unique<Impl>(*this, versions, std::move(baseDirectory))) {
	impl_->readDerived();
	versions.setObserver(impl_.get());
}

Runtime::~Runtime() {
	impl_->versions.setObserver(nullptr);
	Window* window = impl_->window;
	if(!window) return;
	// The timers point at the runtime: stop them, or the window would run them on freed memory
	for(auto& [panel, record] : impl_->records) {
		if(record.everyTimer) window->cancelTimer(record.everyTimer);
		if(record.colorTween.timer) window->cancelTimer(record.colorTween.timer);
		if(record.textTween.timer) window->cancelTimer(record.textTween.timer);
	}
	for(const auto& [place, timer] : impl_->debounces) window->cancelTimer(timer);
	for(const auto& [pointer, timer] : impl_->animations) window->cancelTimer(timer);
	for(const std::uint64_t timer : impl_->onceTimers) window->cancelTimer(timer);
}

void Runtime::registerType(std::string name, Factory factory, std::vector<std::string> properties,
                           ApplyProperties apply) {
	impl_->types.insert_or_assign(std::move(name), Impl::CustomType{std::move(factory), std::move(properties), std::move(apply)});
}

void Runtime::setPlaceholderTypes(bool enabled) { impl_->placeholderTypes = enabled; }

Window& Runtime::createWindow(Application& app) {
	const json& document = impl_->versions.document();
	const json settings = document.contains("window") ? object(document["window"], "window") : json::object();

	for(const auto& [key, value] : settings.items()) {
		if(key != "title" && key != "width" && key != "height" && key != "minWidth" && key != "minHeight")
			fail("window." + key, "unknown property");
	}

	Window& window =
	    app.createWindow(settings.contains("title") ? string(settings["title"], "window.title") : "AffrescoUI",
		                 settings.contains("width") ? integer(settings["width"], "window.width") : 800,
		                 settings.contains("height") ? integer(settings["height"], "window.height") : 600);
	window.setMinSize(settings.contains("minWidth") ? integer(settings["minWidth"], "window.minWidth") : 0,
	                  settings.contains("minHeight") ? integer(settings["minHeight"], "window.minHeight") : 0);

	build(window);
	return window;
}

void Runtime::build(Window& window) {
	Impl* impl = impl_.get();
	impl->window = &window;
	impl->reconcile();

	// Bindings can use the window size
	window.onResize.connect([impl](const ResizeEvent&) { impl->requestReconcile(); });

	// Screen-wide shortcuts ("keys"); a focused text area keeps plain keys for typing
	window.onKey.connect([impl](const KeyEvent& event) {
		if(event.action == affresco::Action::Release) return;
		if(event.key == Key::Escape) impl->dismissOverlays(nullptr);
		const json& document = impl->versions.document();
		if(!document.contains("keys") || !document["keys"].is_object()) return;
		const std::string name = shortcut(event);
		const Panel* focused = impl->window->focusedPanel();
		const bool typing = dynamic_cast<const TextArea*>(focused) || dynamic_cast<const TextEditor*>(focused);
		if(name.empty() || (typing && !event.mods.super() && !event.mods.control() && !event.mods.alt() &&
		                    name != "escape"))
			return;
		for(const auto& [written, handler] : document["keys"].items()) {
			if(normalizeShortcut(written) != name) continue;
			const json steps = handler;
			impl->handle(steps, impl->window->root(), {{"type", "key"}, {"key", name}}, *noVariables(),
			             "keys." + written);
			return;
		}
	});

	// A press outside an open overlay that closes on it
	window.onMouseButton.connect([impl](const MouseButtonEvent& event) {
		if(event.action == affresco::Action::Press) impl->dismissOverlays(&event.position);
	});

	// "onLoad": once, after the first update
	const json& document = impl->versions.document();
	if(document.contains("onLoad"))
		window.defer(impl->later(document["onLoad"], window.root(), {{"type", "load"}}, noVariables(), "onLoad"));
}

Panel& Runtime::instantiate(std::string_view name, Panel& parent) {
	const json& document = impl_->versions.document();
	const std::string key(name);
	if(!document.contains("templates") || !document["templates"].contains(key))
		throw DocumentError("AffrescoUI screen: unknown template '" + key + "'");
	return impl_->buildDetached(document["templates"][key], parent, "templates." + key);
}

void Runtime::onAction(std::string name, Action action) {
	impl_->nativeActions.insert_or_assign(std::move(name), std::move(action));
}

void Runtime::onCall(std::string name, Call call) { impl_->nativeCalls.insert_or_assign(std::move(name), std::move(call)); }

void Runtime::onUnhandledAction(UnhandledAction fallback) { impl_->unhandled = std::move(fallback); }

void Runtime::runAction(std::string_view name, Panel& sender, const json& event) {
	const auto it = impl_->records.find(&sender);
	const SharedScope locals = it != impl_->records.end() ? it->second.locals : noVariables();
	impl_->handle(json(std::string(name)), sender, event, *locals, "runAction");
}

bool Runtime::hasAction(std::string_view name) const {
	const json& document = impl_->versions.document();
	return document.contains("actions") && document["actions"].contains(std::string(name));
}

Window* Runtime::window() const noexcept { return impl_->window; }

json Runtime::catalog() const {
	auto names = [](const auto& map) {
		json list = json::array();
		for(const auto& [name, value] : map) list.push_back(name);
		return list;
	};
	const json common = {"id",       "style",    "children", "x",      "y",       "width",     "height",  "minWidth",
	                     "minHeight", "anchors", "fill",     "grow",   "color",   "visible",   "cursor",  "tooltip",
	                     "disabled", "scroll",   "overlay",  "transition", "hover", "pressed", "focused",
	                     "dragging", "dragOver", "repeat",   "when",   "let",     "use",       "with",    "keys",
	                     "every",    "drag",     "locked",   "note"};
	const json text = {"text", "textColor", "font", "padding", "align", "wrap", "fitText"};
	json textarea = text;
	for(const char* key : {"placeholder", "placeholderColor", "bind", "check", "autofocus"}) textarea.push_back(key);

	json events = json::array();
	for(std::string_view name : handlerNames) events.push_back(std::string(name));

	json types = {
	    {"panel", {{"properties", common}, {"about", "A rectangle; children are placed by their anchors"}}},
	    {"label", {{"properties", text}, {"about", "Text"}}},
	    {"textarea", {{"properties", textarea}, {"about", "A single-line text input"}}},
	    {"editor",
		 {{"properties",
		   {"bind", "text", "caret", "font", "textColor", "lineNumbers", "lineNumberColor", "activeLineNumberColor",
		    "lineColor", "selectionColor", "caretColor", "autofocus"}},
		  {"about", "A multi-line text editor: selection, undo, clipboard, optional line numbers"}}},
	    {"stack", {{"properties", {"gap", "direction", "from", "wrap"}}, {"about", "Children in a column or a row"}}},
	    {"row", {{"properties", {"gap", "from", "wrap"}}, {"about", "A stack in a row"}}},
	    {"column", {{"properties", {"gap", "from"}}, {"about", "A stack in a column"}}},
	    {"grid", {{"properties", {"columns", "gap"}}, {"about", "Children in equal columns"}}},
	    {"toggle",
		 {{"properties", {"bind", "checked", "activeColor", "trackColor", "knobColor"}}, {"about", "A switch"}}},
	    {"checkbox",
		 {{"properties", {"bind", "checked", "activeColor", "trackColor", "knobColor"}}, {"about", "A checkbox"}}},
	    {"slider",
		 {{"properties", {"bind", "value", "min", "max", "step", "trackColor", "activeColor", "knobColor"}},
		  {"about", "A value in a range"}}},
	    {"outlet", {{"properties", json::array()}, {"about", "Shows the current screen from \"screens\""}}}};
	for(const auto& [name, type] : impl_->types)
		types[name] = {{"properties", type.properties}, {"about", "Registered by the application"}};

	return {{"types", types},
	        {"common", common},
	        {"events", events},
	        {"steps", {"set",      "push",  "remove",   "toggle",  "increment", "if",    "forEach", "while",
	                   "run",      "call",  "emit",     "send",    "connect",   "patch", "focus",   "scrollTo",
	                   "navigate", "after", "debounce", "animate", "copy",      "log",   "stop",    "note"}},
	        {"controlSteps",
	         {"accept", "reject", "preview", "endPreview", "restore", "revert", "undo", "patch", "call", "send",
	          "connect"}},
	        {"operators", operatorNames()},
	        {"variables",
	         {"state", "event", "window", "revision", "proposals", "history", "preview", "mode", "computed", "theme",
	          "screen", "nodes"}},
	        {"sections",
	         {"window", "fonts", "colors", "theme", "styles", "components", "templates", "screens", "state", "computed",
	          "watch", "actions", "keys", "onLoad", "channels", "persist", "include", "permissions", "root"}},
	        {"customTypes", names(impl_->types)},
	        {"nativeActions", names(impl_->nativeActions)},
	        {"nativeCalls", names(impl_->nativeCalls)}};
}

Panel* Runtime::find(std::string_view id) noexcept {
	return impl_->window ? impl_->window->root().findById(id) : nullptr;
}

Color Runtime::resolveColor(const json& value) const { return impl_->color(value, "color"); }

Color Runtime::color(std::string_view name) const {
	const auto it = impl_->colors.find(name);
	if(it == impl_->colors.end()) throw DocumentError("AffrescoUI screen: unknown color '" + std::string(name) + "'");
	const auto variant = it->second.themes.find(impl_->theme);
	return variant != it->second.themes.end() ? variant->second : it->second.base;
}

std::shared_ptr<const Font> Runtime::font(std::string_view name) const {
	const auto it = impl_->fonts.find(name);
	if(it == impl_->fonts.end()) throw DocumentError("AffrescoUI screen: unknown font '" + std::string(name) + "'");
	return it->second;
}

} // namespace affresco::format
