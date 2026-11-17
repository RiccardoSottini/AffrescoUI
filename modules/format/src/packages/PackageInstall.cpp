/// @file
/// @brief A package's summary, which the person reviews, what refuses installing one, the trusted publishers' file,
/// and a kind "app" package's files written into a new folder (README.md here).

#include "affresco/format/PackageInstall.hpp"
#include "FeatureSignatures.hpp" // parsePublicKey(): a trusted publisher is a key
#include "PackageParts.hpp"
#include "affresco/core/Assets.hpp"
#include "affresco/format/App.hpp"
#include "affresco/format/Migrate.hpp"
#include "affresco/format/Rules.hpp"
#include "affresco/lisp/Document.hpp"
#include "affresco/lisp/Program.hpp"
#include "affresco/notation/Notation.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace affresco::format {

namespace {

/// The sections a kind "changes" package never changes: what a revision of a person's app can't change either, or
/// what reaches beyond the app (its peers, its saved state, its services and servers).
constexpr auto fixedSections =
    std::to_array<std::string_view>({"permissions", "channels", "persist", "services", "connections"});
/// The steps that write records, as a JSON action writes them and as a program's code names them.
constexpr auto jsonWrites =
    std::to_array<std::string_view>({"insert", "update", "delete", "undoData", "redoData", "importCsv"});
/// The program's words for them.
constexpr auto programWrites =
    std::to_array<std::string_view>({"insert", "update-record", "delete", "undoData", "redoData", "importCsv"});
/// The steps that use a service or a connection.
constexpr auto useSteps = std::to_array<std::string_view>({"invoke", "request", "subscribe"});
/// The sections whose values hold no steps.
constexpr auto valueSections =
    std::to_array<std::string_view>({"state", "services", "connections", "program", "assets", "models", "permissions"});
/// The length of a public key's text: "ed25519:" and 43 characters.
constexpr std::size_t publicKeyLength = 51;

/// @brief Refuses something about installing a package.
/// @param place Where.
/// @param problem What is wrong.
/// @throws DocumentError Always.
[[noreturn]] void refuse(const std::string& place, const std::string& problem) {
	throw DocumentError("AffrescoUI package: " + place + ": " + problem);
}

/// @brief Escapes a key for a JSON Pointer (RFC 6901): "~" as "~0", "/" as "~1".
/// @param key The key.
/// @return The escaped key.
std::string pointerKey(std::string_view key) {
	std::string escaped;
	for(const char c : key) {
		if(c == '~') escaped += "~0";
		else if(c == '/') escaped += "~1";
		else escaped += c;
	}
	return escaped;
}

/// @brief A document as everything but version control reads it: what its program builds, with its other sections.
/// @param rules The rules.
/// @param document The document.
/// @return It.
/// @throws DocumentError If its program can't be built.
json builtOf(const Rules& rules, const json& document) { return rules.built(document).value_or(document); }

/// @brief What a document's hooks and advice are, one entry per target: `{"kind", "name", "target"}`.
/// @param document The document.
/// @return Them, as their entries' texts (a set, so two documents compare).
/// @throws DocumentError If its program can't be built.
std::set<std::string> extensionsOf(const json& document) {
	std::set<std::string> found;
	if(!lisp::hasProgram(document)) return found;
	for(const lisp::Extension& extension : lisp::builtProgram(document["program"])->extensions)
		if(!extension.targets.empty()) // the hook point, or what the advice joins
			found.insert(
			    json{{"kind", extension.kind}, {"name", extension.name}, {"target", extension.targets[0]}}.dump());
	return found;
}

/// @brief The entries of one set another hasn't, read back as JSON.
/// @param from The set.
/// @param without The other.
/// @return Them, in order.
json onlyIn(const std::set<std::string>& from, const std::set<std::string>& without) {
	json result = json::array();
	for(const std::string& entry : from)
		if(!without.contains(entry)) result.push_back(json::parse(entry));
	return result;
}

/// @brief What the steps in a JSON value write and use, each as `{"step", "collection" | "use", "in"}`.
/// @param value The value.
/// @param where Its place, a JSON Pointer.
/// @param writes Gets the writes.
/// @param uses Gets the uses.
void collectSteps(const json& value, const std::string& where, std::set<std::string>& writes,
                  std::set<std::string>& uses) {
	if(value.is_array()) {
		for(std::size_t i = 0; i < value.size(); ++i)
			collectSteps(value[i], where + "/" + std::to_string(i), writes, uses);
		return;
	}
	if(!value.is_object()) return;
	for(const std::string_view step : jsonWrites)
		if(const auto found = value.find(step); found != value.end() && (found->is_string() || found->is_boolean()))
			writes.insert(json{{"step", step}, {"collection", *found}, {"in", where}}.dump());
	for(const std::string_view step : useSteps)
		if(const auto found = value.find(step); found != value.end() && found->is_string())
			uses.insert(json{{"step", step}, {"use", *found}, {"in", where}}.dump());
	for(const auto& [key, item] : value.items()) collectSteps(item, where + "/" + pointerKey(key), writes, uses);
}

/// @brief What a program's definitions write and use, as their code names it.
/// @param document The document.
/// @param writes Gets the writes, `{"step", "collection", "in": "file/name"}` (no collection where it is computed).
/// @param uses Gets the uses written out, `{"step", "use": "service.method", "in"}`.
/// @throws DocumentError If the program can't be built.
void collectProgram(const json& document, std::set<std::string>& writes, std::set<std::string>& uses) {
	if(!lisp::hasProgram(document)) return;
	const auto built = lisp::builtProgram(document["program"]);
	for(const auto& [file, entries] : document["program"].items())
		for(const auto& [name, text] : entries.items()) {
			std::string in = file;
			in.append("/").append(name);
			const auto calls = built->namedCalls.find(name);
			const auto textsOf = [&](std::string_view step) {
				if(calls == built->namedCalls.end()) return std::vector<std::string>{};
				const auto texts = calls->second.texts.find(std::string(step));
				return texts == calls->second.texts.end() ? std::vector<std::string>{} : texts->second;
			};
			if(const auto used = built->namesUsed.find(name); used != built->namesUsed.end())
				for(const std::string_view step : programWrites) {
					if(std::ranges::find(used->second, step) == used->second.end()) continue;
					const std::vector<std::string> collections =
					    textsOf(step); // as written out: `(insert "notes" ...)`
					if(collections.empty()) writes.insert(json{{"step", step}, {"in", in}}.dump());
					for(const std::string& collection : collections)
						writes.insert(json{{"step", step}, {"collection", collection}, {"in", in}}.dump());
				}
			for(const std::string_view step : useSteps)
				for(const std::string& method : textsOf(step))
					uses.insert(json{{"step", step}, {"use", method}, {"in", in}}.dump());
		}
}

/// @brief What a document's actions and definitions write and use.
/// @param document The document.
/// @param writes Gets the writes.
/// @param uses Gets the uses.
/// @throws DocumentError If its program can't be built.
void collectDocument(const json& document, std::set<std::string>& writes, std::set<std::string>& uses) {
	if(!document.is_object()) return;
	for(const auto& [key, value] : document.items())
		if(std::ranges::find(valueSections, key) == valueSections.end())
			collectSteps(value, "/" + pointerKey(key), writes, uses);
	collectProgram(document, writes, uses);
}

/// @brief The migrations a model has that it didn't have before.
/// @param old The model before (anything but an object for none).
/// @param now The model after.
/// @return Them, in its order.
json addedMigrations(const json& old, const json& now) {
	json migrations = json::array();
	const json oldMigrations = old.is_object() ? old.value("migrations", json::array()) : json::array();
	if(!now.is_object() || !now.contains("migrations") || !now["migrations"].is_array()) return migrations;
	for(const json& migration : now["migrations"])
		if(!oldMigrations.is_array() || std::ranges::find(oldMigrations, migration) == oldMigrations.end())
			migrations.push_back(migration);
	return migrations;
}

/// @brief The collections whose model a package changes: `{"collection", "from", "to", "migrations"}`, the versions
/// before and after (null where there is no model) and the migrations it adds.
/// @param before The models before (an object, or anything else for none).
/// @param after The models after.
/// @return Them.
json modelChanges(const json& before, const json& after) {
	std::set<std::string> names;
	for(const json* models : {&before, &after})
		if(models->is_object())
			for(const auto& [name, unused] : models->items()) names.insert(name);
	json result = json::array();
	for(const std::string& name : names) {
		const json old = before.is_object() ? before.value(name, json()) : json();
		const json now = after.is_object() ? after.value(name, json()) : json();
		if(old == now) continue;
		const json migrations = addedMigrations(old, now);
		const auto versionOf = [](const json& model) {
			return model.is_object() ? model.value("version", json()) : json();
		};
		result.push_back(
		    {{"collection", name}, {"from", versionOf(old)}, {"to", versionOf(now)}, {"migrations", migrations}});
	}
	return result;
}

/// @brief A file's bytes in a package's `base.files`: its text, or its base64 data decoded.
/// @param file The file (checked by parsePackage()).
/// @return The bytes, or "" if they can't be read.
std::string fileBytes(const json& file) {
	if(file.contains("text") && file["text"].is_string()) return file["text"].get<std::string>();
	if(!file.contains("data") || !file["data"].is_string()) return {};
	return decodeBase64(file["data"].get_ref<const std::string&>(), maxPackageFileBytes).value_or(std::string());
}

/// @brief A kind "app" package's manifest, as its files hold it.
/// @param package The package (checked by parsePackage(), which read the manifest).
/// @return It.
AppManifest manifestOf(const Package& package) {
	const json& files = package.base.files;
	const std::string name = files.contains("app.json") ? "app.json" : "app.lisp";
	return parseApp(notation::readDocument(fileBytes(files[name]), name));
}

/// @brief How many records a seed file holds: a JSON or Lisp list's items (or its "records"), a CSV's rows less its
/// header.
/// @param text The file's text.
/// @param file Its name.
/// @return The count, or null if it can't be read.
json recordCount(const std::string& text, const std::string& file) {
	if(file.ends_with(".csv")) {
		std::size_t rows = 0;
		std::size_t start = 0;
		while(start < text.size()) {
			std::size_t end = text.find('\n', start);
			if(end == std::string::npos) end = text.size();
			if(text.find_first_not_of(" \t\r", start) < end) ++rows;
			start = end + 1;
		}
		return rows == 0 ? 0 : rows - 1;
	}
	try {
		const json value =
		    file.ends_with(".lisp") ? notation::read(text, notation::Notation::Lisp) : parseUntrusted(text);
		if(value.is_array()) return value.size();
		if(value.is_object() && value.contains("records") && value["records"].is_array())
			return value["records"].size();
	} catch(const json::exception&) {
		return {};
	} catch(const DocumentError&) { // Lisp text that doesn't read
		return {};
	}
	return {};
}

/// @brief A kind "app" package's capabilities, permissions and seed data, as the summary lists them.
/// @param summary Gets "capabilities", "permissions" and "seed".
/// @param package The package.
/// @param receiver The person's copy: a capability its manifest doesn't declare is `"added": true`.
void addAppParts(json& summary, const Package& package, const PackageReceiver& receiver) {
	const AppManifest app = manifestOf(package);
	json capabilities = json::array();
	for(const AppCapability& capability : app.capabilities) {
		const std::string key = capability.key();
		capabilities.push_back(
		    {{"capability", key},
			 {"why", capability.why},
			 {"added", std::ranges::find(receiver.capabilities, key) == receiver.capabilities.end()}});
	}
	json permissions = json::array();
	if(const json section = package.document.value("permissions", json()); section.is_object())
		for(const auto& [key, value] : section.items()) {
			if(!value.is_array())
				permissions.push_back(key + ": " + (value.is_string() ? value.get<std::string>() : value.dump()));
			else
				for(const json& rule : value)
					permissions.push_back(key + ": " + (rule.is_string() ? rule.get<std::string>() : rule.dump()));
		}
	json seed = json::array();
	for(const auto& [collection, file] : app.seed) {
		const json entry = package.base.files.value(file, json());
		seed.push_back({{"collection", collection},
		                {"file", file},
		                {"records", entry.is_object() ? recordCount(fileBytes(entry), file) : json()}});
	}
	summary["capabilities"] = std::move(capabilities);
	summary["permissions"] = std::move(permissions);
	summary["seed"] = std::move(seed);
}

/// @brief What installing a package changes, as the summary lists it: impact, extensions, writes and uses (all of a
/// kind "app" package's, what a kind "changes" one adds to its base).
/// @param summary Gets them.
/// @param package The package.
/// @param base Its base's document.
/// @throws DocumentError If a program can't be built.
void addChangeParts(json& summary, const Package& package, const json& base) {
	const Rules rules;
	const json from = package.kind == PackageKind::App ? json::object() : base;
	summary["impact"] = rules.impact(builtOf(rules, base), builtOf(rules, package.document));
	const std::set<std::string> before = extensionsOf(from), after = extensionsOf(package.document);
	summary["extensions"] = {{"added", onlyIn(after, before)}, {"removed", onlyIn(before, after)}};
	std::set<std::string> writesBefore, usesBefore, writesAfter, usesAfter;
	collectDocument(from, writesBefore, usesBefore);
	collectDocument(package.document, writesAfter, usesAfter);
	summary["writes"] = onlyIn(writesAfter, writesBefore);
	summary["uses"] = onlyIn(usesAfter, usesBefore);
	summary["models"] = modelChanges(from.value("models", json()), package.document.value("models", json()));
}

/// @brief The heads of the program's macros: a definition form may be written with one.
/// @param document The document.
/// @return Their names.
std::set<std::string> macroNames(const json& document) {
	std::set<std::string> names;
	for(const auto& [key, text] : detail::definitionsOf(document).items()) {
		if(!text.is_string()) continue;
		try {
			for(const lisp::FormAt& form : lisp::formsOf(text.get_ref<const std::string&>()))
				if(form.kind == "defmacro") names.insert(form.name.substr(form.name.rfind('/') + 1));
			// NOLINTNEXTLINE(bugprone-empty-catch): a text that doesn't read is its definition's problem
		} catch(const DocumentError&) {
		}
	}
	return names;
}

/// @brief A form's head word: what follows its opening parenthesis.
/// @param text The form's text.
/// @return The word, or "" if it isn't a list.
std::string headOf(std::string_view text) {
	if(!text.starts_with('(')) return {};
	text.remove_prefix(1);
	const std::size_t end = text.find_first_of(" \t\r\n()[]{}\"';");
	return std::string(text.substr(0, end));
}

/// @brief Adds a problem for each definition a package adds or changes that holds a form that isn't a definition:
/// such a form runs when the program is built, before anyone accepts it.
/// @param package The package.
/// @param base Its base.
/// @param problems Gets them.
void checkDefinitionForms(const Package& package, const json& base, std::vector<std::string>& problems) {
	const json before = detail::definitionsOf(package.kind == PackageKind::App ? json::object() : base);
	const std::set<std::string> macros = macroNames(package.document);
	for(const auto& [key, text] : detail::definitionsOf(package.document).items()) {
		if(before.contains(key) && before[key] == text) continue;
		const std::string place = "document.program." + key;
		if(!text.is_string()) {
			problems.push_back(place + ": expected a definition's text");
			continue;
		}
		const bool declaration = key.ends_with("/" + std::string(lisp::namespaceEntry));
		try {
			for(const lisp::FormAt& form : lisp::formsOf(text.get_ref<const std::string&>(), key))
				if(form.kind.empty() && !macros.contains(headOf(form.text)) &&
				   !(declaration && headOf(form.text) == "ns")) {
					problems.push_back(place + ": holds a form that isn't a definition (line " +
					                   std::to_string(form.line) + "), which would run as the program is built");
					break;
				}
		} catch(const DocumentError& error) {
			problems.push_back(place + ": doesn't read: " + error.what());
		}
	}
}

/// @brief Adds a problem for a document of a newer format than this engine reads.
/// @param document The document.
/// @param place Its place ("document", "base").
/// @param problems Gets it.
void checkFormat(const json& document, const std::string& place, std::vector<std::string>& problems) {
	const json version = document.is_object() ? document.value("version", json()) : json();
	if(version.is_number_integer() && version.get<std::int64_t>() > formatVersion)
		problems.push_back(place + ".version: the document is of format version " + version.dump() +
		                   ", newer than this engine reads (version " + std::to_string(formatVersion) + ")");
}

/// @brief Adds the problems of a kind "changes" package: a section it can't change, and an app that isn't here.
/// @param package The package.
/// @param base Its base.
/// @param receiver The person's copy.
/// @param problems Gets them.
void checkChanges(const Package& package, const json& base, const PackageReceiver& receiver,
                  std::vector<std::string>& problems) {
	for(const std::string_view section : fixedSections) {
		const std::string key(section);
		if(base.value(key, json()) != package.document.value(key, json()))
			problems.push_back("document." + key +
			                   ": a package of changes can't change it (a revision can't either): " +
			                   "it comes only with the app as a whole, a package of kind \"app\"");
	}
	if(lisp::hasProgram(base) != lisp::hasProgram(package.document))
		problems.emplace_back(
		    "document.program: a package of changes can't add or take away the app's program as a whole");
	if(receiver.known && (receiver.copy.is_null() || receiver.app != package.base.app))
		problems.push_back("base.app: made for " + package.base.name + ", which isn't here");
}

} // namespace

