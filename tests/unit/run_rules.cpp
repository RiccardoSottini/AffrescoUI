/// @file
/// @brief vcs::Runs: who may run which command as a peer (the rules of `permissions.run`, the mode, the reviewers,
/// the grants), the runs that wait and their limits, the log, and a run's state changes judged by the data rules
/// (VERSION_CONTROL.md, "Running commands").

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>
#include <affresco/vcs/Runs.hpp>
#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
using affresco::RunVerdict;

namespace {

/// A time to start the tests' clock at: milliseconds since 1970.
constexpr std::int64_t startTime = 1'800'000'000'000;
/// A minute, in milliseconds.
constexpr std::int64_t minute = 60'000;

/// @brief Parses a run rule, `author` or `author(command)`, as the format would.
/// @param text The rule.
/// @return The rule.
affresco::PermissionRule runRule(std::string_view text) {
	affresco::PermissionRule rule{std::string(text), std::string(text), {}};
	if(const std::size_t open = text.find('('); open != std::string_view::npos) {
		rule.author = text.substr(0, open);
		rule.pattern = text.substr(open + 1, text.size() - open - 2);
	}
	return rule;
}

/// @brief The format's rules, with run rules of the test's own in `permissions.run`, and a count of the reads.
class WithRunRules final : public affresco::DocumentRules {
public:
	/// @brief Wraps the format's rules.
	/// @param format The format's rules; outlives this.
	explicit WithRunRules(const affresco::DocumentRules& format) : format_(format) {}

	/// @brief Checks a document, as the format does.
	/// @param document The document.
	void validate(const json& document) const override { format_.validate(document); }
	/// @brief Finds a node, as the format does.
	/// @param document The document.
	/// @param id The id.
	/// @return Its pointer.
	[[nodiscard]] std::optional<std::string> findNode(const json& document, std::string_view id) const override {
		return format_.findNode(document, id);
	}
	/// @brief The format's permissions, with the test's run rules, counted.
	/// @param document The document.
	/// @return The permissions.
	[[nodiscard]] affresco::Permissions permissions(const json& document) const override {
		++reads;
		affresco::Permissions permissions = format_.permissions(document);
		permissions.run = run;
		if(unconfigured) permissions = {};
		return permissions;
	}
	/// @brief Checks a change, as the format does.
	/// @param before The document before.
	/// @param after The document after.
	void checkChange(const json& before, const json& after) const override { format_.checkChange(before, after); }

	affresco::RunRules run; ///< The run rules every read gives.
	mutable int reads = 0;  ///< How often the permissions were read.
	/// Read the document as if it had no `permissions` section, while the repository still sees one (so that the
	/// runner may not review, and the last rows of the table are reached).
	bool unconfigured = false;

private:
	const affresco::DocumentRules& format_; ///< The format's rules.
};

/// @brief A screen whose studio changes anything and reviews.
/// @param extra More keys of its permissions (e.g. "data", "review").
/// @return The document.
json screen(const json& extra = json::object()) {
	json document = json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"state": { "cards": [], "board": "todo", "token": "" },
		"permissions": { "allow": ["studio"], "reviewers": ["studio"] },
		"root": { "type": "panel", "id": "main", "children": [ { "type": "panel", "id": "board" } ] }
	})json");
	document["permissions"].update(extra);
	return document;
}

/// @brief A repository, the run rules and the runs, on a clock the test moves.
struct Fixture {
	std::int64_t now = startTime;                 ///< The clock.
	affresco::format::Rules format;               ///< The format's rules.
	WithRunRules rules{format};                   ///< With the test's run rules.
	affresco::vcs::Repository repository;         ///< The document.
	affresco::vcs::Runs runs;                     ///< Under test.
	std::vector<affresco::RunDecision> decisions; ///< Every decision, oldest first.
	int changes = 0;                              ///< How often onChanged fired.

