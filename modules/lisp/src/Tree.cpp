/// @file
/// @brief What walks a whole value: making it a JSON tree, printing it, comparing it. Each walk spends from the budget,
/// so a small value that shares itself many times can't take long, and stops at a depth, so it can't overflow the
/// stack.

#include "Machine.hpp"
#include "Sequences.hpp"
#include "Sets.hpp"

#include <affresco/notation/Lexer.hpp>
#include <affresco/notation/Words.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

using Members = std::vector<std::pair<const std::string, json>>; ///< What an ordered_json object stores.

/// @brief Makes a value the JSON tree it stands for.
class TreeBuilder {
public:
	/// @brief Makes a builder.
	/// @param machine The machine, whose budget it spends.
	/// @param definition The definition it builds for, for errors, or "".
	TreeBuilder(Machine& machine, std::string definition) : machine_(machine), definition_(std::move(definition)) {}

	/// @brief A value as a tree.
	/// @param value The value.
	/// @param depth How many objects and lists hold it.
	/// @param where The closest place known, for an error.
	/// @return The tree.
	json value(const Value& value, int depth, Place where) {
		machine_.spend(1);
		machine_.heap.charge(sizeof(json));
		const Place place = value.place().line != 0 ? value.place() : where;
		switch(value.kind()) {
		case Kind::Nil:
			return nullptr;
		case Kind::Boolean:
			return value.asBoolean();
		case Kind::Integer:
		case Kind::Unsigned:
		case Kind::Float:
			return value.numberAsJson();
		case Kind::String:
		case Kind::Symbol:
		case Kind::Keyword:
			machine_.heap.charge(value.text().size());
			return std::string(value.text());
		case Kind::Vector:
		case Kind::Set: // as its items, in the order they were added
			return vector(value, depth, place);
		case Kind::Map:
			return map(value, depth, place);
		case Kind::List:
			return node(value, depth, place);
		default:
			fail(place, "a function can't be part of a tree");
		}
	}

private:
	/// @brief Reports a problem.
	/// @param place Where.
	/// @param message What is wrong.
	/// @throws LispError Always.
	[[noreturn]] void fail(Place place, const std::string& message) const {
		throw LispError(place.file ? *place.file : "", place.line, place.column, definition_, message);
	}

	/// @brief Refuses a container that would nest deeper than maxNesting.
	/// @param levels How many levels it would reach.
	/// @param place Where it is.
	void shallowEnough(int levels, Place place) const {
		if(levels > maxNesting) fail(place, "nested more than " + std::to_string(maxNesting) + " levels deep");
	}

	/// @brief A vector or a set as an array.
	/// @param from The vector or the set.
	/// @param depth How many objects and lists hold it.
	/// @param place Where it is.
	/// @return The array.
	json vector(const Value& from, int depth, Place place) {
		shallowEnough(depth + 1, place);
		json items = json::array();
		items.get_ref<json::array_t&>().reserve(from.count());
		for(std::size_t at = 0; at < from.count(); ++at) items.push_back(value(from.at(at), depth + 1, place));
		return items;
	}

	/// @brief A map as an object.
	/// @param from The map.
	/// @param depth How many objects and lists hold it.
	/// @param place Where it is.
	/// @return The object.
	json map(const Value& from, int depth, Place place) {
		shallowEnough(depth + 1, place);
		json object = json::object();
		auto& members = static_cast<Members&>(object.get_ref<json::object_t&>());
		members.reserve(from.count());
		for(std::size_t at = 0; at < from.count(); ++at) {
			machine_.heap.charge(from.keyAt(at).size());
			members.emplace_back(std::string(from.keyAt(at)), value(from.valueAt(at), depth + 1, place));
		}
		return object;
	}

