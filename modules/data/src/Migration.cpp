/// @file
/// @brief Migrations: reading them, applying their steps to records, and their inverses (Migration.hpp).

#include "affresco/data/Migration.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace affresco::data {

namespace {

/// @brief Fails with a document error at a place.
/// @param where The place.
/// @param message What is wrong.
[[noreturn]] void malformed(const std::string& where, const std::string& message) { expect::fail(where, message); }

/// @brief Reads a field's name in a step.
/// @param value The value.
/// @param where Its place.
/// @return The name.
std::string fieldName(const json& value, const std::string& where) {
	if(!value.is_string() || !isName(value.get<std::string>()) || isSystemField(value.get<std::string>()))
		malformed(where, "expected a field's name");
	return value.get<std::string>();
}

/// @brief Reads a list of at least two field names.
/// @param value The value.
/// @param where Its place.
/// @return The names.
std::vector<std::string> fieldNames(const json& value, const std::string& where) {
	if(!value.is_array() || value.size() < 2 || value.size() > 16) malformed(where, "expected 2 to 16 fields' names");
	std::vector<std::string> names;
	for(std::size_t i = 0; i < value.size(); ++i) names.push_back(fieldName(value[i], std::format("{}[{}]", where, i)));
	return names;
}

/// @brief Checks that a step has only the keys it reads.
/// @param step The step.
/// @param keys Its keys.
/// @param where Its place.
void onlyKeys(const json& step, std::initializer_list<std::string_view> keys, const std::string& where) {
	for(const auto& [key, value] : step.items())
		if(key != "note" && std::ranges::find(keys, key) == keys.end()) malformed(where + "." + key, "unknown key");
}

/// @brief Reads a list of values or expressions, one per field.
/// @param value The value.
/// @param count How many.
/// @param where Its place.
void expectList(const json& value, std::size_t count, const std::string& where) {
	if(!value.is_array() || value.size() != count) malformed(where, std::format("expected a list of {}", count));
}

/// @brief Reads one step.
/// @param step The step.
/// @param where Its place.
/// @return It.
MigrationStep parseStep(const json& step, const std::string& where) {
	if(!step.is_object() || step.empty())
		malformed(where, "expected a step: rename, convert, fill, drop, split or merge");
	MigrationStep read;
	if(step.contains("rename")) {
		onlyKeys(step, {"rename", "to"}, where);
		read.kind = MigrationStep::Kind::Rename;
		read.fields = {fieldName(step["rename"], where + ".rename")};
		read.into = {fieldName(step.value("to", json()), where + ".to")};
	} else if(step.contains("convert")) {
		onlyKeys(step, {"convert", "with", "inverse"}, where);
		read.kind = MigrationStep::Kind::Convert;
		read.fields = read.into = {fieldName(step["convert"], where + ".convert")};
		if(!step.contains("with")) malformed(where, "a convert needs \"with\": the new value, \"{{value * 1000}}\"");
		read.with = step["with"];
		read.inverse = step.value("inverse", json());
	} else if(step.contains("fill")) {
		onlyKeys(step, {"fill", "with"}, where);
		read.kind = MigrationStep::Kind::Fill;
		read.into = {fieldName(step["fill"], where + ".fill")};
		if(!step.contains("with")) malformed(where, "a fill needs \"with\": the value");
		read.with = step["with"];
	} else if(step.contains("drop")) {
		onlyKeys(step, {"drop"}, where);
		read.kind = MigrationStep::Kind::Drop;
		read.fields = {fieldName(step["drop"], where + ".drop")};
	} else if(step.contains("split")) {
		onlyKeys(step, {"split", "into", "with", "inverse"}, where);
		read.kind = MigrationStep::Kind::Split;
		read.fields = {fieldName(step["split"], where + ".split")};
		read.into = fieldNames(step.value("into", json()), where + ".into");
		read.with = step.value("with", json());
		expectList(read.with, read.into.size(), where + ".with");
		read.inverse = step.value("inverse", json());
	} else if(step.contains("merge")) {
		onlyKeys(step, {"merge", "into", "with", "inverse"}, where);
		read.kind = MigrationStep::Kind::Merge;
		read.fields = fieldNames(step["merge"], where + ".merge");
		read.into = {fieldName(step.value("into", json()), where + ".into")};
		if(!step.contains("with")) malformed(where, "a merge needs \"with\": the merged value");
		read.with = step["with"];
		read.inverse = step.value("inverse", json());
		if(!read.inverse.is_null()) expectList(read.inverse, read.fields.size(), where + ".inverse");
	} else {
		malformed(where, "unknown step (rename, convert, fill, drop, split, merge)");
	}
	return read;
}

/// @brief A value of a step: as written, or an expression's result.
/// @param with The value or expression.
/// @param variables `value` and `record`.
/// @param evaluate The evaluator.
/// @return The value.
json valueOf(const json& with, const json& variables, const Evaluator& evaluate) {
	if(!with.is_string() || !with.get_ref<const std::string&>().contains("{{")) return with;
	if(!evaluate) throw DataError("migration", "a migration's expression needs the screen format to evaluate it");
	try {
		return evaluate(with.get<std::string>(), variables);
	} catch(const DataError&) {
		throw;
	} catch(const std::exception& error) {
		throw DataError("migration", with.get<std::string>() + ": " + error.what());
	}
}

/// @brief A field's value, or null.
/// @param fields The fields.
/// @param name The field.
/// @return The value.
json valueAt(const json& fields, const std::string& name) {
	const auto it = fields.find(name);
	return it == fields.end() ? json() : *it;
}

/// @brief Sets a field, or removes it for null.
/// @param fields The fields.
/// @param name The field.
/// @param value The value.
void put(json& fields, const std::string& name, json value) {
	if(value.is_null()) fields.erase(name);
	else fields[name] = std::move(value);
}

} // namespace

