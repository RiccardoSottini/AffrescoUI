/// @file
/// @brief The write steps of the data layer: insert, update, delete, undoData, redoData and importCsv. Each writes the
/// screen's store (DataLayer.hpp) as one step of its undo, every record checked against its model; a write that is
/// refused runs "onError" with `error` ({code, message}), or is reported. They are no control steps: the app uses them
/// freely, but a revision that adds one is always reviewed (Rules::reviewLines), and an author other than the person
/// writes only what the screen's data rules let it.

#include "Engine.hpp"

#include "affresco/data/Csv.hpp"

#include <fstream>
#include <sstream>

namespace affresco::format {

namespace {

/// @brief Runs a step's "then" with `result`, or puts the result "into" a state path.
/// @param s The step.
/// @param result The result.
/// @return Whether to keep going.
bool succeeded(const Running& s, const json& result) {
	if(s.step.contains("into")) {
		const json::json_pointer target = s.target("into");
		s.steps.engine().document.markStateChanged(target);
		statePlace(s.steps.engine().versions.state(), target, s.where + ".into") = result;
	}
	if(!s.step.contains("then")) return true;
	const Scope scope{&s.locals, nullptr, {{"result", result}}, {}, {}};
	return s.run(s.step["then"], scope, s.where + ".then");
}

/// @brief Runs a step's "onError" with `error`, or reports it.
/// @param s The step.
/// @param code The error's code.
/// @param message Its message.
/// @return Whether to keep going.
bool refused(const Running& s, const std::string& code, const std::string& message) {
	if(!s.step.contains("onError")) {
		s.steps.engine().report("AffrescoUI screen: " + s.where + ": " + message);
		return true;
	}
	const Scope scope{&s.locals, nullptr, {{"error", {{"code", code}, {"message", message}}}}, {}, {}};
	return s.run(s.step["onError"], scope, s.where + ".onError");
}

/// @brief The ids a step names: its "id" (one, or a list) or its "where" (a filter of the collection).
/// @param s The step.
/// @param store The store.
/// @param collection The collection.
/// @return The ids.
std::vector<std::string> idsOf(const Running& s, const data::Store& store, const std::string& collection) {
	std::vector<std::string> ids;
	if(s.step.contains("where")) {
		const json where = s.value("where");
		for(const json& record : store.run({{"from", collection}, {"where", where}}, s.where + ".where"))
			ids.push_back(record["id"].get<std::string>());
		return ids;
	}
	if(!s.step.contains("id")) fail(s.where, "expected \"id\" (one or a list) or \"where\"");
	const json id = s.value("id");
	for(const json& one : id.is_array() ? id : json::array({id})) {
		if(!one.is_string() && !one.is_number_integer()) fail(s.where + ".id", "expected a record's id");
		ids.push_back(one.is_string() ? one.get<std::string>() : std::to_string(one.get<std::int64_t>()));
	}
	if(ids.size() > data::Limits::queryResults) fail(s.where + ".id", "at most 10000 records at once");
	return ids;
}

/// @brief The CSV text an importCsv step reads: its "text", or its "file" in an allowed folder.
/// @param s The step.
/// @param layer The data layer.
/// @return The text.
std::string csvText(const Running& s, const DataLayer& layer) {
	if(s.step.contains("text")) {
		const json text = s.value("text");
		if(!text.is_string()) fail(s.where + ".text", "expected the CSV's text");
		return text.get<std::string>();
	}
	const json file = s.value("file");
	if(!file.is_string()) fail(s.where, "expected \"file\" (in the screen's folder) or \"text\"");
	const std::filesystem::path path = layer.allowedFile(file.get<std::string>(), s.where + ".file");
	std::error_code error;
	if(std::filesystem::file_size(path, error) > data::Limits::csvBytes || error)
		fail(s.where + ".file", "a CSV file is at most 16 MiB");
	const std::ifstream in(path, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

/// @brief Runs one write on the store.
/// @param s The step.
/// @param kind "insert", "update", "delete", "undoData", "redoData" or "importCsv".
/// @return The result.
json write(const Running& s, const std::string& kind) {
	Engine& engine = s.steps.engine();
	DataLayer& layer = engine.data;
	data::Store& store = layer.store();
	if(kind == "undoData" || kind == "redoData") {
		if(layer.author() != "app") fail(s.where, "only the person undoes and redoes the records");
		return kind == "undoData" ? store.undo() : store.redo();
	}
	const json named = s.value(kind.c_str());
	if(!named.is_string()) fail(s.where + "." + kind, "expected a collection's name");
	const std::string collection = named.get<std::string>();
	layer.checkWrite(collection, s.where);
	const bool atomic = s.step.contains("values") || kind == "importCsv"; // a step of undo of their own
	if(atomic) layer.endGroup();
	else layer.groupEvent(); // the event's writes: one step of undo
	if(kind == "insert") {
		if(s.step.contains("values")) { // many records at once: one step of undo, all of them or none
			const json values = s.value("values");
			if(!values.is_array() || values.size() > data::Limits::seedRecords)
				fail(s.where + ".values", "expected a list of at most 10000 records");
			const data::Store::Batch batch(store);
			json results = json::array();
			try {
				for(const json& value : values)
					results.push_back(store.insert(collection, value, layer.author()).toJson());
			} catch(const data::DataError&) {
				if(!results.empty()) store.undo(); // the batch's records go again
				throw;
			}
			return results;
		}
		if(!s.step.contains("value")) fail(s.where, "expected \"value\" (the record's fields) or \"values\"");
		return store.insert(collection, s.value("value"), layer.author()).toJson();
	}
	if(kind == "importCsv") {
		data::CsvImport options;
		options.collection = collection;
		options.author = layer.author();
		if(s.step.contains("columns")) options.columns = s.step["columns"]; // their "with" run per row, not now
		if(s.step.contains("atomic")) options.atomic = boolean(s.value("atomic"), s.where + ".atomic");
		if(s.step.contains("separator")) {
			const json separator = s.value("separator");
			if(separator != "," && separator != ";" && separator != "\t")
				fail(s.where + ".separator", "expected \",\", \";\" or a tab");
			options.separator = separator.get<std::string>()[0];
		}
		return data::importCsv(store, csvText(s, layer), options, DataLayer::evaluate);
	}
	const std::vector<std::string> ids = idsOf(s, store, collection);
	const data::Store::Batch batch(store); // one step of undo
	json results = json::array();
	if(kind == "update") {
		if(!s.step.contains("fields")) fail(s.where, "expected \"fields\": the fields to change");
		const json changes = s.value("fields");
		for(const std::string& id : ids)
			results.push_back(store.update(collection, id, changes, layer.author()).toJson());
	} else {
		for(const std::string& id : ids) store.remove(collection, id, layer.author());
		return static_cast<std::int64_t>(ids.size());
	}
	return s.step.contains("where") || s.value("id").is_array() ? results : results.at(0);
}

/// @brief Runs a write step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runDataStep(const Running& s) {
	static constexpr std::array<const char*, 6> kinds = {"insert",   "update",   "delete",
	                                                     "undoData", "redoData", "importCsv"};
	const auto kind = std::ranges::find_if(kinds, [&](const char* k) { return s.step.contains(k); });
	if(kind == kinds.end()) return std::nullopt;
	json result;
	try {
		result = write(s, *kind);
	} catch(const data::DataError& error) {
		return refused(s, error.code(), error.what());
	} catch(const DocumentError& error) {
		if(!s.step.contains("onError")) throw; // a malformed step: reported with its place
		return refused(s, "invalid", error.what());
	}
	return succeeded(s, result);
}

} // namespace

void registerDataSteps(StepRegistry& registry) {
	registry.add({"data",
	              {{"insert", {"value", "values", "into", "then", "onError"}},
	               {"update", {"id", "where", "fields", "into", "then", "onError"}},
	               {"delete", {"id", "where", "into", "then", "onError"}},
	               {"undoData", {"into", "then", "onError"}},
	               {"redoData", {"into", "then", "onError"}},
	               {"importCsv", {"file", "text", "columns", "separator", "atomic", "into", "then", "onError"}}},
	              runDataStep});
}

} // namespace affresco::format
