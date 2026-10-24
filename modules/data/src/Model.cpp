/// @file
/// @brief Reading models and checking records against them (Model.hpp).

#include "affresco/data/Model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

namespace affresco::data {

namespace {

/// The keys a field's definition may have.
constexpr auto fieldKeys = std::to_array<std::string_view>(
    {"type", "required", "unique", "default", "minimum", "maximum", "minLength", "maxLength", "minItems", "maxItems",
	 "values", "to", "onDelete", "of", "fields", "title", "description", "note"});

/// The keys a model may have.
constexpr auto modelKeys = std::to_array<std::string_view>(
    {"version", "fields", "seed", "migrations", "importFrom", "title", "description", "note"});

/// The field types, by name.
constexpr std::array<std::pair<std::string_view, FieldType>, 10> typeNames = {{{"string", FieldType::String},
                                                                               {"number", FieldType::Number},
                                                                               {"integer", FieldType::Integer},
                                                                               {"boolean", FieldType::Boolean},
                                                                               {"date", FieldType::Date},
                                                                               {"dateTime", FieldType::DateTime},
                                                                               {"enum", FieldType::Enum},
                                                                               {"reference", FieldType::Reference},
                                                                               {"list", FieldType::List},
                                                                               {"object", FieldType::Object}}};

/// @brief Fails with a document error at a place.
/// @param where The place.
/// @param message What is wrong.
[[noreturn]] void malformed(const std::string& where, const std::string& message) { expect::fail(where, message); }

/// @brief Fails with an invalid record.
/// @param where The place.
/// @param message What is wrong.
[[noreturn]] void invalid(const std::string& where, const std::string& message) {
	throw DataError("invalid", where + ": " + message);
}

/// @brief Reads a count (a whole number from 0).
/// @param value The value.
/// @param where Its place.
/// @return The count.
std::size_t countOf(const json& value, const std::string& where) {
	if(!value.is_number_integer() || value.get<std::int64_t>() < 0) malformed(where, "expected a whole number from 0");
	return value.get<std::size_t>();
}

/// @brief Reads a number.
/// @param value The value.
/// @param where Its place.
/// @return The number.
double numberOf(const json& value, const std::string& where) {
	if(!value.is_number()) malformed(where, "expected a number");
	return value.get<double>();
}

/// @brief The characters of a UTF-8 text (its bytes that don't continue a character).
/// @param text The text.
/// @return The count.
std::size_t characters(std::string_view text) noexcept {
	return static_cast<std::size_t>(
	    std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0u) != 0x80u; }));
}

/// @brief Whether a text is a calendar date, "YYYY-MM-DD", from year 1 to 9999.
/// @param text The text.
/// @return true if it is one.
bool isDate(std::string_view text) noexcept {
	if(text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
	for(const std::size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u})
		if(text[i] < '0' || text[i] > '9') return false;
	const auto digits = [&](std::size_t from, std::size_t count) {
		int value = 0;
		for(std::size_t i = from; i < from + count; ++i) value = value * 10 + (text[i] - '0');
		return value;
	};
	const int year = digits(0, 4), month = digits(5, 2), day = digits(8, 2);
	if(year < 1 || month < 1 || month > 12 || day < 1) return false;
	static constexpr std::array<int, 12> lengths = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
	return day <= lengths[static_cast<std::size_t>(month - 1)] + (month == 2 && leap ? 1 : 0);
}

