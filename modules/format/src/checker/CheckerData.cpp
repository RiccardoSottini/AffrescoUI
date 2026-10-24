/// @file
/// @brief The checker's reading of the data layer: `data.<collection>` and `data.<query>` hold lists of records with
/// the fields their model declares, and nothing else.

#include "CheckerImpl.hpp"

#include "affresco/data/Model.hpp"

namespace affresco::format::checker {

void Checker::learnData() {
	data::Models models;
	try {
		models = data::parseModels(document_.value("models", json()));
	} catch(const DocumentError&) {
		return; // a malformed section is the section's to report: data then reads as anything
	}
	auto& fields = data_.get_ref<Shape::object_t&>();
	std::map<std::string, Shape> records;
	for(const auto& [name, model] : models) {
		Shape list = Shape::array({shapeOfType(data::schemaOf(model), 0)});
		records.emplace(name, list);
		fields.emplace_back(name, std::move(list));
	}
	const auto queries = document_.find("queries");
	if(queries == document_.end() || !queries->is_object()) return;
	for(const auto& [name, query] : queries->items()) {
		if(records.contains(name) || !query.is_object()) continue;
		const auto from = records.find(query.value("from", std::string()));
		const bool counts = query.value("count", json(false)) != false;
		fields.emplace_back(name, counts || from == records.end() ? anything : from->second);
	}
}

} // namespace affresco::format::checker