	/// @brief Checks a list's head can be a node's type.
	/// @param list The list.
	/// @param place Where it is.
	/// @return The type; `node` for a node without one.
	std::string_view typeOf(const Value& list, Place place) const {
		if(list.count() == 0 || list.at(0).kind() != Kind::Symbol)
			fail(place, "expected the node's type, or node, after '('" +
			                (list.count() == 0 ? std::string() : ", found " + std::string(describeKind(list.at(0)))));
		const std::string_view type = list.at(0).text();
		const std::string shown = notation::Lexer::excerpt(type);
		if(!notation::isPlainWord(type))
			fail(place, "\"" + shown + "\" is not a node type; to run it, write ~(" + shown + " ...)");
		if(notation::isLanguageWord(type))
			fail(place, "\"" + shown + "\" is not a node type; to run it, write ~(" + shown +
			                " ...); for a node of that type, write (node :type " + shown + " ...)");
		return type;
	}

	/// @brief A list as a node: `(type :key value ... child ...)`.
	/// @param list The list.
	/// @param depth How many objects and lists hold it.
	/// @param place Where it is.
	/// @return The object: `type` first (unless the head is `node`), then the keys in the order written, then
	/// `children`.
	json node(const Value& list, int depth, Place place) {
		shallowEnough(depth + 1, place);
		const std::string_view type = typeOf(list, place);
		json object = json::object();
		auto& members = static_cast<Members&>(object.get_ref<json::object_t&>());
		std::set<std::string_view> seen;
		if(type != notation::untypedHead) {
			members.emplace_back("type", std::string(type));
			seen.insert("type");
		}
		json children = json::array();
		for(std::size_t at = 1; at < list.count(); ++at) {
			const Value item = list.at(at);
			if(item.kind() != Kind::Keyword) {
				shallowEnough(depth + 2, place);
				children.push_back(value(item, depth + 2, place));
				continue;
			}
			const std::string key(item.text());
			if(!children.empty())
				fail(item.place(),
				     "':" + notation::Lexer::excerpt(key) + "' after the node's children: properties come first");
			if(at + 1 >= list.count())
				fail(item.place(), "the key \"" + notation::Lexer::excerpt(key) + "\" has no value");
			if(!seen.insert(item.text()).second)
				fail(item.place(), "the key \"" + notation::Lexer::excerpt(key) + "\" twice: a key has one value");
			members.emplace_back(key, value(list.at(++at), depth + 1, place));
		}
		if(!children.empty()) {
			if(seen.contains("children")) fail(place, "a node with :children and children after its properties");
			members.emplace_back("children", std::move(children));
		}
		return object;
	}

	Machine& machine_;       ///< See the constructor.
	std::string definition_; ///< See the constructor.
};

/// @brief Writes a value as Lisp text.
class Printer {
public:
	/// @brief Makes a printer.
	/// @param machine The machine, whose budget it spends.
	/// @param out Gets the text.
	/// @param readable Strings in quotes.
	Printer(Machine& machine, std::string& out, bool readable) : machine_(machine), out_(out), readable_(readable) {}

	/// @brief Writes a value.
	/// @param value The value.
	/// @param depth How many values hold it.
	void write(const Value& value, int depth) {
		if(depth > maxValueDepth) Machine::exhausted("a value nested too deeply to print");
		machine_.spend(1);
		if(value.isSequence()) sequence(value, depth);
		else if(value.kind() == Kind::Map) map(value, depth);
		else if(value.kind() == Kind::Set) set(value, depth);
		else if(value.isNumber()) number(value, depth);
		else if(value.kind() == Kind::String) string(value, depth);
		else if(value.kind() == Kind::Nil) add(readable_ || depth > 0 ? "nil" : "");
		else if(value.kind() == Kind::Boolean) add(value.asBoolean() ? "true" : "false");
		else if(value.kind() == Kind::Symbol) add(value.text());
		else if(value.kind() == Kind::Keyword) keyword(value);
		else add("#<function>");
	}

private:
	/// @brief Writes a number as JSON does; as text that isn't meant to read back (`str`), a whole number that is a
	/// double as an expression shows it, without ".0".
	/// @param value It.
	/// @param depth How many values hold it.
	void number(const Value& value, int depth) {
		constexpr double shownWhole = 1e15; // as format's Value::toText()
		if(!readable_ && depth == 0 && value.kind() == Kind::Float && std::abs(value.asDouble()) < shownWhole) {
			if(const std::optional<std::int64_t> whole = wholeNumber(value)) {
				add(std::to_string(*whole));
				return;
			}
		}
		add(value.numberAsJson().dump());
	}