/// @brief Reads the type-specific keys of a field.
/// @param field The field, its type set.
/// @param definition Its definition.
/// @param where Its place.
/// @param depth Objects and lists it is inside.
/// @param count Fields read so far.
void parseTypeKeys(Field& field, const json& definition, const std::string& where, int depth, std::size_t& count) {
	if(const auto values = definition.find("values"); values != definition.end()) {
		if(field.type != FieldType::Enum) malformed(where + ".values", "only an enum has values");
		if(!values->is_array() || values->empty() || values->size() > Limits::enumValues)
			malformed(where + ".values", std::format("expected a list of 1 to {} texts", Limits::enumValues));
		for(const json& value : *values) {
			if(!value.is_string()) malformed(where + ".values", "expected texts");
			if(std::ranges::find(field.values, value.get<std::string>()) != field.values.end())
				malformed(where + ".values", "'" + value.get<std::string>() + "' is there twice");
			field.values.push_back(value.get<std::string>());
		}
	} else if(field.type == FieldType::Enum) {
		malformed(where, "an enum needs \"values\"");
	}
	if(const auto to = definition.find("to"); to != definition.end()) {
		if(field.type != FieldType::Reference) malformed(where + ".to", "only a reference has \"to\"");
		if(!to->is_string() || !isName(to->get<std::string>()))
			malformed(where + ".to", "expected a collection's name");
		field.to = to->get<std::string>();
	} else if(field.type == FieldType::Reference) {
		malformed(where, "a reference needs \"to\": the collection it refers to");
	}
	if(const auto onDelete = definition.find("onDelete"); onDelete != definition.end()) {
		if(field.type != FieldType::Reference) malformed(where + ".onDelete", "only a reference has \"onDelete\"");
		if(*onDelete != "restrict" && *onDelete != "nullify")
			malformed(where + ".onDelete", "expected \"restrict\" or \"nullify\"");
		field.onDelete = onDelete->get<std::string>();
	}
	if(const auto of = definition.find("of"); of != definition.end()) {
		if(field.type != FieldType::List) malformed(where + ".of", "only a list has \"of\"");
		Field item = parseField(*of, where + ".of", depth + 1, count);
		if(item.required || item.unique || item.defaultValue)
			malformed(where + ".of", "a list's items take no "
			                         "required, unique or default");
		field.of = std::make_shared<const Field>(std::move(item));
	} else if(field.type == FieldType::List) {
		malformed(where, "a list needs \"of\": its items' type");
	}
	if(const auto fields = definition.find("fields"); fields != definition.end()) {
		if(field.type != FieldType::Object) malformed(where + ".fields", "only an object has \"fields\"");
		if(!fields->is_object()) malformed(where + ".fields", "expected an object: name -> field");
		for(const auto& [name, inner] : fields->items()) {
			if(!isName(name) || isSystemField(name)) malformed(where + ".fields", "'" + name + "' can't name a field");
			field.fields.emplace(name, parseField(inner, where + ".fields." + name, depth + 1, count));
			field.order.push_back(name);
		}
	} else if(field.type == FieldType::Object) {
		malformed(where, "an object needs \"fields\"");
	}
}

/// @brief Reads the bounds of a field.
/// @param field The field, its type set.
/// @param definition Its definition.
/// @param where Its place.
void parseBounds(Field& field, const json& definition, const std::string& where) {
	const bool numeric =
	    field.type == FieldType::Number || field.type == FieldType::Integer || field.type == FieldType::DateTime;
	for(const char* key : {"minimum", "maximum"})
		if(definition.contains(key)) {
			if(!numeric) malformed(where + "." + key, "only a number has a " + std::string(key));
			(key[1] == 'i' ? field.minimum : field.maximum) = numberOf(definition[key], where + "." + key);
		}
	for(const char* key : {"minLength", "maxLength"})
		if(definition.contains(key)) {
			if(field.type != FieldType::String) malformed(where + "." + key, "only a text has a " + std::string(key));
			(key[1] == 'i' ? field.minLength : field.maxLength) = countOf(definition[key], where + "." + key);
		}
	for(const char* key : {"minItems", "maxItems"})
		if(definition.contains(key)) {
			if(field.type != FieldType::List) malformed(where + "." + key, "only a list has " + std::string(key));
			(key[1] == 'i' ? field.minItems : field.maxItems) = countOf(definition[key], where + "." + key);
		}
	if(field.minimum && field.maximum && *field.minimum > *field.maximum) malformed(where, "minimum is above maximum");
	if(field.minLength && field.maxLength && *field.minLength > *field.maxLength)
		malformed(where, "minLength is above maxLength");
	if(field.minItems && field.maxItems && *field.minItems > *field.maxItems)
		malformed(where, "minItems is above maxItems");
}