Migration parseMigration(const json& definition, const std::string& collection, const std::string& where) {
	if(!definition.is_object()) malformed(where, "expected {\"from\", \"to\", \"steps\"}");
	onlyKeys(definition, {"from", "to", "steps"}, where);
	Migration migration;
	migration.collection = collection;
	migration.written = definition;
	for(const char* key : {"from", "to"})
		if(!definition.contains(key) || !definition[key].is_number_integer() ||
		   definition[key].get<std::int64_t>() < 1 || definition[key].get<std::int64_t>() > 1000000)
			malformed(where + "." + key, "expected a model version, a whole number from 1");
	migration.from = definition["from"].get<int>();
	migration.to = definition["to"].get<int>();
	if(migration.to <= migration.from) malformed(where + ".to", "expected a version above \"from\"");
	const auto steps = definition.find("steps");
	if(steps == definition.end() || !steps->is_array() || steps->size() > Limits::steps)
		malformed(where + ".steps", std::format("expected a list of at most {} steps", Limits::steps));
	for(std::size_t i = 0; i < steps->size(); ++i)
		migration.steps.push_back(parseStep((*steps)[i], std::format("{}.steps[{}]", where, i)));
	return migration;
}

std::vector<Migration> migrationsOf(const Model& model, const std::string& where) {
	std::vector<Migration> migrations;
	for(std::size_t i = 0; i < model.migrations.size(); ++i) {
		const std::string at = std::format("{}.migrations[{}]", where, i);
		Migration migration = parseMigration(model.migrations[i], model.name, at);
		if(!migrations.empty() && migration.from < migrations.back().to)
			malformed(at + ".from",
			          std::format("starts before the migration before it ends (at {})", migrations.back().to));
		if(migration.to > model.version)
			malformed(at + ".to", std::format("ends past the model's version ({})", model.version));
		migrations.push_back(std::move(migration));
	}
	return migrations;
}

