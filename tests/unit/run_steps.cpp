/// @file
/// @brief The seams of commands as tools: the run steps (allowRun, denyRun, undoRun) decide through the run control
/// as the person and only from the person's own code, the `runs` and `runGrants` variables list it, and a peer's
/// command messages reach the screen's commands.

#include "Headless.hpp"
#include "Helpers.hpp"
#include "LispApp.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A run control that records what it was asked, and lists what a test gives it.
class FakeRunControl final : public affresco::RunControl {
public:
	/// @brief One allow() call.
	struct Allowed {
		int run = 0;               ///< The run.
		std::string reviewer = {}; ///< Who allowed it.
		bool always = false;       ///< Whether it was granted.
		std::string duration = {}; ///< For how long.
	};

	std::vector<Allowed> allowed = {};                    ///< The allow() calls.
	std::vector<std::pair<int, std::string>> denied = {}; ///< The deny() calls: the run and the reason.
	std::vector<affresco::RunRecord> kept = {};           ///< What runs() gives.
	std::vector<affresco::Grant> granted = {};            ///< What grants() gives.

	/// @brief Allows everything.
	/// @return Allow.
	[[nodiscard]] affresco::RunJudgement judge(std::string_view /*author*/, std::string_view /*command*/,
	                                           const affresco::CommandReach& /*reach*/) const override {
		return {affresco::RunVerdict::Allow, ""};
	}
	/// @brief Begins nothing.
	/// @return No run.
	Begun begin(std::string_view /*author*/, std::string_view /*command*/, const json& /*arguments*/,
	            std::string_view /*title*/, affresco::RunVerdict /*verdict*/, std::string_view /*reason*/) override {
		return {0, "no"};
	}
	/// @brief Records the call.
	/// @param run The run.
	/// @param reviewer Who allows it.
	/// @param always Whether to grant it.
	/// @param duration For how long.
	/// @return std::nullopt.
	std::optional<std::string> allow(int run, std::string_view reviewer, bool always,
	                                 std::string_view duration) override {
		allowed.push_back({run, std::string(reviewer), always, std::string(duration)});
		return std::nullopt;
	}
	/// @brief Records the call.
	/// @param run The run.
	/// @param reason Why.
	/// @return std::nullopt.
	std::optional<std::string> deny(int run, std::string_view reason) override {
		denied.emplace_back(run, std::string(reason));
		return std::nullopt;
	}
	/// @brief Records nothing.
	void finish(int /*run*/, affresco::RunState /*state*/, std::string_view /*reason*/, std::size_t /*changes*/,
	            std::vector<std::string> /*errors*/, affresco::Undoable /*undoable*/) override {}
	/// @brief What the test gave.
	/// @return kept.
	[[nodiscard]] std::vector<affresco::RunRecord> runs() const override { return kept; }
	/// @brief What the test gave.
	/// @return granted.
	[[nodiscard]] std::vector<affresco::Grant> grants() const override { return granted; }
	/// @brief Revokes nothing.
	/// @return false.
	bool revoke(std::string_view /*rule*/) override { return false; }
	/// @brief Expires nothing.
	void expire(std::int64_t /*now*/) override {}
	/// @brief Refuses nothing.
	/// @return std::nullopt.
	[[nodiscard]] std::optional<std::string> stateDenial(std::string_view /*author*/, const json& /*before*/,
	                                                     const json& /*after*/) const override {
		return std::nullopt;
	}
};

/// @brief A screen whose person decides on runs, and reads the runs.
constexpr const char* runScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "run": 2, "runs": null, "grants": null },
	"permissions": { "allow": ["agent"] },
	"actions": {
		"allow": [{ "allowRun": "{{state.run}}", "always": true, "for": "8h" }],
		"allowOnce": [{ "allowRun": 5 }],
		"allowWords": [{ "allowRun": "two" }],
		"deny": [{ "denyRun": 3, "reason": "not now" }],
		"undo": [{ "undoRun": 4 }],
		"look": [{ "set": "state.runs", "to": "{{runs}}" }, { "set": "state.grants", "to": "{{runGrants}}" }]
	},
	"root": { "type": "label", "id": "root", "text": "Runs" }
})json";