namespace detail {

void addInstallParts(json& summary, const Package& package, const json& base, const PackageReceiver& receiver) {
	summary["base"]["installedRelease"] = receiver.release.empty() ? json() : json(receiver.release);
	std::vector<std::string> problems = installProblems(package, receiver);
	// every part is there, empty, should the version not build: a reader never meets a part that's missing
	summary["impact"] = json::object();
	summary["extensions"] = {{"added", json::array()}, {"removed", json::array()}};
	for(const char* part : {"writes", "uses", "models"}) summary[part] = json::array();
	try {
		addChangeParts(summary, package, base);
	} catch(const DocumentError& error) {
		problems.push_back(std::string("document.program: can't be built: ") + error.what());
	}
	if(package.kind == PackageKind::App) addAppParts(summary, package, receiver);
	summary["problems"] = problems;
	summary["conflicts"] = receiver.conflicts;
}

} // namespace detail

std::vector<std::string> installProblems(const Package& package, const PackageReceiver& receiver) {
	const json base = baseOf(package);
	std::vector<std::string> problems;
	if(package.kind == PackageKind::Changes) checkChanges(package, base, receiver, problems);
	checkFormat(package.document, "document", problems);
	checkFormat(base, "base", problems);
	checkDefinitionForms(package, base, problems);
	return problems;
}

