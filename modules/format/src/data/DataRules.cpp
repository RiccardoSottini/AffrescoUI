/// @file
/// @brief The data layer as rules for version control (DataRules.hpp).

#include "DataRules.hpp"
#include "DataLayer.hpp"

#include "affresco/data/Changes.hpp"
#include "affresco/data/Query.hpp"
#include "affresco/data/Store.hpp"
#include "affresco/format/NodeIds.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <map>
#include <set>

namespace affresco::format {

namespace {

/// The write steps, by their first key.
constexpr auto writeSteps =
    std::to_array<std::string_view>({"insert", "update", "delete", "importCsv", "undoData", "redoData"});

/// The sections of the view layer, besides the nodes.
constexpr auto viewSections = std::to_array<const char*>(
    {"styles", "components", "colors", "tokens", "theme", "fonts", "window", "strings", "languages"});

/// The sections of the logic layer (the view-model), besides the nodes' handlers and the state.
constexpr auto logicSections = std::to_array<const char*>(
    {"actions", "computed", "watch", "keys", "onLoad", "menus", "settings", "queries", "stateTypes"});

/// @brief A section, or null.
/// @param document The document.
/// @param name The section.
/// @return It.
const json& sectionOf(const json& document, const char* name) {
	static const json none;
	const auto it = document.find(name);
	return it == document.end() || !document.is_object() ? none : *it;
}

/// @brief Whether a value is written as an expression: `{{...}}` or a `$` operator.
/// @param value The value.
/// @return true if it is.
bool dynamic(const json& value) {
	if(value.is_string()) return value.get_ref<const std::string&>().contains("{{");
	return value.is_object() && value.size() == 1 && value.begin().key().starts_with('$');
}

/// @brief A query declaration with its expressions taken out, to check what it says as written.
/// @param value The declaration.
/// @param depth How deep so far.
/// @return It, each expression null.
json staticPart(const json& value, int depth = 0) {
	if(dynamic(value) || depth > 32) return nullptr;
	if(!value.is_structured()) return value;
	json result = value;
	for(auto& item : result) item = staticPart(item, depth + 1);
	return result;
}

/// @brief The models of a document, read.
/// @param document The document.
/// @return They (none for a malformed section: validation reports it).
data::Models modelsOf(const json& document) {
	try {
		return data::parseModels(sectionOf(document, "models"));
	} catch(const DocumentError&) {
		return {};
	}
}

/// @brief Collects the write steps of a document: each step object whose key names a write step.
/// @param value A value of the document.
/// @param found The steps, as their JSON text.
/// @param depth How deep so far.
void collectWrites(const json& value, std::multiset<std::string>& found, int depth = 0) {
	if(depth > maxDocumentNesting || !value.is_structured()) return;
	if(value.is_object())
		for(const std::string_view kind : writeSteps)
			if(const auto it = value.find(std::string(kind));
			   it != value.end() && (it->is_string() || it->is_boolean()))
				found.insert(value.dump());
	for(const json& item : value) collectWrites(item, found, depth + 1);
}

/// @brief The collection a write step writes, as written.
/// @param step The step's JSON text.
/// @return E.g. "insert into 'songs'".
std::string describeWrite(const std::string& step) {
	const json parsed = json::parse(step);
	for(const std::string_view kind : writeSteps)
		if(const auto it = parsed.find(std::string(kind)); it != parsed.end())
			return std::string(kind) + (it->is_string() ? " '" + it->get<std::string>() + "'" : "");
	return "a write";
}

/// @brief How many entries of a section differ between two documents.
/// @param before The document before.
/// @param after The document after.
/// @param name The section.
/// @return The count: keys added, removed or changed (1 for a section that isn't an object).
std::size_t changedEntries(const json& before, const json& after, const char* name) {
	const json& a = sectionOf(before, name);
	const json& b = sectionOf(after, name);
	if(a == b) return 0;
	if(!a.is_object() || !b.is_object()) return 1;
	std::size_t count = 0;
	for(const auto& [key, value] : b.items())
		if(!a.contains(key) || a[key] != value) ++count;
	for(const auto& [key, value] : a.items())
		if(!b.contains(key)) ++count;
	return count;
}

/// @brief A node's own part, without its children: its look (view) or its handlers (logic).
struct NodeParts {
	json view = json::object();  ///< Its keys but the handlers.
	json logic = json::object(); ///< Its handlers ("on...") and "repeat", "when", "bind".
};

/// @brief The nodes of a document by id (or place, without one), split into their view and logic parts.
/// @param document The document.
/// @return They.
std::map<std::string, NodeParts> nodePartsOf(const json& document) {
	std::map<std::string, NodeParts> nodes;
	if(!document.is_object()) return nodes;
	forEachNode(document, [&](const json& node, const std::string& pointer) {
		if(!node.is_object()) return;
		NodeParts parts;
		for(const auto& [key, value] : node.items()) {
			if(key == "children") continue;
			const bool handler =
			    (key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]))) ||
			    key == "bind" || key == "repeat" || key == "when";
			(handler ? parts.logic : parts.view)[key] = value;
		}
		const auto id = node.find("id");
		nodes.insert_or_assign(id != node.end() && id->is_string() ? "#" + id->get<std::string>() : pointer,
		                       std::move(parts));
	});
	return nodes;
}