void applyStep(const MigrationStep& step, json& fields, const Evaluator& evaluate, const json* archived) {
	using Kind = MigrationStep::Kind;
	switch(step.kind) {
	case Kind::Rename: {
		json value = valueAt(fields, step.fields[0]);
		fields.erase(step.fields[0]);
		put(fields, step.into[0], std::move(value));
		return;
	}
	case Kind::Convert: {
		const json value = valueAt(fields, step.fields[0]);
		if(value.is_null()) return; // nothing to convert
		put(fields, step.into[0], valueOf(step.with, {{"value", value}, {"record", fields}}, evaluate));
		return;
	}
	case Kind::Fill:
		if(valueAt(fields, step.into[0]).is_null())
			put(fields, step.into[0], valueOf(step.with, {{"value", nullptr}, {"record", fields}}, evaluate));
		return;
	case Kind::Drop:
		fields.erase(step.fields[0]);
		return;
	case Kind::Split: {
		const json value = valueAt(fields, step.fields[0]);
		const json variables = {{"value", value}, {"record", fields}};
		std::vector<json> parts;
		for(const json& with : step.with) parts.push_back(valueOf(with, variables, evaluate));
		fields.erase(step.fields[0]);
		for(std::size_t i = 0; i < step.into.size(); ++i) put(fields, step.into[i], std::move(parts[i]));
		return;
	}
	case Kind::Merge: {
		json merged = valueOf(step.with, {{"value", nullptr}, {"record", fields}}, evaluate);
		for(const std::string& field : step.fields) fields.erase(field);
		put(fields, step.into[0], std::move(merged));
		return;
	}
	case Kind::Restore:
		if(archived && archived->contains(step.into[0])) fields[step.into[0]] = (*archived)[step.into[0]];
		return;
	}
}

std::optional<std::vector<MigrationStep>> inverseOf(const Migration& migration) {
	using Kind = MigrationStep::Kind;
	std::vector<MigrationStep> inverse;
	for(const MigrationStep& step : std::views::reverse(migration.steps)) {
		MigrationStep back;
		switch(step.kind) {
		case Kind::Rename:
			back = {Kind::Rename, step.into, step.fields, {}, {}};
			break;
		case Kind::Convert:
			if(step.inverse.is_null()) return std::nullopt;
			back = {Kind::Convert, step.fields, step.fields, step.inverse, step.with};
			break;
		case Kind::Fill:
			continue; // the value stays, unread by the older model
		case Kind::Drop:
			back = {Kind::Restore, {}, step.fields, {}, {}};
			break;
		case Kind::Split:
			if(step.inverse.is_null()) return std::nullopt;
			back = {Kind::Merge, step.into, step.fields, step.inverse, step.with};
			break;
		case Kind::Merge:
			if(step.inverse.is_null()) return std::nullopt;
			back = {Kind::Split, step.into, step.fields, step.inverse, step.with};
			break;
		case Kind::Restore:
			return std::nullopt;
		}
		inverse.push_back(std::move(back));
	}
	return inverse;
}

json seedFields(const Model& model, const json& record, const Evaluator& evaluate, const std::string& where) {
	if(!record.is_object()) throw DataError("invalid", where + ": expected a record");
	json fields = record;
	fields.erase("id");
	if(!model.migrations.empty()) {
		const std::vector<Migration> chain = migrationsOf(model, "models." + model.name);
		int at = chain.front().from;
		for(const Migration& migration : chain) {
			if(migration.from != at) continue;
			for(const MigrationStep& step : migration.steps) applyStep(step, fields, evaluate);
			at = migration.to;
		}
	}
	json declared = json::object();
	for(const auto& [name, value] : fields.items())
		if(model.fields.contains(name)) declared[name] = value;
	json checked = validateRecord(model, declared, where);
	if(record.contains("id")) checked["id"] = record["id"];
	return checked;
}

bool dropsData(const Migration& migration) noexcept {
	return std::ranges::any_of(migration.steps,
	                           [](const MigrationStep& step) { return step.kind == MigrationStep::Kind::Drop; });
}

std::string describe(const MigrationStep& step) {
	const auto joined = [](const std::vector<std::string>& names) {
		std::string text;
		for(std::size_t i = 0; i < names.size(); ++i) text += (i ? ", " : "") + names[i];
		return text;
	};
	using Kind = MigrationStep::Kind;
	switch(step.kind) {
	case Kind::Rename:
		return "renames " + step.fields[0] + " to " + step.into[0];
	case Kind::Convert:
		return "converts " + step.fields[0] + " with " + toText(step.with) +
		       (step.inverse.is_null() ? " (no inverse: undoing it restores the archive)" : "");
	case Kind::Fill:
		return "fills " + step.into[0] + " with " + toText(step.with) + " where it has no value";
	case Kind::Drop:
		return "drops " + step.fields[0] + " (destructive: its values are archived)";
	case Kind::Split:
		return "splits " + step.fields[0] + " into " + joined(step.into);
	case Kind::Merge:
		return "merges " + joined(step.fields) + " into " + step.into[0];
	case Kind::Restore:
		return "restores " + step.into[0] + " from the archive";
	}
	return {};
}

} // namespace affresco::data
