/// @file
/// @brief format::Models: the application's data models, their places and what agents see of them.

#include "Models.hpp"

#include "Engine.hpp"
#include "Reads.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace affresco::format {

namespace {

/// Rows changed at once that are marked one by one; more mark the whole model, which is cheaper than many places.
constexpr std::size_t rowsMarkedOneByOne = 16;

/// @brief Whether a name can be a model's: letters, digits and "_", not starting with a digit.
/// @param name The name.
/// @return true if it can.
bool isModelName(std::string_view name) noexcept {
	if(name.empty() || (name.front() >= '0' && name.front() <= '9')) return false;
	return std::ranges::all_of(name, [](char c) {
		return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
	});
}

/// @brief The type of each key of a row, for agents.
/// @param row A row.
/// @return `{"name": "string", "size": "number"}`, or the row's own type if it isn't an object.
json shapeOf(const json& row) {
	if(!row.is_object()) return row.type_name();
	json shape = json::object();
	for(const auto& [key, value] : row.items()) shape[key] = value.type_name();
	return shape;
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): every substr starts inside the path, so none throws
std::optional<std::string_view> modelName(std::string_view path) noexcept {
	if(path.starts_with("{{") && path.ends_with("}}") && path.size() >= 4) path = path.substr(2, path.size() - 4);
	constexpr std::string_view prefix = "models.";
	if(!path.starts_with(prefix)) return std::nullopt;
	path.remove_prefix(prefix.size());
	if(!isModelName(path)) return std::nullopt;
	return path;
}

Models::~Models() {
	for(auto& [name, entry] : models_) entry.model->onChange.disconnect(entry.connection);
}

void Models::set(std::string name, std::shared_ptr<DataModel> model) {
	if(!isModelName(name)) throw std::invalid_argument("'" + name + "' can't name a data model");
	if(const auto old = models_.find(name); old != models_.end()) {
		old->second.model->onChange.disconnect(old->second.connection);
		models_.erase(old);
	}
	if(model) {
		const Connection connection =
		    model->onChange.connect([this, name](const ModelChange& change) { changed(name, change); });
		models_.emplace(name, Entry{std::move(model), connection});
	}
	changed(name, {});
}

const DataModel* Models::find(std::string_view name) const {
	const auto it = models_.find(name);
	return it == models_.end() ? nullptr : it->second.model.get();
}

std::string Models::placeOf(std::string_view name) {
	std::string place(modelPlaces);
	appendKey(place, name);
	return place;
}

json Models::at(std::string_view place) const {
	if(!place.starts_with(modelPlaces) || place.size() <= modelPlaces.size() || place[modelPlaces.size()] != '/')
		return nullptr;
	place.remove_prefix(modelPlaces.size() + 1);
	const std::size_t nameEnd = std::min(place.find('/'), place.size());
	const DataModel* model = find(place.substr(0, nameEnd));
	if(!model || nameEnd == place.size()) return nullptr;
	place.remove_prefix(nameEnd + 1);
	const std::size_t rowEnd = std::min(place.find('/'), place.size());
	std::size_t row = 0;
	// NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage): the end is given, inside the place
	const auto [last, error] = std::from_chars(place.data(), place.data() + rowEnd, row);
	if(error != std::errc() || last != place.data() + rowEnd || row >= model->size()) return nullptr;
	json value = model->row(row);
	if(rowEnd == place.size()) return value;
	try { // inside the row: "/~m/files/3/tags/0"
		const json::json_pointer inside{std::string(place.substr(rowEnd))};
		return value.contains(inside) ? value.at(inside) : json();
	} catch(const json::exception&) {
		return nullptr;
	}
}

json Models::outline() const {
	json outline = json::object();
	for(const auto& [name, entry] : models_) {
		const std::size_t count = entry.model->size();
		const json sample = count > 0 ? entry.model->row(0) : json();
		outline[name] = {{"count", count}, {"shape", shapeOf(sample)}, {"sample", sample}};
	}
	return outline;
}

std::string Models::names() const {
	std::string names;
	for(const auto& [name, entry] : models_) names += (names.empty() ? "" : ", ") + name;
	return names.empty() ? "none" : names;
}

void Models::changed(const std::string& name, const ModelChange& change) {
	const std::string place = placeOf(name);
	if(change.kind == ModelChange::Kind::Changed && change.count <= rowsMarkedOneByOne) {
		for(std::size_t row = change.first; row < change.first + change.count; ++row)
			engine_.document.markPlaceChanged(place + "/" + std::to_string(row));
	} else { // rows moved, or many changed: whatever shows the model looks at it again
		engine_.document.markPlaceChanged(place);
	}
	engine_.reconciler.requestStateUpdate();
}

} // namespace affresco::format
