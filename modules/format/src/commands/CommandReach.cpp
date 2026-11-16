/// @file
/// @brief What a command can reach (CommandReach.hpp).

#include "CommandReach.hpp"
#include "Picks.hpp"
#include "Programs.hpp"
#include "ProgramRules.hpp"
#include "RulesControl.hpp"
#include "ServiceRules.hpp"

#include <affresco/lisp/Document.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// How many `run`s of the document's own actions the reason of a control step is followed through.
constexpr int maxReasonDepth = 16;
/// How many rounds between the document's actions and the program's functions the walk takes.
constexpr int maxRounds = 16;

/// @brief A name for a message: cut short.
/// @param name The name.
/// @return It, at most 64 bytes, quoted.
std::string quoted(const std::string& name) { return "'" + cutToCharacter(name, 64) + "'"; }

/// @brief Says why a command may run a control step, from the control scan's findings (controlFindings()).
class Reasons {
public:
	/// @brief Sets it up.
	/// @param document The document.
	/// @param scan What the scan found, for the command and every action it may run.
	Reasons(const json& document, ControlFindings scan) : document_(document), scan_(std::move(scan)) {}

	/// @brief Why an action may run one.
	/// @param name The action.
	/// @param depth How many runs led here.
	/// @return The reason, or "" if it can't.
	std::string of(const std::string& name, int depth = 0) {
		const auto found = scan_.findings.find(name);
		if(found == scan_.findings.end() || found->second.empty())
			return scan_.control.contains(name) ? "a control or a review step" : "";
		const std::string& first = found->second.front();
		const std::size_t start = std::string("actions.").size() + name.size();
		const std::size_t colon = first.find(": ", std::min(start, first.size()));
		return describe(colon == std::string::npos ? first : first.substr(colon + 2), name, depth);
	}

private:
	/// @brief Puts one finding in words.
	/// @param finding What follows its place: an object's JSON, `run <name>`, `lisp <name>` or `use ...`.
	/// @param from The action it was found in.
	/// @param depth How many runs led here.
	/// @return The words.
	std::string describe(const std::string& finding, const std::string& from, int depth) {
		if(finding.starts_with("use ")) return "a component that runs computed actions";
		if(finding.starts_with("lisp ")) {
			const json target = json::parse(finding.substr(5), nullptr, false);
			const std::string name = target.is_string() ? target.get<std::string>() : "";
			if(!scan_.program.defined.contains(name)) return "the function " + quoted(name) + ", which isn't defined";
			return "the function " + quoted(name) + " reaches a control or a review step";
		}
		if(finding.starts_with("run ")) return describeRun(json::parse(finding.substr(4), nullptr, false), from, depth);
		const json step = json::parse(finding, nullptr, false);
		if(step.is_object())
			for(const auto& [key, value] : step.items())
				if(stepKind(key) == StepKind::Control || stepKind(key) == StepKind::Review) return key;
		return "a control or a review step";
	}

	/// @brief Puts a `run` that may control in words: the reason of the action it names, if it is the document's.
	/// @param target What it runs.
	/// @param from The action it was found in.
	/// @param depth How many runs led here.
	/// @return The words.
	std::string describeRun(const json& target, const std::string& from, int depth) {
		if(!target.is_string() || target.get_ref<const std::string&>().contains("{{")) return "a computed run";
		const std::string& name = target.get_ref<const std::string&>();
		if(document_["actions"].contains(name)) {
			if(depth >= maxReasonDepth || name == from) return "run " + quoted(name);
			if(!scan_.findings.contains(name)) {
				std::map<std::string, std::vector<std::string>> more = controlFindings(document_, {name}).findings;
				scan_.findings.merge(more);
			}
			const std::string inner = of(name, depth + 1);
			return inner.empty() ? "run " + quoted(name) : inner;
		}
		if(scan_.program.defined.contains(name))
			return "the function " + quoted(name) + " reaches a control or a review step";
		return "the application's own action " + quoted(name);
	}

	const json& document_; ///< The document.
	ControlFindings scan_; ///< What the scan found.
};

