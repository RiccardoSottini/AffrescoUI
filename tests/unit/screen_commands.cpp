/// @file
/// @brief Unit tests for detail::ScreenCommands, the screen's commands as tools, over a fake run control: the order of
/// a run's checks, the reviewers' decisions, the person's question, undo, and what a limited reader is shown. The run
/// control's own rules are tested through the real stack in command_e2e.cpp.

#include "CommandBoard.hpp"
#include "Headless.hpp"
#include "Helpers.hpp"
#include "commands/ScreenCommands.hpp"  // modules/screen/src
#include "pictures/OffscreenImages.hpp" // modules/screen/src

#include <affresco/core/Commands.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Input.hpp>
#include <affresco/render/Panel.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using affresco::json;
using affresco::RunRecord;
using affresco::RunState;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A run control that records what it was asked, judges as a test says, and lists what a test gives it.
class FakeRunControl final : public affresco::RunControl {
public:
	affresco::RunJudgement judgement = {affresco::RunVerdict::Allow, ""}; ///< What judge() gives.
	std::vector<std::pair<int, std::string>> allowed = {};                ///< The allow() calls: run and reviewer.
	std::vector<std::pair<int, std::string>> denied = {};                 ///< The deny() calls: run and reason.
	std::vector<std::pair<int, RunState>> finished = {};                  ///< The finish() calls: run and state.
	std::vector<RunRecord> kept = {};                                     ///< What runs() gives.
	int begun = 0;                                                        ///< The last run begun.

	/// @brief The test's judgement.
	/// @return judgement.
	[[nodiscard]] affresco::RunJudgement judge(std::string_view /*author*/, std::string_view /*command*/,
	                                           const affresco::CommandReach& /*reach*/) const override {
		return judgement;
	}
	/// @brief Begins the next run.
	/// @return Its number.
	Begun begin(std::string_view /*author*/, std::string_view /*command*/, const json& /*arguments*/,
	            std::string_view /*title*/, affresco::RunVerdict /*verdict*/, std::string_view /*reason*/) override {
		return {++begun, ""};
	}
	/// @brief Records the call and tells who listens.
	/// @param run The run.
	/// @param reviewer Who allows it.
	/// @return std::nullopt.
	std::optional<std::string> allow(int run, std::string_view reviewer, bool /*always*/,
	                                 std::string_view /*duration*/) override {
		allowed.emplace_back(run, std::string(reviewer));
		onDecision({run, true, std::string(reviewer), ""});
		return std::nullopt;
	}
	/// @brief Records the call and tells who listens.
	/// @param run The run.
	/// @param reason Why.
	/// @return std::nullopt.
	std::optional<std::string> deny(int run, std::string_view reason) override {
		denied.emplace_back(run, std::string(reason));
		onDecision({run, false, "", std::string(reason)});
		return std::nullopt;
	}
	/// @brief Records the call.
	/// @param run The run.
	/// @param state How it ended.
	void finish(int run, RunState state, std::string_view /*reason*/, std::size_t /*changes*/,
	            std::vector<std::string> /*errors*/, affresco::Undoable /*undoable*/) override {
		finished.emplace_back(run, state);
	}
	/// @brief What the test gave.
	/// @return kept.
	[[nodiscard]] std::vector<RunRecord> runs() const override { return kept; }
	/// @brief No grants.
	/// @return Nothing.
	[[nodiscard]] std::vector<affresco::Grant> grants() const override { return {}; }
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

/// @brief The test screen, whose agent reads only the count.
/// @return The document.
json limitedScreen() {
	json document = affresco::test::permissionsScreen();
	document["permissions"]["data"] = json::array({{{"author", "agent"}, {"read", {"/state/count"}}}});
	return document;
}

/// @brief The test screen with a command, "bump", that adds one to the count.
/// @return The document.
json bumpScreen() {
	json document = limitedScreen();
	document["actions"]["bump"] = {{"title", "Bump"},
	                               {"do", json::array({{{"set", "state.count"}, {"to", "{{state.count + 1}}"}}})}};
	return document;
}

/// @brief A button of the window's question, by its name.
/// @param panel Where to look.
/// @param name The name.
/// @return It, or nullptr.
affresco::Panel* questionButton(affresco::Panel* panel, std::string_view name) {
	if(!panel) return nullptr;
	if(panel->accessibleRole() == "button" && panel->accessibleLabel() == name) return panel;
	for(const auto& child : panel->children())
		if(affresco::Panel* found = questionButton(child.get(), name)) return found;
	return nullptr;
}

/// @brief A run of the agent's, kept by the run control.
/// @param id Its number.
/// @param author Its runner.
/// @param state Where it is.
/// @return The record.
RunRecord record(int id, std::string author, RunState state) {
	RunRecord run;
	run.id = id;
	run.author = std::move(author);
	run.command = "open";
	run.arguments = {{"text", "a value"}};
	run.state = state;
	run.errors = {"step 1 said a value"};
	return run;
}

/// @brief A screen's runtime, pictures and commands, built in a headless window or not.
struct Fixture {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the runtime.
	affresco::format::Rules rules;                                                    ///< The format.
	affresco::vcs::Repository repository;                                             ///< The document.
	affresco::format::Runtime runtime{repository, "."};                               ///< The screen.
	FakeRunControl control;                                                           ///< Who may run what.
	affresco::detail::OffscreenImages images{repository, runtime};                    ///< The pictures.
	affresco::detail::ScreenCommands commands{runtime, control, repository, images};  ///< Under test.
	std::vector<json> answers;                                                        ///< What run() answered.

