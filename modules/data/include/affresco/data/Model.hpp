#pragma once

/// @file
/// @brief data::Model: the schema layer: collections of records with typed fields, read from a screen's "models"
/// section, and the check every record passes before it is stored.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::data {

/// @brief A write or a model the store refuses. what() names the place; code() says what kind of refusal it is.
class DataError : public DocumentError {
public:
	/// @brief An error.
	/// @param code "invalid", "notFound", "unique", "reference", "limit", "model", "migration", "readOnly" or "io".
	/// @param message The message, with its place.
	DataError(std::string code, const std::string& message) : DocumentError(message), code_(std::move(code)) {}
	/// @brief What kind of refusal it is.
	/// @return The code.
	[[nodiscard]] const std::string& code() const noexcept { return code_; }

private:
	std::string code_; ///< See code().
};

/// @brief The bounds of models, records, queries and the database (SECURITY.md, "Limits").
struct Limits {
	static constexpr std::size_t collections = 256;          ///< Collections per screen.
	static constexpr std::size_t fields = 256;               ///< Fields per model, nested ones included.
	static constexpr int depth = 8;                          ///< Objects and lists inside fields.
	static constexpr std::size_t enumValues = 1024;          ///< Values of one enum.
	static constexpr std::size_t nameBytes = 64;             ///< A collection's, a field's or an id's name.
	static constexpr std::size_t recordBytes = 1u << 20;     ///< One record, as JSON: 1 MiB.
	static constexpr std::size_t records = 1000000;          ///< Records per collection.
	static constexpr std::size_t seedRecords = 10000;        ///< Records one model's seed or import brings.
	static constexpr std::size_t queryResults = 10000;       ///< Records one query gives at most.
	static constexpr std::size_t conditions = 64;            ///< Conditions of one query.
	static constexpr std::size_t migrations = 256;           ///< Migrations one model keeps.
	static constexpr std::size_t steps = 64;                 ///< Steps of one migration.
	static constexpr std::int64_t databaseBytes = 1ll << 30; ///< The database file: 1 GiB.
	static constexpr std::size_t journalEntries = 100000;    ///< Journal entries kept (the oldest go first).
	static constexpr std::size_t csvBytes = 16u << 20;       ///< A CSV file imported: 16 MiB.
	static constexpr std::size_t csvRows = 100000;           ///< Its rows.
	static constexpr std::size_t csvColumns = 256;           ///< Its columns.
	static constexpr std::size_t failuresReported = 100;     ///< Failed records a report names one by one.
};

/// @brief The kinds of field.
enum class FieldType {
	String,    ///< Text: "minLength", "maxLength" (characters).
	Number,    ///< A number: "minimum", "maximum".
	Integer,   ///< A whole number: "minimum", "maximum".
	Boolean,   ///< true or false.
	Date,      ///< A calendar date, "YYYY-MM-DD" (SCREEN_FORMAT.md, "Dates").
	DateTime,  ///< A moment: milliseconds since 1970-01-01 UTC, as now() gives.
	Enum,      ///< One of "values" (texts).
	Reference, ///< The id of a record of the collection "to".
	List,      ///< A list of "of" values: "minItems", "maxItems".
	Object,    ///< An object with typed "fields".
	Map        ///< An object of any keys (texts, at most 256 bytes), each value an "of": "maxItems" keys at most.
};

/// @brief One field of a model, or the type of a list's items.
struct Field {
	FieldType type = FieldType::String;   ///< Its kind.
	bool required = false;                ///< Every record has it (not null).
	bool unique = false;                  ///< No two records have the same value (top-level scalars only).
	std::optional<json> defaultValue;     ///< Given to a new record that doesn't have it.
	std::optional<double> minimum;        ///< Numbers: the least value.
	std::optional<double> maximum;        ///< Numbers: the greatest value.
	std::optional<std::size_t> minLength; ///< Texts: the fewest characters.
	std::optional<std::size_t> maxLength; ///< Texts: the most characters.
	std::optional<std::size_t> minItems;  ///< Lists: the fewest items.
	std::optional<std::size_t> maxItems;  ///< Lists: the most items.
	std::vector<std::string> values;      ///< Enums: the values it takes, in order.
	std::string to;                       ///< References: the collection.
	std::string onDelete = "restrict";    ///< References: "restrict" (refuse) or "nullify" the field.
	std::shared_ptr<const Field> of;      ///< Lists: the items' type.
	std::map<std::string, Field> fields;  ///< Objects: their fields, by name.
	std::vector<std::string> order;       ///< Objects: their fields' names, as written.
	json written;                         ///< The definition as the document writes it.
};

