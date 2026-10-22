/// @file
/// @brief Material node graphs (MaterialGraph.hpp).

#include "affresco/scene3d/MaterialGraph.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <map>

namespace affresco::scene3d {

namespace {

/// @brief One input port of an operation.
struct PortSpec {
	const char* id = "";       ///< Its id, unique in the operation.
	const char* label = "";    ///< Shown to people.
	const char* kind = "";     ///< "color" or "float".
	const char* fallback = ""; ///< The GLSL used while it isn't linked (a fixed constant).
};

/// @brief One operation: its ports and its snippet ($0 to $3 are the inputs, $v the node's value).
struct OperationSpec {
	const char* name = "";            ///< Its name, in a node's `op`.
	const char* title = "";           ///< Shown on the node.
	std::array<PortSpec, 4> inputs{}; ///< Its inputs; unused ones have an empty id.
	const char* outputKind = "";      ///< "color", "float", or empty for the output node.
	const char* snippet = "";         ///< The GLSL expression.
};

/// @brief The operations, in menu order.
const std::array<OperationSpec, 12> operations{{
    {"output",
	 "Material output",
	 {{{"baseColor", "Base colour", "color", "baseColor"},
	   {"metallic", "Metallic", "float", "metallic"},
	   {"roughness", "Roughness", "float", "roughness"},
	   {"emissive", "Emission", "color", "emissive"}}},
	 "",
	 ""},
    {"color", "Colour", {}, "color", "$v"},
    {"number", "Number", {}, "float", "$v"},
    {"mix",
	 "Mix",
	 {{{"a", "A", "color", "vec3(0.0)"}, {"b", "B", "color", "vec3(1.0)"}, {"factor", "Factor", "float", "0.5"}}},
	 "color",
	 "mix($0, $1, clamp($2, 0.0, 1.0))"},
    {"multiply",
	 "Multiply",
	 {{{"a", "A", "color", "vec3(1.0)"}, {"b", "B", "color", "vec3(1.0)"}}},
	 "color",
	 "$0 * $1"},
    {"add", "Add", {{{"a", "A", "color", "vec3(0.0)"}, {"b", "B", "color", "vec3(0.0)"}}}, "color", "$0 + $1"},
    {"scale",
	 "Scale",
	 {{{"color", "Colour", "color", "vec3(1.0)"}, {"factor", "Factor", "float", "1.0"}}},
	 "color",
	 "$0 * $1"},
    {"invert", "Invert", {{{"value", "Value", "float", "0.0"}}}, "float", "1.0 - $0"},
    {"fresnel",
	 "Fresnel",
	 {{{"power", "Power", "float", "5.0"}}},
	 "float",
	 "pow(1.0 - clamp(dot(normalize(normal), normalize(toEye)), 0.0, 1.0), max($0, 0.0))"},
    {"checker",
	 "Checker",
	 {{{"scale", "Scale", "float", "4.0"}}},
	 "float",
	 "mod(floor(position.x * $0) + floor(position.y * $0) + floor(position.z * $0), 2.0)"},
    {"noise", "Noise", {{{"scale", "Scale", "float", "4.0"}}}, "float", "graphNoise(position * $0)"},
    {"height", "Height", {}, "float", "clamp(position.y * 0.5 + 0.5, 0.0, 1.0)"},
}};

/// @brief The value noise the "noise" snippet calls: fixed GLSL, the same in every graph.
constexpr const char* noiseFunction = R"(float graphHash(vec3 p) {
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}
float graphNoise(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = mix(mix(graphHash(i), graphHash(i + vec3(1, 0, 0)), f.x), mix(graphHash(i + vec3(0, 1, 0)), graphHash(i + vec3(1, 1, 0)), f.x), f.y);
	float b = mix(mix(graphHash(i + vec3(0, 0, 1)), graphHash(i + vec3(1, 0, 1)), f.x), mix(graphHash(i + vec3(0, 1, 1)), graphHash(i + vec3(1, 1, 1)), f.x), f.y);
	return mix(a, b, f.z);
}
)";