	/// @brief Sets everything up.
	/// @param document The document.
	explicit Fixture(json document = screen())
	    : repository(format, std::move(document)), runs(repository, rules, [this] { return now; }) {
		runs.onDecision.connect([this](const affresco::RunDecision& d) { decisions.push_back(d); });
		runs.onChanged.connect([this] { ++changes; });
	}

	/// @brief Sets the run rules.
	/// @param allow The allow rules.
	/// @param ask The ask rules.
	/// @param deny The deny rules.
	void setRules(const std::vector<std::string>& allow, const std::vector<std::string>& ask = {},
	              const std::vector<std::string>& deny = {}) {
		rules.run = {};
		for(const auto& text : allow) rules.run.allow.push_back(runRule(text));
		for(const auto& text : ask) rules.run.ask.push_back(runRule(text));
		for(const auto& text : deny) rules.run.deny.push_back(runRule(text));
		// a new head commit is what makes the runs read the permissions again
		REQUIRE_FALSE(
		    repository.commit({{"author", "studio"}, {"set", {{"#board/color", "#00000" + std::to_string(++edits)}}}}));
	}

	/// @brief Judges a run of a command that reaches nothing special.
	/// @param author The runner.
	/// @param command The command.
	/// @return The verdict.
	affresco::RunJudgement judge(std::string_view author, std::string_view command) const {
		return runs.judge(author, command, {});
	}

	/// @brief Begins a run that waits for a decision.
	/// @param author The runner.
	/// @param command The command.
	/// @param verdict Ask or Person.
	/// @return Its number (0 when refused).
	int wait(std::string_view author = "agent/claude-code", std::string_view command = "move-card",
	         RunVerdict verdict = RunVerdict::Ask) {
		return runs.begin(author, command, json::object(), "Move a card", verdict, "asked").run;
	}

	int edits = 0; ///< The commits setRules made.
};

/// @brief A reach of a command that uses a service that writes.
/// @return The reach.
affresco::CommandReach outside() {
	affresco::CommandReach reach;
	reach.personUses = {"invoke files.write (writes files in 'workspace')"};
	return reach;
}

/// @brief A reach of a command that runs a control step.
/// @return The reach.
affresco::CommandReach control() {
	affresco::CommandReach reach;
	reach.control = true;
	reach.controlStep = "call";
	return reach;
}

} // namespace

/* The table, row by row */

TEST_CASE("vcs: a run a deny rule matches is denied, naming the rule (row 1)") {
	Fixture f;
	f.setRules({"agent"}, {}, {"*(clear-board)"});
	const auto judged = f.judge("agent/claude-code", "clear-board");
	CHECK((judged.verdict == RunVerdict::Deny));
	CHECK(judged.reason == "denied by the run rule '*(clear-board)'");
}

TEST_CASE("vcs: a run of a command that reaches a control step is denied, naming the step (row 2)") {
	Fixture f;
	f.setRules({"agent"});
	const auto judged = f.runs.judge("agent/claude-code", "export", control());
	CHECK((judged.verdict == RunVerdict::Deny));
	CHECK(judged.reason == "'export' reaches the control step \"call\", which only the person may run");
	affresco::CommandReach computed = control();
	computed.controlStep = "a computed run";
	CHECK(f.runs.judge("agent/claude-code", "export", computed).reason ==
	      "'export' reaches a computed run, which only the person may run");
}

TEST_CASE("vcs: a run in plan mode is denied for an author who may not review (row 3)") {
	Fixture f;
	f.setRules({"agent"});
	f.repository.setMode(affresco::Mode::Plan);
	const auto judged = f.judge("agent/claude-code", "move-card");
	CHECK((judged.verdict == RunVerdict::Deny));
	CHECK(judged.reason == "plan mode: commands don't run; say what you would run");
}