	/// @brief Builds it.
	/// @param build Whether to build the screen in the window.
	/// @param document The screen.
	explicit Fixture(bool build = true, json document = limitedScreen()) : repository(rules, std::move(document)) {
		if(build) runtime.build(*window);
	}

	/// @brief The count in the runtime's state.
	/// @return It.
	[[nodiscard]] int count() const { return repository.state().at("count").get<int>(); }

	/// @brief Clicks a button of the window's question with the person's own mouse, once shown long enough.
	/// @param name The button's name.
	void press(std::string_view name) {
		clock.advance(600.0);
		window->render();
		affresco::Panel* found = questionButton(window->confirmation(), name);
		REQUIRE(found != nullptr);
		const affresco::Rect& frame = found->frame();
		const affresco::Vec2 at{(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
		window->routeMouseMove(at);
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		window->render();
	}

	/// @brief Runs a command as @p author.
	/// @param author The runner.
	/// @param message The runCommand, as the session checked it.
	/// @return The first answer.
	json run(std::string_view author, const json& message) {
		answers.clear();
		commands.run(author, message, [this](const json& answer) { answers.push_back(answer); });
		REQUIRE_FALSE(answers.empty());
		return answers.front();
	}
};

} // namespace

TEST_CASE("screen: with no commands in the catalog, a peer lists none and runs none, with a hint of why") {
	Fixture f;
	CHECK(f.commands.list("agent", "", true) == json{{"type", "commands"}, {"commands", json::array()}});
	const json answer = f.run("agent", {{"command", "open"}});
	CHECK(answer["type"] == "rejected");
	CHECK(answer["error"] == "no command 'open'");
	CHECK(f.control.begun == 0);
}

TEST_CASE("screen: a run is refused first because runs are off, then because there is no window") {
	Fixture unbuilt(false);
	CHECK(unbuilt.run("agent", {{"command", "open"}})["error"] == "the screen has no window yet");
	unbuilt.commands.setAllowed(false);
	CHECK(unbuilt.run("agent", {{"command", "open"}})["error"] == "this host runs no commands");
	unbuilt.commands.setAllowed(true);
	CHECK(
	    unbuilt.run("agent", {{"command", "open"}, {"arguments", json::array()}})["error"].get<std::string>().contains(
	        "expected an object"));
}

TEST_CASE("screen: an attack: a run is never allowed from its own channel, and a personOnly one only by the person") {
	Fixture f;
	f.control.kept = {record(1, "agent/claude", RunState::Waiting), record(2, "agent", RunState::Waiting)};
	f.control.kept[1].personOnly = true;
	const auto allow = [&f](std::string_view reviewer, int run) {
		return f.commands.decide(reviewer, {{"type", "allowRun"}, {"run", run}, {"always", false}, {"for", ""}});
	};
	CHECK(allow("agent/other", 1)["error"] == "a run is never allowed from its own channel");
	CHECK(allow("studio", 2)["error"] == "only the person may allow this run");
	CHECK(f.control.allowed.empty());
	CHECK(allow("studio", 1)["type"] == "done");
	CHECK(allow("user", 2)["type"] == "done");
	CHECK(f.control.allowed == std::vector<std::pair<int, std::string>>{{1, "studio"}, {2, "user"}});
}

TEST_CASE("screen: denyRun and revokeRun pass to the run control, which says what it refused") {
	Fixture f;
	CHECK(f.commands.decide("studio", {{"type", "denyRun"}, {"run", 3}, {"reason", "not now"}})["type"] == "done");
	CHECK(f.control.denied == std::vector<std::pair<int, std::string>>{{3, "not now"}});
	CHECK(f.commands.decide("studio", {{"type", "revokeRun"}, {"rule", "agent:open"}})["error"] ==
	      "no run grant 'agent:open'");
	CHECK(f.commands.decide("studio", {{"type", "allowRun"}, {"run", "1"}})["error"].get<std::string>().contains(
	    "allowRun.run"));
}

TEST_CASE("screen: undoRun: only a run that ran, of the peer's own channel or for a reviewer") {
	Fixture f;
	f.control.kept = {record(1, "agent/claude", RunState::Ran), record(2, "agent", RunState::Failed)};
	CHECK(f.commands.undo("agent", {{"run", 9}})["error"] == "no run 9");
	CHECK(f.commands.undo("other", {{"run", 1}})["error"] == "permission denied: run 1 is another channel's");
	CHECK(f.commands.undo("studio", {{"run", 2}})["error"] == "run 2 can't be undone: it is failed");
	const json own = f.commands.undo("agent/other", {{"run", 1}});
	CHECK(own["type"] == "rejected"); // nothing in the runtime to undo: it says so
	CHECK(own["run"] == 1);
	CHECK(f.control.finished.empty());
}

TEST_CASE("screen: a reader whose reads are limited gets the runs without their arguments and errors") {
	Fixture f;
	f.control.kept = {record(1, "agent", RunState::Ran), record(2, "agent", RunState::Failed)};
	f.control.kept[1].reason = "expected a list, got \"s3cret\"";
	f.control.kept[1].errors = {f.control.kept[1].reason};
	const json limited = f.commands.pending("agent");
	REQUIRE(limited["runs"].size() == 2);
	CHECK(limited["runs"][1]["reason"] == "the run failed"); // an attack: why it failed can quote what it read
	CHECK_FALSE(limited["runs"][0].contains("arguments"));
	CHECK_FALSE(limited["runs"][0].contains("errors"));
	CHECK(limited["runs"][0]["command"] == "open");
	const json reviewer = f.commands.pending("studio");
	CHECK(reviewer["runs"][0]["arguments"] == json{{"text", "a value"}});
	CHECK(reviewer["runGrants"] == json::array());
}

TEST_CASE("screen: a run the run control allows runs at once, and answers ran with what changed") {
	Fixture f(true, bumpScreen());
	const json ran = f.run("agent/claude", {{"command", "bump"}});
	CHECK(ran["type"] == "ran");
	CHECK(ran["run"] == 1);
	CHECK(ran["failed"] == false);
	CHECK(ran.contains("changed"));
	CHECK(f.count() == 1);
	CHECK(f.control.finished == std::vector<std::pair<int, RunState>>{{1, RunState::Ran}});
	CHECK_FALSE(f.run("agent", {{"command", "bump"}, {"evidence", "none"}}).contains("changed"));
}

TEST_CASE("screen: a run that waits for a reviewer runs once allowed, checked again, and is refused when denied") {
	Fixture f(true, bumpScreen());
	f.control.judgement = {affresco::RunVerdict::Ask, "no rule allows it"};
	const json waiting = f.run("agent", {{"command", "bump"}});
	CHECK(waiting == json{{"type", "waiting"}, {"run", 1}, {"reason", "no rule allows it"}, {"expires", 120}});
	CHECK(f.count() == 0);
	(void)f.control.allow(1, "studio", false, "");
	REQUIRE(f.answers.size() == 2);
	CHECK(f.answers[1]["type"] == "ran");
	CHECK(f.count() == 1);

	f.run("agent", {{"command", "bump"}});
	f.control.judgement = {affresco::RunVerdict::Deny, "denied by *(bump)"}; // checked again once allowed
	(void)f.control.allow(2, "studio", false, "");
	REQUIRE(f.answers.size() == 2);
	CHECK(f.answers[1] == json{{"type", "rejected"}, {"error", "denied by *(bump)"}, {"run", 2}});
	CHECK(f.control.finished.back() == std::pair<int, RunState>{2, RunState::Failed});
	f.control.judgement = {affresco::RunVerdict::Ask, "no rule allows it"};

	f.run("agent", {{"command", "bump"}});
	(void)f.control.deny(3, "not now");
	REQUIRE(f.answers.size() == 2);
	CHECK(f.answers[1] == json{{"type", "rejected"}, {"error", "not now"}, {"run", 3}});
	CHECK(f.count() == 1);
}

TEST_CASE("screen: an attack: a run that now needs the person's yes is refused when a reviewer allows it") {
	// It waited as Ask; a commit made the command reach a service write, so only the person's own question may let it
	Fixture f(true, bumpScreen());
	f.control.judgement = {affresco::RunVerdict::Ask, "no rule allows it"};
	REQUIRE(f.run("agent", {{"command", "bump"}})["type"] == "waiting");
	f.control.judgement = {affresco::RunVerdict::Person, "it uses files.write"};
	(void)f.control.allow(1, "studio", false, "");
	REQUIRE(f.answers.size() == 2);
	CHECK(f.answers[1]["type"] == "rejected");
	CHECK(f.answers[1]["error"].get<std::string>().find("changed while it waited") != std::string::npos);
	CHECK(f.control.finished.back() == std::pair<int, RunState>{1, RunState::Failed});
	CHECK(f.count() == 0);
	CHECK(f.window->confirmation() == nullptr);

	// The person's own yes in the chat, for a run that now uses more than when it was asked, is refused too
	f.control.judgement = {affresco::RunVerdict::Ask, "no rule allows it"};
	REQUIRE(f.run("agent", {{"command", "bump"}})["type"] == "waiting");
	f.control.judgement = {affresco::RunVerdict::Person, "it uses files.write"};
	(void)f.control.allow(2, "user", false, "");
	REQUIRE(f.answers.size() == 2);
	CHECK(f.answers[1]["type"] == "rejected");
	CHECK(f.count() == 0);

	// A Person run the person allowed stays a Person run: it runs
	REQUIRE(f.run("agent", {{"command", "bump"}})["type"] == "waiting");
	f.press("Run");
	REQUIRE(f.answers.size() == 2);
	CHECK(f.answers[1]["type"] == "ran");
	CHECK(f.count() == 1);
}

TEST_CASE("screen: a run for the person asks in the window's own question, Run or Don't run, each time") {
	Fixture f(true, bumpScreen());
	f.control.judgement = {affresco::RunVerdict::Person, "it uses files.write"};
	for(int time = 1; time <= 2; ++time) {
		CAPTURE(time);
		CHECK(f.run("agent/claude", {{"command", "bump"}})["type"] == "waiting");
		REQUIRE(f.window->confirmation() != nullptr);
		f.press("Run");
		REQUIRE(f.answers.size() == 2);
		CHECK(f.answers[1]["type"] == "ran");
		CHECK(f.window->confirmation() == nullptr);
	}
	CHECK(f.control.allowed == std::vector<std::pair<int, std::string>>{{1, "user"}, {2, "user"}});
	f.run("agent", {{"command", "bump"}});
	f.press("Don't run");
	REQUIRE(f.answers.size() == 2);
	CHECK(f.answers[1] == json{{"type", "rejected"}, {"error", "the person said no"}, {"run", 3}});
	CHECK(f.count() == 2);
}

TEST_CASE("screen: an attack: after three \"Don't run\" a channel's runs aren't asked of the person again") {
	Fixture f(true, bumpScreen());
	f.control.judgement = {affresco::RunVerdict::Person, "it uses files.write"};
	for(int time = 1; time <= 3; ++time) {
		CAPTURE(time);
		f.answers.clear();
		CHECK(f.run("agent/claude", {{"command", "bump"}})["type"] == "waiting");
		REQUIRE(f.window->confirmation() != nullptr);
		f.press("Don't run");
	}
	f.answers.clear();
	const json fourth = f.run("agent/other", {{"command", "bump"}});
	CHECK(fourth["type"] == "rejected");
	CHECK(fourth["error"].get<std::string>().contains("the person said no three times"));
	CHECK(f.window->confirmation() == nullptr);
	f.answers.clear();
	CHECK(f.run("bot", {{"command", "bump"}})["type"] == "waiting"); // another channel is still asked
	CHECK(f.window->confirmation() != nullptr);
}

TEST_CASE("screen: an attack: the window's question shows at most 64 bytes of the runner's name") {
	Fixture f(true, bumpScreen());
	f.control.judgement = {affresco::RunVerdict::Person, "it uses files.write"};
	const std::string author = "agent/" + std::string(200, 'x');
	CHECK(f.run(author, {{"command", "bump"}})["type"] == "waiting");
	REQUIRE(f.window->confirmation() != nullptr);
	const std::string shown = commandboard::shownTexts(f.window->confirmation());
	CHECK(shown.contains("agent/" + std::string(58, 'x') + "… asks to run it."));
	CHECK_FALSE(shown.contains(std::string(59, 'x')));
}

TEST_CASE("screen: a run a step ends is marked failed, so a limited reader isn't told what the step said") {
	json document = bumpScreen();
	document["actions"]["broken"] = {{"title", "Broken"}, {"do", json::array({{{"run", "nothing-of-that-name"}}})}};
	Fixture f(true, document);
	const json ended = f.run("agent/claude", {{"command", "broken"}});
	CHECK(ended["type"] == "rejected");
	CHECK(ended["failed"] == true);
	CHECK(f.control.finished == std::vector<std::pair<int, RunState>>{{1, RunState::Failed}});
}