/// @brief A checked node.
struct GraphNode {
	const OperationSpec* operation = nullptr; ///< What it does.
	std::string value;                        ///< Its value as GLSL the host formatted, for "color" and "number".
	std::array<std::string, 4> inputs;        ///< Per input, the id of the node linked to it, or empty.
};

/// @brief The operation of a name. @param name It. @return It, or null.
[[nodiscard]] const OperationSpec* operationNamed(std::string_view name) noexcept {
	for(const auto& operation : operations)
		if(name == operation.name) return &operation;
	return nullptr;
}

/// @brief An input's index. @param operation The operation. @param id The port. @return It, or -1.
[[nodiscard]] int inputIndex(const OperationSpec& operation, std::string_view id) noexcept {
	for(std::size_t i = 0; i < operation.inputs.size(); ++i)
		if(*operation.inputs[i].id != '\0' && id == operation.inputs[i].id) return static_cast<int>(i);
	return -1;
}

/// @brief A number as GLSL. @param value It. @return A float literal, always with a point.
[[nodiscard]] std::string literal(double value) {
	return std::format("{:.6f}", std::isfinite(value) ? std::clamp(value, -1e6, 1e6) : 0.0);
}

/// @brief Text for an error, shortened. @param text It. @return At most 64 characters.
[[nodiscard]] std::string shown(std::string_view text) { return std::string(text.substr(0, 64)); }

/// @brief A number node's value as GLSL. @param node The JSON. @param errors Receives a problem.
/// @return The literal, 0.5 by default.
[[nodiscard]] std::string numberValue(const json& node, std::vector<std::string>& errors) {
	const auto found = node.find("value");
	if(found != node.end() && !found->is_number()) errors.emplace_back("a number node's value must be a number");
	return literal(found != node.end() && found->is_number() ? found->get<double>() : 0.5);
}

/// @brief A colour node's value as GLSL. @param node The JSON. @param errors Receives a problem.
/// @return The vec3, a light grey by default.
[[nodiscard]] std::string colorValue(const json& node, std::vector<std::string>& errors) {
	std::array<double, 3> rgb{0.8, 0.8, 0.8};
	if(const auto found = node.find("value"); found != node.end()) {
		if(!found->is_array() || found->size() != 3 ||
		   !std::ranges::all_of(*found, [](const json& c) { return c.is_number(); }))
			errors.emplace_back("a colour node's value must be three numbers");
		else
			for(std::size_t i = 0; i < 3; ++i) rgb[i] = (*found)[i].get<double>();
	}
	return "vec3(" + literal(rgb[0]) + ", " + literal(rgb[1]) + ", " + literal(rgb[2]) + ")";
}

/// @brief A node's value as GLSL. @param node The JSON. @param operation Its operation.
/// @param errors Receives a problem. @return The literal, or empty for operations without one.
[[nodiscard]] std::string valueOf(const json& node, const OperationSpec& operation, std::vector<std::string>& errors) {
	const std::string_view name = operation.name;
	if(name == "number") return numberValue(node, errors);
	if(name == "color") return colorValue(node, errors);
	return {};
}

/// @brief Reads one node into the nodes. @param node The JSON. @param nodes The nodes so far.
/// @param errors Receives problems. @param output Receives the output node's id.
void readNode(const json& node, std::map<std::string, GraphNode, std::less<>>& nodes, std::vector<std::string>& errors,
              std::string& output) {
	const auto id = node.is_object() ? node.find("id") : node.end();
	const auto op = node.is_object() ? node.find("op") : node.end();
	if(id == node.end() || !id->is_string() || op == node.end() || !op->is_string()) {
		errors.emplace_back("every node needs an id and an op");
		return;
	}
	const OperationSpec* operation = operationNamed(op->get<std::string>());
	const auto& key = id->get_ref<const std::string&>();
	if(operation == nullptr) {
		errors.push_back("the node " + shown(key) + " has an unknown op " + shown(op->get<std::string>()));
		return;
	}
	if(nodes.contains(key)) {
		errors.push_back("two nodes are called " + shown(key));
		return;
	}
	if(std::string_view(operation->name) == "output") {
		if(!output.empty()) errors.emplace_back("a graph has one output node");
		output = key;
	}
	nodes.emplace(key, GraphNode{operation, valueOf(node, *operation, errors), {}});
}