std::string packageAuthor(const Package& package) {
	const std::string name = appId(package.name);
	return "plugin:" + (name.empty() ? std::string("package") : name);
}

json stateOperations(const Package& package, const json& receiverState) {
	json operations = json::array();
	if(!package.state.is_object()) return operations;
	for(const auto& [key, value] : package.state.items())
		if(!receiverState.is_object() || !receiverState.contains(key))
			operations.push_back({{"op", "add"}, {"path", "/state/" + pointerKey(key)}, {"value", value}});
	return operations;
}

json revisionAssets(const Package& package) {
	json assets = json::array();
	if(!package.assets.is_object()) return assets;
	for(const auto& [name, asset] : package.assets.items()) {
		json entry = asset;
		entry["name"] = name;
		assets.push_back(std::move(entry));
	}
	return assets;
}

json parseTrustedPublishers(std::string_view text) {
	const std::string place = "publishers.json";
	if(text.size() > maxTrustedPublishersBytes)
		refuse(place, "larger than " + std::to_string(maxTrustedPublishersBytes >> 10U) + " KiB");
	json value;
	try {
		value = parseUntrusted(text);
	} catch(const json::exception& error) {
		refuse(place, std::string("not JSON: ") + error.what());
	}
	if(!value.is_object()) refuse(place, R"(expected {"ed25519:<key>": "<name>"})");
	if(value.size() > maxTrustedPublishers)
		refuse(place, "more than " + std::to_string(maxTrustedPublishers) + " keys");
	for(const auto& [key, name] : value.items()) {
		if(key.size() != publicKeyLength || !parsePublicKey(key))
			refuse(place, "'" + key.substr(0, 64) + "' isn't a public key");
		if(!name.is_string() || name.get_ref<const std::string&>().empty() ||
		   name.get_ref<const std::string&>().size() > maxPackageNameLength ||
		   std::ranges::any_of(name.get_ref<const std::string&>(),
		                       [](char c) { return static_cast<unsigned char>(c) < 0x20U; }))
			refuse("publishers.json." + key,
			       "expected a name, at most " + std::to_string(maxPackageNameLength) + " characters");
	}
	return value;
}