/// @brief A dry run of a version of the models on a copy of the store, kept while neither changes: one change is
/// judged, reviewed and described by a few calls (checkChange, reviewLines, impact), which share one dry run.
/// @param store The store.
/// @param models The models.
/// @return The report.
const data::FollowReport& planOf(const data::Store& store, const json& models) {
	struct Last {
		const data::Store* store = nullptr; ///< Of this store.
		std::uint64_t changes = 0;          ///< At this count of its changes.
		json models;                        ///< For these models.
		data::FollowReport report;          ///< The dry run.
	};
	static Last last; // the main thread's: rules are used there only
	if(last.store != &store || last.changes != store.changes() || last.models != models) {
		data::FollowReport report = store.plan(models); // may throw: nothing is kept then
		last = {&store, store.changes(), models, std::move(report)};
	}
	return last.report;
}

/// @brief What a model change does to the records, from a dry run if there is a store.
/// @param diff The model changes.
/// @param after The document after.
/// @param store The store, or nullptr.
/// @return "none", "migrates N records", "destructive: ..." or the dry run's failure.
std::string dataEffect(const data::ModelDiff& diff, const json& after, const data::Store* store) {
	if(diff.changes.empty()) return "none";
	std::optional<data::FollowReport> plan;
	if(store) try {
			plan = planOf(*store, sectionOf(after, "models"));
		} catch(const DocumentError& error) {
			return std::string("refused: ") + error.what();
		}
	std::string effect;
	if(diff.worst() == data::ChangeClass::Destructive) {
		effect = "destructive";
		if(plan) {
			std::size_t archived = 0;
			for(const data::CollectionReport& c : plan->collections) archived += c.archived;
			effect += std::format(": {} records change, {} archived (restorable)", plan->changed(), archived);
		}
	} else if(plan) {
		effect = plan->changed() ? std::format("migrates {} records", plan->changed()) : "none";
	} else {
		effect = diff.worst() == data::ChangeClass::Transforming ? "migrates the records" : "none";
	}
	if(plan && !plan->ok()) effect += "; refused: " + plan->text();
	return effect;
}

} // namespace

void checkModelsSection(const json& value, const std::string& name) {
	const data::Models models = data::parseModels(value, name);
	for(const auto& [collection, model] : models) {
		const std::string where = std::format("{}.{}", name, collection);
		(void)data::migrationsOf(model, where);
		for(std::size_t i = 0; i < model.seed.size(); ++i) {
			const std::string at = std::format("{}.seed[{}]", where, i);
			try {
				(void)data::seedFields(model, model.seed[i], DataLayer::evaluate, at);
			} catch(const data::DataError& error) {
				expect::fail("", error.what());
			}
			if(const auto id = model.seed[i].find("id"); id != model.seed[i].end())
				if(!(id->is_string() && data::isRecordId(id->get<std::string>())) && !id->is_number_integer())
					expect::fail(at + ".id", "expected letters, digits, - and _ (at most 64)");
		}
	}
}

void checkQueriesSection(const json& value, const std::string& name) {
	if(!value.is_object()) expect::fail(name, R"(expected an object: name -> {"from", "where", "sort", ...})");
	if(value.size() > 256) expect::fail(name, "at most 256 queries");
	for(const auto& [query, declaration] : value.items()) {
		if(!data::isName(query))
			expect::fail(std::format("{}.{}", name, query), "can't name a query (a letter, then letters, digits, _)");
		if(!declaration.is_object() || !declaration.contains("from") || !declaration["from"].is_string())
			expect::fail(std::format("{}.{}", name, query), R"(expected {"from": a collection, ...})");
	}
}

