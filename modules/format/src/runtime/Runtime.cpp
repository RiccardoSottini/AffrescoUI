/// @file
/// @brief format::Runtime: the public API, forwarding to the engine's subsystems (Engine.hpp).

#include "Catalog.hpp"
#include "ComposedVersions.hpp"
#include "Engine.hpp"
#include "Reference.hpp"

namespace affresco::format {

namespace {

/// @brief The document's version control as the engine sees it, a base so it is made before the engine.
struct ComposedHolder {
	/// @brief Wraps the document's version control.
	/// @param versions It.
	explicit ComposedHolder(VersionControl& versions) : composed(versions) {}
	ComposedVersions composed; ///< The document with its features merged in (Composition.hpp).
};

} // namespace

/// @brief The runtime's private parts: the engine, out of the public header.
// NOLINTNEXTLINE(misc-multiple-inheritance): base-from-member, the holder is built before the engine that uses it
struct Runtime::Impl final : ComposedHolder, Engine {
	/// @brief Builds the engine on the composed document.
	/// @param runtime The runtime.
	/// @param control The document's version control.
	/// @param baseDirectory Where relative paths start.
	Impl(Runtime& runtime, VersionControl& control, std::filesystem::path baseDirectory)
	    : ComposedHolder(control), Engine(runtime, composed, std::move(baseDirectory)) {}
};

Runtime::Runtime(VersionControl& versions, std::filesystem::path baseDirectory)
    : impl_(std::make_unique<Impl>(*this, versions, std::move(baseDirectory))) {
	impl_->document.readDerived();
	impl_->composed.setObserver(impl_.get());
}

Runtime::~Runtime() {
	impl_->composed.setObserver(nullptr);
	impl_->events.disconnectAll(); // the window and the panels it keeps would call into the runtime on their next event
	impl_->scheduler.cancelAll();  // and its timers and deferred tasks would run on freed memory
	impl_->calls.cancelAll();      // and what slow calls return would reach it
	impl_->serviceCalls.cancelAll();                        // and so would what services return
	if(impl_->identity) impl_->identity->onStatus(nullptr); // and so would a sign-in
}

void Runtime::registerType(std::string name, Factory factory, std::vector<std::string> properties,
                           ApplyProperties apply) {
	auto own = std::make_shared<PropertySet>();
	for(std::string& property : properties) own->properties.push_back({std::move(property)});
	// Called when the values change; whatever it throws is reported at the node
	own->apply = [apply = std::move(apply)](Panel& panel, const json& values, const PropertyReader&,
	                                        const std::string& path) {
		if(!apply) return;
		try {
			apply(panel, values);
		} catch(const std::exception& error) {
			fail(path, error.what());
		}
	};
	impl_->types.add({std::move(name), "Registered by the application", std::move(factory), std::move(own), true});
	impl_->document.changeAppearance(); // panels of the type apply its new function at the next update
}

void Runtime::registerSurface(std::string name, SurfaceFactory factory, SurfaceOptions options) {
	impl_->types.surfaces()->insert_or_assign(std::move(name), RegisteredSurface{std::move(factory), options});
}

void Runtime::setPlaceholderTypes(bool enabled) { impl_->placeholderTypes = enabled; }

Window& Runtime::createWindow(Application& app) {
	const json& document = impl_->versions.document();
	const json settings = document.contains("window") ? object(document["window"], "window") : json::object();

	for(const auto& [key, value] : settings.items()) {
		if(key != "title" && key != "width" && key != "height" && key != "minWidth" && key != "minHeight" &&
		   key != "menuBar" && key != "focusRing" && key != "icon")
			fail("window." + key, "unknown property");
	}

	Window& window =
	    app.createWindow(settings.contains("title") ? string(settings["title"], "window.title") : "AffrescoUI",
		                 settings.contains("width") ? integer(settings["width"], "window.width") : 800,
		                 settings.contains("height") ? integer(settings["height"], "window.height") : 600);
	window.setMinSize(settings.contains("minWidth") ? integer(settings["minWidth"], "window.minWidth") : 0,
	                  settings.contains("minHeight") ? integer(settings["minHeight"], "window.minHeight") : 0);

	impl_->document.images().setExecutor(&app); // decoded files arrive through the event loop from now on
	impl_->calls.setExecutor(&app);             // and so do the results of slow calls
	impl_->serviceCalls.setExecutor(&app);      // and of services
	build(window);
	return window;
}

void Runtime::setImage(const std::string& name, const std::shared_ptr<const Bitmap>& bitmap) {
	impl_->document.images().setNamed(name, bitmap);
}

void Runtime::allowImageDirectory(const std::filesystem::path& directory) {
	impl_->document.images().allowDirectory(directory);
}

void Runtime::setAudio(std::shared_ptr<audio::Engine> engine) { impl_->audio.setEngine(std::move(engine)); }

std::shared_ptr<audio::Engine> Runtime::audio() const { return impl_->audio.engine(); }

void Runtime::setAudioSource(const std::string& name, std::shared_ptr<audio::Source> source) {
	impl_->audio.setSource(name, std::move(source));
}

void Runtime::allowAudioDirectory(const std::filesystem::path& directory) { impl_->audio.allowDirectory(directory); }

void Runtime::waitForImages() { impl_->document.images().finishPending(); }

void Runtime::build(Window& window) {
	Engine& engine = *impl_;
	engine.window = &window;
	engine.serviceCalls.load(); // the grants are in place by now
	engine.reconciler.reconcile();
	engine.events.connect(window);

	// "onLoad": once, after the first update
	const json& document = engine.versions.document();
	if(document.contains("onLoad"))
		engine.scheduler.defer(
		    engine.runner.later(document["onLoad"], window.root(), {{"type", "load"}}, noVariables(), "onLoad"));
}

Panel& Runtime::instantiate(std::string_view name, Panel& parent) {
	const json& document = impl_->versions.document();
	const std::string key(name);
	if(!document.contains("templates") || !document["templates"].contains(key))
		throw DocumentError("AffrescoUI screen: unknown template '" + key + "'");
	return impl_->reconciler.buildDetached(document["templates"][key], parent, "templates." + key);
}

void Runtime::onAction(std::string name, Action action) {
	impl_->nativeActions.insert_or_assign(std::move(name), std::move(action));
}

void Runtime::onCall(std::string name, Call call) {
	impl_->nativeCalls.insert_or_assign(std::move(name), std::move(call));
}

void Runtime::onAsyncCall(std::string name, AsyncCall call) {
	impl_->asyncCalls.insert_or_assign(std::move(name), std::move(call));
}

void Runtime::setExecutor(Executor* executor) noexcept {
	impl_->calls.setExecutor(executor);
	impl_->serviceCalls.setExecutor(executor);
}

void Runtime::setServices(services::Services* services) {
	impl_->serviceCalls.setServices(services);
	impl_->document.globalsChanged(); // the "services" variable
	impl_->reconciler.requestReconcile();
}

void Runtime::setNetwork(network::Network* network) {
	impl_->connections.setNetwork(network);
	impl_->audio.setNetwork(network); // the sounds' URLs
	impl_->document.globalsChanged(); // the "connections" variable
	impl_->reconciler.requestReconcile();
}

void Runtime::setLocale(std::string tag) {
	impl_->document.setHostLocale(std::move(tag));
	impl_->document.globalsChanged(); // the "locale" variable
	impl_->reconciler.requestReconcile();
}

void Runtime::setIdentity(identity::Identity* identity) {
	if(impl_->identity) impl_->identity->onStatus(nullptr);
	impl_->identity = identity;
	if(identity)
		identity->onStatus([engine = impl_.get()] {
			engine->document.globalsChanged(); // the "auth" variable
			engine->connections.authChanged();
			engine->reconciler.requestReconcile();
		});
	impl_->connections.declareSignIns();
	impl_->document.globalsChanged();
	impl_->reconciler.requestReconcile();
}

void Runtime::setModel(std::string name, std::shared_ptr<DataModel> model) {
	impl_->models.set(std::move(name), std::move(model));
}

void Runtime::setDataStore(data::Store* store, const json* oldState) { impl_->data.setStore(store, oldState); }

data::Store& Runtime::dataStore() noexcept { return impl_->data.mainStore(); }

json Runtime::dataValue() {
	const json* value = impl_->data.variable();
	return value ? *value : json::object();
}

void Runtime::setDataAuthor(std::string author) { impl_->data.setAuthor(std::move(author)); }

void Runtime::allowDataDirectory(const std::filesystem::path& directory) { impl_->data.allowDirectory(directory); }

void Runtime::onUnhandledAction(UnhandledAction fallback) { impl_->unhandled = std::move(fallback); }

void Runtime::runAction(std::string_view name, Panel& sender, const json& event) {
	const Record* record = impl_->reconciler.recordOf(&sender);
	const SharedScope locals = record ? record->locals : noVariables();
	impl_->runner.handle(json(std::string(name)), sender, event, *locals, "runAction");
}

void Runtime::runOnce(const json& steps, const std::string& commit) {
	const std::string where = "commit " + commit.substr(0, 7) + ".run";
	const json event = {{"type", "run"}, {"commit", commit}};
	if(!impl_->window) {
		impl_->runner.handle(steps, impl_->unattached, event, *noVariables(), where);
		return;
	}
	impl_->scheduler.defer([engine = impl_.get(), alive = std::weak_ptr<bool>(impl_->alive), steps, event, where] {
		if(!alive.lock() || !engine->window) return;
		engine->runner.handle(steps, engine->window->root(), event, *noVariables(), where);
	});
}

void Runtime::setHandlersSuspended(bool suspended) noexcept { impl_->handlersSuspended = suspended; }

bool Runtime::hasAction(std::string_view name) const {
	const json& document = impl_->versions.document();
	return document.contains("actions") && document["actions"].contains(std::string(name));
}

bool Runtime::receiveReply(const json& reply) { return impl_->asks.receive(reply); }

Window* Runtime::window() const noexcept { return impl_->window; }

const UpdateStatistics& Runtime::lastUpdate() const noexcept { return impl_->reconciler.statistics(); }

json Runtime::inspect(const Panel& panel) {
	const Panel* built = &panel;
	const Record* record = impl_->reconciler.recordOf(built);
	for(; !record && built->parent(); record = impl_->reconciler.recordOf(built)) built = built->parent();
	if(!record) return nullptr;

	json bindings = json::object();
	for(const CompiledProperty& property : record->node->properties)
		if(property.value.dynamic()) bindings[property.key] = property.value.written();
	json handlers = json::array();
	for(const auto& [key, handler] : record->handlers.items()) handlers.push_back(key);
	json answer = {{"node", built->id().empty() ? record->path : "#" + built->id()},
	               {"type", record->type},
	               {"path", record->path}};
	if(record->bind) answer["bind"] = *record->bind;
	answer["bindings"] = std::move(bindings);
	answer["handlers"] = std::move(handlers);
	return answer;
}

json Runtime::catalog() const { return format::catalog(*impl_); }

json Runtime::schema() const { return format::schema(*impl_); }

std::string Runtime::referenceTables(std::string_view markdown) const {
	return format::referenceTables(*impl_, markdown);
}

Panel* Runtime::find(std::string_view id) noexcept {
	return impl_->window ? impl_->window->root().findById(id) : nullptr;
}

Color Runtime::resolveColor(const json& value) const { return impl_->document.color(value, "color"); }

Color Runtime::color(std::string_view name) const { return impl_->document.namedColor(name); }

std::shared_ptr<const Font> Runtime::font(std::string_view name) const {
	auto found = impl_->document.findFont(name);
	if(!found) throw DocumentError("AffrescoUI screen: unknown font '" + std::string(name) + "'");
	return found;
}

} // namespace affresco::format
