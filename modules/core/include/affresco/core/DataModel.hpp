#pragma once

/// @file
/// @brief DataModel: rows the application owns and a document shows (`"repeat": {"in": "models.files"}`), read one
/// at a time instead of copied into the state.

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace affresco {

/// @brief What changed in a data model: a range of rows.
struct ModelChange {
	/// @brief How the rows changed.
	enum class Kind {
		Changed,  ///< Rows [first, first + count) have new values.
		Inserted, ///< Rows were inserted at first: the later ones moved down.
		Removed,  ///< Rows [first, first + count) were removed: the later ones moved up.
		Reset     ///< Anything may have changed.
	};
	Kind kind = Kind::Reset; ///< How.
	std::size_t first = 0;   ///< The first row.
	std::size_t count = 0;   ///< How many rows.
};

/// @brief Rows the application owns, e.g. the files of a folder or a million log lines, that documents show with
/// `"repeat": {"in": "models.<name>"}` (format::Runtime::setModel()).
///
/// The document reads only the rows it shows, by index; agents see the count, the shape and a sample row, never
/// every row. Tell what changed with onChange, so an update evaluates again only the values that read those rows.
/// Invariants: called and emitted on the main thread only (other threads post through the Executor).
class DataModel {
public:
	DataModel() = default;
	DataModel(const DataModel&) = delete;
	DataModel& operator=(const DataModel&) = delete;
	DataModel(DataModel&&) = delete;
	DataModel& operator=(DataModel&&) = delete;
	virtual ~DataModel() = default;

	/// @brief The number of rows.
	/// @return It.
	[[nodiscard]] virtual std::size_t size() const = 0;

	/// @brief One row.
	/// @param index Its index, below size().
	/// @return Its value: usually an object, as the document reads `row.name`.
	[[nodiscard]] virtual json row(std::size_t index) const = 0;

	Signal<const ModelChange&> onChange; ///< Emitted after rows changed.
};

/// @brief A data model kept in memory: a list of rows that tells what changed when it is edited.
class ListModel final : public DataModel {
public:
	/// @brief A model with some rows.
	/// @param rows The rows.
	explicit ListModel(std::vector<json> rows = {}) : rows_(std::move(rows)) {}

	/// @brief The number of rows.
	/// @return It.
	[[nodiscard]] std::size_t size() const override { return rows_.size(); }

	/// @brief One row.
	/// @param index Its index; past the end gives null.
	/// @return A copy of its value.
	[[nodiscard]] json row(std::size_t index) const override { return index < rows_.size() ? rows_[index] : json(); }

	/// @brief Replaces one row.
	/// @param index Its index; past the end does nothing.
	/// @param value Its new value.
	void set(std::size_t index, json value) {
		if(index >= rows_.size()) return;
		rows_[index] = std::move(value);
		onChange.emit({ModelChange::Kind::Changed, index, 1});
	}

	/// @brief Inserts rows.
	/// @param index Where; past the end appends.
	/// @param values The rows.
	void insert(std::size_t index, std::vector<json> values) {
		index = std::min(index, rows_.size());
		const std::size_t count = values.size();
		rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(index), std::make_move_iterator(values.begin()),
		             std::make_move_iterator(values.end()));
		onChange.emit({ModelChange::Kind::Inserted, index, count});
	}

	/// @brief Removes rows.
	/// @param index The first one; past the end does nothing.
	/// @param count How many, at most up to the end.
	void remove(std::size_t index, std::size_t count) {
		if(index >= rows_.size()) return;
		count = std::min(count, rows_.size() - index);
		const auto first = rows_.begin() + static_cast<std::ptrdiff_t>(index);
		rows_.erase(first, first + static_cast<std::ptrdiff_t>(count));
		onChange.emit({ModelChange::Kind::Removed, index, count});
	}

	/// @brief Replaces every row.
	/// @param rows The rows.
	void reset(std::vector<json> rows) {
		rows_ = std::move(rows);
		onChange.emit({ModelChange::Kind::Reset, 0, rows_.size()});
	}

private:
	std::vector<json> rows_; ///< The rows.
};

} // namespace affresco