/// @brief Reads a model's "importFrom".
/// @param value The value: "state.habits" or {"path": "state.habits", "with": "{{...}}"}.
/// @param where Its place.
/// @return The import.
Import parseImport(const json& value, const std::string& where) {
	Import import;
	const json& path = value.is_object() ? value.value("path", json()) : value;
	if(!path.is_string() || !path.get<std::string>().starts_with("state.") || path.get<std::string>().size() < 7)
		malformed(where, "expected a state path, \"state.habits\", or {\"path\", \"with\"}");
	import.path = path.get<std::string>();
	if(value.is_object()) {
		for(const auto& [key, inner] : value.items())
			if(key != "path" && key != "with" && key != "note") malformed(where + "." + key, "unknown key");
		if(value.contains("with")) {
			if(!value["with"].is_string()) malformed(where + ".with", "expected an expression, \"{{...}}\"");
			import.expression = value["with"].get<std::string>();
		}
	}
	return import;
}

/// @brief Reads one model.
/// @param name The collection.
/// @param definition Its definition.
/// @param where Its place.
/// @return The model.
Model parseModel(const std::string& name, const json& definition, const std::string& where) {
	if(!definition.is_object()) malformed(where, "expected an object: {\"version\", \"fields\", ...}");
	for(const auto& [key, value] : definition.items())
		if(std::ranges::find(modelKeys, key) == modelKeys.end())
			malformed(where + "." + key, "unknown key (a model has version, fields, seed, migrations, importFrom, "
			                             "title, description, note)");
	Model model;
	model.name = name;
	model.written = definition;
	if(definition.contains("version")) {
		const json& version = definition["version"];
		if(!version.is_number_integer() || version.get<std::int64_t>() < 1 || version.get<std::int64_t>() > 1000000)
			malformed(where + ".version", "expected a whole number from 1");
		model.version = version.get<int>();
	}
	const auto fields = definition.find("fields");
	if(fields == definition.end() || !fields->is_object()) malformed(where, "expected \"fields\": name -> field");
	std::size_t count = 0;
	for(const auto& [field, inner] : fields->items()) {
		if(!isName(field) || isSystemField(field))
			malformed(where + ".fields", "'" + field +
			                                 "' can't name a field (letters, digits and _; not id, created "
			                                 "or updated, which every record has)");
		Field read = parseField(inner, where + ".fields." + field, 0, count);
		if(read.unique && (read.type == FieldType::List || read.type == FieldType::Object))
			malformed(where + ".fields." + field + ".unique", "only a single value can be unique");
		model.fields.emplace(field, std::move(read));
		model.order.push_back(field);
	}
	if(definition.contains("seed")) {
		model.seed = definition["seed"];
		if(!model.seed.is_array() || model.seed.size() > Limits::seedRecords)
			malformed(where + ".seed", std::format("expected a list of at most {} records", Limits::seedRecords));
		for(std::size_t i = 0; i < model.seed.size(); ++i)
			if(!model.seed[i].is_object()) malformed(std::format("{}.seed[{}]", where, i), "expected a record");
	}
	if(definition.contains("migrations")) {
		model.migrations = definition["migrations"];
		if(!model.migrations.is_array() || model.migrations.size() > Limits::migrations)
			malformed(where + ".migrations",
			          std::format("expected a list of at most {} migrations", Limits::migrations));
	}
	if(definition.contains("importFrom"))
		model.importFrom = parseImport(definition["importFrom"], where + ".importFrom");
	return model;
}

/// @brief Checks the items of a list value.
/// @param field The list field.
/// @param value The list.
/// @param where Its place.
void validateList(const Field& field, const json& value, const std::string& where) {
	if(!value.is_array()) invalid(where, "expected a list, got " + std::string(value.type_name()));
	if(field.minItems && value.size() < *field.minItems)
		invalid(where, std::format("expected at least {} items, got {}", *field.minItems, value.size()));
	if(field.maxItems && value.size() > *field.maxItems)
		invalid(where, std::format("expected at most {} items, got {}", *field.maxItems, value.size()));
	for(std::size_t i = 0; i < value.size(); ++i) {
		if(value[i].is_null()) invalid(std::format("{}[{}]", where, i), "a list's item can't be null");
		validateValue(*field.of, value[i], std::format("{}[{}]", where, i));
	}
}

/// @brief Checks an object value's fields.
/// @param field The object field.
/// @param value The object.
/// @param where Its place.
void validateObject(const Field& field, const json& value, const std::string& where) {
	if(!value.is_object()) invalid(where, "expected an object, got " + std::string(value.type_name()));
	for(const auto& [name, inner] : value.items())
		if(!field.fields.contains(name)) invalid(where + "." + name, "no such field");
	for(const auto& [name, inner] : field.fields) {
		const auto it = value.find(name);
		if(it == value.end() || it->is_null()) {
			if(inner.required) invalid(where + "." + name, "required");
			continue;
		}
		validateValue(inner, *it, where + "." + name);
	}
}