	/// @brief Writes a string: in quotes when it is meant to read back, or inside something.
	/// @param value It.
	/// @param depth How many values hold it.
	void string(const Value& value, int depth) {
		if(readable_ || depth > 0) add(json(value.text()).dump(-1, ' ', false, json::error_handler_t::replace));
		else add(value.text());
	}

	/// @brief Writes a keyword.
	/// @param value It.
	void keyword(const Value& value) {
		add(":");
		add(value.text());
	}

	/// @brief Writes a list or a vector.
	/// @param value It.
	/// @param depth How many values hold it.
	void sequence(const Value& value, int depth) {
		add(value.kind() == Kind::List ? "(" : "[");
		for(std::size_t at = 0; at < value.count(); ++at) {
			if(at > 0) add(" ");
			write(value.at(at), depth + 1);
		}
		add(value.kind() == Kind::List ? ")" : "]");
	}

	/// @brief Writes a set, `#{1 2}`, its items in the order they were added.
	/// @param value It.
	/// @param depth How many values hold it.
	void set(const Value& value, int depth) {
		add("#{");
		for(std::size_t at = 0; at < value.count(); ++at) {
			if(at > 0) add(" ");
			write(value.at(at), depth + 1);
		}
		add("}");
	}

	/// @brief Writes a map.
	/// @param value It.
	/// @param depth How many values hold it.
	void map(const Value& value, int depth) {
		add("{");
		for(std::size_t at = 0; at < value.count(); ++at) {
			if(at > 0) add(" ");
			const std::string_view key = value.keyAt(at);
			if(notation::isPlainWord(key)) {
				add(":");
				add(key);
			} else {
				add(json(key).dump(-1, ' ', false, json::error_handler_t::replace));
			}
			add(" ");
			write(value.valueAt(at), depth + 1);
		}
		add("}");
	}

	/// @brief Adds text, at its cost.
	/// @param text The text.
	void add(std::string_view text) {
		machine_.heap.charge(text.size());
		out_ += text;
	}

	Machine& machine_; ///< See the constructor.
	std::string& out_; ///< See the constructor.
	bool readable_;    ///< See the constructor.
};

/// @brief Spends operations, if there is a machine. @param machine It, or nullptr. @param count How many.
void spend(Machine* machine, std::size_t count) {
	if(machine) machine->spend(count);
}

/// @brief Spends going through a text, if there is a machine. @param machine It, or nullptr. @param bytes Its size.
void spendText(Machine* machine, std::size_t bytes) {
	if(machine) machine->spendText(bytes);
}

/// @brief Spends finding a key in a map, if there is a machine.
/// @param machine It, or nullptr.
/// @param map The map.
/// @param key The key.
void spendLookup(Machine* machine, const Value& map, std::string_view key) {
	if(machine) machine->spendLookup(map, key);
}

/// @brief Compares two numbers by value.
/// @param first One.
/// @param second The other.
/// @return true if they are equal.
bool numbersEqual(const Value& first, const Value& second) noexcept {
	if(first.kind() == second.kind() && first.kind() != Kind::Float)
		return first.kind() == Kind::Integer ? first.asInteger() == second.asInteger()
		                                     : first.asUnsigned() == second.asUnsigned();
	if(first.kind() != Kind::Float && second.kind() != Kind::Float) return false; // an Unsigned is above every Integer
	return first.asDouble() == second.asDouble();
}

bool equalAt(Machine* machine, const Value& first, const Value& second, int depth);

/// @brief Tells whether a value is text as `=` sees it: a keyword is the string of its text (decision D2), since a
/// binding writes `(= state.mode :edit)` as `state.mode == 'edit'`, and data never holds a keyword once it is in an
/// app's state. A symbol stays apart.
/// @param value The value.
/// @return true for a string or a keyword.
bool isText(const Value& value) noexcept { return value.kind() == Kind::String || value.kind() == Kind::Keyword; }

/// @brief Compares the texts of two values.
/// @param machine The machine, whose budget it spends, or nullptr to spend nothing (the reader).
/// @param first One.
/// @param second The other.
/// @return true if they are the same text.
bool textsEqual(Machine* machine, const Value& first, const Value& second) {
	if(first.text().size() == second.text().size()) spendText(machine, first.text().size());
	return first.text() == second.text();
}