/// @brief Reads the nodes. @param graph The graph. @param limits The limits. @param errors Receives problems.
/// @param output Receives the output node's id. @return The nodes by id.
[[nodiscard]] std::map<std::string, GraphNode, std::less<>>
readNodes(const json& graph, const MaterialGraphLimits& limits, std::vector<std::string>& errors, std::string& output) {
	std::map<std::string, GraphNode, std::less<>> nodes;
	const auto list = graph.find("nodes");
	if(list == graph.end() || !list->is_array()) {
		errors.emplace_back("the graph needs a list of nodes");
		return nodes;
	}
	if(list->size() > limits.nodes) errors.push_back(std::format("more than {} nodes", limits.nodes));
	for(const auto& node : *list) readNode(node, nodes, errors, output);
	if(output.empty()) errors.emplace_back("the graph has no output node");
	return nodes;
}

/// @brief Reads one link into its target. @param link The JSON. @param nodes The nodes. @param errors Receives
/// problems.
void readLink(const json& link, std::map<std::string, GraphNode, std::less<>>& nodes,
              std::vector<std::string>& errors) {
	const auto member = [&](const char* key) -> std::string {
		const auto found = link.is_object() ? link.find(key) : link.end();
		return found != link.end() && found->is_string() ? found->get<std::string>() : std::string();
	};
	const std::string from = member("from"), output = member("output"), to = member("to"), input = member("input");
	const auto source = nodes.find(from), target = nodes.find(to);
	if(source == nodes.end() || target == nodes.end()) {
		errors.emplace_back("a link joins a node that isn't there");
		return;
	}
	const int index = inputIndex(*target->second.operation, input);
	if(*source->second.operation->outputKind == '\0' || output != "out" || index < 0) {
		errors.push_back("a link from " + shown(from) + " to " + shown(to) + " names a port that isn't there");
		return;
	}
	const auto& port = target->second.operation->inputs[static_cast<std::size_t>(index)];
	if(std::string_view(port.kind) != source->second.operation->outputKind)
		errors.push_back("a " + std::string(source->second.operation->outputKind) + " can't go into " + shown(to) +
		                 "." + port.id + ", which takes a " + port.kind);
	else if(!target->second.inputs[static_cast<std::size_t>(index)].empty())
		errors.push_back(shown(to) + "." + port.id + " has two links");
	else target->second.inputs[static_cast<std::size_t>(index)] = from;
}

/// @brief Whether following inputs from a node comes back to a node on the path.
/// @param nodes The nodes. @param id Where to start. @param state Per node: 1 on the path, 2 done.
/// @param depth How deep the walk is. @return true if there is a cycle.
[[nodiscard]] bool hasCycle(const std::map<std::string, GraphNode, std::less<>>& nodes, const std::string& id,
                            std::map<std::string, int, std::less<>>& state, std::size_t depth) {
	auto& mark = state[id];
	if(mark == 2) return false;
	if(mark == 1 || depth > nodes.size()) return true;
	mark = 1;
	for(const auto& input : nodes.at(id).inputs)
		if(!input.empty() && hasCycle(nodes, input, state, depth + 1)) return true;
	state[id] = 2;
	return false;
}