json readTrustedPublishers(const std::filesystem::path& path) {
	std::error_code error;
	if(!std::filesystem::exists(path, error)) return json::object();
	const std::uintmax_t size = std::filesystem::file_size(path, error);
	if(error) refuse("publishers.json", "can't be read: " + error.message());
	if(size > maxTrustedPublishersBytes)
		refuse("publishers.json", "larger than " + std::to_string(maxTrustedPublishersBytes >> 10U) + " KiB");
	std::ifstream in(path, std::ios::binary);
	std::string text(static_cast<std::size_t>(size), '\0');
	if(!in.read(text.data(), static_cast<std::streamsize>(text.size()))) refuse("publishers.json", "can't be read");
	return parseTrustedPublishers(text);
}

namespace {

/// @brief Checks that each of an app's files lands inside its folder: relative, with no root and no `..` or `.` part.
/// @param files The files, by path.
/// @throws DocumentError If one doesn't.
void checkInstallPaths(const json& files) {
	if(!files.is_object() || files.size() > maxPackageFiles) refuse("base.files", "expected the app's files, by path");
	for(const auto& [path, file] : files.items()) {
		const std::filesystem::path relative(path);
		if(!insideApp(path) || relative.is_absolute() || relative.has_root_name() ||
		   std::ranges::any_of(relative, [](const std::filesystem::path& part) { return part == ".." || part == "."; }))
			refuse("base.files." + path.substr(0, 1024), "not a file inside the app's folder");
	}
}

/// @brief Writes an app's files into a new folder.
/// @param files The files, by path, checked by checkInstallPaths().
/// @param folder The folder.
/// @throws DocumentError If two are the same file on this disk, or one can't be written.
/// @throws std::filesystem::filesystem_error If a folder can't be made.
void writeAppFiles(const json& files, const std::filesystem::path& folder) {
	std::error_code error;
	std::filesystem::create_directories(folder);
	for(const auto& [path, file] : files.items()) {
		const std::filesystem::path target = folder / path;
		std::filesystem::create_directories(target.parent_path());
		// The folder is new: a file there already is another of the package's, by a name this disk takes as the
		// same (it differs by case, or by Unicode's forms), and the later one would replace it unseen
		if(std::filesystem::symlink_status(target, error).type() != std::filesystem::file_type::not_found)
			refuse("base.files." + path.substr(0, 1024), "is the same file on this disk as another of its files");
		std::ofstream out(target, std::ios::binary | std::ios::trunc);
		const std::string bytes = fileBytes(file);
		if(!out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
			refuse("base.files." + path, "can't be written");
	}
}

} // namespace

void installAppFiles(const Package& package, const std::filesystem::path& folder) {
	if(package.kind != PackageKind::App) refuse("kind", "only a package of kind \"app\" holds an app's files");
	std::error_code error;
	if(std::filesystem::symlink_status(folder, error).type() != std::filesystem::file_type::not_found)
		refuse("folder", "'" + folder.string() + "' already exists: install into a new folder");
	checkInstallPaths(package.base.files);
	try {
		writeAppFiles(package.base.files, folder);
	} catch(const DocumentError&) {
		std::filesystem::remove_all(folder, error); // nothing is left of a folder half written
		throw;
	} catch(const std::filesystem::filesystem_error& failure) {
		std::filesystem::remove_all(folder, error);
		refuse("folder", failure.what());
	}
}

} // namespace affresco::format
