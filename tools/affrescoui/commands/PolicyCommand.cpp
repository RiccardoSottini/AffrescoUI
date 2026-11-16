/// @file
/// @brief `affrescoui policy test`: plays the example revisions and command runs of `*.policy.json` files against their
/// screens' permissions and reports the ones whose decision (allow, ask, person or deny) isn't the one expected
/// (VERSION_CONTROL.md, "Policy tests"). Nothing is applied and nothing runs.

#include "Cli.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/notation/Notation.hpp>
#include <affresco/vcs/Repository.hpp>
#include <affresco/vcs/Runs.hpp>

#include <array>
#include <iostream>

namespace affresco::cli {

namespace {

/// @brief The most cases one policy file holds: what it reads from a file is bounded.
constexpr std::size_t maxCases = 1000;

/// @brief What one case gave.
struct CaseResult {
	std::string name;     ///< As written.
	std::string expected; ///< "allow", "ask", "deny" or, for a run, "person".
	std::string decision; ///< What the screen's permissions decided.
	std::string reason;   ///< Why, for ask and deny.
	bool passed = false;  ///< The decision is the one expected, and the reason holds the expected text.
};

/// @brief Loads a policy file's screen as the library does ("include"s in place), with its log quiet.
/// @param policyFile The policy file.
/// @param policy Its content.
/// @return The screen's document as version control keeps it (a program's source, not what it builds, which a
/// repository builds again and would find written twice), its live state in "state".
/// @throws FileProblem If the screen can't be read or loaded.
json screenOf(const std::filesystem::path& policyFile, const json& policy) {
	const std::filesystem::path file =
	    policyFile.parent_path() / expect::string(policy.value("screen", json()), "screen");
	setLogSink([](LogLevel, std::string_view) {});
	try {
		Screen screen = Screen::fromString(lisp::jsonText(readFile(file), file), file.parent_path());
		setLogSink({});
		json document = screen.versions().sourceDocument();
		document["state"] = json::parse(screen.stateJson());
		return document;
	} catch(const std::exception& error) {
		setLogSink({});
		throw FileProblem(file.string() + ": " + error.what());
	}
}

/// @brief What judges a policy file's cases: its screen's permissions, and its commands' run rules.
struct Judges {
	const vcs::Repository& repository; ///< Judges revisions.
	const format::Runtime& runtime;    ///< Knows the commands: their arguments and what they reach.
	const vcs::Runs& runs;             ///< Judges runs.
};

/// @brief Judges a run without running it: its arguments checked as a peer's would be, then the run rules.
/// @param judges The screen's.
/// @param run The case's "run": "author", "command" and, if any, "arguments".
/// @param at Where it is, for errors.
/// @param result Gets the decision and the reason; an unknown command or wrong arguments are a "deny".
void judgeRun(const Judges& judges, const json& run, const std::string& at, CaseResult& result) {
	expect::object(run, at);
	const std::string author = expect::string(run.value("author", json()), at + ".author");
	const std::string command = expect::string(run.value("command", json()), at + ".command");
	json arguments = run.value("arguments", json::object());
	if(!arguments.is_object()) expect::fail(at + ".arguments", "expected an object");
	result.decision = "deny";
	if(!judges.runtime.command(command)) {
		result.reason = "no command '" + command + "'";
		return;
	}
	if(const std::optional<std::string> wrong = judges.runtime.checkCommandArguments(command, arguments)) {
		result.reason = *wrong;
		return;
	}
	const RunJudgement judgement = judges.runs.judge(author, command, judges.runtime.commandReach(command));
	result.decision = toString(judgement.verdict);
	result.reason = judgement.reason;
}

/// @brief Plays one case.
/// @param judges The screen's.
/// @param item The case: "name", "revision" or "run", "expect" and, if any, "reason".
/// @param at Where it is, for errors.
/// @return What it gave.
CaseResult play(const Judges& judges, const json& item, const std::string& at) {
	CaseResult result;
	result.name = expect::string(item.value("name", json()), at + ".name");
	result.expected = expect::string(item.value("expect", json()), at + ".expect");
	const bool isRun = item.contains("run");
	if(isRun == item.contains("revision")) expect::fail(at, R"(expected "revision" or "run", not both)");
	if(result.expected != "allow" && result.expected != "ask" && result.expected != "deny" &&
	   (!isRun || result.expected != "person"))
		expect::fail(at + ".expect",
		             isRun ? R"(expected "allow", "ask", "person" or "deny")" : R"(expected "allow", "ask" or "deny")");
	const std::string reason = item.contains("reason") ? expect::string(item["reason"], at + ".reason") : "";
	if(isRun) judgeRun(judges, item["run"], at + ".run", result);
	else {
		const vcs::Repository::Judgement judgement = judges.repository.judge(item["revision"]);
		result.decision = judgement.decision;
		result.reason = judgement.reason;
	}
	result.passed = result.decision == result.expected && result.reason.contains(reason);
	return result;
}

/// @brief Plays a policy file.
/// @param file The file.
/// @return Each case's result, in order.
/// @throws FileProblem If the file, or its screen, is malformed.
std::vector<CaseResult> playFile(const std::filesystem::path& file) {
	try {
		const json policy = notation::readDocument(readFile(file), file);
		expect::object(policy, "policy");
		const json& cases = policy.value("cases", json());
		if(!cases.is_array() || cases.empty() || cases.size() > maxCases)
			expect::fail("cases", "expected a list of 1 to 1000 cases");
		const format::Rules rules;
		vcs::Repository repository(rules, screenOf(file, policy));
		const json roles = policy.value("roles", json::object()); // named: a loop over a temporary's items dangles
		for(const auto& [author, role] : expect::object(roles, "roles").items())
			if(const auto refused = repository.roles().set(author, expect::string(role, "roles." + author)))
				expect::fail("roles." + author, *refused);
		setLogSink([](LogLevel, std::string_view) {});
		const format::Runtime runtime(repository, file.parent_path());
		const vcs::Runs runs(repository, rules);
		setLogSink({});
		const Judges judges{repository, runtime, runs};
		std::vector<CaseResult> results;
		results.reserve(cases.size());
		for(std::size_t i = 0; i < cases.size(); ++i)
			results.push_back(play(judges, expect::object(cases[i], "cases"), "cases[" + std::to_string(i) + "]"));
		return results;
	} catch(const DocumentError& error) {
		throw FileProblem(file.string() + ": " + error.what());
	} catch(const json::exception& error) {
		throw FileProblem(file.string() + ": " + error.what());
	}
}

/// @brief Prints a file's results.
/// @param file The file.
/// @param results Its results.
/// @param asJson One JSON line instead of text.
void report(const std::filesystem::path& file, const std::vector<CaseResult>& results, bool asJson) {
	std::size_t failed = 0;
	json list = json::array();
	for(const CaseResult& result : results) {
		failed += result.passed ? 0 : 1;
		list.push_back({{"name", result.name},
		                {"expected", result.expected},
		                {"decision", result.decision},
		                {"reason", result.reason},
		                {"passed", result.passed}});
		if(!asJson && !result.passed)
			std::cout << file.string() << ": FAIL " << result.name << ": expected " << result.expected << ", got "
			          << result.decision << (result.reason.empty() ? "" : " (" + result.reason + ")") << '\n';
	}
	if(asJson) std::cout << json{{"file", file.string()}, {"failed", failed}, {"cases", list}}.dump() << '\n';
	else std::cout << file.string() << ": " << results.size() - failed << " passed, " << failed << " failed\n";
}

} // namespace

Exit policyCommand(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	if(args.empty() || args[0] != "test") throw UsageError("expected \"test\"");
	const Arguments arguments(args.subspan(1), {}, flags);
	if(arguments.positional().empty()) throw UsageError("expected one policy file or more");
	bool passed = true;
	for(const std::string& file : arguments.positional()) {
		const std::vector<CaseResult> results = playFile(file);
		passed = passed && std::ranges::all_of(results, &CaseResult::passed);
		report(file, results, arguments.flag("--json"));
	}
	return passed ? Exit::Success : Exit::Problems;
}

} // namespace affresco::cli
