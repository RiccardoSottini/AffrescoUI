/// @file
/// @brief Comparing two versions of the models (Changes.hpp).

#include "affresco/data/Changes.hpp"

#include <algorithm>
#include <format>
#include <set>

namespace affresco::data {

namespace {

/// @brief One change found inside a field.
struct Found {
	std::string text; ///< What it is.
	ChangeClass kind; ///< Its class.
};

/// @brief Compares an optional bound: a lower one widens when it falls, an upper one when it rises.
/// @param name The bound's name.
/// @param before It before.
/// @param after It after.
/// @param lower Whether it is a lower bound.
/// @param found Where changes go.
template <typename T>
void compareBound(std::string_view name, const std::optional<T>& before, const std::optional<T>& after, bool lower,
                  std::vector<Found>& found) {
	if(before == after) return;
	const bool widens = !after || (before && (lower ? *after < *before : *after > *before));
	found.push_back({std::format("{} {} {}", widens ? "widens" : "narrows", name,
	                             after ? std::format("to {}", *after) : std::string("(none now)")),
	                 widens ? ChangeClass::Compatible : ChangeClass::Destructive});
}

void compareField(const Field& before, const Field& after, std::vector<Found>& found);

/// @brief Compares the fields of two objects (or two models).
/// @param before The fields before.
/// @param after The fields after.
/// @param prefix The path of the object: "" or "meta.".
/// @param found Where changes go.
void compareFields(const std::map<std::string, Field>& before, const std::map<std::string, Field>& after,
                   const std::string& prefix, std::vector<std::pair<std::string, Found>>& found) {
	for(const auto& [name, field] : after) {
		const auto old = before.find(name);
		if(old == before.end()) {
			const bool fits = !field.required || field.defaultValue;
			found.push_back({prefix + name,
			                 {std::string("adds the ") + (field.required ? "required" : "optional") + " field '" +
			                      prefix + name + "'" + (field.required && field.defaultValue ? " with a default" : ""),
			                  fits ? ChangeClass::Compatible : ChangeClass::Transforming}});
			continue;
		}
		std::vector<Found> inner;
		compareField(old->second, field, inner);
		for(Found& change : inner)
			found.push_back({prefix + name, {"'" + prefix + name + "' " + change.text, change.kind}});
	}
	for(const auto& [name, field] : before)
		if(!after.contains(name))
			found.push_back({prefix + name,
			                 {"removes the field '" + prefix + name +
			                      "' (its values stay in the store, "
			                      "unread)",
			                  ChangeClass::Destructive}});
}

/// @brief Compares what a field requires of its values: required, unique and its bounds.
/// @param before It before.
/// @param after It after.
/// @param found Where changes go.
void compareConstraints(const Field& before, const Field& after, std::vector<Found>& found) {
	if(before.required != after.required)
		found.push_back({after.required ? "becomes required" : "becomes optional",
		                 !after.required || after.defaultValue ? ChangeClass::Compatible : ChangeClass::Transforming});
	if(before.unique != after.unique)
		found.push_back({after.unique ? "becomes unique" : "is no longer unique",
		                 after.unique ? ChangeClass::Destructive : ChangeClass::Compatible});
	compareBound("minimum", before.minimum, after.minimum, true, found);
	compareBound("maximum", before.maximum, after.maximum, false, found);
	compareBound("minLength", before.minLength, after.minLength, true, found);
	compareBound("maxLength", before.maxLength, after.maxLength, false, found);
	compareBound("minItems", before.minItems, after.minItems, true, found);
	compareBound("maxItems", before.maxItems, after.maxItems, false, found);
}

/// @brief Compares the values a field takes: an enum's, a reference's target and what deleting it does, a default.
/// @param before It before.
/// @param after It after.
/// @param found Where changes go.
void compareValues(const Field& before, const Field& after, std::vector<Found>& found) {
	if(before.values != after.values) {
		const bool kept = std::ranges::all_of(
		    before.values, [&](const std::string& value) { return std::ranges::contains(after.values, value); });
		found.push_back({kept ? "widens its values" : "narrows its values (some are gone)",
		                 kept ? ChangeClass::Compatible : ChangeClass::Destructive});
	}
	if(before.to != after.to)
		found.push_back({"refers to '" + after.to + "' instead of '" + before.to + "'", ChangeClass::Transforming});
	if(before.onDelete != after.onDelete) found.push_back({"on delete: " + after.onDelete, ChangeClass::Compatible});
	if(before.defaultValue != after.defaultValue) found.push_back({"changes its default", ChangeClass::Compatible});
}

/// @brief Compares two definitions of one field.
/// @param before It before.
/// @param after It after.
/// @param found Where changes go.
void compareField(const Field& before, const Field& after, std::vector<Found>& found) {
	if(before.type != after.type) {
		const bool widens = before.type == FieldType::Integer && after.type == FieldType::Number;
		found.push_back({std::format("{} from {} to {}", widens ? "widens" : "changes type", typeName(before.type),
		                             typeName(after.type)),
		                 widens ? ChangeClass::Compatible : ChangeClass::Transforming});
		return;
	}
	compareConstraints(before, after, found);
	compareValues(before, after, found);
	if(before.of && after.of) {
		std::vector<Found> items;
		compareField(*before.of, *after.of, items);
		for(Found& change : items) found.push_back({"items: " + change.text, change.kind});
	}
	if(before.type == FieldType::Object) {
		std::vector<std::pair<std::string, Found>> inner;
		compareFields(before.fields, after.fields, "", inner);
		for(auto& [name, change] : inner) found.push_back({"field " + change.text, change.kind});
	}
}

/// @brief The fields the new migrations of a model move away (renamed, split or merged) or drop.
/// @param before The model before.
/// @param after The model after.
/// @param moved Fields moved away.
/// @param dropped Fields dropped.
void explained(const Model& before, const Model& after, std::set<std::string>& moved, std::set<std::string>& dropped) {
	for(const Migration& migration : migrationsFrom(after, before.version, "models." + after.name))
		for(const MigrationStep& step : migration.steps) {
			using Kind = MigrationStep::Kind;
			if(step.kind == Kind::Drop) dropped.insert(step.fields.begin(), step.fields.end());
			if(step.kind == Kind::Rename || step.kind == Kind::Split || step.kind == Kind::Merge)
				moved.insert(step.fields.begin(), step.fields.end());
		}
}

/// @brief Compares two versions of one collection's model.
/// @param before It before.
/// @param after It after.
/// @param diff Where changes go.
void compareModel(const Model& before, const Model& after, ModelDiff& diff) {
	std::set<std::string> moved, dropped;
	explained(before, after, moved, dropped);
	std::vector<std::pair<std::string, Found>> found;
	compareFields(before.fields, after.fields, "", found);
	for(auto& [field, change] : found) {
		const std::string top = field.substr(0, field.find('.'));
		if(change.kind == ChangeClass::Destructive && moved.contains(top) && !after.fields.contains(top)) {
			change.kind = ChangeClass::Transforming; // renamed, split or merged into other fields
			change.text = "moves the field '" + top + "' (a migration)";
		}
		diff.changes.push_back({after.name, field, after.name + ": " + change.text, change.kind});
	}
	for(const std::string& field : dropped)
		diff.changes.push_back({after.name, field,
		                        after.name + ": drops the values of '" + field + "' (archived, restorable)",
		                        ChangeClass::Destructive});
	if(diff.changes.empty() && before.version != after.version)
		diff.changes.push_back({after.name, "",
		                        std::format("{}: version {} to {}", after.name, before.version, after.version),
		                        ChangeClass::Compatible});
}

} // namespace

ChangeClass ModelDiff::worst() const noexcept {
	ChangeClass result = ChangeClass::Compatible;
	for(const ModelChange& change : changes) result = std::max(result, change.kind);
	return result;
}

ChangeClass ModelDiff::worst(std::string_view collection) const noexcept {
	ChangeClass result = ChangeClass::Compatible;
	for(const ModelChange& change : changes)
		if(change.collection == collection) result = std::max(result, change.kind);
	return result;
}

json ModelDiff::toJson() const {
	json list = json::array();
	for(const ModelChange& change : changes)
		list.push_back({{"collection", change.collection},
		                {"field", change.field},
		                {"change", change.text},
		                {"class", className(change.kind)}});
	return list;
}

std::string_view className(ChangeClass kind) noexcept {
	switch(kind) {
	case ChangeClass::Compatible:
		return "compatible";
	case ChangeClass::Transforming:
		return "transforming";
	case ChangeClass::Destructive:
		return "destructive";
	}
	return "compatible";
}

std::vector<Migration> migrationsFrom(const Model& model, int from, const std::string& where) {
	std::vector<Migration> chain;
	int at = from;
	for(Migration& migration : migrationsOf(model, where))
		if(migration.from == at) {
			at = migration.to;
			chain.push_back(std::move(migration));
		}
	return chain;
}

ModelDiff compareModels(const Models& before, const Models& after) {
	ModelDiff diff;
	for(const auto& [name, model] : after) {
		const auto old = before.find(name);
		if(old == before.end()) {
			diff.changes.push_back({name, "", name + ": a new collection", ChangeClass::Compatible});
			continue;
		}
		compareModel(old->second, model, diff);
	}
	for(const auto& [name, model] : before)
		if(!after.contains(name))
			diff.changes.push_back({name, "", name + ": removes the collection (its records stay in the store, unread)",
			                        ChangeClass::Destructive});
	return diff;
}

} // namespace affresco::data
