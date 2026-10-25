/// @file
/// @brief Reading models and checking records against them (Model.hpp).

#include "affresco/data/Model.hpp"
#include "Places.hpp"

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
constexpr std::array<std::pair<std::string_view, FieldType>, 11> typeNames = {{{"string", FieldType::String},
                                                                               {"number", FieldType::Number},
                                                                               {"integer", FieldType::Integer},
                                                                               {"boolean", FieldType::Boolean},
                                                                               {"date", FieldType::Date},
                                                                               {"dateTime", FieldType::DateTime},
                                                                               {"enum", FieldType::Enum},
                                                                               {"reference", FieldType::Reference},
                                                                               {"list", FieldType::List},
                                                                               {"object", FieldType::Object},
                                                                               {"map", FieldType::Map}}};

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
	if(!std::ranges::all_of(std::array<std::size_t, 8>{0, 1, 2, 3, 5, 6, 8, 9},
	                        [&](std::size_t i) { return text[i] >= '0' && text[i] <= '9'; }))
		return false;
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

/// @brief Fails when a key a field's definition has belongs to other types, or a key its type needs is missing.
/// @param has Whether the definition has the key.
/// @param belongs Whether the field's type takes it.
/// @param needed Whether the field's type needs it.
/// @param key The key.
/// @param where The field's place.
/// @param needs What to say when it is missing.
void expectKey(bool has, bool belongs, bool needed, const char* key, const std::string& where, const char* needs) {
	if(has && !belongs) malformed(placeOf(where, key), std::string(R"(this type takes no ")") + key + R"(")");
	if(!has && needed) malformed(where, needs);
}

/// @brief Reads an enum's "values".
/// @param field The field.
/// @param values The values.
/// @param where Its place.
void parseValues(Field& field, const json& values, const std::string& where) {
	if(!values.is_array() || values.empty() || values.size() > Limits::enumValues)
		malformed(placeOf(where, "values"), std::format("expected a list of 1 to {} texts", Limits::enumValues));
	for(const json& value : values) {
		if(!value.is_string()) malformed(placeOf(where, "values"), "expected texts");
		if(std::ranges::find(field.values, value.get<std::string>()) != field.values.end())
			malformed(placeOf(where, "values"), "'" + value.get<std::string>() + "' is there twice");
		field.values.push_back(value.get<std::string>());
	}
}

/// @brief Reads a reference's "to" and "onDelete".
/// @param field The field.
/// @param definition Its definition.
/// @param where Its place.
void parseReference(Field& field, const json& definition, const std::string& where) {
	const json& to = definition["to"];
	if(!to.is_string() || !isName(to.get<std::string>()))
		malformed(placeOf(where, "to"), "expected a collection's name");
	field.to = to.get<std::string>();
	if(const auto onDelete = definition.find("onDelete"); onDelete != definition.end()) {
		if(*onDelete != "restrict" && *onDelete != "nullify")
			malformed(placeOf(where, "onDelete"), R"(expected "restrict" or "nullify")");
		field.onDelete = onDelete->get<std::string>();
	}
}

/// @brief Reads an object's "fields".
/// @param field The field.
/// @param fields Its fields' definitions.
/// @param where Its place.
/// @param depth Objects and lists it is inside.
/// @param count Fields read so far.
void parseObjectFields(Field& field, const json& fields, const std::string& where, int depth, std::size_t& count) {
	if(!fields.is_object()) malformed(placeOf(where, "fields"), "expected an object: name -> field");
	for(const auto& [name, inner] : fields.items()) {
		if(!isName(name) || isSystemField(name))
			malformed(placeOf(where, "fields"), "'" + name + "' can't name a field");
		field.fields.emplace(name, parseField(inner, placeOf(where, {"fields", name}), depth + 1, count));
		field.order.push_back(name);
	}
}

/// @brief Reads the type-specific keys of a field: an enum's values, a reference's target, a list's or a map's items,
/// an object's fields.
/// @param field The field, its type set.
/// @param definition Its definition.
/// @param where Its place.
/// @param depth Objects and lists it is inside.
/// @param count Fields read so far.
void parseTypeKeys(Field& field, const json& definition, const std::string& where, int depth, std::size_t& count) {
	const FieldType type = field.type;
	const bool items = type == FieldType::List || type == FieldType::Map;
	expectKey(definition.contains("values"), type == FieldType::Enum, type == FieldType::Enum, "values", where,
	          R"(an enum needs "values")");
	expectKey(definition.contains("to"), type == FieldType::Reference, type == FieldType::Reference, "to", where,
	          R"(a reference needs "to": the collection it refers to)");
	expectKey(definition.contains("onDelete"), type == FieldType::Reference, false, "onDelete", where, "");
	expectKey(definition.contains("of"), items, items, "of", where, R"(a list or a map needs "of": its items' type)");
	expectKey(definition.contains("fields"), type == FieldType::Object, type == FieldType::Object, "fields", where,
	          R"(an object needs "fields")");
	if(type == FieldType::Enum) parseValues(field, definition["values"], where);
	if(type == FieldType::Reference) parseReference(field, definition, where);
	if(type == FieldType::Object) parseObjectFields(field, definition["fields"], where, depth, count);
	if(!items) return;
	Field item = parseField(definition["of"], placeOf(where, "of"), depth + 1, count);
	if(item.required || item.unique || item.defaultValue)
		malformed(placeOf(where, "of"), "items take no required, unique or default");
	field.of = std::make_shared<const Field>(std::move(item));
}

/// @brief Reads one pair of bounds of a field, e.g. minLength and maxLength.
/// @param definition The field's definition.
/// @param keys The two keys, the lower first.
/// @param takes Whether the field's type takes each.
/// @param where The field's place.
/// @param read Reads one value.
/// @param lower Gets the lower bound.
/// @param upper Gets the upper bound.
template <typename T, typename Read>
void parsePair(const json& definition, std::array<const char*, 2> keys, std::array<bool, 2> takes,
               const std::string& where, Read read, std::optional<T>& lower, std::optional<T>& upper) {
	for(std::size_t i = 0; i < 2; ++i) {
		if(!definition.contains(keys[i])) continue;
		if(!takes[i]) malformed(placeOf(where, keys[i]), std::string(R"(this type takes no ")") + keys[i] + R"(")");
		(i == 0 ? lower : upper) = read(definition[keys[i]], placeOf(where, keys[i]));
	}
	if(lower && upper && *lower > *upper) malformed(where, std::string(keys[0]) + " is above " + keys[1]);
}

/// @brief Reads the bounds of a field.
/// @param field The field, its type set.
/// @param definition Its definition.
/// @param where Its place.
void parseBounds(Field& field, const json& definition, const std::string& where) {
	const FieldType type = field.type;
	const bool numeric = type == FieldType::Number || type == FieldType::Integer || type == FieldType::DateTime;
	const bool text = type == FieldType::String, list = type == FieldType::List;
	parsePair<double>(definition, {"minimum", "maximum"}, {numeric, numeric}, where, numberOf, field.minimum,
	                  field.maximum);
	parsePair<std::size_t>(definition, {"minLength", "maxLength"}, {text, text}, where, countOf, field.minLength,
	                       field.maxLength);
	parsePair<std::size_t>(definition, {"minItems", "maxItems"}, {list, list || type == FieldType::Map}, where, countOf,
	                       field.minItems, field.maxItems);
}

/// @brief Reads a model's "importFrom".
/// @param value The value: "state.habits" or {"path": "state.habits", "with": "{{...}}"}.
/// @param where Its place.
/// @return The import.
Import parseImport(const json& value, const std::string& where) {
	Import import;
	const json& path = value.is_object() ? value.value("path", json()) : value;
	if(!path.is_string() || !path.get<std::string>().starts_with("state.") || path.get<std::string>().size() < 7)
		malformed(where, R"(expected a state path, "state.habits", or {"path", "with"})");
	import.path = path.get<std::string>();
	if(value.is_object()) {
		for(const auto& [key, inner] : value.items())
			if(key != "path" && key != "with" && key != "note") malformed(placeOf(where, key), "unknown key");
		if(value.contains("with")) {
			if(!value["with"].is_string()) malformed(placeOf(where, "with"), R"(expected an expression, "{{...}}")");
			import.expression = value["with"].get<std::string>();
		}
	}
	return import;
}

/// @brief Reads a model's fields.
/// @param model The model.
/// @param fields Their definitions.
/// @param where The model's place.
void parseModelFields(Model& model, const json& fields, const std::string& where) {
	if(!fields.is_object()) malformed(where, R"(expected "fields": name -> field)");
	std::size_t count = 0;
	for(const auto& [field, inner] : fields.items()) {
		if(!isName(field) || isSystemField(field))
			malformed(placeOf(where, "fields"), "'" + field +
			                                        "' can't name a field (letters, digits and _; not id, created "
			                                        "or updated, which every record has)");
		Field read = parseField(inner, placeOf(where, {"fields", field}), 0, count);
		const bool single =
		    read.type != FieldType::List && read.type != FieldType::Object && read.type != FieldType::Map;
		if(read.unique && !single)
			malformed(placeOf(where, {"fields", field, "unique"}), "only a single value can be unique");
		model.fields.emplace(field, std::move(read));
		model.order.push_back(field);
	}
}

/// @brief Reads a list of a model: its seed or its migrations.
/// @param value The list.
/// @param most How many it may have.
/// @param where Its place.
/// @param what What its items are, for the message.
/// @return It.
json listOf(const json& value, std::size_t most, const std::string& where, const char* what) {
	if(!value.is_array() || value.size() > most)
		malformed(where, std::format("expected a list of at most {} {}", most, what));
	return value;
}

/// @brief Reads one model.
/// @param name The collection.
/// @param definition Its definition.
/// @param where Its place.
/// @return The model.
Model parseModel(const std::string& name, const json& definition, const std::string& where) {
	if(!definition.is_object()) malformed(where, R"(expected an object: {"version", "fields", ...})");
	for(const auto& [key, value] : definition.items())
		if(std::ranges::find(modelKeys, key) == modelKeys.end())
			malformed(placeOf(where, key), "unknown key (a model has version, fields, seed, migrations, importFrom, "
			                               "title, description, note)");
	Model model;
	model.name = name;
	model.written = definition;
	if(const auto version = definition.find("version"); version != definition.end()) {
		if(!version->is_number_integer() || version->get<std::int64_t>() < 1 || version->get<std::int64_t>() > 1000000)
			malformed(placeOf(where, "version"), "expected a whole number from 1");
		model.version = version->get<int>();
	}
	parseModelFields(model, definition.value("fields", json()), where);
	if(definition.contains("seed"))
		model.seed = listOf(definition["seed"], Limits::seedRecords, placeOf(where, "seed"), "records");
	for(std::size_t i = 0; i < model.seed.size(); ++i)
		if(!model.seed[i].is_object()) malformed(std::format("{}.seed[{}]", where, i), "expected a record");
	if(definition.contains("migrations"))
		model.migrations =
		    listOf(definition["migrations"], Limits::migrations, placeOf(where, "migrations"), "migrations");
	if(definition.contains("importFrom"))
		model.importFrom = parseImport(definition["importFrom"], placeOf(where, "importFrom"));
	return model;
}

/// @brief Checks a text against a field: its length in characters.
/// @param field The field.
/// @param value The value.
/// @param where Its place.
void validateText(const Field& field, const json& value, const std::string& where) {
	if(!value.is_string()) invalid(where, "expected a text, got " + std::string(value.type_name()));
	const std::size_t length = characters(value.get_ref<const std::string&>());
	if((field.minLength && length < *field.minLength) || (field.maxLength && length > *field.maxLength))
		invalid(where, std::format("expected {} to {} characters, got {}", field.minLength.value_or(0),
		                           field.maxLength ? std::to_string(*field.maxLength) : "any", length));
}

/// @brief Checks a value of one word against a field: a boolean, a date, an enum's value or a record's id.
/// @param field The field.
/// @param value The value.
/// @param where Its place.
void validateWord(const Field& field, const json& value, const std::string& where) {
	const bool text = value.is_string();
	if(field.type == FieldType::Boolean && !value.is_boolean())
		invalid(where, "expected true or false, got " + value.dump());
	if(field.type == FieldType::Date && !(text && isDate(value.get_ref<const std::string&>())))
		invalid(where, R"(expected a date, "YYYY-MM-DD", got )" + value.dump());
	if(field.type == FieldType::Enum && !(text && std::ranges::contains(field.values, value.get<std::string>())))
		invalid(where, "expected one of the enum's values, got " + value.dump());
	if(field.type == FieldType::Reference && !(text && isRecordId(value.get_ref<const std::string&>())))
		invalid(where, "expected the id of a record of '" + field.to + "', got " + value.dump());
}

/// @brief Checks a map's keys and values against a field.
/// @param field The map field.
/// @param value The value.
/// @param where Its place.
void validateMap(const Field& field, const json& value, const std::string& where) {
	if(!value.is_object()) invalid(where, "expected an object (a map), got " + std::string(value.type_name()));
	const std::size_t most = field.maxItems.value_or(Limits::queryResults);
	if(value.size() > most) invalid(where, std::format("expected at most {} keys, got {}", most, value.size()));
	for(const auto& [key, inner] : value.items()) {
		if(key.empty() || key.size() > 256) invalid(where, "a map's key is 1 to 256 bytes");
		if(inner.is_null()) invalid(placeOf(where, key), "a map's value can't be null");
		validateValue(*field.of, inner, placeOf(where, key));
	}
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
		if(!field.fields.contains(name)) invalid(placeOf(where, name), "no such field");
	for(const auto& [name, inner] : field.fields) {
		const auto it = value.find(name);
		if(it == value.end() || it->is_null()) {
			if(inner.required) invalid(placeOf(where, name), "required");
			continue;
		}
		validateValue(inner, *it, placeOf(where, name));
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
		validateValue(field, *field.defaultValue, placeOf(where, "default"));
	} catch(const DataError& error) {
		malformed(placeOf(where, "default"), std::string("doesn't fit the field (") + error.what() + ")");
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
	if(!definition.is_object()) malformed(where, R"(expected a field: {"type": "string", ...})");
	for(const auto& [key, value] : definition.items())
		if(std::ranges::find(fieldKeys, key) == fieldKeys.end()) malformed(placeOf(where, key), "unknown key");
	Field field;
	field.written = definition;
	const auto type = definition.find("type");
	if(type == definition.end() || !type->is_string()) malformed(where, R"(expected "type")");
	const auto named =
	    std::ranges::find(typeNames, type->get<std::string>(), &std::pair<std::string_view, FieldType>::first);
	if(named == typeNames.end())
		malformed(placeOf(where, "type"),
		          "unknown type '" + type->get<std::string>() +
		              "' (string, number, integer, boolean, date, dateTime, enum, reference, list, "
		              "object, map)");
	field.type = named->second;
	if(definition.contains("required"))
		field.required = expect::boolean(definition["required"], placeOf(where, "required"));
	if(definition.contains("unique")) field.unique = expect::boolean(definition["unique"], placeOf(where, "unique"));
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
			malformed(placeOf(where, name), "can't name a collection (a letter, then letters, digits and _)");
		models.emplace(name, parseModel(name, definition, placeOf(where, name)));
	}
	for(const auto& [name, model] : models) // references reach declared collections
		for(const auto& [field, definition] : model.fields) {
			const Field* reference = &definition;
			while(reference->type == FieldType::List) reference = reference->of.get();
			if(reference->type == FieldType::Reference && !models.contains(reference->to))
				malformed(placeOf(where, {name, "fields", field, "to"}),
				          "no collection '" + reference->to + R"(' in "models")");
		}
	return models;
}

void validateValue(const Field& field, const json& value, const std::string& where) {
	switch(field.type) {
	case FieldType::String:
		return validateText(field, value, where);
	case FieldType::Number:
	case FieldType::Integer:
	case FieldType::DateTime:
		return validateNumber(field, value, where);
	case FieldType::Boolean:
	case FieldType::Date:
	case FieldType::Enum:
	case FieldType::Reference:
		return validateWord(field, value, where);
	case FieldType::List:
		return validateList(field, value, where);
	case FieldType::Object:
		return validateObject(field, value, where);
	case FieldType::Map:
		return validateMap(field, value, where);
	}
}

json validateRecord(const Model& model, const json& fields, const std::string& where) {
	if(!fields.is_object()) invalid(where, "expected a record: an object of fields");
	json stored = json::object();
	for(const auto& [name, value] : fields.items()) {
		if(isSystemField(name)) continue; // the store's to give
		if(!model.fields.contains(name))
			invalid(placeOf(where, name), "'" + model.name + "' has no field '" + name + "'");
	}
	for(const std::string& name : model.order) {
		const Field& field = model.fields.at(name);
		const auto it = fields.find(name);
		const json* value = it == fields.end() || it->is_null() ? nullptr : &*it;
		if(!value && field.defaultValue) value = &*field.defaultValue;
		if(!value) {
			if(field.required) invalid(placeOf(where, name), "required");
			continue;
		}
		validateValue(field, *value, placeOf(where, name));
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
		case FieldType::Map:
			return {{"type", "object"}, {"additionalProperties", self(self, *field.of)}};
		}
		return json(true);
	};
	json properties = {
	    {"id", {{"type", "string"}}}, {"created", {{"type", "integer"}}}, {"updated", {{"type", "integer"}}}};
	for(const std::string& name : model.order) properties[name] = typeOf(typeOf, model.fields.at(name));
	return {{"type", "object"}, {"properties", properties}, {"additionalProperties", false}};
}

} // namespace affresco::data