TEST_CASE("vcs: a run that uses a service that writes waits for the person, listing the uses (row 4)") {
	Fixture f;
	f.setRules({"agent"});
	const auto judged = f.runs.judge("agent/claude-code", "export", outside());
	CHECK((judged.verdict == RunVerdict::Person));
	CHECK(judged.reason ==
	      "'export' needs the person's yes each time: invoke files.write (writes files in 'workspace')");
}

TEST_CASE("vcs: a run by a reviewer is allowed without a rule (row 5)") {
	const Fixture f;
	CHECK((f.judge("studio/anna", "move-card").verdict == RunVerdict::Allow));
}

TEST_CASE("vcs: a run an ask rule matches waits, naming the rule (row 6)") {
	Fixture f;
	f.setRules({}, {"agent"});
	const auto judged = f.judge("agent/claude-code", "move-card");
	CHECK((judged.verdict == RunVerdict::Ask));
	CHECK(judged.reason == "asked by the run rule 'agent'");
}

TEST_CASE("vcs: a run an allow rule matches is allowed (row 7)") {
	Fixture f;
	f.setRules({"agent(move-card)"});
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
}

TEST_CASE("vcs: a run on a document without permissions is allowed (row 8)") {
	Fixture f;
	f.rules.unconfigured = true;
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
	json document = screen();
	document.erase("permissions");
	const Fixture open(document); // without permissions anyone reviews: allowed as well
	CHECK((open.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
}

TEST_CASE("vcs: a run no rule allows waits, saying which rule would (row 9)") {
	const Fixture f;
	const auto judged = f.judge("agent/claude-code", "move-card");
	CHECK((judged.verdict == RunVerdict::Ask));
	CHECK(judged.reason ==
	      "no run rule allows 'move-card' for 'agent'; a rule \"agent(move-card)\" in permissions.run.allow would");
}

/* Neighbouring rows: which wins */

TEST_CASE("vcs: a run's deny rule wins over a control reach (rows 1 and 2)") {
	Fixture f;
	f.setRules({}, {}, {"agent(export)"});
	CHECK(f.runs.judge("agent/claude-code", "export", control()).reason == "denied by the run rule 'agent(export)'");
}

TEST_CASE("vcs: a run's control reach wins over plan mode (rows 2 and 3)") {
	Fixture f;
	f.repository.setMode(affresco::Mode::Plan);
	CHECK(f.runs.judge("agent/claude-code", "export", control()).reason.find("control step") != std::string::npos);
}

TEST_CASE("vcs: a run in plan mode is denied even when it would wait for the person (rows 3 and 4)") {
	Fixture f;
	f.repository.setMode(affresco::Mode::Plan);
	CHECK((f.runs.judge("agent/claude-code", "export", outside()).verdict == RunVerdict::Deny));
	// a reviewer isn't held by plan mode: the person still says yes to what reaches outside
	CHECK((f.runs.judge("studio/anna", "export", outside()).verdict == RunVerdict::Person));
}

TEST_CASE("vcs: a run by a reviewer still waits for the person when it reaches outside (rows 4 and 5)") {
	const Fixture f;
	CHECK((f.runs.judge("studio/anna", "export", outside()).verdict == RunVerdict::Person));
}

TEST_CASE("vcs: a run by a reviewer is allowed even when an ask rule matches (rows 5 and 6)") {
	Fixture f;
	f.setRules({}, {"*"});
	CHECK((f.judge("studio/anna", "move-card").verdict == RunVerdict::Allow));
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
}

TEST_CASE("vcs: a run an ask rule matches waits even when an allow rule matches too (rows 6 and 7)") {
	Fixture f;
	f.setRules({"agent"}, {"agent(clear-*)"});
	CHECK((f.judge("agent/claude-code", "clear-board").verdict == RunVerdict::Ask));
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
}

TEST_CASE("vcs: a run on a document with permissions needs a rule, without them none (rows 7, 8 and 9)") {
	Fixture configured;
	CHECK((configured.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
	configured.setRules({"agent"});
	CHECK((configured.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
	Fixture open;
	open.rules.unconfigured = true;
	CHECK((open.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
	CHECK((open.runs.judge("agent/claude-code", "export", outside()).verdict == RunVerdict::Person));
}

TEST_CASE("vcs: a run's deny rule wins over a reviewer (rows 1 and 5)") {
	Fixture f;
	f.setRules({}, {}, {"*(clear-board)"});
	CHECK((f.judge("studio/anna", "clear-board").verdict == RunVerdict::Deny));
}

TEST_CASE("vcs: a run is judged the same in acceptEdits mode") {
	Fixture f;
	f.setRules({}, {"agent(clear-board)"});
	f.repository.setMode(affresco::Mode::AcceptEdits);
	CHECK((f.judge("agent/claude-code", "clear-board").verdict == RunVerdict::Ask));
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
}

/* Rule matching */

TEST_CASE("vcs: a run rule matches by author and by command name") {
	Fixture f;
	SUBCASE("every author") {
		f.setRules({"*(move-card)"});
		CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
		CHECK((f.judge("bot/x", "move-card").verdict == RunVerdict::Allow));
	}
	SUBCASE("a channel") {
		f.setRules({"agent(move-card)"});
		CHECK((f.judge("agent/other", "move-card").verdict == RunVerdict::Allow));
		CHECK((f.judge("bot/x", "move-card").verdict == RunVerdict::Ask));
	}
	SUBCASE("a full name") {
		f.setRules({"agent/claude-code(move-card)"});
		CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
		CHECK((f.judge("agent/other", "move-card").verdict == RunVerdict::Ask));
	}
	SUBCASE("a prefix") {
		f.setRules({"agent(card-*)"});
		CHECK((f.judge("agent/claude-code", "card-move").verdict == RunVerdict::Allow));
		CHECK((f.judge("agent/claude-code", "card-").verdict == RunVerdict::Allow));
		CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
	}
	SUBCASE("an exact name, never a prefix of it") {
		f.setRules({"agent(move)"});
		CHECK((f.judge("agent/claude-code", "move").verdict == RunVerdict::Allow));
		CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
	}
	SUBCASE("no pattern: every command") {
		f.setRules({"agent"});
		CHECK((f.judge("agent/claude-code", "anything").verdict == RunVerdict::Allow));
	}
	SUBCASE("a role covers nobody") {
		f.setRules({"role:design"});
		CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
	}
}

TEST_CASE("vcs: a run's rules are read again only when the head commit changes") {
	Fixture f;
	f.setRules({"agent"});
	(void)f.judge("agent/claude-code", "move-card");
	const int reads = f.rules.reads;
	for(int i = 0; i < 10; ++i) (void)f.judge("agent/claude-code", "move-card");
	CHECK(f.rules.reads == reads);
	f.setRules({}, {}, {"agent"});
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Deny));
}

/* Deciding on a waiting run */

TEST_CASE("vcs: a run can't be allowed by a reviewer of the runner's own channel") {
	Fixture f;
	const int run = f.wait("studio/bob");
	CHECK(f.runs.allow(run, "studio/anna", false, "") == "a reviewer can't allow a run from its own channel");
	CHECK(f.decisions.empty());
}

TEST_CASE("vcs: a run is allowed by the person or a reviewer of another channel") {
	Fixture f;
	const int first = f.wait();
	const int second = f.wait();
	CHECK_FALSE(f.runs.allow(first, "user", false, ""));
	CHECK_FALSE(f.runs.allow(second, "studio/anna", false, ""));
	REQUIRE(f.decisions.size() == 2);
	CHECK(f.decisions[0].allowed);
	CHECK(f.decisions[0].reviewer == "user");
	CHECK(f.decisions[1].reviewer == "studio/anna");
	// allowed, it runs: Waiting until finish(), but no longer for a decision
	CHECK(f.runs.allow(first, "user", false, "") == "no run 1 waits");
	CHECK((f.runs.runs()[0].state == affresco::RunState::Waiting));
}

TEST_CASE("vcs: a run can't be allowed by someone who may not review") {
	Fixture f;
	const int run = f.wait();
	CHECK(f.runs.allow(run, "bot/x", false, "") == "'bot/x' may not review runs");
}

TEST_CASE("vcs: a run that reaches outside is allowed only by the person, never always") {
	Fixture f;
	const int run = f.wait("agent/claude-code", "export", RunVerdict::Person);
	CHECK(f.runs.runs()[0].personOnly);
	CHECK(f.runs.allow(run, "studio/anna", false, "") == "run 1 only the person may allow");
	CHECK(f.runs.allow(run, "user", true, "").value_or("").find("asked each time") != std::string::npos);
	CHECK(f.runs.grants().empty());
	CHECK_FALSE(f.runs.allow(run, "user", false, ""));
	REQUIRE(f.decisions.size() == 1);
	CHECK(f.decisions[0].allowed);
}

TEST_CASE("vcs: a run that is denied is logged Denied and tells the runner") {
	Fixture f;
	const int run = f.wait();
	CHECK_FALSE(f.runs.deny(run, "not now"));
	REQUIRE(f.decisions.size() == 1);
	CHECK_FALSE(f.decisions[0].allowed);
	CHECK(f.decisions[0].reason == "not now");
	CHECK((f.runs.runs()[0].state == affresco::RunState::Denied));
	CHECK(f.runs.deny(run, "") == "no run 1 waits");
	CHECK(f.runs.deny(99, "") == "no run 99 waits");
}

TEST_CASE("vcs: a run that is allowed at once is finished by the caller, and logged") {
	Fixture f;
	const auto begun =
	    f.runs.begin("agent/claude-code", "move-card", {{"card", 3}}, "Move a card", RunVerdict::Allow, "");
	CHECK(begun.run == 1);
	CHECK(f.runs.allow(1, "user", false, "") == "no run 1 waits"); // it doesn't wait for a decision
	f.runs.finish(1, affresco::RunState::Ran, "", 2, {"a warning"}, affresco::Undoable::State);
	const auto record = f.runs.runs().at(0);
	CHECK((record.state == affresco::RunState::Ran));
	CHECK(record.author == "agent/claude-code");
	CHECK(record.arguments == json{{"card", 3}});
	CHECK(record.time == startTime);
	CHECK(record.changes == 2);
	CHECK(record.errors == std::vector<std::string>{"a warning"});
	CHECK((record.undoable == affresco::Undoable::State));
	CHECK(f.runs.begin("agent/claude-code", "x", {}, "", RunVerdict::Deny, "no").run == 0);
}

TEST_CASE("vcs: a run's every change fires onChanged once") {
	Fixture f;
	const int run = f.wait();
	CHECK(f.changes == 1);
	CHECK_FALSE(f.runs.allow(run, "user", true, ""));
	CHECK(f.changes == 2);
	f.runs.finish(run, affresco::RunState::Ran, "", 0, {}, affresco::Undoable::None);
	CHECK(f.changes == 3);
	CHECK(f.runs.revoke("agent(run:move-card)"));
	CHECK(f.changes == 4);
	f.runs.expire(f.now);
	CHECK(f.changes == 4); // nothing changed
}

/* Limits */

TEST_CASE("vcs: a run past 16 waiting is refused") {
	Fixture f;
	const std::array<std::string_view, 4> channels = {"a", "b", "c", "d"};
	for(const std::string_view channel : channels)
		for(int i = 0; i < 4; ++i) REQUIRE(f.wait(std::string(channel) + "/x") > 0);
	const auto refused = f.runs.begin("e/x", "move-card", {}, "", RunVerdict::Ask, "");
	CHECK(refused.run == 0);
	CHECK(refused.error == "too many runs are waiting");
}

TEST_CASE("vcs: a run past 4 waiting for one channel is refused, others still wait") {
	Fixture f;
	for(int i = 0; i < 4; ++i) REQUIRE(f.wait("agent/" + std::to_string(i)) > 0);
	CHECK(f.wait("agent/claude-code") == 0);
	CHECK(f.wait("bot/x") == 5);
	CHECK_FALSE(f.runs.deny(1, ""));
	CHECK(f.wait("agent/claude-code") == 6);
}

TEST_CASE("vcs: a run that waits more than 120 seconds expires") {
	Fixture f;
	const int run = f.wait();
	f.runs.expire(startTime + affresco::runWaitMilliseconds);
	CHECK(f.decisions.empty());
	f.runs.expire(startTime + affresco::runWaitMilliseconds + 1);
	REQUIRE(f.decisions.size() == 1);
	CHECK(f.decisions[0].run == run);
	CHECK_FALSE(f.decisions[0].allowed);
	CHECK(f.decisions[0].reviewer.empty());
	CHECK(f.decisions[0].reason == "nobody answered in 120 s");
	CHECK((f.runs.runs()[0].state == affresco::RunState::Expired));
	CHECK(f.runs.allow(run, "user", false, "") == "no run 1 waits");
}

TEST_CASE("vcs: a run's log keeps the newest 64, dropping finished ones before waiting ones") {
	Fixture f;
	const int waiting = f.wait();
	for(int i = 0; i < 64; ++i) {
		const int run = f.runs.begin("agent/claude-code", "move-card", {}, "", RunVerdict::Allow, "").run;
		f.runs.finish(run, affresco::RunState::Ran, "", 0, {}, affresco::Undoable::None);
	}
	const auto records = f.runs.runs();
	REQUIRE(records.size() == affresco::maxKeptRuns);
	CHECK(records.front().id == waiting); // the waiting one stays
	CHECK(records[1].id == 3);            // the first finished one (2) went
	CHECK(records.back().id == 65);
}

TEST_CASE("vcs: a run's arguments over 4 KiB are kept cut") {
	Fixture f;
	const json arguments = {{"text", std::string(5000, 'x')}, {"card", 3}};
	(void)f.runs.begin("agent/claude-code", "move-card", arguments, "", RunVerdict::Ask, "");
	const json kept = f.runs.runs()[0].arguments;
	CHECK(kept["text"] == "(too large to show)");
	CHECK(kept["card"] == 3);
	CHECK(kept.dump().size() <= affresco::vcs::Runs::maxArgumentBytes);
	json many = json::object();
	for(int i = 0; i < 400; ++i) many["argument-number-" + std::to_string(i)] = i;
	(void)f.runs.begin("agent/claude-code", "move-card", many, "", RunVerdict::Ask, "");
	CHECK(f.runs.runs()[1].arguments.dump().size() <= affresco::vcs::Runs::maxArgumentBytes);
}

/* Grants */

TEST_CASE("vcs: a run allowed always grants its channel that one command") {
	Fixture f;
	const int run = f.wait();
	CHECK_FALSE(f.runs.allow(run, "studio/anna", true, ""));
	const auto grants = f.runs.grants();
	REQUIRE(grants.size() == 1);
	CHECK(grants[0].rule == "agent(run:move-card)");
	CHECK(grants[0].grantedBy == "studio/anna");
	CHECK(grants[0].expires.empty());
	CHECK((f.judge("agent/other", "move-card").verdict == RunVerdict::Allow));
	CHECK((f.judge("agent/other", "clear-board").verdict == RunVerdict::Ask));
	CHECK((f.judge("bot/x", "move-card").verdict == RunVerdict::Ask));
}

TEST_CASE("vcs: a run grant expires") {
	Fixture f;
	CHECK_FALSE(f.runs.allow(f.wait(), "user", true, "30m"));
	CHECK(f.runs.grants().at(0).expires == "2027-01-15T08:30:00Z");
	f.now += 30 * minute - 1;
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
	f.now += 1;
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
	CHECK(f.runs.grants().empty());
	CHECK(f.runs.allow(f.wait(), "user", true, "soon").value_or("").find("is not a duration") != std::string::npos);
}

TEST_CASE("vcs: a run grant is capped by grantsLast") {
	Fixture f(screen({{"review", {{"grantsLast", "1h"}}}}));
	CHECK_FALSE(f.runs.allow(f.wait(), "user", true, "7d"));
	CHECK(f.runs.grants().at(0).expires == "2027-01-15T09:00:00Z");
	f.now += 60 * minute;
	f.runs.expire(f.now);
	CHECK(f.runs.grants().empty());
	CHECK_FALSE(f.runs.allow(f.wait(), "user", true, ""));
	CHECK(f.runs.grants().at(0).expires == "2027-01-15T10:00:00Z");
}

TEST_CASE("vcs: an attack: a grantsLast past a year caps a grant at a year, never leaves it unlimited") {
	Fixture f(screen({{"review", {{"grantsLast", "400d"}}}}));
	CHECK_FALSE(f.runs.allow(f.wait(), "user", true, ""));
	CHECK(f.runs.grants().at(0).expires == "2028-01-16T08:00:00Z");
}

TEST_CASE("vcs: a run grant can be revoked") {
	Fixture f;
	CHECK_FALSE(f.runs.allow(f.wait(), "user", true, ""));
	CHECK_FALSE(f.runs.revoke("agent(run:clear-board)"));
	CHECK(f.runs.revoke("agent(run:move-card)"));
	CHECK(f.runs.grants().empty());
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
}

TEST_CASE("vcs: a run grant loses to a deny rule, an ask rule, plan mode, a control reach and outside uses") {
	Fixture f;
	CHECK_FALSE(f.runs.allow(f.wait(), "user", true, ""));
	CHECK((f.runs.judge("agent/claude-code", "move-card", control()).verdict == RunVerdict::Deny));
	CHECK((f.runs.judge("agent/claude-code", "move-card", outside()).verdict == RunVerdict::Person));
	f.setRules({}, {"agent(move-card)"});
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Ask));
	f.setRules({}, {}, {"agent(move-card)"});
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Deny));
	f.setRules({});
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Allow));
	f.repository.setMode(affresco::Mode::Plan);
	CHECK((f.judge("agent/claude-code", "move-card").verdict == RunVerdict::Deny));
}

