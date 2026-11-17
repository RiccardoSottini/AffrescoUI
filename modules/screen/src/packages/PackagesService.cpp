/// @file
/// @brief The `packages` service (PackagesService.hpp): `export`, `inspect` and `install`, each on a thread of its
/// own (or at once, for scripted runs), every message naming the roots a screen knows instead of the folders they are.

#include "packages/PackagesService.hpp"
#include "ScratchFolder.hpp"
#include "affresco/Screen.hpp"
#include "affresco/data/Store.hpp"
#include "affresco/format/App.hpp"
#include "affresco/format/Package.hpp"
#include "affresco/format/PackageFiles.hpp"
#include "affresco/format/PackageInstall.hpp"
#include "affresco/lisp/Document.hpp"
#include "affresco/notation/Notation.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifndef AFFRESCOUI_VERSION
#define AFFRESCOUI_VERSION "" // set by the build (modules/screen/CMakeLists.txt)
#endif

namespace affresco::detail {

namespace {

namespace fs = std::filesystem;
using services::Call;
using services::Effect;
using services::MethodSpec;

/// The most calls that run at once.
constexpr std::size_t maxCalls = 4;

/// The longest name of a root, an app's folder or an author.
constexpr std::size_t maxShortText = 64;

/// The longest author's name.
constexpr std::size_t maxAuthorLength = 200;

/// What a copy of an app leaves out, as the Launcher's copies do: C++, build files, scripts and hidden files.
constexpr std::array<std::string_view, 9> excludedSuffixes = {".cpp", ".hpp",   ".h",  ".c",  ".cc",
                                                              ".mm",  ".cmake", ".py", ".pyc"};

/// @brief Whether a text is an app's id, and so a folder name with no path in it: [a-z0-9-], 1 to 64, no '-' at
/// either end.
/// @param text The text.
/// @return Whether it is.
bool isAppId(std::string_view text) {
	if(text.empty() || text.size() > maxShortText || text.front() == '-' || text.back() == '-') return false;
	return std::ranges::all_of(text,
	                           [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

/// @brief Reads a whole file, at most some bytes.
/// @param file The file.
/// @param most The most it may hold.
/// @return Its bytes, or std::nullopt if it can't be read or is larger.
std::optional<std::string> readSmall(const fs::path& file, std::size_t most) {
	std::error_code error;
	const std::uintmax_t size = fs::file_size(file, error);
	if(error || size > most) return std::nullopt;
	std::ifstream in(file, std::ios::binary);
	std::string text(static_cast<std::size_t>(size), '\0');
	if(!in.read(text.data(), static_cast<std::streamsize>(text.size()))) return std::nullopt;
	return text;
}

/// @brief Writes a whole file.
/// @param file The file.
/// @param text What it holds.
/// @throws PackagesFailure If it can't be written.
void writeWhole(const fs::path& file, const std::string& text) {
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if(!(out << text) || !out.flush()) throw PackagesFailure("failed", "a file of the new app can't be written");
}

/// @brief An app in a root: the folder of its manifest.
struct FoundApp {
	std::string root = {};  ///< The root's name.
	fs::path top = {};      ///< `<root>/<folder>`.
	std::string sub = {};   ///< Where its manifest is in it: "", "json" or "lisp".
	fs::path manifest = {}; ///< Its app.json or app.lisp.
	std::string id = {};    ///< The id its manifest gives.
};

/// @brief The app a folder of a root holds: its own app.json or app.lisp, else a json or a lisp folder's (an example
/// written in both notations, or a copy of one).
/// @param root The root's name.
/// @param top The folder.
/// @return It, or std::nullopt if the folder holds none whose manifest reads.
std::optional<FoundApp> appIn(const std::string& root, const fs::path& top) {
	static const std::array<std::pair<std::string_view, std::string_view>, 4> places = {
	    {{"", "app.json"}, {"", "app.lisp"}, {"json", "app.json"}, {"lisp", "app.lisp"}}};
	for(const auto& [sub, name] : places) {
		const fs::path manifest = (sub.empty() ? top : top / sub) / name;
		std::error_code error;
		if(!fs::is_regular_file(fs::symlink_status(manifest, error))) continue;
		const std::optional<std::string> text = readSmall(manifest, format::maxAppManifestBytes);
		if(!text) continue;
		try {
			const format::AppManifest app = format::parseApp(notation::readDocument(*text, manifest));
			return FoundApp{root, top, std::string(sub), manifest, app.id};
		} catch(const std::exception&) {
			continue; // not an app: another place may be
		}
	}
	return std::nullopt;
}

/// @brief The apps of every granted root whose manifest gives an id, in the roots' order.
/// @param grant The grant.
/// @param id The id.
/// @return Them.
std::vector<FoundApp> appsWithId(const PackagesGrant& grant, std::string_view id) {
	std::vector<FoundApp> found;
	for(const auto& [name, root] : grant.roots) {
		std::error_code error;
		for(fs::directory_iterator entry(root, error), end; !error && entry != end; entry.increment(error)) {
			const std::string folder = entry->path().filename().string();
			if(folder.starts_with('.') || !entry->is_directory(error) || entry->is_symlink(error)) continue;
			if(std::optional<FoundApp> app = appIn(name, entry->path()); app && app->id == id)
				found.push_back(*std::move(app));
		}
	}
	return found;
}

/// @brief The tree of an app's document as it is now, as a package of its changes names its base: the app opened
/// with nothing kept.
/// @param app The app.
/// @return Its tree, or "" if it doesn't open.
std::string treeOf(const FoundApp& app) {
	try {
		const Screen screen = Screen::fromApp(app.manifest, {}, Screen::History::InMemory);
		format::PackageOptions options;
		options.engine = AFFRESCOUI_VERSION;
		return screen.exportPackage(options).base.tree;
	} catch(const std::exception&) {
		return {};
	}
}

/// @brief The copy of a package's base app the granted roots hold: the one whose document is exactly its base, else
/// one in the package's notation, else the first.
/// @param grant The grant.
/// @param package The package.
/// @return It, or std::nullopt if no root holds the app.
std::optional<FoundApp> baseCopy(const PackagesGrant& grant, const format::Package& package) {
	const std::vector<FoundApp> found = appsWithId(grant, package.base.app);
	for(const FoundApp& app : found)
		if(treeOf(app) == package.base.tree) return app;
	const std::string extension = package.base.notation == "lisp" ? ".lisp" : ".json";
	for(const FoundApp& app : found)
		if(app.manifest.extension() == extension) return app;
	if(found.empty()) return std::nullopt;
	return found.front();
}

/// @brief Says where a path is the way the screen knows it: each root by its name ("apps:my-app/app.json"), the data
/// folder as "data:", the home folder as "~", any other folder taken away.
class PlaceNames {
public:
	/// @brief Names a grant's folders.
	/// @param grant The grant.
	explicit PlaceNames(const PackagesGrant& grant) {
		for(const auto& [name, root] : grant.roots) add(root, name + ":");
		if(!grant.dataDirectory.empty()) add(grant.dataDirectory, "data:");
		add(fs::temp_directory_path(), "");
		for(const char* variable : {"HOME", "USERPROFILE"})
			if(const char* home = std::getenv(variable);
			   home != nullptr && *home != '\0') // NOLINT(concurrency-mt-unsafe)
				add(home, "~/");
		// The longest first, so a root inside the home folder is named as the root
		std::ranges::sort(places_, [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
	}
	/// @brief A text with every folder it names said the screen's way.
	/// @param text The text (an error's message).
	/// @return It.
	[[nodiscard]] std::string operator()(std::string text) const {
		for(const auto& [path, name] : places_)
			for(auto at = text.find(path); at != std::string::npos; at = text.find(path, at + name.size())) {
				std::size_t length = path.size();
				if(at + length < text.size() && (text[at + length] == '/' || text[at + length] == '\\')) ++length;
				text.replace(at, length, name);
			}
		return text;
	}
	/// @brief The private texts a package must never hold (format::scrubProblems()): every folder named here, but the
	/// temporary folder.
	/// @return Them.
	[[nodiscard]] std::vector<std::string> privateTexts() const {
		std::vector<std::string> texts;
		for(const auto& [path, name] : places_)
			if(!name.empty()) texts.push_back(path);
		return texts;
	}

private:
	/// @brief Names a folder, as it was given and as it really is.
	/// @param folder The folder.
	/// @param name Its name.
	void add(const fs::path& folder, const std::string& name) {
		std::error_code error;
		for(const fs::path& path :
		    {fs::absolute(folder, error).lexically_normal(), fs::weakly_canonical(folder, error)}) {
			std::string text = path.string();
			while(text.size() > 1 && (text.back() == '/' || text.back() == '\\')) text.pop_back();
			if(text.size() > 1 && std::ranges::none_of(places_, [&](const auto& place) { return place.first == text; }))
				places_.emplace_back(std::move(text), name);
		}
	}

	std::vector<std::pair<std::string, std::string>> places_; ///< Each folder's text, and its name.
};

/// @brief A granted root's folder.
/// @param grant The grant.
/// @param name The root's name.
/// @return It.
/// @throws PackagesFailure If no root has that name.
const fs::path& rootNamed(const PackagesGrant& grant, const std::string& name) {
	const auto found = grant.roots.find(name);
	if(found == grant.roots.end()) throw PackagesFailure("invalid", "root: no root is named \"" + name + "\"");
	return found->second;
}

/// @brief The trusted publishers the person keeps in the data folder (none without one).
/// @param grant The grant.
/// @return readTrustedPublishers().
json trustedPublishers(const PackagesGrant& grant) {
	if(grant.dataDirectory.empty()) return json::object();
	try {
		return format::readTrustedPublishers(grant.dataDirectory / "publishers.json");
	} catch(const std::exception&) {
		return json::object(); // a damaged file trusts nobody
	}
}

/// @brief A package from a call's arguments, its own limits applied before it is read.
/// @param with The arguments: `package`.
/// @param text Receives its text.
/// @return It.
/// @throws PackagesFailure If it is too large or isn't a package.
format::Package packageOf(const json& with, std::string& text) {
	text = with.at("package").dump();
	if(text.size() > format::maxPackageBytes)
		throw PackagesFailure("tooLarge",
		                      "package: larger than " + std::to_string(format::maxPackageBytes >> 20U) + " MiB");
	try {
		return format::readPackage(text);
	} catch(const std::exception& error) {
		throw PackagesFailure("invalid", std::string("package: not one AffrescoUI can install: ") + error.what());
	}
}

/// @brief Copies an app's data folder (each of its names in `<data>/apps`) into another data folder, its history's
/// lock left out, so the app can be opened there with its history while a window of it keeps its own.
/// @param data The host's data folder.
/// @param id The app's id.
/// @param into The other data folder.
void copyData(const fs::path& data, const std::string& id, const fs::path& into) {
	std::error_code error;
	const fs::path apps = data / "apps";
	for(fs::directory_iterator entry(apps, error), end; !error && entry != end; entry.increment(error)) {
		const std::string name = entry->path().filename().string();
		if(name != id && !name.starts_with(id + "~")) continue;
		std::error_code walking;
		for(fs::recursive_directory_iterator file(entry->path(), walking), last; !walking && file != last;
		    file.increment(walking)) {
			const fs::path relative = file->path().lexically_relative(entry->path());
			const fs::path target = into / "apps" / name / relative;
			std::error_code copying;
			if(file->is_symlink(copying)) {
				file.disable_recursion_pending();
			} else if(file->is_directory(copying)) {
				fs::create_directories(target, copying);
			} else if(relative.filename() != "lock") {
				fs::create_directories(target.parent_path(), copying);
				fs::copy_file(file->path(), target, fs::copy_options::overwrite_existing, copying);
			}
		}
	}
}

/// @brief Whether a copy of an app leaves a file or folder out.
/// @param name Its name.
/// @return Whether it does.
bool excluded(const std::string& name) {
	if(name.starts_with('.') || name == "CMakeLists.txt") return true;
	return std::ranges::any_of(excludedSuffixes, [&](std::string_view suffix) { return name.ends_with(suffix); });
}

/// @brief Copies a folder into a new one, without the files a copy leaves out and without links.
/// @param from The folder.
/// @param to The new folder.
/// @param stop Asks the copy to stop.
/// @throws PackagesFailure If it can't be copied, or was asked to stop.
void copyFolder(const fs::path& from, const fs::path& to, const std::stop_token& stop) {
	std::error_code error;
	fs::create_directories(to, error);
	if(error) throw PackagesFailure("failed", "the new app's folder can't be made");
	for(fs::directory_iterator entry(from, error), end; !error && entry != end; entry.increment(error)) {
		if(stop.stop_requested()) throw PackagesFailure("failed", "stopped");
		const std::string name = entry->path().filename().string();
		std::error_code kind;
		if(excluded(name) || entry->is_symlink(kind)) continue;
		if(entry->is_directory(kind)) {
			copyFolder(entry->path(), to / name, stop);
		} else if(entry->is_regular_file(kind)) {
			fs::copy_file(entry->path(), to / name, error);
			if(error) throw PackagesFailure("failed", "a file of the app can't be copied: " + name);
		}
	}
	if(error) throw PackagesFailure("failed", "the app's folder can't be read");
}

/// @brief Gives a new app's manifest its name, and a JSON entry's window its title, as the Launcher's copies have:
/// every other byte of a program's app.lisp as it was. Its id stays the package's app's, so the package applies.
/// @param manifest Its app.json or app.lisp.
/// @param name The name.
/// @throws PackagesFailure If it can't be read or written.
void giveName(const fs::path& manifest, const std::string& name) {
	const std::optional<std::string> text = readSmall(manifest, format::maxAppManifestBytes);
	if(!text) throw PackagesFailure("failed", "the new app's manifest can't be read");
	format::AppManifest app;
	try {
		app = format::parseApp(notation::readDocument(*text, manifest));
		if(manifest.extension() == ".lisp") {
			const std::array<std::string, 1> keyword = {"name"};
			writeWhole(manifest, lisp::withKeyword(*text, keyword, lisp::keywordValueText(name)).value_or(*text));
		} else {
			json value = json::parse(*text);
			value["name"] = name;
			writeWhole(manifest, value.dump(1, '\t') + "\n");
		}
	} catch(const PackagesFailure&) {
		throw;
	} catch(const std::exception& error) {
		throw PackagesFailure("failed", std::string("the new app's manifest: ") + error.what());
	}
	const fs::path entry = manifest.parent_path() / app.entry;
	if(entry.extension() != ".json" || !format::insideApp(app.entry)) return;
	const std::optional<std::string> screen = readSmall(entry, format::maxPackageBytes);
	if(!screen) return;
	json document = json::parse(*screen, nullptr, false);
	if(!document.is_object() || !document.contains("window") || !document["window"].is_object() ||
	   !document["window"].value("title", json()).is_string())
		return;
	document["window"]["title"] = name;
	writeWhole(entry, document.dump(1, '\t') + "\n");
}

/// @brief Removes a folder it made when an install doesn't finish.
class MadeFolder {
public:
	/// @brief Makes the folder.
	/// @param folder It: it must not exist.
	/// @param id Its name, for errors.
	/// @param root Its root's name, for errors.
	/// @throws PackagesFailure If it exists, or can't be made.
	MadeFolder(fs::path folder, const std::string& id, const std::string& root) : folder_(std::move(folder)) {
		std::error_code error;
		const fs::file_status status = fs::symlink_status(folder_, error);
		if(fs::exists(status) || !fs::create_directory(folder_, error))
			throw PackagesFailure("exists", root + ":" + id + " is there already: choose another name");
	}
	/// @brief Removes the folder, unless kept.
	~MadeFolder() {
		std::error_code error;
		if(!kept_) fs::remove_all(folder_, error);
	}
	MadeFolder(const MadeFolder&) = delete;
	MadeFolder& operator=(const MadeFolder&) = delete;
	MadeFolder(MadeFolder&&) = delete;
	MadeFolder& operator=(MadeFolder&&) = delete;
	/// @brief Keeps the folder: the install finished.
	void keep() noexcept { kept_ = true; }

private:
	fs::path folder_;   ///< See the constructor.
	bool kept_ = false; ///< See keep().
};

/// @brief The `packages` service.
class PackagesService final : public services::Service {
public:
	/// @brief Serves a grant.
	/// @param grant What it may touch.
	/// @param now Whether each call's work runs before invoke() returns (ServiceContext::now).
	PackagesService(PackagesGrant grant, bool now) : grant_(std::move(grant)), now_(now) {}
	/// @brief Asks each call's work to stop, and waits for it.
	~PackagesService() override {
		for(Task& task : tasks_) task.thread.request_stop();
		tasks_.clear(); // joins
	}
	PackagesService(const PackagesService&) = delete;
	PackagesService& operator=(const PackagesService&) = delete;
	PackagesService(PackagesService&&) = delete;
	PackagesService& operator=(PackagesService&&) = delete;

	/// @brief Starts a call to one of its methods. Main thread; returns at once (or once done, for scripted runs).
	/// @param method The method (its arguments already checked).
	/// @param call Where the result goes.
	/// @return An error if the call can't start, else std::nullopt.
	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		std::erase_if(tasks_, [](const Task& task) { return task.done->load(); });
		if(tasks_.size() >= maxCalls) return "busy: " + std::to_string(maxCalls) + " packages are being made already";
		auto work = [this, name = method.name, call](const std::stop_token& stop) { run(name, *call, stop); };
		if(now_) {
			work(std::stop_token{});
			return std::nullopt;
		}
		auto done = std::make_shared<std::atomic<bool>>(false);
		std::jthread thread([work = std::move(work), done](const std::stop_token& stop) {
			work(stop);
			done->store(true);
		});
		tasks_.push_back({std::move(done), std::move(thread)});
		return std::nullopt;
	}

private:
	/// @brief One call's work, on its thread.
	struct Task {
		std::shared_ptr<std::atomic<bool>> done = {}; ///< Set when its work returns.
		std::jthread thread = {};                     ///< Its thread.
	};

	/// @brief Does a call's work and ends it.
	/// @param method The method's name.
	/// @param call The call.
	/// @param stop The service's stop token.
	void run(const std::string& method, Call& call, const std::stop_token& stop) const {
		const PlaceNames names(grant_);
		try {
			const json& with = call.arguments();
			json result;
			if(method == "export") result = exportVersion(grant_, with, stop);
			else if(method == "inspect") result = inspectVersion(grant_, with);
			else result = installVersion(grant_, with, stop);
			call.finish(std::move(result));
		} catch(const PackagesFailure& failure) {
			call.fail(failure.code(), names(failure.what()));
		} catch(const std::exception& failure) {
			call.fail("failed", names(failure.what()));
		}
	}

	PackagesGrant grant_;     ///< See the constructor.
	bool now_ = false;        ///< See the constructor.
	std::vector<Task> tasks_; ///< The calls started, finished ones pruned at each invoke().
};

/// @brief The `packages` provider.
class PackagesProvider final : public services::Provider {
public:
	/// @brief Serves a grant.
	/// @param grant What it may touch.
	explicit PackagesProvider(PackagesGrant grant) : grant_(std::move(grant)) {}

	/// @brief The name declarations use.
	/// @return "packages".
	std::string_view name() const override { return "packages"; }
	/// @brief One sentence for the catalog.
	/// @return It.
	std::string_view about() const override {
		return "the host's apps exported as one shared file, and shared files reviewed and installed as new apps";
	}
	/// @brief The JSON Schema of its configuration.
	/// @return An empty object: it takes none.
	json configurationSchema() const override {
		return {{"type", "object"}, {"additionalProperties", false}, {"properties", json::object()}};
	}
	/// @brief Its methods.
	/// @return export, inspect and install.
	std::vector<MethodSpec> methods(const json& /*configuration*/) const override {
		const auto text = [](std::size_t most) { return json{{"type", "string"}, {"maxLength", most}}; };
		const json named = {{"type", "string"}, {"minLength", 1}, {"maxLength", maxShortText}};
		const json package = {{"type", "object"}};
		// None may be added by a revision: what it reads and writes is the person's apps, not the screen's
		return {
		    {"export",
			 Effect::Read,
			 false,
			 false,
			 {{"type", "object"},
			  {"required", {"root", "app", "name"}},
			  {"properties",
			   {{"root", named},
			    {"app", named},
			    {"name", text(format::maxPackageNameLength)},
			    {"release", text(32)},
			    {"description", text(format::maxPackageDescriptionLength)},
			    {"author", text(maxAuthorLength)},
			    {"whole", {{"type", "boolean"}}}}},
			  {"additionalProperties", false}},
			 "a package of an app in a root, its changes or the whole app: {package, summary, leftOut: {records, "
			 "commits}}"},
		    {"inspect",
			 Effect::Read,
			 false,
			 false,
			 {{"type", "object"},
			  {"required", {"package"}},
			  {"properties", {{"package", package}}},
			  {"additionalProperties", false}},
			 "what installing a package would do, against the copy of its app a root holds: {summary}, with its "
			 "problems"},
		    {"install",
			 Effect::Write,
			 false,
			 false,
			 {{"type", "object"},
			  {"required", {"package", "id", "name"}},
			  {"properties",
			   {{"package", package},
			    {"id", named},
			    {"name", {{"type", "string"}, {"minLength", 1}, {"maxLength", format::maxPackageNameLength}}}}},
			  {"additionalProperties", false}},
			 "installs a package as a new app in the writable root, its changes a proposal only the person accepts: "
			 "{id, sub, proposal}"}};
	}
	/// @brief Creates the service. Main thread.
	/// @param context What it works with: whether calls run at once.
	/// @return The service.
	std::unique_ptr<services::Service> create(const json& /*configuration*/,
	                                          services::ServiceContext context) override {
		return std::make_unique<PackagesService>(grant_, context.now);
	}

private:
	PackagesGrant grant_; ///< See the constructor.
};

} // namespace

json exportVersion(const PackagesGrant& grant, const json& with, const std::stop_token& stop) {
	const std::string rootName = with.at("root").get<std::string>(), folder = with.at("app").get<std::string>();
	const fs::path& root = rootNamed(grant, rootName);
	if(!isAppId(folder)) throw PackagesFailure("invalid", "app: expected the name of an app's folder, not a path");
	const std::optional<FoundApp> app = appIn(rootName, root / folder);
	if(!app) throw PackagesFailure("notFound", rootName + ":" + folder + " isn't an app: it has no app.json");

	const ScratchFolder data;
	if(data.path().empty()) throw PackagesFailure("failed", "no temporary folder to read the app's history in");
	if(!grant.dataDirectory.empty()) copyData(grant.dataDirectory, app->id, data.path());
	if(stop.stop_requested()) throw PackagesFailure("failed", "stopped");
	format::PackageOptions options;
	options.kind = with.value("whole", false) ? format::PackageKind::App : format::PackageKind::Changes;
	options.name = with.value("name", std::string());
	options.release = with.value("release", std::string("1.0.0"));
	options.description = with.value("description", std::string());
	options.author = with.value("author", std::string());
	options.engine = AFFRESCOUI_VERSION;

	Screen screen = Screen::fromApp(app->manifest, data.path(), Screen::History::OnDisk);
	format::Package package;
	try {
		package = screen.exportPackage(options);
		if(options.kind == format::PackageKind::Changes) {
			// Its changes only where another root has the app they were made on: else the whole app
			const std::vector<FoundApp> others = appsWithId(grant, app->id);
			const bool based = std::ranges::any_of(others, [&](const FoundApp& other) {
				return other.root != rootName && treeOf(other) == package.base.tree;
			});
			if(!based) {
				options.kind = format::PackageKind::App;
				package = screen.exportPackage(options);
			}
		}
	} catch(const std::exception& error) {
		throw PackagesFailure("invalid", error.what());
	}
	const PlaceNames names(grant);
	std::vector<std::string> privates = names.privateTexts();
	std::error_code error;
	privates.push_back(fs::weakly_canonical(app->manifest.parent_path(), error).string());
	if(const auto problems = format::scrubProblems(package, privates); !problems.empty()) {
		std::string why = "the version holds what must never be shared:";
		for(const std::string& problem : problems) why += " " + names(problem) + ";";
		why.pop_back();
		throw PackagesFailure("refused", why);
	}
	std::size_t records = 0;
	const data::Store& store = screen.data();
	for(const auto& [collection, unused] : store.models()) records += store.size(collection);
	return {{"package", format::packageJson(package)},
	        {"summary", format::packageSummary(package, format::PackageReceiver{}, trustedPublishers(grant))},
	        {"leftOut", {{"records", records}, {"commits", screen.log().size()}}}};
}

json inspectVersion(const PackagesGrant& grant, const json& with) {
	std::string text;
	const format::Package package = packageOf(with, text);
	const json trusted = trustedPublishers(grant);
	if(package.kind == format::PackageKind::Changes)
		if(const std::optional<FoundApp> base = baseCopy(grant, package)) {
			const Screen screen = Screen::fromApp(base->manifest, {}, Screen::History::InMemory);
			return {{"summary", screen.inspectPackage(text, trusted)}};
		}
	// The whole app, installed as a new one: nothing of it is the person's yet, every capability new
	format::PackageReceiver receiver;
	receiver.known = true;
	json summary = format::packageSummary(package, receiver, trusted);
	if(const std::string problem = Screen::appFilesProblem(text); !problem.empty())
		summary["problems"].push_back(problem);
	return {{"summary", std::move(summary)}};
}

namespace {

/// @brief Puts an app's files in a new folder: a copy of its base from the root that has it, or a package's own.
/// @param package The package.
/// @param base The copy of its base app, for a package of changes.
/// @param target The new folder.
/// @param stop Asks the work to stop.
/// @return Where its manifest is in it: "", "json" or "lisp".
std::string placeFiles(const format::Package& package, const std::optional<FoundApp>& base, const fs::path& target,
                       const std::stop_token& stop) {
	if(!base) {
		const std::string sub = package.base.notation == "lisp" ? "lisp" : "json";
		try {
			format::installAppFiles(package, target / sub);
		} catch(const std::exception& error) {
			throw PackagesFailure("invalid", error.what());
		}
		return sub;
	}
	copyFolder(base->sub.empty() ? base->top : base->top / base->sub, base->sub.empty() ? target : target / base->sub,
	           stop);
	std::error_code error;
	if(!base->sub.empty() && fs::is_directory(base->top / "assets", error))
		copyFolder(base->top / "assets", target / "assets", stop);
	return base->sub;
}

/// @brief Whether a package is a whole app with nothing on top of its own files, so that it has nothing to propose.
/// @param package The package.
/// @return Whether it is.
bool isPlainApp(const format::Package& package) {
	const auto none = [](const json& value) { return value.is_null() || value.empty(); };
	return package.kind == format::PackageKind::App && format::baseOf(package) == package.document &&
	       none(package.state) && none(package.assets);
}

/// @brief Proposes a package on the app a manifest opens, its history closed once done.
/// @param grant What the service may touch: its data folder.
/// @param manifest The new app's manifest.
/// @param text The package's text.
/// @return Whether a proposal waits for the person.
/// @throws PackagesFailure If it is refused.
bool proposeOn(const PackagesGrant& grant, const fs::path& manifest, const std::string& text) {
	std::string why;
	RevisionResult result;
	{
		Screen screen =
		    Screen::fromApp(manifest, grant.dataDirectory,
			                grant.dataDirectory.empty() ? Screen::History::InMemory : Screen::History::OnDisk);
		result = screen.proposePackage(text, &why);
	} // its history closed, so the window that opens it next takes it
	if(result.status == RevisionStatus::Rejected) throw PackagesFailure("refused", why);
	return result.status == RevisionStatus::Proposed;
}

} // namespace

json installVersion(const PackagesGrant& grant, const json& with, const std::stop_token& stop) {
	const std::string id = with.at("id").get<std::string>(), name = with.at("name").get<std::string>();
	if(!isAppId(id)) throw PackagesFailure("invalid", "id: expected an app's id ([a-z0-9-]), not a path");
	// "shared" holds the kits every app in the root includes, "launcher" the Launcher: never an installed app
	if(id == "shared" || id == "launcher") throw PackagesFailure("invalid", "id: '" + id + "' is kept for the root's own");
	if(grant.writable.empty() || !grant.roots.contains(grant.writable))
		throw PackagesFailure("refused", "no root is granted to install into");
	std::string text;
	const format::Package package = packageOf(with, text);
	std::optional<FoundApp> base;
	if(package.kind == format::PackageKind::Changes) {
		base = baseCopy(grant, package);
		if(!base) throw PackagesFailure("refused", "base.app: made for " + package.base.name + ", which isn't here");
	} else if(const std::string problem = Screen::appFilesProblem(text); !problem.empty()) {
		throw PackagesFailure("refused", problem); // what goes in is what inspecting it showed, or nothing
	}

	const fs::path target = rootNamed(grant, grant.writable) / id;
	MadeFolder made(target, id, grant.writable);
	const std::string sub = placeFiles(package, base, target, stop);
	const fs::path folder = sub.empty() ? target : target / sub;
	std::error_code error;
	const fs::path manifest = fs::exists(folder / "app.json", error) ? folder / "app.json" : folder / "app.lisp";
	giveName(manifest, name);
	if(stop.stop_requested()) throw PackagesFailure("failed", "stopped");
	// A whole app with nothing on top of its own files is those files: there is nothing to propose
	const bool proposal = !isPlainApp(package) && proposeOn(grant, manifest, text);
	made.keep();
	return {{"id", id}, {"sub", sub.empty() ? "" : "/" + sub}, {"proposal", proposal}};
}

} // namespace affresco::detail

namespace affresco {

std::unique_ptr<services::Provider> makePackagesProvider(PackagesGrant grant) {
	return std::make_unique<detail::PackagesProvider>(std::move(grant));
}

} // namespace affresco