/// @brief Writes a node and what feeds it, each once, in order.
/// @param nodes The nodes. @param id The node. @param names The variables written so far, by node id.
/// @param body Receives the lines. @return The node's variable.
std::string emit(const std::map<std::string, GraphNode, std::less<>>& nodes, const std::string& id,
                 std::map<std::string, std::string, std::less<>>& names, std::string& body) {
	if(const auto done = names.find(id); done != names.end()) return done->second;
	const GraphNode& node = nodes.at(id);
	std::string expression = node.operation->snippet;
	for(std::size_t i = 0; i < node.inputs.size(); ++i) {
		if(*node.operation->inputs[i].id == '\0') continue;
		const std::string argument =
		    node.inputs[i].empty() ? node.operation->inputs[i].fallback : emit(nodes, node.inputs[i], names, body);
		const std::string mark = "$" + std::to_string(i);
		for(auto at = expression.find(mark); at != std::string::npos; at = expression.find(mark, at + argument.size()))
			expression.replace(at, mark.size(), argument);
	}
	if(const auto at = expression.find("$v"); at != std::string::npos) expression.replace(at, 2, node.value);
	const std::string name = "n" + std::to_string(names.size());
	names.emplace(id, name);
	body += std::format("\t{} {} = {};\n", std::string_view(node.operation->outputKind) == "color" ? "vec3" : "float",
	                    name, expression);
	return name;
}

} // namespace

const std::vector<std::string>& materialOperations() {
	static const std::vector<std::string> names = [] {
		std::vector<std::string> list;
		for(const auto& operation : operations) list.emplace_back(operation.name);
		return list;
	}();
	return names;
}

json makeMaterialNode(std::string_view operation, std::string_view id, double x, double y) {
	const OperationSpec* spec = operationNamed(operation);
	if(spec == nullptr) return nullptr;
	json inputs = json::array();
	for(const auto& port : spec->inputs)
		if(*port.id != '\0')
			inputs.push_back({{"id", port.id}, {"label", port.label}, {"kind", port.kind}, {"links", 1}});
	json outputs = json::array();
	if(*spec->outputKind != '\0')
		outputs.push_back({{"id", "out"},
		                   {"label", std::string_view(spec->outputKind) == "color" ? "Colour" : "Value"},
		                   {"kind", spec->outputKind}});
	json node{{"id", id}, {"op", spec->name}, {"title", spec->title}, {"x", x},
	          {"y", y},   {"inputs", inputs}, {"outputs", outputs}};
	if(operation == "number") node["value"] = 0.5;
	if(operation == "color") node["value"] = {0.8, 0.8, 0.8};
	return node;
}

MaterialGraphResult compileMaterialGraph(const json& graph, const MaterialGraphLimits& limits) {
	MaterialGraphResult result;
	if(!graph.is_object()) {
		result.errors.emplace_back("the graph must be an object");
		return result;
	}
	std::string output;
	auto nodes = readNodes(graph, limits, result.errors, output);
	if(const auto links = graph.find("links"); links != graph.end()) {
		if(!links->is_array()) result.errors.emplace_back("the links must be a list");
		else if(links->size() > limits.links) result.errors.push_back(std::format("more than {} links", limits.links));
		else
			for(const auto& link : *links) readLink(link, nodes, result.errors);
	}
	std::map<std::string, int, std::less<>> state;
	for(const auto& [id, node] : nodes)
		if(hasCycle(nodes, id, state, 0)) {
			result.errors.push_back("the links make a cycle through " + shown(id));
			break;
		}
	if(!result.ok()) return result;
	std::map<std::string, std::string, std::less<>> names;
	std::string body;
	const GraphNode& out = nodes.at(output);
	for(std::size_t i = 0; i < out.inputs.size(); ++i)
		if(!out.inputs[i].empty()) {
			const std::string value = emit(nodes, out.inputs[i], names, body);
			body += std::format("\t{} = {};\n", out.operation->inputs[i].fallback, value);
		}
	result.glsl =
	    std::string(noiseFunction) +
	    "void graphMaterial(vec3 position, vec3 normal, vec3 toEye, inout vec3 baseColor, inout float metallic, "
	    "inout float roughness, inout vec3 emissive) {\n" +
	    body + "}\n";
	return result;
}

} // namespace affresco::scene3d
