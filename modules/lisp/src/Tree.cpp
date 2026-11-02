/// @file
/// @brief What walks a whole value: making it a JSON tree, printing it, comparing it. Each walk spends from the budget,
/// so a small value that shares itself many times can't take long, and stops at a depth, so it can't overflow the
/// stack.

#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>
#include <affresco/notation/Words.hpp>

#include <cmath>
#include <string>
#include <unordered_set>
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

	/// @brief A vector as an array.
	/// @param from The vector.
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
		std::unordered_set<std::string_view> seen;
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
		switch(value.kind()) {
		case Kind::Nil:
			add(readable_ || depth > 0 ? "nil" : "");
			return;
		case Kind::Boolean:
			add(value.asBoolean() ? "true" : "false");
			return;
		case Kind::Integer:
		case Kind::Unsigned:
		case Kind::Float:
			add(value.numberAsJson().dump());
			return;
		case Kind::String:
			if(readable_ || depth > 0) add(json(value.text()).dump(-1, ' ', false, json::error_handler_t::replace));
			else add(value.text());
			return;
		case Kind::Symbol:
			add(value.text());
			return;
		case Kind::Keyword:
			add(":");
			add(value.text());
			return;
		case Kind::List:
		case Kind::Vector:
			sequence(value, depth);
			return;
		case Kind::Map:
			map(value, depth);
			return;
		default:
			add("#<function>");
		}
	}

private:
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

/// @brief Compares two values.
/// @param machine The machine, whose budget it spends.
/// @param first One.
/// @param second The other.
/// @param depth How deep it has gone.
/// @return true if they are equal.
bool equalAt(Machine& machine, const Value& first, const Value& second, int depth) {
	if(depth > maxValueDepth) Machine::exhausted("values nested too deeply to compare");
	machine.spend(1);
	if(first.isNumber() && second.isNumber()) return numbersEqual(first, second);
	if(first.isSequence() && second.isSequence()) {
		if(first.count() != second.count()) return false;
		for(std::size_t at = 0; at < first.count(); ++at)
			if(!equalAt(machine, first.at(at), second.at(at), depth + 1)) return false;
		return true;
	}
	if(first.kind() != second.kind()) return false;
	switch(first.kind()) {
	case Kind::Nil:
		return true;
	case Kind::Boolean:
		return first.asBoolean() == second.asBoolean();
	case Kind::String:
	case Kind::Symbol:
	case Kind::Keyword:
		return first.text() == second.text();
	case Kind::Map: {
		if(first.count() != second.count()) return false;
		for(std::size_t at = 0; at < first.count(); ++at) {
			bool found = false;
			const Value other = second.lookup(first.keyAt(at), &found);
			if(!found || !equalAt(machine, first.valueAt(at), other, depth + 1)) return false;
		}
		return true;
	}
	default:
		return first.pointer() == second.pointer();
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
	return equalAt(machine, first, second, 0);
}

} // namespace affresco::lisp