/// @brief Checks a number against a field.
/// @param field The field.
/// @param value The value.
/// @param where Its place.
void validateNumber(const Field& field, const json& value, const std::string& where) {
	const bool whole = field.type != FieldType::Number;
	if(!value.is_number() ||
	   (whole && !value.is_number_integer() &&
	    (std::trunc(value.get<double>()) != value.get<double>() || std::abs(value.get<double>()) > 9007199254740992.0)))
		invalid(where, std::string("expected ") + (whole ? "an integer" : "a number") + ", got " + value.dump());
	const double number = value.get<double>();
	if(!std::isfinite(number)) invalid(where, "expected a finite number");
	if(field.minimum && number < *field.minimum) invalid(where, std::format("expected at least {}", *field.minimum));
	if(field.maximum && number > *field.maximum) invalid(where, std::format("expected at most {}", *field.maximum));
}

/// @brief Checks that a field's default fits it.
/// @param field The field.
/// @param where Its place.
void checkDefault(const Field& field, const std::string& where) {
	if(!field.defaultValue) return;
	try {
		validateValue(field, *field.defaultValue, where + ".default");
	} catch(const DataError& error) {
		malformed(where + ".default", std::string("doesn't fit the field (") + error.what() + ")");
	}
}

} // namespace

bool isSystemField(std::string_view name) noexcept { return name == "id" || name == "created" || name == "updated"; }

bool isName(std::string_view name) noexcept {
	if(name.empty() || name.size() > Limits::nameBytes) return false;
	const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
	if(!letter(name.front())) return false;
	return std::ranges::all_of(name, [&](char c) { return letter(c) || (c >= '0' && c <= '9') || c == '_'; });
}

bool isRecordId(std::string_view id) noexcept {
	if(id.empty() || id.size() > Limits::nameBytes) return false;
	return std::ranges::all_of(id, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
	});
}

std::string_view typeName(FieldType type) noexcept {
	for(const auto& [name, value] : typeNames)
		if(value == type) return name;
	return "string";
}

Field parseField(const json& definition, const std::string& where, int depth, std::size_t& count) {
	if(depth > Limits::depth) malformed(where, std::format("fields nest at most {} deep", Limits::depth));
	if(++count > Limits::fields) malformed(where, std::format("a model has at most {} fields", Limits::fields));
	if(!definition.is_object()) malformed(where, "expected a field: {\"type\": \"string\", ...}");
	for(const auto& [key, value] : definition.items())
		if(std::ranges::find(fieldKeys, key) == fieldKeys.end()) malformed(where + "." + key, "unknown key");
	Field field;
	field.written = definition;
	const auto type = definition.find("type");
	if(type == definition.end() || !type->is_string()) malformed(where, "expected \"type\"");
	const auto named =
	    std::ranges::find(typeNames, type->get<std::string>(), &std::pair<std::string_view, FieldType>::first);
	if(named == typeNames.end())
		malformed(where + ".type", "unknown type '" + type->get<std::string>() +
		                               "' (string, number, integer, boolean, date, dateTime, enum, reference, list, "
		                               "object)");
	field.type = named->second;
	if(definition.contains("required")) field.required = expect::boolean(definition["required"], where + ".required");
	if(definition.contains("unique")) field.unique = expect::boolean(definition["unique"], where + ".unique");
	parseTypeKeys(field, definition, where, depth, count);
	parseBounds(field, definition, where);
	if(definition.contains("default") && !definition["default"].is_null()) field.defaultValue = definition["default"];
	checkDefault(field, where);
	return field;
}

Models parseModels(const json& section, const std::string& where) {
	Models models;
	if(section.is_null()) return models;
	if(!section.is_object()) malformed(where, "expected an object: collection -> model");
	if(section.size() > Limits::collections)
		malformed(where, std::format("at most {} collections", Limits::collections));
	for(const auto& [name, definition] : section.items()) {
		if(!isName(name))
			malformed(where + "." + name, "can't name a collection (a letter, then letters, digits and _)");
		models.emplace(name, parseModel(name, definition, where + "." + name));
	}
	for(const auto& [name, model] : models) // references reach declared collections
		for(const auto& [field, definition] : model.fields) {
			const Field* reference = &definition;
			while(reference->type == FieldType::List) reference = reference->of.get();
			if(reference->type == FieldType::Reference && !models.contains(reference->to))
				malformed(where + "." + name + ".fields." + field + ".to",
				          "no collection '" + reference->to + "' in \"models\"");
		}
	return models;
}

