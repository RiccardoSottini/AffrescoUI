/// @file
/// @brief Apps' manifests (app.json): read and checked, and the capabilities a screen uses set against them.

#include "affresco/format/App.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <set>

namespace affresco::format {

namespace {

/// The keys a manifest may have.
constexpr auto manifestKeys =
    std::to_array<std::string_view>({"$schema", "format", "version", "note", "id", "name", "description", "icon",
	                                 "entry", "themes", "capabilities", "data", "assets"});

/// The kinds of capability, as "capabilities" names them.
constexpr auto capabilityKeys = std::to_array<std::string_view>(
    {"network", "folders", "programs", "audio", "assistant", "files", "windows", "channels"});

constexpr std::size_t maxNameLength = 100;   ///< The longest name.
constexpr std::size_t maxTextLength = 300;   ///< The longest description or reason.
constexpr std::size_t maxEntries = 64;       ///< The most capabilities of one kind, themes or seed files.
constexpr std::size_t maxAssets = 256;       ///< The most assets.
constexpr std::size_t maxIdLength = 64;      ///< The longest id, root or program name.
constexpr std::size_t maxOriginLength = 256; ///< The longest origin.
constexpr std::size_t maxPathLength = 1024;  ///< The longest path.

/// @brief Reports a problem in a manifest.
/// @param place Where.
/// @param problem What.
/// @throws DocumentError Always: "AffrescoUI app: <place>: <problem>".
[[noreturn]] void fail(const std::string& place, const std::string& problem) {
	throw DocumentError("AffrescoUI app: " + (place.empty() ? problem : place + ": " + problem));
}

/// @brief Reads a text of at most a length.
/// @param value The value.
/// @param place Where, for the error.
/// @param longest Its longest.
/// @param empty Whether it may be "".
/// @return The text.
std::string text(const json& value, const std::string& place, std::size_t longest, bool empty = false) {
	if(!value.is_string()) fail(place, "expected a string");
	std::string read = value.get<std::string>();
	if(read.empty() && !empty) fail(place, "empty");
	if(read.size() > longest) fail(place, "longer than " + std::to_string(longest) + " bytes");
	if(std::ranges::any_of(read, [](char c) { return static_cast<unsigned char>(c) < 0x20; }))
		fail(place, "holds a control character");
	return read;
}

/// @brief Reads a path inside the app's folder (insideApp()).
/// @param value The value.
/// @param place Where, for the error.
/// @return The path.
std::string path(const json& value, const std::string& place) {
	std::string read = text(value, place, maxPathLength);
	if(!insideApp(read)) fail(place, "'" + read + "' is not a path inside the app's folder");
	return read;
}

/// @brief Reads an object of at most a number of members.
/// @param value The value.
/// @param place Where, for the error.
/// @param most Its most members.
/// @return It.
const json& members(const json& value, const std::string& place, std::size_t most) {
	if(!value.is_object()) fail(place, "expected an object");
	if(value.size() > most) fail(place, "more than " + std::to_string(most) + " entries");
	return value; // NOLINT(bugprone-return-const-ref-from-parameter): returning the value read is the point
}

/// @brief Whether a text is a name: [A-Za-z0-9._-], at most maxIdLength, starting with a letter or digit.
/// @param name The text.
/// @return Whether it is.
bool plainName(std::string_view name) {
	return !name.empty() && name.size() <= maxIdLength && std::isalnum(static_cast<unsigned char>(name.front())) &&
	       std::ranges::all_of(name, [](char c) {
		       return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-';
	       });
}

/// @brief Whether a text is a port: one to five digits.
/// @param port The text.
/// @return Whether it is.
bool isPort(std::string_view port) {
	return !port.empty() && port.size() <= 5 && std::ranges::all_of(port, [](char c) { return c >= '0' && c <= '9'; });
}

/// @brief Whether a text is a host's name: letters, digits, '.' and '-', starting with neither of the last two.
/// @param host The text.
/// @return Whether it is.
bool isHostName(std::string_view host) {
	return !host.empty() && host.front() != '.' && host.front() != '-' && std::ranges::all_of(host, [](char c) {
		return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-';
	});
}

/// @brief Whether a text is an origin the host can allow: http, https, ws or wss, then a host (letters, digits, '.',
/// '-', and "*." first for any subdomain) and an optional port; no path.
/// @param origin The text.
/// @return Whether it is.
bool isOrigin(std::string_view origin) {
	const std::size_t separator = origin.find("://");
	if(origin.size() > maxOriginLength || separator == std::string_view::npos) return false;
	const std::string_view scheme = origin.substr(0, separator);
	if(scheme != "http" && scheme != "https" && scheme != "ws" && scheme != "wss") return false;
	std::string_view host = origin.substr(separator + 3);
	if(host.starts_with("*.")) host.remove_prefix(2);
	const std::size_t colon = host.find(':');
	if(colon != std::string_view::npos && !isPort(host.substr(colon + 1))) return false;
	return isHostName(host.substr(0, colon));
}

/// @brief The origin of a URL: its scheme and host, lower case, and its port.
/// @param url The URL.
/// @return The origin, or "" if it has none an app could declare.
std::string originOf(std::string_view url) {
	const std::size_t separator = url.find("://");
	if(separator == std::string_view::npos) return "";
	const std::size_t end = url.find_first_of("/?#", separator + 3);
	std::string_view authority =
	    url.substr(separator + 3, end == std::string_view::npos ? std::string_view::npos : end - separator - 3);
	if(const std::size_t at = authority.rfind('@'); at != std::string_view::npos) authority.remove_prefix(at + 1);
	std::string origin = std::string(url.substr(0, separator)) + "://" + std::string(authority);
	std::ranges::transform(origin, origin.begin(),
	                       [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
	return isOrigin(origin) && !origin.contains('*') ? origin : "";
}

/// @brief Whether a declared origin covers one a screen uses: the same, or "*.domain" for one of its subdomains.
/// @param declared The manifest's.
/// @param used The screen's.
/// @return Whether it does.
bool covers(std::string_view declared, std::string_view used) {
	if(declared == used) return true;
	const std::size_t wildcard = declared.find("://*.");
	if(wildcard == std::string_view::npos || !used.starts_with(declared.substr(0, wildcard + 3))) return false;
	const std::string_view suffix = declared.substr(wildcard + 4); // ".example.com", and its port
	const std::string_view host = used.substr(wildcard + 3);
	return host.size() > suffix.size() && host.ends_with(suffix) &&
	       !host.substr(0, host.size() - suffix.size()).contains(':');
}

/// @brief Reads one of "capabilities.folders": { "access": "read" or "write", "why": ... }.
/// @param root The folder's name.
/// @param folder Its entry.
/// @return The capability.
AppCapability readFolder(const std::string& root, const json& folder) {
	const std::string place = "capabilities.folders." + root;
	if(!plainName(root)) fail("capabilities.folders", "'" + root + "' is not a folder's name");
	members(folder, place, 2);
	const std::string access = text(folder.value("access", json("read")), place + ".access", 8);
	if(access != "read" && access != "write") fail(place + ".access", R"(expected "read" or "write")");
	if(folder.size() != (folder.contains("access") ? 2U : 1U)) fail(place, R"(expected "access" and "why")");
	return {AppCapability::Kind::Folder, root, text(folder.value("why", json()), place + ".why", maxTextLength),
	        access == "write"};
}

/// @brief Reads "capabilities".
/// @param section The section.
/// @return The capabilities, kind by kind.
std::vector<AppCapability> readCapabilities(const json& section) {
	using Kind = AppCapability::Kind;
	for(const auto& [key, value] : members(section, "capabilities", maxEntries).items())
		if(std::ranges::find(capabilityKeys, key) == capabilityKeys.end())
			fail("capabilities", "unknown kind '" + key + "'");
	std::vector<AppCapability> read;
	if(const auto network = section.find("network"); network != section.end())
		for(const auto& [origin, why] : members(*network, "capabilities.network", maxEntries).items()) {
			if(!isOrigin(origin))
				fail("capabilities.network", "'" + origin + "' is not an origin (https://host[:port])");
			read.push_back({Kind::Network, origin, text(why, "capabilities.network." + origin, maxTextLength), false});
		}
	if(const auto folders = section.find("folders"); folders != section.end())
		for(const auto& [root, folder] : members(*folders, "capabilities.folders", maxEntries).items())
			read.push_back(readFolder(root, folder));
	if(const auto programs = section.find("programs"); programs != section.end())
		for(const auto& [program, why] : members(*programs, "capabilities.programs", maxEntries).items()) {
			if(!plainName(program)) fail("capabilities.programs", "'" + program + "' is not a program's name");
			read.push_back(
			    {Kind::Program, program, text(why, "capabilities.programs." + program, maxTextLength), false});
		}
	// The kinds with no names: the reason alone
	for(const auto& [kind, key] :
	    {std::pair{Kind::Audio, "audio"}, std::pair{Kind::Assistant, "assistant"}, std::pair{Kind::Files, "files"},
	     std::pair{Kind::Windows, "windows"}, std::pair{Kind::Channels, "channels"}})
		if(const auto why = section.find(key); why != section.end())
			read.push_back({kind, key, text(*why, std::string("capabilities.") + key, maxTextLength), false});
	return read;
}

/// @brief Reads "data": the models file and the seed files.
/// @param section The section.
/// @param manifest Receives them.
void readData(const json& section, AppManifest& manifest) {
	members(section, "data", 2);
	for(const auto& [key, value] : section.items())
		if(key != "models" && key != "seed") fail("data", "unknown key '" + key + "'");
	if(section.contains("models")) manifest.models = path(section["models"], "data.models");
	if(!section.contains("seed")) return;
	for(const auto& [collection, file] : members(section["seed"], "data.seed", maxEntries).items()) {
		const std::string place = "data.seed." + collection;
		if(!plainName(collection) || collection.find_first_of(".-") != std::string::npos)
			fail("data.seed", "'" + collection + "' is not a collection's name");
		std::string read = path(file, place);
		if(!read.ends_with(".json") && !read.ends_with(".csv")) fail(place, "expected a .json or .csv file");
		manifest.seed.emplace_back(collection, std::move(read));
	}
}

/// @brief Reads a list of paths.
/// @param value The list.
/// @param place Where, for the error.
/// @param most Its most items.
/// @return The paths.
std::vector<std::string> paths(const json& value, const std::string& place, std::size_t most) {
	if(!value.is_array()) fail(place, "expected a list of paths");
	if(value.size() > most) fail(place, "more than " + std::to_string(most) + " paths");
	std::vector<std::string> read;
	read.reserve(value.size());
	for(std::size_t i = 0; i < value.size(); ++i) read.push_back(path(value[i], place + "[" + std::to_string(i) + "]"));
	return read;
}

/// @brief Reads the manifest's own fields: format, version, id, name, description, icon and entry.
/// @param manifest The manifest.
/// @param read Receives them.
void readHeader(const json& manifest, AppManifest& read) {
	if(manifest.value("format", json()) != "affrescoui-app") fail("format", "expected \"affrescoui-app\"");
	if(manifest.value("version", json()) != 1) fail("version", "expected 1");
	read.id = text(manifest.value("id", json()), "id", maxIdLength);
	if(appId(read.id) != read.id) fail("id", "expected lower-case letters, digits and '-' ('" + appId(read.id) + "')");
	read.name = text(manifest.value("name", json()), "name", maxNameLength);
	if(manifest.contains("description"))
		read.description = text(manifest["description"], "description", maxTextLength, true);
	if(manifest.contains("icon")) read.icon = path(manifest["icon"], "icon");
	if(manifest.contains("entry")) read.entry = path(manifest["entry"], "entry");
	if(!read.entry.ends_with(".json")) fail("entry", "expected a screen file (.json)");
	if(manifest.contains("note")) static_cast<void>(text(manifest["note"], "note", 4096, true));
}

/// @brief Whether an object is an `ask` step (an "ask" text).
/// @param step The object.
/// @return true if it is.
bool isAsk(const json& step) {
	const auto ask = step.find("ask");
	return ask != step.end() && ask->is_string();
}

/// @brief Whether an object is a step that shows a file dialog: `saveFile`, `openFile`, `export`, or an `image`
/// step's "open" (not of an asset) or "save". Not an action named "saveFile" (an array), nor an adapter binding's
/// "export" (no "pages").
/// @param step The object.
/// @return true if it is.
bool showsFileDialog(const json& step) {
	const auto names = [&step](const char* key) {
		const auto found = step.find(key);
		return found != step.end() && found->is_string();
	};
	if(names("saveFile") || names("openFile") || (names("export") && step.contains("pages"))) return true;
	const json operation = step.value("image", json());
	if(operation == "save") return true;
	const auto with = step.find("with");
	return operation == "open" && !(with != step.end() && with->is_object() && with->contains("asset"));
}

/// @brief Whether a step anywhere in a screen is one of a kind, outside its data: its state, strings, tokens and
/// models.
/// @param value The screen, or a part of it.
/// @param is Whether an object is one.
/// @param depth How deep this part is, so a deep document can't exhaust the stack.
/// @return true if one is.
bool hasStep(const json& value, bool (*is)(const json&), int depth = 0) {
	if(depth > 256) return false;
	if(value.is_array())
		return std::ranges::any_of(value, [is, depth](const json& item) { return hasStep(item, is, depth + 1); });
	if(!value.is_object()) return false;
	if(is(value)) return true;
	return std::ranges::any_of(value.items(), [is, depth](const auto& entry) {
		const std::string& key = entry.key();
		const bool data = key == "state" || key == "strings" || key == "tokens" || key == "models";
		return (depth != 0 || !data) && hasStep(entry.value(), is, depth + 1);
	});
}

/// @brief Adds a capability a screen uses, once: a folder used for writing somewhere is used for writing.
/// @param used The capabilities so far.
/// @param capability The one to add.
void addCapability(std::vector<AppCapability>& used, AppCapability capability) {
	for(AppCapability& known : used)
		if(known.kind == capability.kind && known.name == capability.name) {
			known.write = known.write || capability.write;
			return;
		}
	used.push_back(std::move(capability));
}

/// @brief Adds the network capability of a URL, if it is a text with an origin.
/// @param used The capabilities so far.
/// @param url The URL.
void addOrigin(std::vector<AppCapability>& used, const json& url) {
	if(url.is_string())
		if(const std::string found = originOf(url.get<std::string>()); !found.empty())
			addCapability(used, {AppCapability::Kind::Network, found, "", false});
}

/// @brief Adds the origins a screen's "connections" reach: their URLs and their authentication's.
/// @param used The capabilities so far.
/// @param document The screen.
void addConnections(std::vector<AppCapability>& used, const json& document) {
	const auto connections = document.find("connections");
	if(connections == document.end() || !connections->is_object()) return;
	for(const json& connection : *connections) {
		if(!connection.is_object()) continue;
		addOrigin(used, connection.value("url", json()));
		if(const auto auth = connection.find("auth"); auth != connection.end() && auth->is_object())
			for(const json& value : *auth) addOrigin(used, value);
	}
}

/// @brief Adds the folders and programs a screen's "services" use.
/// @param used The capabilities so far.
/// @param document The screen.
void addServices(std::vector<AppCapability>& used, const json& document) {
	const auto services = document.find("services");
	if(services == document.end() || !services->is_object()) return;
	for(const json& service : *services) {
		if(!service.is_object()) continue;
		if(const json root = service.value("root", json()); root.is_string())
			addCapability(
			    used, {AppCapability::Kind::Folder, root.get<std::string>(), "", !service.value("readOnly", false)});
		if(const json program = service.value("process", json());
		   program.is_string() && service.value("provider", json()) == "process")
			addCapability(used, {AppCapability::Kind::Program, program.get<std::string>(), "", false});
	}
}

} // namespace

std::string AppCapability::key() const {
	switch(kind) {
	case Kind::Network:
		return "network:" + name;
	case Kind::Folder:
		return "folder:" + name + (write ? ":write" : "");
	case Kind::Program:
		return "program:" + name;
	case Kind::Audio:
		return "audio";
	case Kind::Assistant:
		return "assistant";
	case Kind::Files:
		return "files";
	case Kind::Windows:
		return "windows";
	case Kind::Channels:
		return "channels";
	}
	return "";
}

bool AppCapability::runsAnyCommand() const {
	// Shells and interpreters, by their base name without ".exe" or a version
	static const std::set<std::string, std::less<>> runners = {
	    "ash",   "bash", "bun",        "busybox", "cmd",    "csh",    "dash",    "deno", "doas",
	    "env",   "fish", "ksh",        "lua",     "luajit", "mksh",   "node",    "nu",   "osascript",
	    "perl",  "php",  "powershell", "pwsh",    "py",     "python", "pythonw", "ruby", "sh",
	    "shell", "sudo", "tclsh",      "tcsh",    "wish",   "xargs",  "zsh"};
	if(kind != Kind::Program) return false;
	const std::size_t slash = name.find_last_of("/\\");
	std::string base = slash == std::string::npos ? name : name.substr(slash + 1); // "/usr/bin/python3.12"
	std::ranges::transform(base, base.begin(),
	                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	if(base.ends_with(".exe")) base.resize(base.size() - 4);
	while(!base.empty() && (std::isdigit(static_cast<unsigned char>(base.back())) || base.back() == '.'))
		base.pop_back();
	return runners.contains(base);
}

bool AppCapability::startsAllowed() const {
	switch(kind) {
	case Kind::Audio:
	case Kind::Files:
	case Kind::Windows:
		return true;
	case Kind::Folder:
		return !write;
	case Kind::Network:
	case Kind::Program:
	case Kind::Assistant:
	case Kind::Channels:
		return false;
	}
	return false;
}

AppManifest parseApp(const json& manifest) {
	if(!manifest.is_object()) fail("", "expected an object");
	for(const auto& [key, value] : manifest.items())
		if(std::ranges::find(manifestKeys, key) == manifestKeys.end()) fail("", "unknown key '" + key + "'");
	AppManifest read;
	readHeader(manifest, read);
	if(manifest.contains("themes")) read.themes = paths(manifest["themes"], "themes", maxEntries);
	if(manifest.contains("capabilities")) read.capabilities = readCapabilities(manifest["capabilities"]);
	if(manifest.contains("data")) readData(manifest["data"], read);
	if(manifest.contains("assets")) read.assets = paths(manifest["assets"], "assets", maxAssets);
	return read;
}

std::vector<AppCapability> usedCapabilities(const json& document) {
	using Kind = AppCapability::Kind;
	std::vector<AppCapability> used;
	addConnections(used, document);
	addServices(used, document);
	if(document.contains("audio")) addCapability(used, {Kind::Audio, "audio", "", false});
	if(hasStep(document, isAsk)) addCapability(used, {Kind::Assistant, "assistant", "", false});
	if(hasStep(document, showsFileDialog)) addCapability(used, {Kind::Files, "files", "", false});
	if(const auto windows = document.find("windows"); windows != document.end() && !windows->empty())
		addCapability(used, {Kind::Windows, "windows", "", false});
	if(const auto channels = document.find("channels"); channels != document.end() && !channels->empty())
		addCapability(used, {Kind::Channels, "channels", "", false});
	return used;
}

std::vector<std::string> undeclaredCapabilities(const AppManifest& manifest, const json& document) {
	std::vector<std::string> missing;
	for(const AppCapability& used : usedCapabilities(document)) {
		const bool declared = std::ranges::any_of(manifest.capabilities, [&used](const AppCapability& capability) {
			if(capability.kind != used.kind) return false;
			if(used.kind == AppCapability::Kind::Network) return covers(capability.name, used.name);
			return capability.name == used.name && (capability.write || !used.write);
		});
		if(!declared) missing.push_back(used.key());
	}
	return missing;
}

AppManifest manifestOf(std::string id, std::string name, std::string entry, const json& document) {
	AppManifest manifest;
	manifest.id = std::move(id);
	manifest.name = std::move(name);
	manifest.entry = std::move(entry);
	manifest.capabilities = usedCapabilities(document);
	return manifest;
}

std::string appId(std::string_view name) {
	std::string id;
	for(const char c : name.substr(0, maxIdLength)) {
		const auto byte = static_cast<unsigned char>(c);
		id += std::isalnum(byte) && byte < 0x80 ? static_cast<char>(std::tolower(byte)) : '-';
	}
	const std::size_t first = id.find_first_not_of('-');
	if(first == std::string::npos) return "app";
	id.erase(0, first);
	id.erase(id.find_last_not_of('-') + 1);
	return id;
}

bool insideApp(std::string_view path) {
	if(path.empty() || path.size() > maxPathLength || path.front() == '/') return false;
	if(std::ranges::any_of(path, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == '\\' || c == ':'; }))
		return false;
	std::size_t start = 0;
	while(start <= path.size()) {
		const std::size_t end = std::min(path.find('/', start), path.size());
		const std::string_view part = path.substr(start, end - start);
		if(part == ".." || (part.empty() && end != path.size())) return false;
		start = end + 1;
	}
	return true;
}

} // namespace affresco::format