/// @brief Compares two lists or vectors.
/// @param machine The machine, whose budget it spends, or nullptr to spend nothing (the reader).
/// @param first One.
/// @param second The other.
/// @param depth How deep it has gone.
/// @return true if they are equal.
bool sequencesEqual(Machine* machine, const Value& first, const Value& second, int depth) {
	if(first.count() != second.count()) return false;
	for(std::size_t at = 0; at < first.count(); ++at)
		if(!equalAt(machine, first.at(at), second.at(at), depth + 1)) return false;
	return true;
}

/// @brief Compares two maps, whatever the order of their keys.
/// @param machine The machine, whose budget it spends, or nullptr to spend nothing (the reader).
/// @param first One.
/// @param second The other.
/// @param depth How deep it has gone.
/// @return true if they are equal.
bool mapsEqual(Machine* machine, const Value& first, const Value& second, int depth) {
	if(first.count() != second.count()) return false;
	for(std::size_t at = 0; at < first.count(); ++at) {
		bool found = false;
		spendLookup(machine, second, first.keyAt(at));
		const Value other = second.lookup(first.keyAt(at), &found);
		if(!found || !equalAt(machine, first.valueAt(at), other, depth + 1)) return false;
	}
	return true;
}

/// @brief The position of a set's member equal to an item: only the members whose hashes meet the item's are compared.
/// @param machine The machine, whose budget it spends, or nullptr to spend nothing (the reader).
/// @param set The set.
/// @param item The item.
/// @param hash The item's hash (hashAt()).
/// @param depth How deep it has gone.
/// @return The position, or std::nullopt if it isn't a member.
std::optional<std::uint32_t> positionAt(Machine* machine, const Value& set, const Value& item, std::size_t hash,
                                        int depth) {
	const SetObject& object = setObjectOf(set);
	const auto [first, last] = object.storage->index.equal_range(hash);
	for(auto entry = first; entry != last; ++entry) {
		spend(machine, 1);
		if(entry->second < object.size && equalAt(machine, object.storage->items->values[entry->second], item, depth))
			return entry->second;
	}
	return std::nullopt;
}

/// @brief Compares two sets, whatever the order of their members: each member of one is looked up in the other by the
/// hash it was added with, so that comparing is linear and not quadratic.
/// @param machine The machine, whose budget it spends, or nullptr to spend nothing (the reader).
/// @param first One.
/// @param second The other.
/// @param depth How deep it has gone.
/// @return true if they are equal.
bool setsEqual(Machine* machine, const Value& first, const Value& second, int depth) {
	if(first.count() != second.count()) return false;
	const SetObject& members = setObjectOf(first);
	for(std::size_t at = 0; at < first.count(); ++at)
		if(!positionAt(machine, second, first.at(at), members.storage->hashes[at], depth + 1)) return false;
	return true;
}

/// @brief Compares two values.
/// @param machine The machine, whose budget it spends, or nullptr to spend nothing (the reader).
/// @param first One.
/// @param second The other.
/// @param depth How deep it has gone.
/// @return true if they are equal.
bool equalAt(Machine* machine, const Value& first, const Value& second, int depth) {
	if(depth > maxValueDepth) Machine::exhausted("values nested too deeply to compare");
	spend(machine, 1);
	if(first.isNumber() && second.isNumber()) return numbersEqual(first, second);
	if(first.isSequence() && second.isSequence()) return sequencesEqual(machine, first, second, depth);
	if(isText(first) && isText(second)) return textsEqual(machine, first, second);
	if(first.kind() != second.kind()) return false;
	if(first.kind() == Kind::Map) return mapsEqual(machine, first, second, depth);
	if(first.kind() == Kind::Set) return setsEqual(machine, first, second, depth);
	if(first.kind() == Kind::Nil) return true;
	if(first.kind() == Kind::Boolean) return first.asBoolean() == second.asBoolean();
	if(first.kind() == Kind::Function || first.kind() == Kind::Native) return first.pointer() == second.pointer();
	return textsEqual(machine, first, second); // two symbols
}