/* State changes, by the data rules */

TEST_CASE("vcs: a run's state changes are judged by the runner's data rules") {
	const Fixture f(screen({{"data", json::array({{{"author", "agent"},
	                                               {"write", json::array({"/state/cards"})},
	                                               {"never", json::array({"/state/token"})}}})}}));
	const json before = screen()["state"];
	json after = before;
	SUBCASE("a path inside write") {
		after["cards"].push_back("a card");
		CHECK_FALSE(f.runs.stateDenial("agent/claude-code", before, after));
	}
	SUBCASE("a path outside write") {
		after["board"] = "done";
		CHECK(f.runs.stateDenial("agent/claude-code", before, after));
		CHECK_FALSE(f.runs.stateDenial("studio/anna", before, after));
	}
	SUBCASE("a never path") {
		after["token"] = "changed";
		const auto denial = f.runs.stateDenial("agent/claude-code", before, after);
		REQUIRE(denial);
		CHECK(denial.value_or("").find("/state/token") != std::string::npos);
		CHECK(denial.value_or("").find("changed") == std::string::npos); // never a value from the state
	}
}

TEST_CASE("vcs: a run's state changes are free without data rules") {
	const Fixture f;
	CHECK_FALSE(f.runs.stateDenial("agent/claude-code", json{{"board", "todo"}}, json{{"board", "done"}}));
}
