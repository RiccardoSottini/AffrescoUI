/// @file
/// @brief Undoing an earlier change, as the in-process agent offers it to a model (Undo.hpp).

#include "Undo.hpp"

#include <algorithm>
#include <utility>

namespace affresco::intelligence {

using affresco::json;

const char* const undoRules =
    "To undo an earlier change, use the undo (revert_revision with tools, \"undo\" in your answer without them), "
    "never a revision you write as its inverse: it keeps every later change that doesn't depend on the undone one. "
    "Read its plan first. Never drop a later change the person didn't ask to lose, unless the request says so: keep "
    "it, rebuild it with then, or ask the person.";

const char* const undoAnswerRules =
    "Without tools, an undo is {\"reply\": \"...\", \"undo\": {\"revision\": 12, \"drop\": [...], \"keep\": [...], "
    "\"then\": [...], \"head\": \"...\", \"message\": \"...\"}} in place of the revision (each choice optional), and "
    "its plan is {\"plan\": 12} in a reading round, beside read, find and guide.";

namespace {

/// @brief Whether a drop or keep choice is a list of paths or "all", counting its paths.
/// @param choice The choice.
/// @param paths Increased by its paths.
/// @return Whether it is.
bool isPaths(const json& choice, std::size_t& paths) {
	if(choice.is_string()) return choice.get_ref<const std::string&>() == "all";
	if(!choice.is_array()) return false;
	for(const json& path : choice)
		if(!path.is_string()) return false;
	paths += choice.size();
	return true;
}

/// @brief Why the undo's choices can't be sent, if they can't: drop and keep, each a list of paths or "all", at most
/// maxUndoPaths paths together, and then, a list of operations.
/// @param input The model's arguments, an object.
/// @return The reason, naming the choice, or nullopt.
std::optional<std::string> choiceProblem(const json& input) {
	std::size_t paths = 0;
	for(const char* key : {"drop", "keep"})
		if(input.contains(key) && !isPaths(input[key], paths))
			return std::string(key) + ": a list of conflict paths or \"all\"";
	if(paths > maxUndoPaths) return "drop and keep: at most " + std::to_string(maxUndoPaths) + " paths together";
	const auto then = input.find("then");
	if(then == input.end()) return std::nullopt;
	if(!then->is_array() || !std::ranges::all_of(*then, [](const json& operation) { return operation.is_object(); }))
		return "then: a JSON Patch, a list of operations";
	return std::nullopt;
}

} // namespace

ToolSpec undoTool() {
	const json text = {{"type", "string"}};
	const json choice = {{"anyOf", json::array({{{"type", "array"}, {"items", text}},
	                                            {{"type", "string"}, {"enum", json::array({"all"})}}})},
	                     {"description", "Conflict paths from the plan, or \"all\""}};
	json properties = {{"revision", {{"type", json::array({"integer", "string"})}}},
	                   {"dry_run", {{"type", "boolean"}}},
	                   {"drop", choice},
	                   {"keep", choice},
	                   {"then", {{"type", "array"}, {"items", {{"type", "object"}}}}},
	                   {"head", text},
	                   {"message", text}};
	return {"revert_revision",
	        "Undo one earlier revision and keep everything after it that does not depend on it, in one new revision. "
	        "Call it with dry_run=true first: it changes nothing and returns the plan. Its outcome \"clean\" means: "
	        "call it again as it is. \"choose\" lists conflicts, later changes built on it: name each conflict's path "
	        "in drop (the later change is lost there) or keep (it stays, and that part is not undone), or \"all\", "
	        "and pass head from the plan. then adds JSON Patch operations after the undo, to keep what a later change "
	        "built in another place. With drop or then it is always a proposal the person reviews. \"blocked\" says "
	        "why it can't be done. The result: the plan, or proposed, applied, or rejected with the error.",
	        {{"type", "object"}, {"properties", std::move(properties)}, {"required", json::array({"revision"})}}};
}

std::optional<std::string> undoProblem(const json& input) {
	if(!input.is_object()) return "the undo is an object";
	const auto revision = input.find("revision");
	if(revision == input.end() || !(revision->is_number_integer() || revision->is_string()))
		return "revision: a revision number or a commit id";
	if(std::optional<std::string> problem = choiceProblem(input)) return problem;
	for(const char* key : {"head", "message"})
		if(input.contains(key) && !input[key].is_string()) return std::string(key) + ": a text";
	if(input.contains("dry_run") && !input["dry_run"].is_boolean()) return "dry_run: true or false";
	return std::nullopt;
}

json undoMessage(const std::string& type, const json& input, const std::string& author) {
	json message = {{"type", type}, {"revision", input["revision"]}};
	if(type == "revert") message["author"] = author;
	for(const char* key : {"drop", "keep", "then", "head"})
		if(input.contains(key)) message[key] = input[key];
	if(type == "revert" && input.contains("message")) message["message"] = input["message"];
	return message;
}

json planSummary(const json& plan) {
	if(!plan.is_object()) return plan;
	json summary = json::object();
	if(const auto of = plan.find("of"); of != plan.end() && of->is_object()) {
		json commit = json::object();
		for(const char* key : {"number", "short", "message"})
			if(of->contains(key)) commit[key] = (*of)[key];
		summary["of"] = std::move(commit);
	}
	for(const char* key : {"outcome", "blocked", "head", "later", "conflicts", "state", "warnings"})
		if(plan.contains(key)) summary[key] = plan[key];
	for(const char* key : {"laterMore", "conflictsMore"}) // only when there are more
		if(plan.contains(key) && plan[key].is_number() && plan[key].get<double>() > 0) summary[key] = plan[key];
	return summary;
}

bool undoConflict(const std::string& error) { return error.contains("revert: conflict:"); }

} // namespace affresco::intelligence
