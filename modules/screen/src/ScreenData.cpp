/// @file
/// @brief affresco::Screen's records: the database file a screen's records live in, and the one-time import of the
/// records an earlier version of the app kept in its "persist" state file (a model's "importFrom").

#include "ScreenImpl.hpp"

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

void Screen::allowDataDirectory(const std::filesystem::path& directory) {
	impl_->runtime.allowDataDirectory(directory);
}

} // namespace affresco
