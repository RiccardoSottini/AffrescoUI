#pragma once

/// @file
/// @brief format::Models: the application's data models a document shows (`"repeat": {"in": "models.files"}`), and
/// where their rows lie for fine-grained updates.

#include "affresco/core/DataModel.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::format {

struct Engine;

/// Where the rows of data models lie among the places values read (Reads.hpp): "/~m/<name>/<row>". "~m" is no key of
/// a JSON Pointer ("~" escapes only "0" and "1"), so it is never a place of the state.
inline constexpr std::string_view modelPlaces = "/~m";

/// @brief The data model a repeat's "in" names.
/// @param path "in" as written, "models.files", or as compiled, "{{models.files}}".
/// @return "files", or std::nullopt if it names none.
[[nodiscard]] std::optional<std::string_view> modelName(std::string_view path) noexcept;

/// @brief The data models registered (Runtime::setModel()).
///
/// A row change becomes a change of the row's place, so an update evaluates again only what reads that row; an
/// insertion, a removal or a reset becomes a change of the whole model, so the repeats over it build their items
/// again. Invariants: main thread only; every connection to a model ends with its registration.
class Models {
public:
	/// @brief No model yet.
	/// @param engine Marks the changes and asks for updates.
	explicit Models(Engine& engine) noexcept : engine_(engine) {}
	~Models();

	Models(const Models&) = delete;
	Models& operator=(const Models&) = delete;
	Models(Models&&) = delete;
	Models& operator=(Models&&) = delete;

	/// @brief Registers, replaces or removes a model; what shows one is updated.
	/// @param name Its name: letters, digits and "_", not starting with a digit.
	/// @param model The model, or nullptr to remove it.
	/// @throws std::invalid_argument If the name isn't one.
	void set(std::string name, std::shared_ptr<DataModel> model);

	/// @brief Looks a model up.
	/// @param name Its name.
	/// @return It, or nullptr.
	[[nodiscard]] const DataModel* find(std::string_view name) const;

	/// @brief The place of a model, "/~m/files".
	/// @param name Its name.
	/// @return The place.
	[[nodiscard]] static std::string placeOf(std::string_view name);

	/// @brief The value at a place inside a model's row, read now: "/~m/files/3/name".
	/// @param place The place.
	/// @return The value, or null if there is none.
	[[nodiscard]] json at(std::string_view place) const;

	/// @brief What agents see of the models: for each, its rows' count, the shape of a row and a sample row, never
	/// every row.
	/// @return `{"files": {"count": 1000000, "shape": {"name": "string"}, "sample": {...}}}`.
	[[nodiscard]] json outline() const;

	/// @brief The names registered, for an error's hint.
	/// @return E.g. "files, logs", or "none".
	[[nodiscard]] std::string names() const;

private:
	/// @brief Marks what a model's change changed and asks for an update.
	/// @param name The model.
	/// @param change The change.
	void changed(const std::string& name, const ModelChange& change);

	/// @brief A registered model.
	struct Entry {
		std::shared_ptr<DataModel> model; ///< The model.
		Connection connection = 0;        ///< To its onChange.
	};
	Engine& engine_;                                   ///< Marks changes and asks for updates.
	std::map<std::string, Entry, std::less<>> models_; ///< By name.
};

} // namespace affresco::format
