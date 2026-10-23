/// @file
/// @brief `affrescoui policy test`: plays the example revisions of `*.policy.json` files against their screens'
/// permissions and reports the ones whose decision (allow, ask or deny) isn't the one expected (VERSION_CONTROL.md,
/// "Policy tests").

#include "Cli.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <array>
#include <iostream>

namespace affresco::cli {

namespace {

/// @brief The most cases one policy file holds: what it reads from a file is bounded.
constexpr std::size_t maxCases = 1000;

/// @brief What one case gave.
struct CaseResult {
	std::string name;     ///< As written.
	std::string expected; ///< "allow", "ask" or "deny".
	std::string decision; ///< What the screen's permissions decided.
	std::string reason;   ///< Why, for ask and deny.
	bool passed = false;  ///< The decision is the one expected, and the reason holds the expected text.
};

/// @brief Loads a policy file's screen as the library does ("include"s in place), with its log quiet.
/// @param policyFile The policy file.
/// @param policy Its content.
/// @return The screen's document, its live state in "state".
/// @throws FileProblem If the screen can't be read or loaded.
json screenOf(const std::filesystem::path& policyFile, const json& policy) {
	const std::filesystem::path file =
	    policyFile.parent_path() / expect::string(policy.value("screen", json()), "screen");
	setLogSink([](LogLevel, std::string_view) {});
	try {
		const Screen screen = Screen::fromString(readFile(file), file.parent_path());
		setLogSink({});
		json document = json::parse(screen.documentJson());
		document["state"] = json::parse(screen.stateJson());
		return document;
	} catch(const std::exception& error) {
		setLogSink({});
		throw FileProblem(file.string() + ": " + error.what());
	}
}

/// @brief Plays one case.
/// @param repository The screen's repository.
/// @param item The case: "name", "revision", "expect" and, if any, "reason".
/// @param at Where it is, for errors.
/// @return What it gave.
CaseResult play(const vcs::Repository& repository, const json& item, const std::string& at) {
	CaseResult result;
	result.name = expect::string(item.value("name", json()), at + ".name");
	result.expected = expect::string(item.value("expect", json()), at + ".expect");
	if(result.expected != "allow" && result.expected != "ask" && result.expected != "deny")
		expect::fail(at + ".expect", R"(expected "allow", "ask" or "deny")");
	const std::string reason = item.contains("reason") ? expect::string(item["reason"], at + ".reason") : "";
	const vcs::Repository::Judgement judgement = repository.judge(item.value("revision", json()));
	result.decision = judgement.decision;
	result.reason = judgement.reason;
	result.passed = result.decision == result.expected && result.reason.contains(reason);
	return result;
}

/// @brief Plays a policy file.
/// @param file The file.
/// @return Each case's result, in order.
/// @throws FileProblem If the file, or its screen, is malformed.
std::vector<CaseResult> playFile(const std::filesystem::path& file) {
	try {
		const json policy = parseUntrusted(readFile(file));
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
		std::vector<CaseResult> results;
		results.reserve(cases.size());
		for(std::size_t i = 0; i < cases.size(); ++i)
			results.push_back(play(repository, expect::object(cases[i], "cases"), "cases[" + std::to_string(i) + "]"));
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
