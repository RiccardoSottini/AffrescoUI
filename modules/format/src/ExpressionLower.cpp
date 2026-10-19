/// @file
/// @brief Lowering operator trees into typed Nodes, once per expression: what evaluating the JSON form does, decided
/// ahead of time (which function, which keys, which parts are constant).

#include "ExpressionMachine.hpp"

#include <algorithm>

namespace affresco::format::detail {
namespace {

/// @brief A node that gives a value.
/// @param value The value.
/// @return The node.
Node literal(Value value) {
	Node node;
	node.value = std::move(value);
	return node;
}

/// @brief A node for an invalid operator, which fails if it is ever evaluated.
/// @param label The operator, e.g. "$if".
/// @param problem What is wrong.
/// @return The node.
Node failing(std::string_view label, std::string_view problem) {
	Node node;
	node.kind = Node::Kind::Fail;
	node.label = label;
	node.text = problem;
	return node;
}

/// @brief Tells whether every child is a literal, so that the node can be one too.
/// @param node The node.
/// @return true if it is constant.
bool constant(const Node& node) {
	return std::ranges::all_of(node.children, [](const Node& child) { return child.kind == Node::Kind::Literal; });
}

/// @brief A list whose items are evaluated one by one.
/// @param items The JSON array.
/// @return The node.
Node lowerList(const json& items) {
	Node node;
	node.kind = Node::Kind::List;
	node.children.reserve(items.size());
	for(const json& item : items) node.children.push_back(lower(item));
	if(!constant(node)) return node;
	std::vector<Value> values;
	values.reserve(node.children.size());
	for(Node& child : node.children) values.push_back(std::move(child.value));
	return literal(Value::fromList(std::move(values)));
}

/// @brief An object whose fields are evaluated one by one.
/// @param fields The JSON (iterated as evaluating it did: an array's keys are "0", "1", ...).
/// @param label "$object", or empty for an object written as it is.
/// @return The node.
Node lowerObject(const json& fields, std::string_view label) {
	Node node;
	node.kind = Node::Kind::Object;
	node.label = label;
	for(const auto& [key, item] : fields.items()) {
		node.names.push_back(key);
		node.children.push_back(lower(item));
	}
	if(!constant(node)) return node;
	std::vector<Value::Field> values;
	values.reserve(node.children.size());
	for(std::size_t i = 0; i < node.children.size(); ++i)
		values.push_back({std::move(node.names[i]), std::move(node.children[i].value)});
	return literal(Value::fromObject(std::move(values)));
}

/// @brief A path: `{"$get": [root, key, ...]}`, where a string root is a variable and a string key a name.
/// @param raw The arguments.
/// @return The node.
Node lowerGet(const json& raw) {
	if(!raw.is_array() || raw.empty()) return failing("$get", "expected [variable, key, ...]");
	Node node;
	node.kind = Node::Kind::Get;
	node.label = "$get";
	for(std::size_t i = 1; i < raw.size(); ++i) {
		Key key;
		if(raw[i].is_string()) {
			key.name = raw[i].get<std::string>();
		} else if(raw[i].is_number_integer()) {
			key.kind = Key::Kind::Index;
			key.index = raw[i].get<std::int64_t>();
		} else {
			key.kind = Key::Kind::Expression;
			key.child = node.children.size();
			node.children.push_back(lower(raw[i]));
		}
		node.keys.push_back(std::move(key));
	}
	if(raw[0].is_string()) {
		node.text = raw[0].get<std::string>();
		node.root = rootOf(node.text);
	} else { // a path from a computed value, e.g. (a ?? b).name
		node.fromValue = true;
		node.children.push_back(lower(raw[0]));
	}
	return node;
}

/// @brief A function, or an operator no function has (which fails once its arguments are evaluated).
/// @param name The operator, e.g. "$clamp".
/// @param raw Its arguments.
/// @return The node.
Node lowerCall(const std::string& name, const json& raw) {
	Node node;
	node.kind = Node::Kind::Call;
	node.label = name;
	node.function = findOperator(name);
	if(node.function && node.function->takes == Takes::One) { // the value directly: {"$length": [1, 2, 3]}
		node.children.push_back(lower(raw));
		return node;
	}
	if(!raw.is_array())
		return failing(name, node.function && node.function->decide
		                         ? node.function->shape
								 : std::string_view("expected an array of arguments"));
	node.indexed = !node.function || node.function->apply;
	node.children.reserve(raw.size());
	for(const json& item : raw) node.children.push_back(lower(item));
	return node;
}

/// @brief One `$` operator.
/// @param name The operator.
/// @param raw Its arguments.
/// @return The node.
Node lowerOperator(const std::string& name, const json& raw) {
	if(name == "$value") return literal(Value::copyJson(raw));
	if(name == "$object") return lowerObject(raw, "$object");
	if(name == "$get") return lowerGet(raw);
	if(name == "$path") { // a path from before expressions
		if(!raw.is_string()) return failing("$path", "expected a path written as text");
		Node node;
		node.kind = Node::Kind::Path;
		node.text = raw.get<std::string>();
		return node;
	}
	return lowerCall(name, raw);
}

} // namespace

Node lower(const json& tree) {
	if(tree.is_string()) {
		const std::string& text = tree.get_ref<const std::string&>();
		if(!text.contains("{{")) return literal(Value::fromText(text));
		Node node;
		node.kind = Node::Kind::Template;
		node.text = text;
		return node;
	}
	if(tree.is_object()) {
		if(tree.size() == 1 && tree.begin().key().starts_with('$'))
			return lowerOperator(tree.begin().key(), *tree.begin());
		return lowerObject(tree, {});
	}
	if(tree.is_array()) return lowerList(tree);
	return literal(Value::copyJson(tree));
}

Root rootOf(std::string_view name) noexcept {
	if(name == "state") return Root::State;
	if(name == "window") return Root::Window;
	if(name == "revision") return Root::Revision;
	if(name == "event") return Root::Event;
	if(name == "computed") return Root::Computed;
	if(name == "nodes") return Root::Nodes;
	return Root::Named;
}

std::size_t closing(std::string_view text, std::size_t from) noexcept {
	char quote = 0;
	int braces = 0;
	for(std::size_t i = from; i + 1 < text.size(); ++i) {
		if(quote) {
			if(text[i] == '\\') ++i;
			else if(text[i] == quote) quote = 0;
		} else if(text[i] == '\'' || text[i] == '"') {
			quote = text[i];
		} else if(text[i] == '{') {
			++braces;
		} else if(text[i] == '}' && braces > 0) {
			--braces;
		} else if(text[i] == '}' && text[i + 1] == '}') {
			return i;
		}
	}
	return std::string_view::npos;
}

} // namespace affresco::format::detail
