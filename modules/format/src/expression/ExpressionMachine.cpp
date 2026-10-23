/// @file
/// @brief The machine that evaluates typed expressions: nodes, function calls, paths, variables and templates.

#include "ExpressionMachine.hpp"

#include "ExpressionParser.hpp"
#include "Models.hpp"
#include "Reads.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <ranges>
#include <utility>

namespace affresco::format::detail {

/// @brief A key of a path, ready to follow: a name, a position, or neither (which gives null).
struct PathKey {
	/// @brief What a key is.
	enum class Kind : std::uint8_t { Name, Index, Missing };
	Kind kind = Kind::Missing; ///< What it is.
	std::string_view name;     ///< Kind::Name; a view into the node or an evaluated Value.
	std::int64_t index = 0;    ///< Kind::Index.
};

namespace {

/// @brief An evaluated key: a text is a name, an integer a position, anything else leads nowhere.
/// @param value The key's value; must outlive the PathKey.
/// @return The key.
PathKey keyOf(const Value& value) noexcept {
	if(value.isText()) return {PathKey::Kind::Name, value.text(), 0};
	if(value.isInteger()) return {PathKey::Kind::Index, {}, value.integer()};
	return {};
}

/// @brief The value at a place of the state.
/// @param state The state.
/// @param pointer The place, as a JSON Pointer.
/// @return The value, or nullptr if there is none.
const json* lying(const json& state, std::string_view pointer) {
	const json* current = &state;
	while(!pointer.empty()) {
		pointer.remove_prefix(1); // the '/'
		const std::size_t end = std::min(pointer.find('/'), pointer.size());
		std::string key(pointer.substr(0, end));
		pointer.remove_prefix(end);
		for(std::size_t at = key.find('~'); at != std::string::npos; at = key.find('~', at + 1))
			key.replace(at, 2, key[at + 1] == '1' ? "/" : "~");
		if(current->is_array()) {
			std::size_t index = 0;
			const auto [last, error] = std::from_chars(key.data(), key.data() + key.size(), index);
			if(error != std::errc() || last != key.data() + key.size() || index >= current->size()) return nullptr;
			current = &(*current)[index];
		} else if(const auto it = current->find(key); it != current->end()) {
			current = &*it;
		} else {
			return nullptr;
		}
	}
	return current;
}

/// @brief Follows keys from a value.
/// @param current The value.
/// @param keys The keys.
/// @return The value they lead to, or null if it doesn't exist. `length` gives a list's or a text's size.
Value follow(Value current, std::span<const PathKey> keys) {
	for(const PathKey& key : keys) {
		if(key.kind == PathKey::Kind::Name) {
			if(current.isObject())
				if(std::optional<Value> member = current.member(key.name)) {
					current = std::move(*member);
					continue;
				}
			if(key.name == "length" && current.isList())
				return Value::fromInteger(static_cast<std::int64_t>(current.size()));
			if(key.name == "length" && current.isText())
				return Value::fromInteger(static_cast<std::int64_t>(current.text().size()));
			return {};
		}
		if(key.kind == PathKey::Kind::Index && current.isList() && key.index >= 0 &&
		   static_cast<std::size_t>(key.index) < current.size()) {
			current = current.item(static_cast<std::size_t>(key.index));
			continue;
		}
		return {};
	}
	return current;
}

/// @brief A few values on the stack, more on the heap: a call's arguments or a path's keys.
/// @tparam T The element.
template <typename T> class SmallList {
public:
	/// @brief Makes room.
	/// @param size How many there will be.
	explicit SmallList(std::size_t size) : size_(size) {
		if(size > inline_.size()) heap_.resize(size);
	}
	/// @brief The elements.
	[[nodiscard]] std::span<T> items() noexcept {
		return size_ > inline_.size() ? std::span<T>(heap_) : std::span<T>(inline_.data(), size_);
	}

private:
	std::array<T, 4> inline_{}; ///< Up to four.
	std::vector<T> heap_;       ///< More.
	std::size_t size_;          ///< How many.
};

/// @brief Tells whether a function takes a number of arguments.
/// @param function The function.
/// @param count The number.
/// @return Whether it is within the function's fewest and most.
bool takesCount(const Function& function, std::size_t count) noexcept {
	return count >= function.fewest && count <= function.most;
}

/// @brief What a call with the wrong number of arguments says.
/// @param function The function.
/// @return "expected 2 arguments", "expected at least one argument", "expected 2 to 3 arguments".
std::string arityProblem(const Function& function) {
	if(function.fewest == function.most) return "expected " + std::to_string(function.fewest) + " arguments";
	if(function.most == unlimited) return "expected at least one argument";
	return "expected " + std::to_string(function.fewest) + " to " + std::to_string(function.most) + " arguments";
}

} // namespace

/* Places */

std::string Place::text() const {
	std::vector<const Place*> chain;
	for(const Place* place = this; place; place = place->parent) chain.push_back(place);
	std::string result;
	for(const Place* place : chain | std::views::reverse) {
		if(place->root) result += *place->root;
		if(!place->key.empty()) result.append(".").append(place->key);
		if(place->index != noIndex) result += "[" + std::to_string(place->index) + "]";
	}
	return result;
}

void Machine::fail(const Place& place, const std::string& message) { expect::fail(place.text(), message); }

void Machine::appendText(std::string& text, std::string_view piece, const Place& place) {
	// A few operations can build a long text (a join of joins): its length is limited, not only the work
	if(piece.size() > textLimit - text.size())
		fail(place, "text too long (more than " + std::to_string(textLimit >> 20) + " MiB)");
	text += piece;
}

/* Calls */

std::size_t Call::size() const noexcept {
	return node_.function && node_.function->decide ? node_.children.size() : values_.size();
}

double Call::number(std::size_t index) const {
	if(!values_[index].isNumber()) fail("expected a number, got " + values_[index].dump());
	return values_[index].number();
}

bool Call::allIntegers() const noexcept { return std::ranges::all_of(values_, &Value::isInteger); }

Value Call::evaluate(std::size_t index) const { return machine_.run(node_.children[index], at_); }

Value Call::evaluateFor(std::size_t index, const Value& item, std::size_t position) const {
	return machine_.runFor(node_.children[index], at_, item, position);
}

void Call::fail(const std::string& message) const { Machine::fail(at_, message); }

void Call::appendText(std::string& text, std::string_view piece) const { Machine::appendText(text, piece, at_); }

const Strings* Call::strings() const noexcept { return machine_.context().strings; }

/* The machine */

json Machine::evaluateDocument(const json& value, const Place& place) {
	if(value.is_string()) {
		const std::string& text = value.get_ref<const std::string&>();
		return text.contains("{{") ? runTemplate(text, place).toJson() : value;
	}
	if(value.is_object()) {
		if(value.size() == 1 && value.begin().key().starts_with('$')) return run(lower(value), place).toJson();
		json result = json::object();
		for(const auto& [key, item] : value.items()) result[key] = evaluateDocument(item, Place(place, key));
		return result;
	}
	if(value.is_array()) {
		json result = json::array();
		for(std::size_t i = 0; i < value.size(); ++i) result.push_back(evaluateDocument(value[i], Place(place, i)));
		return result;
	}
	return value;
}

// NOLINTNEXTLINE(readability-make-member-function-const): it spends the event's budget, which the machine holds
void Machine::spend(std::size_t operations, const Place& place) {
	WorkBudget* budget = context_.budget;
	if(budget == nullptr) return;
	if(budget->operations < operations) {
		budget->operations = 0; // spent: what follows in the event stops too
		fail(place, "the event did more than " + std::to_string(operationsPerEvent) +
		                " operations, its work budget: what was running stopped");
	}
	budget->operations -= operations;
}

Value Machine::built(Value value, const Place& place) {
	// What a function builds costs its size, so a loop can't fill memory in a few operations
	constexpr std::size_t bytesPerOperation = 64;
	if(value.isText()) spend(value.text().size() / bytesPerOperation, place);
	else spend(value.size(), place);
	return value;
}

Value Machine::run(const Node& node, const Place& place) {
	spend(1, place);
	switch(node.kind) {
	case Node::Kind::Literal:
		return node.value;
	case Node::Kind::Template:
		return runTemplate(node.text, place);
	case Node::Kind::Get:
		return get(node, place);
	case Node::Kind::Path:
		return lookup(node.text, place);
	case Node::Kind::Call:
		return call(node, place);
	case Node::Kind::Fail:
		fail(Place(place, node.label), node.text);
	case Node::Kind::List: {
		std::vector<Value> items;
		items.reserve(node.children.size());
		for(std::size_t i = 0; i < node.children.size(); ++i) items.push_back(run(node.children[i], Place(place, i)));
		return Value::fromList(std::move(items));
	}
	case Node::Kind::Object:
		break;
	}
	const Place labelled(place, node.label);
	const Place& at = node.label.empty() ? place : labelled; // "$object": its fields are "<at>.$object.<key>"
	std::vector<Value::Field> fields;
	fields.reserve(node.children.size());
	for(std::size_t i = 0; i < node.children.size(); ++i)
		fields.push_back({node.names[i], run(node.children[i], Place(at, node.names[i]))});
	return Value::fromObject(std::move(fields));
}

Value Machine::runFor(const Node& node, const Place& place, const Value& item, std::size_t position) {
	const Item frame{item_, item, position};
	const ItemScope scope(item_, frame);
	return run(node, place);
}

Value Machine::call(const Node& node, const Place& place) {
	const Place at(place, node.label);
	const Function* function = node.function;
	if(function && function->changes && context_.reads) context_.reads->always = true; // the clock
	if(function && function->decide) return decide(*function, node, at);

	// Arguments first, then their number, as evaluating the JSON form did
	SmallList<Value> arguments(node.children.size());
	const std::span<Value> values = arguments.items();
	for(std::size_t i = 0; i < values.size(); ++i) values[i] = run(node.children[i], node.indexed ? Place(at, i) : at);
	if(!function) fail(at, "unknown operator '" + node.label + "'");
	if(function->takes == Takes::One) return built(function->apply(Call(*this, node, at, values)), at);
	if(function->spreads && values.size() == 1 && values[0].isList()) return spread(*function, node, at, values[0]);
	if(!takesCount(*function, values.size())) fail(at, arityProblem(*function));
	return built(function->apply(Call(*this, node, at, values)), at);
}

Value Machine::decide(const Function& function, const Node& node, const Place& at) {
	if(!takesCount(function, node.children.size())) fail(at, std::string(function.shape));
	return function.decide(Call(*this, node, at, {}));
}

// max(list): the list's items are the arguments
Value Machine::spread(const Function& function, const Node& node, const Place& at, const Value& list) {
	std::vector<Value> items;
	items.reserve(list.size());
	for(std::size_t i = 0; i < list.size(); ++i) items.push_back(list.item(i));
	if(items.size() < function.fewest) fail(at, arityProblem(function));
	return built(function.apply(Call(*this, node, at, items)), at);
}

Value Machine::get(const Node& node, const Place& place) {
	const Place at(place, node.label);
	// The evaluated keys first, then the variable, as evaluating the JSON form did
	std::vector<Value> evaluated(node.children.size()); // most paths have only names and positions: nothing allocated
	for(const Key& key : node.keys)
		if(key.kind == Key::Kind::Expression) evaluated[key.child] = run(node.children[key.child], at);
	SmallList<PathKey> keys(node.keys.size());
	const std::span<PathKey> path = keys.items();
	for(std::size_t i = 0; i < node.keys.size(); ++i) {
		const Key& key = node.keys[i];
		path[i] = key.kind == Key::Kind::Name    ? PathKey{PathKey::Kind::Name, key.name, 0}
		          : key.kind == Key::Kind::Index ? PathKey{PathKey::Kind::Index, {}, key.index}
		                                         : keyOf(evaluated[key.child]);
	}
	if(!node.fromValue) return walk(node.root, node.text, path, place);
	return follow(run(node.children.back(), at), path);
}

Value Machine::walk(Root root, std::string_view name, std::span<const PathKey> keys, const Place& place) const {
	if(context_.reads) noteRead(root, name, keys);
	if(std::optional<Value> start = variable(root, name, place)) return follow(std::move(*start), keys);
	// computed.<name>: evaluated now; nodes.<id>: a built panel's geometry
	if(keys.empty() || keys.front().kind != PathKey::Kind::Name)
		fail(place, "expected " + std::string(name) + ".<name>");
	const NodeLookup& read = *(root == Root::Computed ? context_.computed : context_.nodes);
	return follow(Value::ownJson(read(keys.front().name)), keys.subspan(1));
}

void Machine::noteRead(Root root, std::string_view name, std::span<const PathKey> keys) const {
	if(root == Root::Nodes) { // a panel's geometry: no state change announces it
		context_.reads->always = true;
		return;
	}
	std::string pointer;
	if(root == Root::Named) { // a repeat's item lying in the state reads it there; other variables never change
		const std::string* origin = context_.locals.originOf(name);
		if(!origin || (item_ && (name == "it" || name == "index"))) return;
		pointer = *origin;
	} else if(root != Root::State) {
		return;
	}
	for(const PathKey& key : keys) { // up to the first key that leads nowhere: what is read lies inside
		if(key.kind == PathKey::Kind::Name) appendKey(pointer, key.name);
		else if(key.kind == PathKey::Kind::Index) appendKey(pointer, std::to_string(key.index));
		else break;
	}
	context_.reads->addPath(pointer);
}

std::optional<Value> Machine::variable(Root root, std::string_view name, const Place& place) const {
	switch(root) {
	case Root::State:
		return Value::fromJson(context_.state);
	case Root::Window:
		return Value::fromJson(context_.window);
	case Root::Revision:
		return Value::fromJson(context_.revision);
	case Root::Event:
		return Value::fromJson(context_.event);
	case Root::Computed:
		if(context_.computed) return std::nullopt;
		break;
	case Root::Nodes:
	case Root::Named:
		break;
	}
	if(std::optional<Value> named = namedVariable(name)) return named;
	if(root == Root::Nodes && context_.nodes) return std::nullopt;
	fail(place,
	     "unknown variable '" + std::string(name) +
	         "' (state, event, window, revision, proposals, history, preview, mode, computed, theme, locale, screen, "
	         "nodes, context, commands, keybindings, services, or a name given by repeat, forEach, let, with or a "
	         "component's params)");
}

std::optional<Value> Machine::namedVariable(std::string_view name) const {
	if(item_) { // a list function's `it` and `index` hide any other variable with their names
		if(name == "it") return item_->value;
		if(name == "index") return Value::fromInteger(static_cast<std::int64_t>(item_->index));
	}
	for(const Scope* scope = &context_.locals; scope; scope = scope->parent) {
		const auto local = scope->vars.find(name);
		if(local == scope->vars.end()) continue;
		// While an update records reads, a repeat's item is read where it lies now: an update that changes only
		// inside it keeps the scope, and its copy, built before
		if(context_.reads && scope->fromState == name) {
			if(context_.models && scope->origin.starts_with(modelPlaces)) // a data model's row
				if(json row = (*context_.models)(scope->origin); !row.is_null()) return Value::ownJson(std::move(row));
			if(const json* live = lying(context_.state, scope->origin)) return Value::fromJson(*live);
		}
		return Value::fromJson(*local);
	}
	if(const auto global = context_.globals.find(name); global != context_.globals.end())
		return Value::fromJson(*global);
	if(context_.variables)
		if(const json* built = (*context_.variables)(name)) return Value::fromJson(*built);
	return std::nullopt;
}

/* Templates and paths written as text */

std::shared_ptr<Template> splitTemplate(std::string_view text, const Place& place) {
	auto shape = std::make_shared<Template>();
	try {
		// Exactly one placeholder and nothing else: the value keeps its own type
		const std::string_view whole = trim(text);
		if(whole.starts_with("{{") && closing(whole, 2) == whole.size() - 2) {
			shape->pieces.push_back({{}, compiled(whole.substr(2, whole.size() - 4), place)});
			shape->whole = true;
			return shape;
		}
		std::size_t position = 0;
		for(std::size_t start; (start = text.find("{{", position)) != std::string_view::npos;) {
			const std::size_t end = closing(text, start + 2);
			if(end == std::string_view::npos) Machine::fail(place, "unclosed '{{' in \"" + std::string(text) + "\"");
			shape->pieces.push_back({std::string(text.substr(position, start - position)),
			                         compiled(text.substr(start + 2, end - start - 2), place)});
			position = end + 2;
		}
		shape->pieces.push_back({std::string(text.substr(position)), nullptr});
	} catch(const DocumentError&) {
		shape->whole = false; // its expression is invalid: run() reports it
		shape->failure = std::current_exception();
	}
	return shape;
}

Value Machine::runTemplate(std::string_view text, const Place& place) {
	if(!text.contains("{{")) return Value::fromText(text);
	return runTemplate(*templateOf(text, place), place);
}

Value Machine::runTemplate(const Template& shape, const Place& place) {
	if(shape.whole) return run(shape.pieces.front().expression->program, place);

	std::string result;
	for(const Template::Piece& piece : shape.pieces) {
		appendText(result, piece.text, place);
		if(piece.expression) appendText(result, run(piece.expression->program, place).toText(), place);
	}
	if(shape.failure) std::rethrow_exception(shape.failure); // after the placeholders before it, as they come
	return built(Value::fromText(std::move(result)), place);
}

std::vector<Value> Machine::segments(std::string_view path, const Place& place) {
	std::vector<Value> result;
	std::size_t i = 0;
	result.push_back(identifierAt(path, i, place));
	while(i < path.size()) {
		if(path[i] == '.') result.push_back(identifierAt(path, ++i, place));
		else if(path[i] == '[') result.push_back(bracketAt(path, ++i, place));
		else fail(place, "invalid path '" + std::string(path) + "'");
	}
	return result;
}

Value Machine::identifierAt(std::string_view path, std::size_t& i, const Place& place) {
	const std::size_t start = i;
	while(i < path.size() && (std::isalnum(static_cast<unsigned char>(path[i])) || path[i] == '_' || path[i] == '-'))
		++i;
	if(i == start) fail(place, "invalid path '" + std::string(path) + "'");
	return Value::fromText(path.substr(start, i - start));
}

// [3] or [state.current] or [c + 1]: the inner part can be any expression, with brackets of its own
Value Machine::bracketAt(std::string_view path, std::size_t& i, const Place& place) {
	int depth = 1;
	const std::size_t start = i;
	for(; i < path.size() && depth > 0; ++i) {
		if(path[i] == '[') ++depth;
		if(path[i] == ']') --depth;
	}
	if(depth != 0) fail(place, "unclosed '[' in path '" + std::string(path) + "'");

	const std::string_view inner = path.substr(start, i - start - 1);
	const bool number = !inner.empty() &&
	                    std::ranges::all_of(inner, [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
	const std::optional<std::int64_t> index = number ? wholeNumber(inner) : std::nullopt;
	if(number && !index) fail(place, "index '" + std::string(inner) + "' is too large");
	Value key = index ? Value::fromInteger(*index) : run(compiled(inner, place)->program, place);
	if(!key.isInteger() && !key.isText())
		fail(place, "index '" + std::string(inner) + "' is neither a number nor a key");
	return key;
}

Value Machine::lookup(std::string_view path, const Place& place) {
	const std::vector<Value> parts = segments(path, place);
	std::vector<PathKey> keys;
	keys.reserve(parts.size() - 1);
	for(std::size_t i = 1; i < parts.size(); ++i) keys.push_back(keyOf(parts[i]));
	const std::string_view root = parts.front().text();
	return walk(rootOf(root), root, keys, place);
}

} // namespace affresco::format::detail