void validateValue(const Field& field, const json& value, const std::string& where) {
	switch(field.type) {
	case FieldType::String:
		if(!value.is_string()) invalid(where, "expected a text, got " + std::string(value.type_name()));
		if(const std::size_t length = characters(value.get_ref<const std::string&>());
		   (field.minLength && length < *field.minLength) || (field.maxLength && length > *field.maxLength))
			invalid(where, std::format("expected {} to {} characters, got {}", field.minLength.value_or(0),
			                           field.maxLength ? std::to_string(*field.maxLength) : "any", length));
		return;
	case FieldType::Number:
	case FieldType::Integer:
	case FieldType::DateTime:
		validateNumber(field, value, where);
		return;
	case FieldType::Boolean:
		if(!value.is_boolean()) invalid(where, "expected true or false, got " + value.dump());
		return;
	case FieldType::Date:
		if(!value.is_string() || !isDate(value.get_ref<const std::string&>()))
			invalid(where, "expected a date, \"YYYY-MM-DD\", got " + value.dump());
		return;
	case FieldType::Enum:
		if(!value.is_string() || std::ranges::find(field.values, value.get<std::string>()) == field.values.end())
			invalid(where, "expected one of the enum's values, got " + value.dump());
		return;
	case FieldType::Reference:
		if(!value.is_string() || !isRecordId(value.get_ref<const std::string&>()))
			invalid(where, "expected the id of a record of '" + field.to + "', got " + value.dump());
		return;
	case FieldType::List:
		validateList(field, value, where);
		return;
	case FieldType::Object:
		validateObject(field, value, where);
		return;
	}
}

json validateRecord(const Model& model, const json& fields, const std::string& where) {
	if(!fields.is_object()) invalid(where, "expected a record: an object of fields");
	json stored = json::object();
	for(const auto& [name, value] : fields.items()) {
		if(isSystemField(name)) continue; // the store's to give
		if(!model.fields.contains(name))
			invalid(where + "." + name, "'" + model.name + "' has no field '" + name + "'");
	}
	for(const std::string& name : model.order) {
		const Field& field = model.fields.at(name);
		const auto it = fields.find(name);
		const json* value = it == fields.end() || it->is_null() ? nullptr : &*it;
		if(!value && field.defaultValue) value = &*field.defaultValue;
		if(!value) {
			if(field.required) invalid(where + "." + name, "required");
			continue;
		}
		validateValue(field, *value, where + "." + name);
		stored[name] = *value;
	}
	if(stored.dump().size() > Limits::recordBytes) throw DataError("limit", where + ": a record holds at most 1 MiB");
	return stored;
}

json schemaOf(const Model& model) {
	const auto typeOf = [](const auto& self, const Field& field) -> json {
		switch(field.type) {
		case FieldType::String:
		case FieldType::Date:
		case FieldType::Reference:
			return {{"type", "string"}};
		case FieldType::Number:
			return {{"type", "number"}};
		case FieldType::Integer:
		case FieldType::DateTime:
			return {{"type", "integer"}};
		case FieldType::Boolean:
			return {{"type", "boolean"}};
		case FieldType::Enum:
			return {{"enum", field.values}};
		case FieldType::List:
			return {{"type", "array"}, {"items", self(self, *field.of)}};
		case FieldType::Object: {
			json properties = json::object();
			for(const std::string& name : field.order) properties[name] = self(self, field.fields.at(name));
			return {{"type", "object"}, {"properties", properties}, {"additionalProperties", false}};
		}
		}
		return json(true);
	};
	json properties = {
	    {"id", {{"type", "string"}}}, {"created", {{"type", "integer"}}}, {"updated", {{"type", "integer"}}}};
	for(const std::string& name : model.order) properties[name] = typeOf(typeOf, model.fields.at(name));
	return {{"type", "object"}, {"properties", properties}, {"additionalProperties", false}};
}

} // namespace affresco::data
