/// @file
/// @brief affresco::Screen's records: the database file a screen's records live in, and the one-time import of the
/// records an earlier version of the app kept in its "persist" state file (a model's "importFrom").

#include "ScreenImpl.hpp"
#include "StateFile.hpp"
#include "StateTypes.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/data/Model.hpp"

#include <fstream>
#include <sstream>

namespace affresco {

namespace {

/// @brief The state an earlier version of the app kept on disk, as a state object: the file holds
/// `{"/habits": [...], "/nextId": 3}`, one JSON Pointer per persisted path.
/// @param file The state file.
/// @return The state, or null if there is none.
json oldStateOf(const std::filesystem::path& file) {
	if(file.empty()) return nullptr;
	const std::ifstream in(file);
	if(!in) return nullptr;
	std::stringstream text;
	text << in.rdbuf();
	json saved;
	try {
		saved = parseUntrusted(text.str());
	} catch(const std::exception&) { // not a state file this library wrote: nothing to import
		return nullptr;
	}
	if(!saved.is_object()) return nullptr;
	json state = json::object();
	for(const auto& [pointer, value] : saved.items()) try {
			if(pointer.starts_with('/')) state[json::json_pointer(pointer)] = value;
		} catch(const json::exception&) { // NOLINT(bugprone-empty-catch): a key that is no pointer: not ours
		}
	return state;
}

/// @brief Whether following the models will import records from an old state: a model with "importFrom" whose
/// collection the store doesn't hold yet, and whose path the old state has.
/// @param document The document.
/// @param store The store.
/// @param state The old state.
/// @return true if it will.
bool importsFrom(const json& document, const data::Store& store, const json& state) {
	if(!state.is_object() || !document.contains("models")) return false;
	data::Models models;
	try {
		models = data::parseModels(document["models"]);
	} catch(const DocumentError&) {
		return false;
	}
	for(const auto& [name, model] : models) {
		if(!model.importFrom || store.models().contains(name)) continue;
		std::string pointer;
		for(char c : model.importFrom->path.substr(6)) pointer += c == '.' ? '/' : c;
		if(state.contains(json::json_pointer("/" + pointer))) return true;
	}
	return false;
}

} // namespace

std::optional<std::string> Screen::Impl::persistedMismatch(const json::json_pointer& pointer, const json& value) const {
	const json& document = repository.document();
	const auto types = document.find("stateTypes");
	if(types == document.end() || !types->is_object()) return std::nullopt;
	const std::string place = pointer.to_string(); // "/habits/list"
	const json* type = nullptr;
	std::string path = "state";
	for(std::size_t start = 1; start <= place.size();) {
		const std::size_t slash = std::min(place.find('/', start), place.size());
		const std::string key = place.substr(start, slash - start);
		type = type ? format::typeInside(*type, &key) : (types->contains(key) ? &(*types)[key] : nullptr);
		if(!type) return std::nullopt; // no declared type there: anything fits
		path += "." + key;
		start = slash + 1;
	}
	const std::optional<format::Mismatch> mismatch = format::typeMismatch(value, *type, path);
	if(!mismatch) return std::nullopt;
	return mismatch->text();
}

void Screen::Impl::keepRejected(const json& rejected) const {
	std::string name = stateFile.filename().string();
	if(name.ends_with(".state.json")) name.resize(name.size() - std::string_view(".state.json").size());
	const std::filesystem::path side = stateFile.parent_path() / (name + ".state.rejected.json");
	json kept = json::object();
	if(std::ifstream in(side); in) { // what an earlier start set aside stays, unless the same place is set aside again
		std::stringstream text;
		text << in.rdbuf();
		try {
			kept = parseUntrusted(text.str());
		} catch(const std::exception&) { // NOLINT(bugprone-empty-catch): not ours: replaced below
		}
		if(!kept.is_object()) kept = json::object();
	}
	for(const auto& [key, entry] : rejected.items()) kept[key] = entry;
	if(!detail::writeStateFile(side, kept.dump(2) + '\n')) {
		affresco::log(LogLevel::Error, "AffrescoUI screen: could not write " + side.string());
		return;
	}
	affresco::log(LogLevel::Warning, "AffrescoUI screen: " + std::to_string(rejected.size()) +
	                                     " values kept in " + stateFile.string() +
	                                     " no longer fit the screen's stateTypes: it opens with its own, and keeps "
	                                     "them in " + side.string());
}

void Screen::setDataFile(const std::filesystem::path& file) {
	Impl& impl = *impl_;
	std::error_code error;
	if(file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), error);
	auto store = std::make_unique<data::Store>(file);
	const json oldState = oldStateOf(impl.stateFile);
	if(importsFrom(impl.repository.document(), *store, oldState)) {
		// Copied, never moved: the app's file stays where it is, and its copy keeps what this version no longer reads
		std::filesystem::path backup = impl.stateFile;
		backup += ".before-data.json";
		if(!std::filesystem::exists(backup, error)) {
			std::filesystem::copy_file(impl.stateFile, backup, error);
			if(error)
				throw ScreenError("AffrescoUI data: could not back up " + impl.stateFile.string() +
				                  " before importing its records: " + error.message());
			affresco::log(LogLevel::Info, "AffrescoUI data: importing the records kept in " + impl.stateFile.string() +
			                                  " (a copy is in " + backup.string() + ")");
		}
	}
	impl.runtime.setDataStore(store.get(), oldState.is_null() ? nullptr : &oldState);
	impl.store = std::move(store);
	impl.rules.setData(impl.store.get());
}

data::Store& Screen::data() noexcept { return impl_->runtime.dataStore(); }

std::string Screen::dataJson() { return impl_->runtime.dataValue().dump(); }

void Screen::allowDataDirectory(const std::filesystem::path& directory) {
	impl_->runtime.allowDataDirectory(directory);
}

} // namespace affresco