void checkDataDocument(const json& document) {
	if(document.contains("data"))
		expect::fail("data", R"(records live in the store, not in the document: give a model a "seed", or write them )"
		                     "with insert steps");
	const json& queries = sectionOf(document, "queries");
	if(!queries.is_object()) return;
	const data::Models models = data::parseModels(sectionOf(document, "models"));
	for(const auto& [name, declaration] : queries.items()) {
		const std::string where = "queries." + name;
		if(models.contains(name)) expect::fail(where, "a collection has this name: data." + name + " is its records");
		try {
			(void)data::parseQuery(staticPart(declaration), models, where);
		} catch(const data::DataError& error) {
			expect::fail("", error.what());
		}
	}
}

void checkModelChange(const json& before, const json& after, const data::Store* store) {
	const json& was = sectionOf(before, "models");
	const json& now = sectionOf(after, "models");
	if(was == now) return;
	const data::Models old = modelsOf(before);
	const data::Models next = data::parseModels(now);
	const data::ModelDiff diff = data::compareModels(old, next);
	for(const auto& [name, model] : next) {
		const auto previous = old.find(name);
		if(previous == old.end()) continue;
		const std::string where = "models." + name;
		const int from = previous->second.version;
		// Migrations already applied (up to the new version) are history: they stay as written
		const json& written = previous->second.migrations;
		for(const json& migration : written)
			if(migration.value("to", 0) <= model.version &&
			   std::ranges::find(model.migrations, migration) == model.migrations.end())
				expect::fail(where + ".migrations", "a migration already applied can't change or go (add a new one)");
		if(model.version < from) continue; // a revert: the store undoes the migrations past it
		if(std::ranges::none_of(diff.changes, [&](const data::ModelChange& change) {
			   return change.collection == name && change.kind == data::ChangeClass::Transforming;
		   }))
			continue;
		if(model.version <= from)
			expect::fail(where + ".version", std::format("this change transforms the records ({}): raise the version "
			                                             "above {} and add its migration",
			                                             diff.changes.front().text, from));
		const std::vector<data::Migration> chain = data::migrationsFrom(model, from, where);
		if(chain.empty() || chain.back().to != model.version)
			expect::fail(where + ".migrations",
			             std::format("this change transforms the records: it needs a migration from {} to {} "
			                         "(rename, convert, fill, split, merge or drop steps)",
			                         from, model.version));
	}
	if(!store) return;
	const data::FollowReport plan = planOf(*store, now);
	if(!plan.ok()) expect::fail("models", "the records don't fit the new models (a dry run): " + plan.text());
}

std::vector<std::string> dataReviewLines(const json& before, const json& after, const data::Store* store) {
	std::vector<std::string> lines;
	if(sectionOf(before, "models") != sectionOf(after, "models")) {
		const data::ModelDiff diff = data::compareModels(modelsOf(before), modelsOf(after));
		const std::string effect = dataEffect(diff, after, store);
		for(const data::ModelChange& change : diff.changes) {
			if(change.kind == data::ChangeClass::Destructive)
				lines.push_back("changes the models destructively: " + change.text + " (accepting confirms it)");
			else if(change.kind == data::ChangeClass::Transforming)
				lines.push_back("migrates the records: " + change.text);
		}
		if(!lines.empty() && effect != "none") lines.push_back("records: " + effect);
	}
	std::multiset<std::string> was, now;
	collectWrites(before, was);
	collectWrites(after, now);
	std::vector<std::string> added;
	std::ranges::set_difference(now, was, std::back_inserter(added));
	for(const std::string& step : added) {
		lines.push_back("writes records: " + describeWrite(step));
		if(lines.size() > 64) break;
	}
	return lines;
}

json dataImpact(const json& before, const json& after, const data::Store* store) {
	std::size_t view = 0, logic = 0;
	const std::map<std::string, NodeParts> a = nodePartsOf(before), b = nodePartsOf(after);
	for(const auto& [key, parts] : b) {
		const auto old = a.find(key);
		if(old == a.end()) {
			++view;
			continue;
		}
		if(old->second.view != parts.view) ++view;
		if(old->second.logic != parts.logic) ++logic;
	}
	for(const auto& [key, parts] : a)
		if(!b.contains(key)) ++view;
	for(const char* name : viewSections) view += changedEntries(before, after, name);
	for(const char* name : logicSections) logic += changedEntries(before, after, name);
	logic += changedEntries(before, after, "state"); // the UI's short-lived state
	json model = json::array();
	std::string effect = "none";
	if(sectionOf(before, "models") != sectionOf(after, "models")) {
		const data::ModelDiff diff = data::compareModels(modelsOf(before), modelsOf(after));
		model = diff.toJson();
		effect = dataEffect(diff, after, store);
	}
	return {{"view", view}, {"logic", logic}, {"model", model}, {"data", effect}};
}

} // namespace affresco::format