/// @brief A runtime on runScreen, in a headless window, with a fake run control.
struct RunApp {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::format::Rules rules;            ///< The format's rules.
	affresco::vcs::Repository repository;     ///< The document.
	affresco::format::Runtime runtime;        ///< The running screen.
	FakeRunControl control;                   ///< The run control.
	std::vector<std::string> errors;          ///< What was reported.

	/// @brief Builds it.
	/// @param withControl Whether to give the runtime the run control.
	explicit RunApp(bool withControl = true)
	    : window(createHeadlessWindow(400, 300, clock)), repository(rules, json::parse(runScreen)),
	      runtime(repository, "") {
		if(withControl) runtime.setRunControl(&control);
		runtime.onError.connect([this](const std::string& message) { errors.push_back(message); });
		runtime.build(*window);
		window->render();
	}
	RunApp(const RunApp&) = delete;
	RunApp& operator=(const RunApp&) = delete;
	RunApp(RunApp&&) = delete;
	RunApp& operator=(RunApp&&) = delete;
	/// @brief Takes the run control away first: it goes after the runtime.
	~RunApp() { runtime.setRunControl(nullptr); }

	/// @brief Runs an action as the person's input would, then what it deferred.
	/// @param name The action.
	void run(const char* name) {
		runtime.runAction(name, window->root(), json::object());
		window->runDeferredTasks();
	}
	/// @brief Whether an error holding a text was reported.
	/// @param text The text.
	/// @return true if one was.
	[[nodiscard]] bool reported(std::string_view text) const {
		return std::ranges::any_of(errors, [text](const std::string& error) { return error.contains(text); });
	}
};

} // namespace

TEST_CASE("format: a run step decides through the run control, as the person") {
	RunApp app;
	app.run("allow");
	app.run("allowOnce");
	app.run("deny");
	REQUIRE(app.control.allowed.size() == 2);
	CHECK(app.control.allowed[0].run == 2);
	CHECK(app.control.allowed[0].reviewer == "user");
	CHECK(app.control.allowed[0].always);
	CHECK(app.control.allowed[0].duration == "8h");
	CHECK(app.control.allowed[1].run == 5);
	CHECK_FALSE(app.control.allowed[1].always);
	CHECK(app.control.allowed[1].duration.empty());
	REQUIRE(app.control.denied.size() == 1);
	CHECK(app.control.denied[0] == std::pair<int, std::string>{3, "not now"});
	CHECK(app.errors.empty());

	app.run("undo"); // no run 4 ran on this screen
	CHECK(app.reported("there is no run 4"));
}

TEST_CASE("format: a run step without a run control, or with a value of the wrong type, fails") {
	RunApp app(false);
	app.run("allow");
	CHECK(app.reported("this screen runs no commands for peers"));
	app.errors.clear();

	RunApp typed;
	typed.run("allowWords");
	CHECK(typed.reported("expected a run's number"));
	CHECK(typed.control.allowed.empty());
}

TEST_CASE("format: a run step's variables list the run control's runs and grants") {
	RunApp app;
	affresco::RunRecord record;
	record.id = 1;
	record.author = "agent/claude-code";
	record.command = "move-card";
	record.title = "Move card";
	record.arguments = {{"card", 3}};
	record.reason = "asks: agent(run:move-card)";
	record.time = 1'700'000'000'000;
	app.control.kept.push_back(record);
	affresco::Grant grant;
	grant.rule = "agent(run:move-card)";
	app.control.granted.push_back(grant);
	app.control.onChanged();
	app.run("look");

	const json runs = app.repository.state()["runs"];
	REQUIRE(runs.size() == 1);
	CHECK(runs[0]["id"] == 1);
	CHECK(runs[0]["author"] == "agent/claude-code");
	CHECK(runs[0]["command"] == "move-card");
	CHECK(runs[0]["arguments"]["card"] == 3);
	CHECK(runs[0]["state"] == "waiting");
	CHECK(runs[0]["undoable"] == "none");
	CHECK(runs[0]["personOnly"] == false);
	const json grants = app.repository.state()["grants"];
	REQUIRE(grants.size() == 1);
	CHECK(grants[0]["rule"] == "agent(run:move-card)");

	RunApp without(false);
	without.run("look");
	CHECK(without.repository.state()["runs"] == json::array());
	CHECK(without.repository.state()["grants"] == json::array());
}

