/// @file
/// @brief affresco::Screen's apps (SCREEN_FORMAT.md, "Apps: app.json"): an app's folder read through its manifest,
/// its kits, themes, models and seed data merged into its entry screen, and its data kept by its id.

#include "ScreenFiles.hpp"
#include "ScreenImpl.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/data/Csv.hpp"
#include "affresco/data/Model.hpp"
#include "affresco/data/Query.hpp"
#include "affresco/format/Adapters.hpp"
#include "affresco/format/Migrate.hpp"
#include "affresco/themes/Themes.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <span>
#include <sstream>
#include <vector>

namespace affresco {

namespace {

/// The most bytes a seed file, a theme or a models file may have.
constexpr std::uintmax_t maxPartBytes = 16U << 20;

/// The sections an app's theme may hold: what a document looks like, never what it does.
constexpr auto themeKeys =
    std::to_array<std::string_view>({"$schema", "note", "components", "styles", "tokens", "colors", "fonts"});

/// The sections an app's models file may hold.
constexpr auto modelsKeys = std::to_array<std::string_view>({"$schema", "note", "models", "queries"});

/// @brief An app's folder and its manifest.
struct AppFolder {
	std::filesystem::path folder; ///< The folder, canonical.
	format::AppManifest manifest; ///< Its app.json.
};

/// @brief Reports a problem with an app's files.
/// @param where The file or the manifest's place.
/// @param problem What.
/// @throws ScreenError Always.
[[noreturn]] void fail(const std::string& where, const std::string& problem) {
	throw ScreenError("AffrescoUI app: " + where + ": " + problem);
}

/// @brief Reads a whole file of at most a size.
/// @param file The file.
/// @param most Its largest size.
/// @return Its bytes.
/// @throws ScreenError If it can't be read or is larger.
std::string readWhole(const std::filesystem::path& file, std::uintmax_t most) {
	std::error_code error;
	const std::uintmax_t size = std::filesystem::file_size(file, error);
	if(error) fail(file.string(), "could not be read");
	if(size > most) fail(file.string(), "larger than " + std::to_string(most) + " bytes");
	const std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	if(!in) fail(file.string(), "could not be read");
	return text.str();
}

/// @brief A file the manifest names, which must be inside the app's folder once links are resolved.
/// @param app The app.
/// @param relative The path the manifest gives.
/// @param place Its place in the manifest.
/// @return The file.
/// @throws ScreenError If it isn't there, or is outside the folder.
std::filesystem::path inside(const AppFolder& app, const std::string& relative, const std::string& place) {
	std::error_code error;
	const std::filesystem::path file = std::filesystem::canonical(app.folder / relative, error);
	if(error) fail(place, "'" + relative + "' is not in the app's folder");
	const auto [end, unused] = std::ranges::mismatch(app.folder, file);
	if(end != app.folder.end()) fail(place, "'" + relative + "' leads out of the app's folder");
	return file;
}

/// @brief The folders an app's includes may be in: its own, and the "shared" folder beside it, where the apps of a
/// folder keep the kits they share (as a release lays them out), if it is a folder and not a link.
/// @param app The app.
/// @return The folders, canonical.
std::vector<std::filesystem::path> includeRoots(const AppFolder& app) {
	std::vector<std::filesystem::path> roots{app.folder};
	const std::filesystem::path shared = app.folder.parent_path() / "shared";
	std::error_code error;
	if(std::filesystem::symlink_status(shared, error).type() == std::filesystem::file_type::directory)
		roots.push_back(shared);
	return roots;
}

/// @brief Reads an app's folder and its manifest.
/// @param location The folder, or its app.json.
/// @return Them.
/// @throws ScreenError If there is no manifest, or it isn't valid.
AppFolder readFolder(const std::filesystem::path& location) {
	const bool isManifest = location.filename() == "app.json";
	std::error_code error;
	AppFolder app{std::filesystem::canonical(isManifest ? location.parent_path() : location, error), {}};
	if(error || !std::filesystem::is_directory(app.folder))
		fail(location.string(), "not an app: expected its folder, or its app.json");
	const std::filesystem::path file = app.folder / "app.json";
	try {
		app.manifest = format::parseApp(parseUntrusted(readWhole(file, format::maxAppManifestBytes)));
	} catch(const json::parse_error& parse) {
		fail(file.string(), std::string("invalid JSON: ") + parse.what());
	} catch(const DocumentError& invalid) {
		fail(file.string(), invalid.what());
	}
	return app;
}

/// @brief Reads a theme or a models file of the app and checks its sections.
/// @param file The file.
/// @param keys The sections it may hold.
/// @throws ScreenError If it can't be read, or holds another section.
template <std::size_t N>
void checkPart(const std::filesystem::path& file, const std::array<std::string_view, N>& keys) {
	json part;
	try {
		part = parseUntrusted(readWhole(file, maxPartBytes));
	} catch(const json::parse_error& parse) {
		fail(file.string(), std::string("invalid JSON: ") + parse.what());
	}
	if(!part.is_object()) fail(file.string(), "expected a JSON object");
	for(const auto& [key, value] : part.items())
		if(std::ranges::find(keys, key) == keys.end()) fail(file.string(), "an app's part can't hold \"" + key + "\"");
}

/// @brief Adds the app's themes and models file to the entry screen's includes, after its own: the screen's entries
/// win, then its own includes'.
/// @param document The entry screen.
/// @param app The app.
/// @param base The entry screen's folder, which its include paths start from.
void includeParts(json& document, const AppFolder& app, const std::filesystem::path& base) {
	json includes = document.value("include", json::array());
	if(includes.is_string()) includes = json::array({includes});
	if(!includes.is_array()) return; // resolveIncludes() reports it
	for(std::size_t i = 0; i < app.manifest.themes.size(); ++i) {
		const std::filesystem::path file = inside(app, app.manifest.themes[i], "themes[" + std::to_string(i) + "]");
		checkPart(file, themeKeys);
		includes.push_back(file.lexically_relative(base).generic_string());
	}
	if(!app.manifest.models.empty()) {
		const std::filesystem::path file = inside(app, app.manifest.models, "data.models");
		checkPart(file, modelsKeys);
		includes.push_back(file.lexically_relative(base).generic_string());
	}
	if(!includes.empty()) document["include"] = std::move(includes);
}

/// @brief The records of a CSV seed: its rows imported into a store of its own with the screen's models (all or
/// none), each cell converted to its field's type; an "id" column gives the records' ids.
/// @param models The screen's models.
/// @param collection The collection.
/// @param text The CSV text.
/// @param place The seed's place, for errors.
/// @return The records.
/// @throws ScreenError If a row doesn't fit the model.
json csvSeed(const json& models, const std::string& collection, const std::string& text, const std::string& place) {
	json plain = models; // the store makes the collections; the seeds are what is being read
	for(auto& [name, model] : plain.items())
		if(model.is_object()) {
			model.erase("seed");
			model.erase("importFrom");
		}
	data::Store store;
	store.follow(plain, "seed");
	const std::vector<std::vector<std::string>> rows = data::parseCsv(text);
	if(rows.empty()) return json::array();
	json columns = json::object(); // fields by header, case aside, leaving "id" out
	const auto lower = [](std::string name) {
		std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return name;
	};
	for(const auto& [field, unused] : models[collection].value("fields", json::object()).items())
		for(const std::string& header : rows.front())
			if(lower(header) == lower(field)) columns[field] = header;
	const json report = data::importCsv(store, text, {collection, columns, ',', true, "seed"}, {});
	if(report.value("failed", 0) > 0)
		fail(place,
		     "row " + report["failures"][0]["row"].dump() + ": " + report["failures"][0]["error"].get<std::string>());
	const auto idColumn =
	    std::ranges::find_if(rows.front(), [&lower](const std::string& h) { return lower(h) == "id"; });
	json records = json::array();
	for(std::size_t r = 0; r < report["ids"].size(); ++r) {
		const auto stored = store.get(collection, report["ids"][r].get<std::string>());
		if(!stored) continue;
		json record = stored->fields;
		const auto column = static_cast<std::size_t>(idColumn - rows.front().begin());
		if(idColumn != rows.front().end() && column < rows[r + 1].size() && !rows[r + 1][column].empty())
			record["id"] = rows[r + 1][column];
		records.push_back(std::move(record));
	}
	return records;
}

/// @brief Gives each seed file's records to its model's "seed".
/// @param document The entry screen, its includes merged.
/// @param app The app.
/// @throws ScreenError If a file can't be read, its model is missing or has a seed of its own, or a record doesn't
/// fit (a CSV's; a JSON seed is checked by the store, as the model's own).
void giveSeeds(json& document, const AppFolder& app) {
	for(const auto& [collection, relative] : app.manifest.seed) {
		const std::string place = "data.seed." + collection;
		json* model = document.contains("models") ? &document["models"] : nullptr;
		if(!model || !model->is_object() || !model->contains(collection) || !(*model)[collection].is_object())
			fail(place, "no model \"" + collection + "\" in the screen or the app's models");
		if((*model)[collection].contains("seed")) fail(place, "the model has a \"seed\" of its own");
		const std::string text = readWhole(inside(app, relative, place), maxPartBytes);
		json records;
		try {
			records = relative.ends_with(".csv") ? csvSeed(*model, collection, text, place) : parseUntrusted(text);
		} catch(const json::parse_error& parse) {
			fail(place, std::string("invalid JSON: ") + parse.what());
		} catch(const DocumentError& invalid) {
			if(std::string_view(invalid.what()).starts_with("AffrescoUI app:")) throw;
			fail(place, invalid.what());
		}
		if(!records.is_array()) fail(place, "expected a list of records");
		(*model)[collection]["seed"] = std::move(records);
	}
}

/// @brief Moves the data an earlier version kept for the entry screen (fromFile(): `<data>/<name>.history` and the
/// others) into the app's folder, the first time the app is opened. Only the entry's own: stateLocation() names another
/// file's data otherwise. A file the app's folder has already is kept.
/// @param dataDirectory The data folder.
/// @param entry The entry screen.
/// @param appData The app's data folder, which doesn't exist yet.
void adoptOldData(const std::filesystem::path& dataDirectory, const std::filesystem::path& entry,
                  const std::filesystem::path& appData) {
	static constexpr std::array<std::array<std::string_view, 2>, 7> moves = {
	    {{".history", "history"},
		 {".state.json", "state.json"},
		 {".state.json.before-data.json", "state.json.before-data.json"},
		 {".data.sqlite", "data.sqlite"},
		 {".data.sqlite-wal", "data.sqlite-wal"},
		 {".data.sqlite-shm", "data.sqlite-shm"},
		 {".usage.json", "usage.json"}}};
	std::error_code error;
	std::filesystem::create_directories(appData, error);
	if(error) return; // the history and state files report it
	for(const auto& [old, now] : moves) {
		const std::filesystem::path from = detail::stateLocation(dataDirectory, entry, old);
		if(!std::filesystem::exists(from, error) || std::filesystem::exists(appData / now, error)) continue;
		std::filesystem::rename(from, appData / now, error);
		affresco::log(error ? LogLevel::Warning : LogLevel::Info,
		              "AffrescoUI app: " + from.string() + (error ? " could not be moved to " : " moved to ") +
		                  (appData / now).string());
	}
}

} // namespace

Screen Screen::fromApp(const std::filesystem::path& location, const std::filesystem::path& dataDirectory,
                       History history) {
	themes::install(); // the kits' components and tokens, before the document is checked against them
	const AppFolder app = readFolder(location);
	const std::filesystem::path entry = inside(app, app.manifest.entry, "entry");
	json document;
	try {
		document = parseUntrusted(readWhole(entry, maxPartBytes));
	} catch(const json::parse_error& parse) {
		fail(entry.string(), std::string("invalid JSON: ") + parse.what());
	}
	if(format::adapters::detect(document) != format::adapters::SourceFormat::Screen)
		fail(entry.string(), R"(an app's entry is a screen ("format": "affrescoui-screen"))");
	bool migrated = !format::migrate(document).empty();
	includeParts(document, app, entry.parent_path());
	detail::resolveIncludes(document, entry.parent_path(), migrated, includeRoots(app));
	giveSeeds(document, app);
	if(migrated)
		affresco::log(LogLevel::Info, "AffrescoUI screen: written for format version 1, read as version 2 "
		                              "(`affrescoui migrate --write <files>` rewrites the files)");

	Screen screen(std::make_unique<Impl>(std::move(document), entry.parent_path()));
	screen.impl_->app = app.manifest;
	screen.impl_->repairs.setTemplateName(detail::templateName(entry));
	if(dataDirectory.empty()) return screen; // a check: nothing kept
	const std::filesystem::path data = appDataDirectory(dataDirectory, app.manifest.id, app.folder);
	// Only the entry's own old data (stateLocation() binds its name to the entry, as it bound it then)
	if(!std::filesystem::exists(data / "history") && !std::filesystem::exists(data / "state.json"))
		adoptOldData(dataDirectory, entry, data);
	// The history first: it sets the live state that the "persist" paths are then loaded into
	if(history == History::OnDisk) screen.setHistoryDirectory(data / "history");
	screen.setStateFile(data / "state.json");
	screen.setDataFile(data / "data.sqlite"); // after the state: they may import it
	screen.setUsageFile(data / "usage.json");
	return screen;
}

std::filesystem::path Screen::appOf(const std::filesystem::path& screenFile) {
	std::error_code error;
	std::filesystem::path manifest = std::filesystem::absolute(screenFile, error).parent_path() / "app.json";
	if(error || !std::filesystem::is_regular_file(manifest, error)) return {};
	const AppFolder app = readFolder(manifest);
	if(!std::filesystem::equivalent(app.folder / app.manifest.entry, screenFile, error)) return {};
	return manifest;
}

std::filesystem::path Screen::appDataDirectory(const std::filesystem::path& dataDirectory, std::string_view id) {
	return dataDirectory / "apps" / format::appId(id);
}

std::filesystem::path Screen::appDataDirectory(const std::filesystem::path& dataDirectory, std::string_view id,
                                               const std::filesystem::path& location) {
	const std::filesystem::path apps = dataDirectory / "apps";
	return apps / detail::boundDataName([&](const std::string& name) { return apps / name / "location"; },
	                                    format::appId(id), location);
}

const format::AppManifest* Screen::app() const noexcept { return impl_->app ? &*impl_->app : nullptr; }

} // namespace affresco
