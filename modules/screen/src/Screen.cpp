/// @file
/// @brief affresco::Screen, the composition root: puts the format's rules, the repository, the runtime and the peers
/// together with the services, and adds includes, declared channels and the "persist" state file.

#include "ScreenFiles.hpp"
#include "ScreenImpl.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/format/Adapters.hpp"
#include "affresco/format/Migrate.hpp"
#include "affresco/format/Rules.hpp"
#include "affresco/format/Runtime.hpp"
#include "affresco/themes/Themes.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace affresco {

namespace {

/// @brief Reads an environment variable that holds a path.
/// @param name The variable's name, in ASCII.
/// @return Its value, or nothing if it is not set or empty.
std::optional<std::filesystem::path> environmentPath(std::string_view name) {
#ifdef _WIN32
	// Read in UTF-16: the ANSI code page can't hold every user name, and profile paths contain it.
	std::wstring wideName;
	for(const char letter : name) wideName.push_back(static_cast<wchar_t>(letter));
	wchar_t* value = nullptr;
	std::size_t length = 0;
	if(_wdupenv_s(&value, &length, wideName.c_str()) != 0 || !value) return std::nullopt;
	const std::unique_ptr<wchar_t, void (*)(void*)> freer(value, [](void* memory) { std::free(memory); });
#else
	const char* value = std::getenv(std::string(name).c_str());
	if(!value) return std::nullopt;
#endif
	if(*value == 0) return std::nullopt;
	return std::filesystem::path(value);
}

/// @brief The folder for application data of this platform, where screens' data is kept by default.
/// @return %APPDATA%\\affrescoui on Windows, ~/.affrescoui elsewhere (in the current folder without a home).
std::filesystem::path platformDataDirectory() {
#ifdef _WIN32
	return environmentPath("APPDATA").value_or(".") / "affrescoui";
#else
	return environmentPath("HOME").value_or(".") / ".affrescoui";
#endif
}

/// @brief Reads an included file.
/// @param path The file.
/// @return Its JSON object.
/// @throws ScreenError If it can't be read, isn't a JSON object, or nests too deeply.
json readInclude(const std::filesystem::path& path) {
	const std::ifstream file(path);
	if(!file) throw ScreenError("AffrescoUI screen: include: could not open " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	json part;
	try {
		part = parseUntrusted(text.str());
	} catch(const json::parse_error&) { // NOLINT(bugprone-empty-catch): reported below, as not a JSON object
	} catch(const DocumentError& deep) {
		throw ScreenError("AffrescoUI screen: include: " + path.string() + ": " + deep.what());
	}
	if(!part.is_object()) throw ScreenError("AffrescoUI screen: include: " + path.string() + " is not a JSON object");
	return part;
}

/// @brief Merges two objects key by key: `part`'s entries come first and `document`'s win.
/// @param document The including file's object, which receives the merge.
/// @param part The included file's.
void mergeObject(json& document, const json& part) {
	json merged = part;
	merged.update(document);
	document = std::move(merged);
}

/// @brief Merges an included file's section into the screen's own section of the same name.
/// @param section The screen's section, which receives the merge.
/// @param key Its name: "menus" and "strings" merge one level deeper, menu by menu and locale by locale.
/// @param part The included file's section.
void mergeSection(json& section, std::string_view key, const json& part) {
	if(key == "menus" || key == "strings") // a locale's keys merge, like a menu's items
		for(const auto& [menu, items] : part.items())
			if(items.is_object() && section.contains(menu) && section[menu].is_object())
				mergeObject(section[menu], items);
	mergeObject(section, part);
}

/// @brief Adds an included file's list to the screen's: its entries first, then the screen's it doesn't have.
/// @param list The screen's list, which receives the merge.
/// @param part The included file's.
void mergeList(json& list, const json& part) {
	json merged = part;
	for(const json& entry : list)
		if(std::ranges::find(merged, entry) == merged.end()) merged.push_back(entry);
	list = std::move(merged);
}

/// @brief Merges one included file into a screen: each of its sections comes first and the screen's entries win;
/// the screen's own root and window (and format and version) are kept, and an include's "$schema" is dropped. Menus
/// merge one level deeper, menu by menu, so several files can add items to the same menu; "persist" lists join.
/// @param document The screen.
/// @param part The included file, its own includes already merged.
void mergeInclude(json& document, const json& part) {
	static constexpr std::array<std::string_view, 5> screensOwn = {"$schema", "format", "version", "root", "window"};
	for(const auto& [key, value] : part.items()) {
		if(std::ranges::find(screensOwn, key) != screensOwn.end()) {
			if(!document.contains(key) && key != "$schema") document[key] = value;
		} else if(!document.contains(key)) {
			document[key] = value;
		} else if(value.is_object() && document[key].is_object()) {
			mergeSection(document[key], key, value);
		} else if(key == "persist" && value.is_array() && document[key].is_array()) {
			mergeList(document[key], value);
		}
	}
}

} // namespace

namespace detail {

/// @brief Merges a screen's "include" files into it (recursively, at most 8 deep): each section of an included file
/// comes first and the including file's entries win; root and window come from the including file.
/// @param document The screen.
/// @param baseDirectory Where its include paths are resolved from.
/// @param migrated Set if one of them was written for version 1, and migrated (Migrate.hpp).
/// @param depth Includes inside includes so far.
/// @throws ScreenError If an include can't be read.
void resolveIncludes(json& document, const std::filesystem::path& baseDirectory, bool& migrated, int depth) {
	if(!document.is_object() || !document.contains("include")) return;
	if(depth > 8) throw ScreenError("AffrescoUI screen: include: nested too deeply (a file includes itself?)");
	const json includes = document["include"].is_string() ? json::array({document["include"]}) : document["include"];
	document.erase("include");
	if(!includes.is_array()) throw ScreenError("AffrescoUI screen: include: expected a list of files");

	for(const json& name : includes) {
		if(!name.is_string()) throw ScreenError("AffrescoUI screen: include: expected file names");
		const std::filesystem::path path = baseDirectory / name.get<std::string>();
		json part = readInclude(path);
		if(!format::migrate(part).empty()) migrated = true;
		resolveIncludes(part, path.parent_path(), migrated, depth + 1);
		mergeInclude(document, part);
	}
}

} // namespace detail

namespace {

/// @brief The name of the folder a screen file is in.
/// @param screenFile The screen file, absolute or relative to the current folder.
/// @return The folder's name, or nothing if it has none (the root) or the path can't be made absolute.
std::optional<std::string> folderName(const std::filesystem::path& screenFile) {
	std::error_code error;
	const std::string folder = std::filesystem::absolute(screenFile, error).parent_path().filename().string();
	if(error || folder.empty()) return std::nullopt;
	return folder;
}

} // namespace

namespace detail {

/// @brief Where a screen file's data on disk is kept: its "persist" state and its history.
/// @param dataDirectory The data folder (Screen::defaultDataDirectory()).
/// @param screenFile The screen file.
/// @param suffix What is kept: ".state.json" or ".history".
/// @return In the data folder, the file's name without ".screen.json", then @p suffix: "habits.screen.json" gives
/// "habits.history". A folder's index screen is named after its folder: "flow/index.screen.json" gives "flow.history".
/// The name is the file's own while no other file has it (boundDataName(), recorded in "habits.location").
std::filesystem::path stateLocation(const std::filesystem::path& dataDirectory, const std::filesystem::path& screenFile,
                                    std::string_view suffix) {
	std::string name = screenFile.filename().string();
	for(const char* extension : {".json", ".screen"})
		if(name.ends_with(extension)) name.resize(name.size() - std::string_view(extension).size());
	if(name == "index") name = folderName(screenFile).value_or(name);
	if(!dataDirectory.empty())
		name = boundDataName([&](const std::string& bound) { return dataDirectory / (bound + ".location"); }, name,
		                     screenFile);
	return dataDirectory / (name + std::string(suffix));
}

/// @brief The name a change set records as its template: the screen file's name, with its folder for an index screen
/// ("flow/index.screen.json"), whose own name says nothing.
/// @param screenFile The screen file.
/// @return The name.
std::string templateName(const std::filesystem::path& screenFile) {
	const std::string file = screenFile.filename().string();
	if(file != "index.screen.json") return file;
	const std::optional<std::string> folder = folderName(screenFile);
	return folder ? *folder + "/" + file : file;
}

} // namespace detail

namespace {

/// @brief A screen converted from another format (SCREEN_FORMAT.md, "Other formats"): its warnings logged, and its
/// later updates (A2UI's) applied as revisions.
/// @param converted The conversion.
/// @param baseDirectory Where its files are.
/// @return The screen.
Screen fromConverted(const format::adapters::Converted& converted, std::filesystem::path baseDirectory) {
	for(const std::string& warning : converted.warnings)
		affresco::log(LogLevel::Warning, "AffrescoUI screen: " + warning);
	Screen screen = Screen::fromString(converted.screen.dump(), std::move(baseDirectory));
	for(const json& revision : converted.revisions) {
		std::string error;
		if(!screen.applyRevision(revision.dump(), &error))
			affresco::log(LogLevel::Warning, "AffrescoUI screen: an update left out: " + error);
	}
	return screen;
}

} // namespace

Screen::Screen(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Screen::Screen(Screen&&) noexcept = default;
Screen& Screen::operator=(Screen&&) noexcept = default;
Screen::~Screen() = default;

std::filesystem::path Screen::defaultDataDirectory() {
	return environmentPath("AFFRESCOUI_DATA_DIR").value_or(platformDataDirectory());
}

Screen Screen::fromFile(const std::filesystem::path& path) { return fromFile(path, defaultDataDirectory()); }

Screen Screen::fromFile(const std::filesystem::path& path, const std::filesystem::path& dataDirectory,
                        History history) {
	if(const std::filesystem::path manifest = appOf(path); !manifest.empty())
		return fromApp(manifest, dataDirectory, history); // as the host opens it: its models, seeds and data
	const std::ifstream file(path);
	if(!file) {
		throw ScreenError(
		    "AffrescoUI screen: could not open " + std::filesystem::absolute(path).string() +
		    " (relative paths start from the current folder: " + std::filesystem::current_path().string() + ")");
	}

	std::stringstream content;
	content << file.rdbuf();
	Screen screen = fromString(content.str(), path.parent_path());
	screen.impl_->repairs.setTemplateName(detail::templateName(path));
	// The history first: it sets the live state that the "persist" paths are then loaded into
	if(history == History::OnDisk) screen.setHistoryDirectory(detail::stateLocation(dataDirectory, path, ".history"));
	screen.setStateFile(detail::stateLocation(dataDirectory, path, ".state.json"));
	screen.setDataFile(
	    detail::stateLocation(dataDirectory, path, ".data.sqlite")); // after the state: they may import it
	screen.setUsageFile(detail::stateLocation(dataDirectory, path, ".usage.json"));
	return screen;
}

Screen Screen::fromString(std::string_view content, std::filesystem::path baseDirectory) {
	themes::install(); // the kits' components and tokens, before the document is checked against them
	json document;
	try {
		document = parseUntrusted(content); // a ScreenError if it nests too deeply
	} catch(const json::parse_error& error) {
		if(auto foreign = format::adapters::convertForeign(content)) // A2UI as JSON Lines
			return fromConverted(*foreign, std::move(baseDirectory));
		throw ScreenError(std::string("AffrescoUI screen: invalid JSON: ") + error.what());
	}
	if(format::adapters::detect(document) != format::adapters::SourceFormat::Screen)
		if(auto foreign = format::adapters::convertForeign(content))
			return fromConverted(*foreign, std::move(baseDirectory));
	bool migrated = !format::migrate(document).empty();
	detail::resolveIncludes(document, baseDirectory, migrated);
	if(migrated) // one notice for the screen and its parts: they work as they are, in memory
		affresco::log(LogLevel::Info,
		              "AffrescoUI screen: written for format version 1, read as version 2 (`affrescoui migrate "
		              "--write <files>` rewrites the files)");
	return Screen(std::make_unique<Impl>(std::move(document), std::move(baseDirectory)));
}

/* Runtime */

void Screen::registerType(std::string name, Factory factory, std::vector<std::string> properties,
                          ApplyProperties apply) {
	format::Runtime::ApplyProperties forward;
	if(apply) forward = [apply = std::move(apply)](Panel& panel, const json& own) { apply(panel, own.dump()); };
	impl_->runtime.registerType(std::move(name), std::move(factory), std::move(properties), std::move(forward));
}

void Screen::registerSurface(std::string name, SurfaceFactory factory, SurfaceOptions options) {
	impl_->runtime.registerSurface(std::move(name), std::move(factory), options);
}

void Screen::setPlaceholderTypes(bool enabled) { impl_->runtime.setPlaceholderTypes(enabled); }

void Screen::allowFileDialogs(bool allowed) { impl_->runtime.allowFileDialogs(allowed); }

void Screen::allowOtherWindows(bool allowed) { impl_->runtime.allowOtherWindows(allowed); }

void Screen::setLocale(std::string tag) { impl_->runtime.setLocale(std::move(tag)); }

Window& Screen::createWindow(Application& app) {
	impl_->warnOfFindings();
	Window& window = impl_->runtime.createWindow(app);
	startChannels(app);
	return window;
}

std::filesystem::path Screen::defaultSocketDirectory() {
	if(auto directory = environmentPath("AFFRESCOUI_SOCKET_DIR")) return *std::move(directory);
#ifdef _WIN32
	// Inside the user's profile, which other users can't write to, so nobody else can make the folder first.
	if(auto local = environmentPath("LOCALAPPDATA")) return *std::move(local) / "affrescoui" / "sockets";
	return std::filesystem::temp_directory_path() / "affrescoui" / "sockets"; // per user too: %LOCALAPPDATA%\Temp
#else
	// In /tmp, so that socket paths stay within their limit of about 100 bytes. The uid keeps users apart, and the
	// examples refuse the folder if another user made it first (apps/shared/SocketDir.hpp).
	return std::filesystem::path("/tmp") / ("affrescoui-" + std::to_string(getuid()));
#endif
}

void Screen::setHistoryDirectory(const std::filesystem::path& directory) {
	const vcs::Repository::OpenedHistory opened = impl_->repository.openHistory(directory);
	if(!opened.setAside.empty())
		affresco::log(LogLevel::Warning, "AffrescoUI history: the screen file changed since " + directory.string() +
		                                     " was written: a new history starts there, and the old one is kept in " +
		                                     opened.setAside.string());
	if(!opened.memoryOnly.empty())
		affresco::log(LogLevel::Warning, "AffrescoUI history: " + directory.string() + ": " + opened.memoryOnly +
		                                     "; changes to this screen are kept in memory only");
}

void Screen::setStateFile(std::filesystem::path file) {
	impl_->stateFile = std::move(file);
	impl_->load();
}

void Screen::setUsageFile(std::filesystem::path file) { impl_->runtime.usage.setFile(std::move(file)); }

void Screen::setUsageEnabled(bool on) { impl_->runtime.usage.setEnabled(on); }

bool Screen::usageConsented() const { return impl_->runtime.usage.consented(); }

void Screen::giveUsageConsent() { impl_->runtime.usage.giveConsent(); }

std::string Screen::usageJson() {
	return impl_->runtime.usage.summary(impl_->repository.document(), format::Usage::now()).dump();
}

void Screen::clearUsage() { impl_->runtime.usage.clear(); }

std::string Screen::catalogJson() const { return impl_->runtime.catalog().dump(); }

json Screen::schema() const { return impl_->runtime.schema(); }

std::string Screen::referenceTables(std::string_view markdown) const {
	return impl_->runtime.referenceTables(markdown);
}

void Screen::onCall(std::string name, Call call) {
	impl_->runtime.onCall(std::move(name), [call = std::move(call)](const json& arguments) {
		const std::string result = call(arguments.dump());
		return result.empty() ? json() : json::parse(result);
	});
}

void Screen::onAsyncCall(std::string name, AsyncCall call) {
	impl_->runtime.onAsyncCall(std::move(name),
	                           [call = std::move(call)](const json& arguments, CallProgress& progress) {
		                           const std::string result = call(arguments.dump(), progress);
		                           return result.empty() ? json() : json::parse(result);
	                           });
}

void Screen::setExecutor(Executor* executor) noexcept {
	impl_->runtime.setExecutor(executor);
	impl_->services.setExecutor(executor);
	impl_->relay.set(executor);
}

services::ServiceRegistry& Screen::services() noexcept { return impl_->services; }

format::Features& Screen::features() noexcept { return impl_->features; }

net::Client& Screen::network() {
	Impl& impl = *impl_;
	if(!impl.network) {
		impl.network = std::make_unique<net::Client>(impl.relay);
		impl.runtime.setNetwork(impl.network.get());
		impl.signIn = std::make_unique<net::SignIn>(impl.relay, *impl.network);
		impl.runtime.setIdentity(impl.signIn.get());
	}
	return *impl.network;
}

void Screen::setSignInOptions(net::SignInOptions options) {
	Impl& impl = *impl_;
	static_cast<void>(network());
	impl.runtime.setIdentity(nullptr); // the old one is gone before the new one declares
	impl.signIn = std::make_unique<net::SignIn>(impl.relay, *impl.network, std::move(options));
	impl.runtime.setIdentity(impl.signIn.get());
}

void Screen::setModel(std::string name, std::shared_ptr<DataModel> model) {
	impl_->runtime.setModel(std::move(name), std::move(model));
}

void Screen::build(Window& window) {
	impl_->warnOfFindings();
	impl_->runtime.build(window);
}

Panel& Screen::instantiate(std::string_view name, Panel& parent) { return impl_->runtime.instantiate(name, parent); }

void Screen::onAction(std::string name, Action action) {
	impl_->rules.addNativeAction(name);
	impl_->runtime.onAction(std::move(name), std::move(action));
}

void Screen::onUnhandledAction(UnhandledAction fallback) { impl_->runtime.onUnhandledAction(std::move(fallback)); }

void Screen::runAction(std::string_view name, Panel& sender, std::string_view eventJson) {
	json event;
	try {
		event = json::parse(eventJson);
	} catch(const json::parse_error& error) {
		throw ScreenError(std::string("AffrescoUI screen: runAction: invalid event JSON: ") + error.what());
	}
	impl_->runtime.runAction(name, sender, event);
}

Panel* Screen::find(std::string_view id) noexcept { return impl_->runtime.find(id); }

Color Screen::color(std::string_view name) const { return impl_->runtime.color(name); }

Color Screen::resolveColor(std::string_view value) const {
	return impl_->runtime.resolveColor(json(std::string(value)));
}

std::shared_ptr<const Font> Screen::font(std::string_view name) const { return impl_->runtime.font(name); }

void Screen::setImage(const std::string& name, const std::shared_ptr<const Bitmap>& bitmap) {
	impl_->runtime.setImage(name, bitmap);
}

void Screen::setImage(const std::string& name, int width, int height, std::span<const std::uint8_t> rgba) {
	setImage(name, Bitmap::fromPixels(width, height, rgba, Bitmap::Limits{}));
}

void Screen::allowImageDirectory(const std::filesystem::path& directory) {
	impl_->runtime.allowImageDirectory(directory);
}

void Screen::waitForImages() { impl_->runtime.waitForImages(); }

Connection Screen::onEvent(std::function<void(const std::string&)> listener) {
	return impl_->events.connect(std::move(listener));
}

} // namespace affresco