TEST_CASE("format: a run step attack: a revision may not add one, nor a merged proposal run one") {
	RunApp app;
	for(const char* step : {R"({"allowRun": 1})", R"({"denyRun": 1})", R"({"undoRun": 1})"}) {
		CAPTURE(step);
		const json added = json::array({json::parse(step)});
		const affresco::RevisionResult handler = app.repository.submit(
		    json{{"author", "agent"},
			     {"patch", json::array({{{"op", "add"}, {"path", "/actions/mine"}, {"value", added}}})}});
		CHECK(handler.status == affresco::RevisionStatus::Rejected);
		const affresco::RevisionResult run = app.repository.submit(json{{"author", "agent"}, {"run", added}});
		CHECK(run.status == affresco::RevisionStatus::Rejected);
		CHECK(run.error.contains("permission denied"));
	}
	CHECK(app.control.allowed.empty());
	CHECK(app.control.denied.empty());
}

TEST_CASE("format: a run step attack: an agent's code may not run allowRun") {
	lispapp::App app(R"lisp(
(defn helper "An agent may replace this." [] 1)
(defcommand c "Calls helper." :title "C" [] (helper))
(defkey "ctrl+c" c)
(defscreen main (panel#root))
)lisp");
	FakeRunControl control;
	app.runtime->setRunControl(&control);
	for(const char* form : {"(allowRun 1)", "(denyRun 1)", "(undoRun 1)"}) {
		CAPTURE(form);
		app.write("helper", std::string(R"((defn helper "Mine." [] )") + form + ")");
		app.errors.clear();
		app.press(affresco::Key::C);
		CHECK(app.reported("agent/x"));
		CHECK(app.reported("only the person's own code"));
	}
	CHECK(control.allowed.empty());
	CHECK(control.denied.empty());
	app.runtime->setRunControl(nullptr);
}

TEST_CASE("format: a run step's peer: the command messages reach the screen's commands, and review carries the runs") {
	affresco::test::MemoryChannel agent;
	const ManualClock clock;
	const std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromString(runScreen);
	screen.build(*window);
	screen.connect(agent, "agent");

	const auto answer = [&agent](const json& message) {
		agent.receive(message.dump());
		return agent.sent.back();
	};
	const json listed = answer({{"type", "getCommands"}, {"id", 1}});
	CHECK(listed["type"] == "commands");
	CHECK(listed["commands"] == json::array()); // the catalog lists none yet (f4c)
	const json ran = answer({{"type", "runCommand"}, {"id", 1}, {"command", "look"}});
	CHECK(ran["type"] == "rejected");
	CHECK(ran["id"] == 1);
	CHECK(ran["error"] == "no command 'look'");
	const json document = answer({{"type", "getDocument"}, {"id", 2}});
	CHECK(document["runs"] == json::array());
	CHECK(document["runGrants"] == json::array());

	screen.allowCommandRuns(false);
	const json refused = answer({{"type", "getCommands"}, {"id", 3}});
	CHECK(refused["error"].get<std::string>().contains("this host runs no commands"));
	CHECK_FALSE(answer({{"type", "getDocument"}, {"id", 4}}).contains("runs"));

	std::string error;
	CHECK_FALSE(screen.allowRun(1, false, error));
	CHECK(error == "no run 1 waits");
	CHECK_FALSE(screen.denyRun(1));
	const json commands = screen.commandsJson();
	CHECK(commands["commands"] == json::array());
	CHECK(commands["runs"] == json::array());
	CHECK(commands["runGrants"] == json::array());
}
