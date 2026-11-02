/// @file
/// @brief The language's values (Value.hpp) and the heap that owns what they point at (Objects.hpp).

#include "Objects.hpp"

#include <iterator>
#include <limits>

namespace affresco::lisp {

namespace {

/// @brief A sequence's object. @param value The list or the vector, not a view. @return It.
const SequenceObject& sequenceOf(const Value& value) { return *static_cast<const SequenceObject*>(value.pointer()); }

/// @brief A map's object. @param value The map, not a view. @return It.
const MapObject& mapOf(const Value& value) { return *static_cast<const MapObject*>(value.pointer()); }

/// @brief The member of a JSON object at an index.
/// @param object The object.
/// @param index The index.
/// @return The member.
const auto& memberOf(const json& object, std::size_t index) {
	return *std::next(object.get_ref<const json::object_t&>().begin(), static_cast<std::ptrdiff_t>(index));
}

} // namespace

Value Value::boolean(bool value) noexcept {
	Value made;
	made.kind_ = Kind::Boolean;
	made.bits_.boolean = value;
	return made;
}

Value Value::integer(std::int64_t value) noexcept {
	Value made;
	made.kind_ = Kind::Integer;
	made.bits_.integer = value;
	return made;
}

Value Value::real(double value) noexcept {
	Value made;
	made.kind_ = Kind::Float;
	made.bits_.real = value;
	return made;
}

Value Value::view(const json& value) {
	Value made;
	switch(value.type()) {
	case json::value_t::boolean:
		return boolean(value.get_ref<const json::boolean_t&>());
	case json::value_t::number_integer:
		return integer(value.get_ref<const json::number_integer_t&>());
	case json::value_t::number_unsigned: { // JSON text's positive integers: Unsigned only above what Integer holds
		const std::uint64_t natural = value.get_ref<const json::number_unsigned_t&>();
		if(natural <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
			return integer(static_cast<std::int64_t>(natural));
		made.kind_ = Kind::Unsigned;
		made.bits_.natural = natural;
		return made;
	}
	case json::value_t::number_float:
		return real(value.get_ref<const json::number_float_t&>());
	case json::value_t::string:
		made.kind_ = Kind::String;
		break;
	case json::value_t::array:
		made.kind_ = Kind::Vector;
		break;
	case json::value_t::object:
		made.kind_ = Kind::Map;
		break;
	default:
		return made; // null, and what JSON text never holds
	}
	made.view_ = true;
	made.bits_.viewed = &value;
	return made;
}

Value Value::object(Kind kind, const void* object) noexcept {
	Value made;
	made.kind_ = kind;
	made.bits_.pointer = object;
	return made;
}

double Value::asDouble() const noexcept {
	switch(kind_) {
	case Kind::Integer:
		return static_cast<double>(bits_.integer);
	case Kind::Unsigned:
		return static_cast<double>(bits_.natural);
	case Kind::Float:
		return bits_.real;
	default:
		return 0;
	}
}

json Value::numberAsJson() const {
	if(kind_ == Kind::Integer) return bits_.integer;
	if(kind_ == Kind::Unsigned) return bits_.natural;
	return bits_.real;
}

std::string_view Value::text() const {
	if(kind_ != Kind::String && kind_ != Kind::Symbol && kind_ != Kind::Keyword) return {};
	if(view_) return bits_.viewed->get_ref<const std::string&>();
	return static_cast<const TextObject*>(bits_.pointer)->text;
}

std::size_t Value::count() const noexcept {
	if(kind_ != Kind::List && kind_ != Kind::Vector && kind_ != Kind::Map) return 0;
	if(view_) return bits_.viewed->size();
	return kind_ == Kind::Map ? mapOf(*this).entries.size() : sequenceOf(*this).size;
}

Value Value::at(std::size_t index) const {
	if(view_) return view((*bits_.viewed)[index]);
	const SequenceObject& sequence = sequenceOf(*this);
	return sequence.items->values[sequence.offset + index];
}

std::string_view Value::keyAt(std::size_t index) const {
	return view_ ? std::string_view(memberOf(*bits_.viewed, index).first) : mapOf(*this).entries[index].key;
}

Value Value::valueAt(std::size_t index) const {
	return view_ ? view(memberOf(*bits_.viewed, index).second) : mapOf(*this).entries[index].value;
}

Value Value::lookup(std::string_view key, bool* found) const {
	if(found) *found = false;
	if(kind_ != Kind::Map) return {};
	if(view_) {
		const auto member = bits_.viewed->find(key);
		if(member == bits_.viewed->end()) return {};
		if(found) *found = true;
		return view(*member);
	}
	const MapObject& map = mapOf(*this);
	const std::size_t index = map.find(key);
	if(index == map.entries.size()) return {};
	if(found) *found = true;
	return map.entries[index].value;
}

Place Value::place() const noexcept {
	if(view_) return {};
	switch(kind_) {
	case Kind::Symbol:
	case Kind::Keyword:
	case Kind::String:
		return static_cast<const TextObject*>(bits_.pointer)->place;
	case Kind::List:
	case Kind::Vector:
		return sequenceOf(*this).place;
	case Kind::Map:
		return mapOf(*this).place;
	default:
		return {};
	}
}

std::string_view describeKind(const Value& value) noexcept {
	switch(value.kind()) {
	case Kind::Nil:
		return "nil";
	case Kind::Boolean:
		return "a boolean";
	case Kind::Integer:
	case Kind::Unsigned:
	case Kind::Float:
		return "a number";
	case Kind::String:
		return "a string";
	case Kind::Symbol:
		return "a symbol";
	case Kind::Keyword:
		return "a keyword";
	case Kind::List:
		return "a list";
	case Kind::Vector:
		return "a vector";
	case Kind::Map:
		return "a map";
	default:
		return "a function";
	}
}

std::size_t MapObject::find(std::string_view key) const {
	if(entries.size() <= searched) {
		for(std::size_t index = 0; index < entries.size(); ++index)
			if(entries[index].key == key) return index;
		return entries.size();
	}
	if(!index_) {
		index_ = std::make_unique<std::unordered_map<std::string_view, std::uint32_t>>();
		index_->reserve(entries.size());
		for(std::size_t index = 0; index < entries.size(); ++index)
			index_->emplace(entries[index].key, static_cast<std::uint32_t>(index));
	}
	const auto found = index_->find(key);
	return found == index_->end() ? entries.size() : found->second;
}

Value Heap::text(Kind kind, std::string_view written, Place place) {
	auto* object = make<TextObject>(written.size());
	object->text = std::string(written);
	object->place = place;
	return Value::object(kind, object);
}

Value Heap::string(std::string_view written, Place place) { return text(Kind::String, written, place); }

Value Heap::symbol(std::string_view written, Place place) { return text(Kind::Symbol, written, place); }

Value Heap::keyword(std::string_view written, Place place) { return text(Kind::Keyword, written, place); }

Value Heap::sequence(Kind kind, std::span<const Value> items, Place place) {
	return sequence(kind, std::vector<Value>(items.begin(), items.end()), place);
}

Value Heap::sequence(Kind kind, std::vector<Value>&& items, Place place) {
	auto* storage = make<Items>(items.size() * sizeof(Value));
	storage->values = std::move(items);
	storage->epoch = epoch_;
	auto* object = make<SequenceObject>();
	object->items = storage;
	object->size = static_cast<std::uint32_t>(storage->values.size());
	object->place = place;
	return Value::object(kind, object);
}

Value Heap::part(const Value& from, std::size_t offset, std::size_t size) {
	if(from.isView()) {
		std::vector<Value> items;
		charge(size * sizeof(Value));
		items.reserve(size);
		for(std::size_t index = 0; index < size; ++index) items.push_back(from.at(offset + index));
		return sequence(from.kind(), std::move(items));
	}
	const SequenceObject& whole = sequenceOf(from);
	auto* object = make<SequenceObject>();
	object->items = whole.items;
	object->offset = whole.offset + static_cast<std::uint32_t>(offset);
	object->size = static_cast<std::uint32_t>(size);
	object->place = whole.place;
	return Value::object(from.kind(), object);
}

Value Heap::appended(const Value& from, const Value& item) {
	if(!from.isView()) {
		const SequenceObject& whole = sequenceOf(from);
		// Only values that can't see past their part share it; and a region's value goes when the region does, so it
		// isn't put in older storage, which stays
		if(whole.offset + whole.size == whole.items->values.size() && whole.items->epoch == epoch_) {
			charge(sizeof(Value));
			const_cast<Items*>(whole.items)->values.push_back(item); // NOLINT(cppcoreguidelines-pro-type-const-cast)
			auto* object = make<SequenceObject>();
			object->items = whole.items;
			object->offset = whole.offset;
			object->size = whole.size + 1;
			object->place = whole.place;
			return Value::object(from.kind(), object);
		}
	}
	std::vector<Value> items;
	charge((from.count() + 1) * sizeof(Value));
	items.reserve(from.count() + 1);
	for(std::size_t index = 0; index < from.count(); ++index) items.push_back(from.at(index));
	items.push_back(item);
	return sequence(from.kind(), std::move(items));
}

Value Heap::map(std::vector<MapEntry>&& entries, Place place) {
	auto* object = make<MapObject>(entries.size() * sizeof(MapEntry));
	object->entries = std::move(entries);
	object->place = place;
	return Value::object(Kind::Map, object);
}

std::string_view Heap::keep(std::string_view written) {
	auto* object = make<TextObject>(written.size());
	object->text = std::string(written);
	return object->text;
}

const std::string* Heap::file(const std::string& name) {
	for(const std::string& kept : files_)
		if(kept == name) return &kept;
	return &files_.emplace_back(name);
}

std::string_view keyText(const Value& key) {
	if(key.kind() != Kind::String && key.kind() != Kind::Keyword)
		throw Problem("a map's key is a keyword or a string, not " + std::string(describeKind(key)));
	return key.text();
}

} // namespace affresco::lisp