/// @brief Spreads a hash over all its bits, from a seed the process draws once (splitmix64's finalizer): numbers in a
/// range have bits in a line, and a sum of hashes is the same for many sets, so that without it a document could fill
/// the buckets of a set or of `distinct` with items that meet, and make reading or building it quadratic. Results
/// don't depend on the seed: a hash only finds the members to compare, and a set keeps the order they came in.
/// @param hash The hash.
/// @return It, spread.
std::size_t spread(std::uint64_t hash) noexcept {
	static const std::uint64_t seed = (std::uint64_t{std::random_device{}()} << 32U) ^ std::random_device{}();
	std::uint64_t bits = hash + seed + 0x9e3779b97f4a7c15U;
	bits = (bits ^ (bits >> 30U)) * 0xbf58476d1ce4e5b9U;
	bits = (bits ^ (bits >> 27U)) * 0x94d049bb133111ebU;
	return static_cast<std::size_t>(bits ^ (bits >> 31U));
}

/// @brief Hashes a value as `=` compares it (valueHash()).
/// @param machine The machine, whose budget it spends, or nullptr to spend nothing (the reader).
/// @param value The value.
/// @param depth How deep it has gone.
/// @return The hash.
std::size_t hashAt(Machine* machine, const Value& value, int depth) {
	if(depth > maxValueDepth) Machine::exhausted("values nested too deeply to compare");
	spend(machine, 1);
	const auto mixed = [](std::size_t seed, std::size_t hash) {
		return seed ^ (hash + 0x9e3779b9U + (seed << 6U) + (seed >> 2U));
	};
	if(value.isNumber()) {
		const double number = value.asDouble(); // `=` compares an integer with a double by value
		return spread(number == 0 ? 0 : std::bit_cast<std::uint64_t>(number)); // 0 and -0 as one
	}
	if(value.isSequence()) { // a list is the vector of its items
		std::size_t hash = 0x51ed27U;
		for(std::size_t at = 0; at < value.count(); ++at) hash = mixed(hash, hashAt(machine, value.at(at), depth + 1));
		return hash;
	}
	switch(value.kind()) {
	case Kind::Map: { // whatever its order: a sum of its members'
		std::size_t hash = 0x7a3b9cU;
		for(std::size_t at = 0; at < value.count(); ++at) {
			spendText(machine, value.keyAt(at).size());
			hash += spread(
			    mixed(std::hash<std::string_view>{}(value.keyAt(at)), hashAt(machine, value.valueAt(at), depth + 1)));
		}
		return hash;
	}
	case Kind::Set: { // whatever its order: a sum of the hashes its members were added with
		const SetObject& object = setObjectOf(value);
		spend(machine, object.size);
		std::size_t hash = 0x6b43a9U;
		for(std::size_t at = 0; at < object.size; ++at) hash += spread(object.storage->hashes[at]);
		return hash;
	}
	case Kind::Nil:
		return 0x2545fU;
	case Kind::Boolean:
		return value.asBoolean() ? 0x3c6ef3U : 0x1b873U;
	case Kind::Function:
	case Kind::Native:
		return std::hash<const void*>{}(value.pointer());
	default: // a keyword as the string of its text (decision D2); a symbol so too, as `=` keeps them apart anyway
		spendText(machine, value.text().size());
		return std::hash<std::string_view>{}(value.text());
	}
}

} // namespace

json valueToTree(Machine& machine, const Value& value, const std::string& definition) {
	return TreeBuilder(machine, definition).value(value, 0, {});
}

void printValue(Machine& machine, const Value& value, std::string& out, bool readable) {
	Printer(machine, out, readable).write(value, 0);
}

bool valuesEqual(Machine& machine, const Value& first, const Value& second) {
	return equalAt(&machine, first, second, 0);
}

std::size_t valueHash(Machine& machine, const Value& value) { return hashAt(&machine, value, 0); }

std::size_t valueHash(Machine* machine, const Value& value) { return hashAt(machine, value, 0); }

std::optional<std::uint32_t> setPosition(Machine* machine, const Value& set, const Value& item, std::size_t hash) {
	return positionAt(machine, set, item, hash, 0);
}

} // namespace affresco::lisp