/// @brief How a record that isn't in the store yet is brought in from a screen's old "persist" state, once.
struct Import {
	std::string path;       ///< The state path, "state.habits"; its list's items become records.
	std::string expression; ///< Maps each item (`it`, `index`) to a record; empty: as it is.
};

/// @brief A collection's model: its fields and version, and the records it starts with.
struct Model {
	std::string name;                    ///< The collection.
	int version = 1;                     ///< Raised by each change that isn't compatible (Changes.hpp).
	std::map<std::string, Field> fields; ///< By name.
	std::vector<std::string> order;      ///< Their names, as written.
	json seed = json::array();           ///< Records stored when the collection is first created.
	json migrations = json::array();     ///< Its migrations, oldest first (Migration.hpp).
	std::optional<Import> importFrom;    ///< Records brought in from an old state file, once.
	json written;                        ///< The model as the document writes it.
};

/// @brief The models of a screen, by collection.
using Models = std::map<std::string, Model, std::less<>>;

/// @brief The fields every record has, which no model declares: "id", "created" and "updated".
/// @param name A name.
/// @return true for one of them.
[[nodiscard]] bool isSystemField(std::string_view name) noexcept;

/// @brief Whether a name can be a collection's or a field's: a letter, then letters, digits or "_", at most 64.
/// @param name The name.
/// @return true if it can.
[[nodiscard]] bool isName(std::string_view name) noexcept;

/// @brief Whether a text can be a record's id: letters, digits, "-" and "_", 1 to 64 of them.
/// @param id The text.
/// @return true if it can.
[[nodiscard]] bool isRecordId(std::string_view id) noexcept;

/// @brief Reads a screen's "models" section.
/// @param section The section (null: no models).
/// @param where Its place, for errors: "models".
/// @return The models.
/// @throws DocumentError If a model is malformed; what() names the place: "models.songs.fields.genre.values: ...".
[[nodiscard]] Models parseModels(const json& section, const std::string& where = "models");

/// @brief Reads one field's definition.
/// @param definition The definition.
/// @param where Its place, for errors.
/// @param depth Objects and lists it is inside.
/// @param count Fields read so far in the model, raised by those inside this one.
/// @return The field.
/// @throws DocumentError If it is malformed.
[[nodiscard]] Field parseField(const json& definition, const std::string& where, int depth, std::size_t& count);

/// @brief Checks a record's fields against its model, and gives the record as it is stored: the defaults of the
/// fields it doesn't give added. Undeclared fields are refused; references and uniqueness are the store's to check.
/// @param model The model.
/// @param fields The record's fields (no "id", "created" or "updated").
/// @param where Its place, for errors: "songs[17]".
/// @return The fields, with defaults.
/// @throws DataError "invalid" naming the field: "songs[17].duration: expected an integer, got 1.5".
[[nodiscard]] json validateRecord(const Model& model, const json& fields, const std::string& where);

/// @brief Checks one value against a field.
/// @param field The field.
/// @param value The value (not null).
/// @param where Its place, for errors.
/// @throws DataError "invalid".
void validateValue(const Field& field, const json& value, const std::string& where);

/// @brief What the fields of a model's records look like, as a typed-state schema (SCREEN_FORMAT.md, "Typed state"),
/// "id", "created" and "updated" included: what the checker and agents read.
/// @param model The model.
/// @return `{"type": "object", "properties": {...}, "additionalProperties": false}`.
[[nodiscard]] json schemaOf(const Model& model);

/// @brief The name of a field type, as the document writes it.
/// @param type The type.
/// @return "string", "dateTime", ...
[[nodiscard]] std::string_view typeName(FieldType type) noexcept;

} // namespace affresco::data