/// @brief Walks what a command runs: its steps, the actions they run, the program's functions, back and forth.
class Walk {
public:
	/// @brief Sets it up.
	/// @param document The document.
	explicit Walk(const json& document) : document_(document) {}

	/// @brief Walks a command and everything it runs.
	/// @param name The command.
	void command(const std::string& name) {
		follow(name);
		for(int round = 0; round < maxRounds; ++round) {
			const ProgramFollow program = followProgram(document_, functions_, hooks_);
			uses_.insert(program.uses.begin(), program.uses.end());
			writesRecords_ = writesRecords_ || program.writesRecords;
			const std::size_t before = actions_.size();
			for(const std::string& run : program.runs) follow(run);
			if(actions_.size() == before) break;
		}
	}

	/// @brief The uses found: step and name.
	[[nodiscard]] const std::set<std::pair<std::string, std::string>>& uses() const noexcept { return uses_; }
	/// @brief Whether a step writes records.
	[[nodiscard]] bool writesRecords() const noexcept { return writesRecords_; }

private:
	/// @brief Follows a name run: the document's action, walked once; else the program's function.
	/// @param name The name.
	void follow(const std::string& name) {
		functions_.push_back(name); // a function by that name, or the advice on the action
		const auto actions = document_.find("actions");
		if(actions == document_.end() || !actions->contains(name) || !actions_.insert(name).second) return;
		walk((*actions)[name]);
	}

	/// @brief Walks a value of steps.
	/// @param value The value.
	void walk(const json& value) {
		if(value.is_array()) {
			for(const json& item : value) walk(item);
			return;
		}
		if(!value.is_object()) return;
		for(const auto& [key, item] : value.items()) {
			note(key, item);
			walk(item);
		}
	}

	/// @brief Notes what one key of a step does: a use written out, a run, a function, hooks, a write of records.
	/// @param key The key.
	/// @param item Its value.
	void note(const std::string& key, const json& item) {
		static constexpr std::array writes = {"insert", "update", "delete", "undoData", "redoData", "importCsv"};
		// Keys whose text names an action to run, as the checker and the steps read them
		static constexpr std::array handlers = {"run", "do", "then", "else", "onDone", "onError", "onEvent"};
		hooks_ = hooks_ || key == "runHooks";
		writesRecords_ = writesRecords_ || std::ranges::find(writes, key) != writes.end();
		if(!item.is_string() || item.get_ref<const std::string&>().contains("{{")) return;
		const std::string& text = item.get_ref<const std::string&>();
		if((key == "invoke" || key == "request" || key == "subscribe") && judgedByUse(key, text))
			uses_.emplace(key, text);
		else if(std::ranges::find(handlers, key) != handlers.end()) follow(text);
		else if(key == "lisp") functions_.push_back(text);
	}

	const json& document_;                               ///< The document.
	std::set<std::string> actions_;                      ///< The document's actions walked.
	std::vector<std::string> functions_;                 ///< The functions run, and the actions whose advice runs.
	std::set<std::pair<std::string, std::string>> uses_; ///< The uses written out.
	bool hooks_ = false;                                 ///< A `runHooks` step runs every hook.
	bool writesRecords_ = false;                         ///< A step writes records.
};

} // namespace

CommandReach reachOf(const json& document, const std::string& name, const services::Services* services) {
	CommandReach reach{false, "", {}, false};
	try {
		if(lisp::hasProgram(document)) static_cast<void>(lisp::builtProgram(document["program"]));
		Walk walk(document);
		walk.command(name);
		reach.writesRecords = walk.writesRecords();
		for(const auto& [step, use] : walk.uses())
			if(std::optional<std::string> line = serviceUseLine(document, step, use, services))
				reach.personUses.push_back(*std::move(line));
	} catch(const DocumentError& error) {
		return {true, "the program can't be built: " + cutToCharacter(error.what(), 160), {}, false};
	}
	std::ranges::sort(reach.personUses);
	reach.personUses.erase(std::ranges::unique(reach.personUses).begin(), reach.personUses.end());
	Reasons reasons(document, controlFindings(document, {name}));
	reach.controlStep = reasons.of(name);
	reach.control = !reach.controlStep.empty();
	return reach;
}

} // namespace affresco::format
