/// @file
/// @brief Unit tests for what a program's function reaches (RFC 0005, stage 6; VERSION_CONTROL.md, "Programs";
/// SECURITY.md, "The Lisp language"): a `run` of a name written out reaches what that name runs, worked out at the
/// build, and the steps a capability gates (`invoke`, `request`, `subscribe`, and `image` short of a dialog) are judged
/// by what they use when the code names it written out, as JSON's are. The attacks are written as an attacker would,
/// one per route: an attack on the first line (the scan of a revision) passes when the revision is refused, one on the
/// second (the check when a step runs, with the scan off) when the step never reached the application.

#include "LispApp.hpp"

#include <affresco/core/Services.hpp>

#include <doctest/doctest.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
using affresco::Key;
using affresco::RevisionStatus;
namespace services = affresco::services;

namespace {

using lispapp::App;

/// @brief Who may change what, in every program below: an agent's revision applies unless it is reviewed.
constexpr const char* kPermissions = R"(:permissions {:allow ["agent"] :reviewers ["studio"]})";

/// @brief A program with the person's control function, a JSON action that only sets, a native action, and a key the
/// file binds, which an agent's revision may wire to something else.
/// @param more Its other definitions.
/// @return Its text.
std::string program(const std::string& more) {
	return R"lisp(
(defn persons "The persons own: reaches outside." [] (call "hit"))
(defn quiet "Only sets." [] (set "state.n" 1))
)lisp" + more +
	       "\n(defscreen main " + kPermissions +
	       R"( :state {:n 0} :actions {:tidy [{:set "state.n" :to 2}] :approving [{:call "hit"}]} (panel#root))
)";
}

/// @brief Submits an agent's revision that sets one definition.
/// @param app The app.
/// @param name The definition's address.
/// @param text Its text.
/// @return What became of it.
affresco::RevisionResult agentSets(App& app, const std::string& name, const std::string& text) {
	return app.repository.submit(
	    {{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/app.lisp/" + name, text}}}});
}

/// @brief Submits an agent's revision binding Ctrl+E to a function or an action.
/// @param app The app.
/// @param what Its name.
/// @return What became of it.
affresco::RevisionResult agentBinds(App& app, const std::string& what) {
	return agentSets(app, "defkey ctrl+e", R"((defkey "ctrl+e" )" + what + ")");
}

} // namespace

TEST_CASE("lisp reach: a run of a name written out reaches what that name runs, not anything") {
	App app(program(R"lisp(
(defn runs-tidy "Runs a JSON action that only sets." [] (run "tidy"))
(defn runs-quiet "Runs a function that only sets." [] (run :quiet))
(defn runs-persons "Runs the persons control function." [] (run "persons"))
(defn runs-approving "Runs a JSON action with a control step." [] (run "approving"))
(defn runs-native "Runs what neither the document nor the program has: the application's." [] (run "native"))
(defn runs-both "One name that reaches nothing, then one that does." [] (run "tidy") (run "persons"))
)lisp"),
	        true);
	for(const char* function : {"runs-tidy", "runs-quiet"}) {
		CAPTURE(function);
		CHECK(agentBinds(app, function).status == RevisionStatus::Applied);
	}
	for(const char* function : {"runs-persons", "runs-approving", "runs-native", "runs-both"}) {
		CAPTURE(function);
		const auto result = agentBinds(app, function);
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.contains("permission denied"));
	}
}

TEST_CASE("lisp reach attack: a target built at run time, passed in, or taken as a value reaches anything") {
	App app(program(R"lisp(
(defn by-str "" [] (run (str "ti" "dy")))
(defn by-local "" [] (let [n "tidy"] (run n)))
(defn by-parameter "" [name] (run name))
(defn by-state "" [] (run state.which))
(defn by-apply "" [] (apply run ["tidy"]))
(defn by-value "" [] (let [f run] (f "tidy")))
(defn by-map "" [] (map run ["tidy"]))
(defn by-template "" [] (run "{{state.which}}"))
(defn by-a-tidy-and-a-value "One written out, then the step as a value." [] (run "tidy") (map run ["tidy"]))
)lisp"),
	        true);
	for(const char* function : {"by-str", "by-local", "by-parameter", "by-state", "by-apply", "by-value", "by-map",
	                            "by-template", "by-a-tidy-and-a-value"}) {
		CAPTURE(function);
		const auto result = agentBinds(app, function);
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.contains("permission denied"));
	}
}

TEST_CASE("lisp reach attack: a function holding a control step can't be altered, its arguments included") {
	// As a JSON action holding a control step: a revision may not point the step elsewhere, nor change what a
	// function that runs one gives it
	App app(program(R"lisp(
(defn reaches "Calls the native with what it is given." [what] (call what))
(defn persons-caller "The person's: gives reaches its argument." [] (reaches "hit"))
)lisp"),
	        true);
	for(const auto& [name, text] : std::vector<std::pair<std::string, std::string>>{
	        {"persons", R"((defn persons "The persons own: reaches outside." [] (call "other")))"},
	        {"persons", R"((defn persons "Reworded." [] (call "hit")))"},
	        {"persons-caller",
			 R"((defn persons-caller "The person's: gives reaches its argument." [] (reaches "other")))"}}) {
		CAPTURE(text);
		const auto result = agentSets(app, name, text);
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.contains("permission denied"));
	}
	// A function that reaches none stays the agent's to change
	CHECK(agentSets(app, "quiet", R"((defn quiet "Only sets." [] (set "state.n" 3)))").status ==
	      RevisionStatus::Applied);
}

TEST_CASE("lisp reach attack: a target redefined by a later revision takes what reaches it with it") {
	App app(program(R"lisp(
(defn target "Only sets, for now." [] (set "state.n" 3))
(defn runs-target "" [] (run "target"))
(defn runs-target-action "" [] (run "tidy"))
)lisp"),
	        true);
	REQUIRE(agentBinds(app, "runs-target").status == RevisionStatus::Applied);
	const auto function = agentSets(app, "target", R"lisp((defn target "" [] (call "hit")))lisp");
	CHECK(function.status == RevisionStatus::Rejected);
	CHECK(function.error.contains("permission denied"));
	const auto through = agentSets(app, "target", R"lisp((defn target "" [] (run "persons")))lisp");
	CHECK(through.status == RevisionStatus::Rejected);
	// The JSON action it names, made to run the person's function
	REQUIRE(agentBinds(app, "runs-target-action").status == RevisionStatus::Applied);
	const auto action = app.repository.submit(
	    {{"author", "agent/x"},
		 {"message", "m"},
		 {"set",
		  {{"/program/app.lisp/main",
		    std::string("(defscreen main ") + kPermissions +
		        R"( :state {:n 0} :actions {:tidy [{:run "persons"}] :approving [{:call "hit"}]} (panel#root)))"}}}});
	CHECK(action.status == RevisionStatus::Rejected);
}

TEST_CASE("lisp reach attack: a cycle reaches what any of it reaches, and ends") {
	App app(program(R"lisp(
(defn ping "" [] (run "pong"))
(defn pong "" [] (run "ping"))
(defn there "" [] (run "back"))
(defn left "" [] (run "right"))
(defn right "" [] (run "left") (call "hit"))
)lisp"),
	        true);
	CHECK(agentBinds(app, "ping").status == RevisionStatus::Applied);
	// Through a JSON action and back
	const auto loop = app.repository.submit(
	    {{"author", "agent/x"},
		 {"message", "m"},
		 {"set",
		  {{"/program/app.lisp/main", std::string("(defscreen main ") + kPermissions +
		                                  R"( :state {:n 0} :actions {:tidy [{:set "state.n" :to 2}]
	                                         :approving [{:call "hit"}] :back [{:run "there"}]} (panel#root)))"}}}});
	REQUIRE(loop.status == RevisionStatus::Applied);
	CHECK(agentBinds(app, "there").status == RevisionStatus::Applied);
	const auto result = agentBinds(app, "left");
	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(result.error.contains("permission denied"));
}

TEST_CASE("lisp reach attack: a target that is an agent's function is what that function reaches") {
	App app(program(R"lisp(
(defn helper "An agent may replace this." [] nil)
(defn persons-runs-helper "The persons: runs the helper by name." [] (run "helper"))
(defkey "ctrl+p" persons-runs-helper)
)lisp"),
	        true);
	const auto result = agentSets(app, "helper", R"lisp((defn helper "" [] (call "hit")))lisp");
	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(result.error.contains("permission denied"));
	// With the scan off, the check when it runs holds by itself
	App unscanned(program(R"lisp(
(defn helper "An agent may replace this." [] nil)
(defn persons-runs-helper "The persons: runs the helper by name." [] (run "helper"))
(defkey "ctrl+p" persons-runs-helper)
)lisp"));
	unscanned.write("helper", R"lisp((defn helper "" [] (call "hit")))lisp");
	unscanned.press(Key::P);
	CHECK(unscanned.hits == 0);
	CHECK(unscanned.reported("agent/x"));
}

TEST_CASE("lisp reach attack: a run of an action that advice reaches control through") {
	App app(program(R"lisp(
(defn runs-tidy "" [] (run "tidy"))
(advice tidy :after persons)
)lisp"),
	        true);
	const auto result = agentBinds(app, "runs-tidy");
	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(result.error.contains("permission denied"));
}

TEST_CASE("lisp reach attack: a step's handler is a function of the program, never a name or steps as data") {
	// `audio` runs anyone's: its onError, given a name, would have run that action by name, which nothing in a scan of
	// names sees
	App app(program(R"lisp(
(defn helper "" [] nil)
(defcommand c "" :title "C" [] (helper))
(defkey "ctrl+c" c)
)lisp"));
	for(const char* body : {R"((audio "play" :of "nothing" :onError "approving"))",
	                        R"((audio "play" :of "nothing" :onError [{:run "approving"}]))",
	                        R"((audio "play" :of "nothing" :onError (str "appro" "ving")))"}) {
		CAPTURE(body);
		app.write("helper", std::string("(defn helper \"\" [] ") + body + ")");
		app.errors.clear();
		app.press(Key::C);
		app.wait(50);
		CHECK(app.hits == 0);
		CHECK(app.reported("a function of the program"));
	}
}

namespace {

/// @brief A host's services, by hand: `files.read` reads, `files.write` writes, `files.secret` writes and no revision
/// may add it; every call that reaches it is counted, and none ends.
class CountedServices final : public services::Services {
public:
	std::map<std::string, int> calls; ///< How many times each method was invoked.

	CountedServices() {
		specs_["read"] = {.name = "read", .effect = services::Effect::Read, .arguments = {}, .about = "reads"};
		specs_["write"] = {.name = "write", .effect = services::Effect::Write, .arguments = {}, .about = "writes"};
		specs_["secret"] = {.name = "secret",
		                    .effect = services::Effect::Write,
		                    .revisable = false,
		                    .arguments = {},
		                    .about = "writes"};
	}
	/// @copydoc Services::load
	[[nodiscard]] std::vector<std::string> load(const json& /*section*/) override { return {}; }
	/// @copydoc Services::method
	[[nodiscard]] const services::MethodSpec* method(std::string_view service, std::string_view name) const override {
		if(service != "files") return nullptr;
		const auto it = specs_.find(std::string(name));
		return it == specs_.end() ? nullptr : &it->second;
	}
	/// @copydoc Services::invoke
	[[nodiscard]] std::optional<std::string> invoke(std::string_view service, std::string_view name,
	                                                std::shared_ptr<services::Call> /*call*/) override {
		++calls[std::string(service) + "." + std::string(name)];
		return std::nullopt;
	}
	/// @copydoc Services::subscribe
	[[nodiscard]] std::optional<std::string> subscribe(std::string_view /*service*/, std::string_view /*stream*/,
	                                                   std::shared_ptr<services::Call> /*call*/) override {
		return "no streams";
	}
	/// @copydoc Services::status
	[[nodiscard]] json status() const override { return json::object(); }
	/// @copydoc Services::onStatus
	void onStatus(std::function<void()> /*callback*/) override {}
	/// @copydoc Services::onRequest
	void onRequest(services::RequestHandler /*handler*/) override {}

private:
	std::map<std::string, services::MethodSpec> specs_; ///< The methods of `files`, by name.
};

/// @brief A program whose file has the person's functions on the service `files`, and a helper an agent may replace.
/// @param more Its other definitions.
/// @return Its text.
std::string serviceProgram(const std::string& more) {
	return R"lisp(
(defn reads "The files: reads, written out." [] (invoke "files.read" :with {:path "a"}))
(defn writes "The files: writes, written out." [path] (invoke "files.write" :with {:path path}))
(defn writes-what "The files: invokes what it is told." [method] (invoke (str "files." method)))
(defn calls-given "The files: calls what it is given with a name written out." [f] (f "files.write"))
(defn helper "An agent may replace this." [] nil)
(defcommand c "Calls the helper." :title "C" [] (helper))
(defkey "ctrl+c" c)
)lisp" + more +
	       "\n(defscreen main " + kPermissions +
	       R"( :services {:files {:provider "fs" :root "work"}} :state {:n 0} (panel#root))
)";
}

/// @brief An app on a service program, with the counted services in its rules and its runtime.
struct ServiceApp {
	CountedServices services; ///< The host's services.
	App app;                  ///< The app.

	/// @brief Builds it.
	/// @param more The program's other definitions.
	/// @param scan Whether revisions are scanned for new ways to reach control steps.
	explicit ServiceApp(const std::string& more = "", bool scan = false) : app(serviceProgram(more), scan) {
		app.rules.setServices(&services);
		app.runtime->setServices(&services);
		app.settle();
	}

	/// @brief Has the agent replace the helper, then presses the key that runs it.
	/// @param body The helper's body.
	void agentRuns(const std::string& body) {
		app.write("helper", "(defn helper \"\" [] " + body + ")");
		app.errors.clear();
		app.press(Key::C);
		app.wait(50);
	}
};

} // namespace

TEST_CASE("lisp capability: an agent's function calling the file's use of a service, written out, runs it, as JSON") {
	ServiceApp host;
	host.agentRuns(R"((reads))");
	CHECK(host.services.calls["files.read"] == 1);
	host.agentRuns(R"((writes "anywhere"))");
	CHECK(host.services.calls["files.write"] == 1);
	CHECK(host.app.errors.empty());
}

TEST_CASE("lisp capability attack: a computed name, or the step taken as a value, stays a control step") {
	ServiceApp host;
	for(const char* body :
	    {R"((writes-what "write"))", R"((invoke (str "files." "write")))", R"((apply invoke ["files.write"]))",
	     R"((calls-given invoke))", R"((let [f invoke] (f "files.write")))", R"((map invoke ["files.write"]))"}) {
		CAPTURE(body);
		host.agentRuns(body);
		CHECK(host.services.calls["files.write"] == 0);
		CHECK(host.app.reported("agent/x"));
	}
}

TEST_CASE("lisp capability attack: a name written out covers that name only, in that code") {
	ServiceApp host(R"lisp(
(defn mixed "Reads, written out, then invokes what it is told." [method] (invoke "files.read") (invoke method))
)lisp");
	host.agentRuns(R"((mixed "files.write"))");
	CHECK(host.services.calls["files.read"] == 1);
	CHECK(host.services.calls["files.write"] == 0);
	CHECK(host.app.reported("agent/x"));
}

TEST_CASE("lisp capability: an agent's own use, written out, is judged as JSON's: declared, revisable, and reviewed") {
	ServiceApp host("", true);
	const auto reads = agentSets(host.app, "helper", R"lisp((defn helper "" [] (invoke "files.read")))lisp");
	CHECK(reads.status == RevisionStatus::Applied);
	const auto writes = agentSets(host.app, "helper", R"lisp((defn helper "" [] (invoke "files.write")))lisp");
	CHECK(writes.status == RevisionStatus::Proposed);
	const auto secret = agentSets(host.app, "helper", R"lisp((defn helper "" [] (invoke "files.secret")))lisp");
	CHECK(secret.status == RevisionStatus::Rejected);
	const auto undeclared = agentSets(host.app, "helper", R"lisp((defn helper "" [] (invoke "other.read")))lisp");
	CHECK(undeclared.status == RevisionStatus::Rejected);
	// A use written out beside a computed one is still a use: the check when it runs lets the one written out through
	const auto hidden =
	    agentSets(host.app, "helper", R"lisp((defn helper "" [m] (invoke "files.secret") (invoke m)))lisp");
	CHECK(hidden.status == RevisionStatus::Rejected);
	// Changing what a use reviewed already is given: a new use, reviewed again
	App& app = host.app;
	app.write("reads", R"lisp((defn reads "" [] (invoke "files.write" :with {:path "a"})))lisp", "file");
	const auto changed =
	    agentSets(app, "reads", R"lisp((defn reads "" [] (invoke "files.write" :with {:path "/elsewhere"})))lisp");
	CHECK(changed.status == RevisionStatus::Proposed);
}

TEST_CASE("lisp capability attack: an agent's macro writing a use into the file's function is a new use") {
	ServiceApp host(R"lisp(
(defmacro written "An agent may replace this." [] nil)
(defn mine "The files: its text never changes." [] (written))
)lisp",
	                true);
	const auto writes = agentSets(host.app, "written", R"lisp((defmacro written "" [] '(invoke "files.write")))lisp");
	CHECK(writes.status == RevisionStatus::Proposed);
	const auto secret = agentSets(host.app, "written", R"lisp((defmacro written "" [] '(invoke "files.secret")))lisp");
	CHECK(secret.status == RevisionStatus::Rejected);
}

TEST_CASE("lisp capability: the scan lets an agent wire a key to a use written out, never to a computed one") {
	ServiceApp host("", true);
	CHECK(agentBinds(host.app, "reads").status == RevisionStatus::Applied);
	CHECK(agentSets(host.app, "defkey ctrl+e", R"((defkey "ctrl+e" {:run writes :with {:path "x"}}))").status ==
	      RevisionStatus::Applied);
	const auto computed = agentSets(host.app, "defkey ctrl+e", R"((defkey "ctrl+e" {:run writes-what}))");
	CHECK(computed.status == RevisionStatus::Rejected);
}

TEST_CASE("lisp capability attack: hooks, advice and callbacks an agent adds reach a use as a button would, no more") {
	ServiceApp host(R"lisp(
(defhook point "")
(defn runs-point "" [] (run-hooks point))
(defkey "ctrl+h" runs-point)
)lisp");
	App& app = host.app;
	// A callback the agent's code schedules: the file's use runs, its computed one doesn't
	host.agentRuns(R"((after 10 writes "x"))");
	CHECK(host.services.calls["files.write"] == 1);
	host.agentRuns(R"((after 10 writes-what "write"))");
	CHECK(host.services.calls["files.write"] == 1);
	CHECK(app.reported("agent/x"));
	// A hook: its own use written out, and one computed
	app.write("agent-hook", R"lisp((defn agent-hook "" [] (invoke "files.read") (writes-what "write")))lisp");
	app.write("add-hook point agent-hook", "(add-hook point agent-hook)");
	app.errors.clear();
	app.press(Key::H);
	CHECK(host.services.calls["files.read"] == 1);
	CHECK(host.services.calls["files.write"] == 1);
	CHECK(app.reported("agent/x"));
	// Advice going on with the file's use
	app.write("wrap", R"lisp((defn wrap "" [proceed] (proceed)))lisp");
	app.write("advice reads around wrap", "(advice reads :around wrap)");
	app.write("helper", R"lisp((defn helper "" [] (reads)))lisp");
	app.press(Key::C);
	CHECK(host.services.calls["files.read"] == 2);
}

TEST_CASE("lisp capability attack: a person's command using an agent's helper keeps its use, and loses its control") {
	ServiceApp host(R"lisp(
(defcommand d "The persons: the helper, then a use, then a control step." :title "D" [] (helper) (reads) (call "hit"))
(defkey "ctrl+d" d)
)lisp");
	App& app = host.app;
	app.write("helper", R"lisp((defn helper "" [] 1))lisp");
	app.press(Key::D);
	CHECK(host.services.calls["files.read"] == 1);
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE(
    "lisp capability attack: an image step short of a dialog is judged as JSON's; a dialog or a computed one isn't") {
	ServiceApp host;
	host.agentRuns(R"((image "addLayer" :of "doc"))");
	CHECK_FALSE(host.app.reported("agent/x"));
	for(const char* body : {R"((image "open" :of "doc"))", R"((image "save" :of "doc"))",
	                        R"((image (str "add" "Layer") :of "doc"))", R"((image "{{state.op}}" :of "doc"))"}) {
		CAPTURE(body);
		host.agentRuns(body);
		CHECK(host.app.reported("agent/x"));
	}
}

TEST_CASE(
    "lisp reach attack: a chain between functions and actions longer than the rounds the scan takes still reaches") {
	// f0 runs the action a1, which runs f1, which runs a2... and the last reaches outside: twenty hops back and forth,
	// past the sixteen rounds the scan settles in, after which every action a function runs by name counts as reaching
	for(const int hops : {3, 20}) {
		CAPTURE(hops);
		std::string functions;
		std::string actions;
		for(int i = 0; i < hops; ++i) {
			const std::string next = std::to_string(i + 1);
			functions.append("(defn f")
			    .append(std::to_string(i))
			    .append(R"( "" [] (run "a)")
			    .append(next)
			    .append("\"))\n");
			actions.append(":a").append(next).append(R"( [{:lisp "f)").append(next).append("\"}] ");
		}
		functions.append("(defn f").append(std::to_string(hops)).append(R"( "" [] (call "hit")))").append("\n");
		functions.append("(defscreen main ").append(kPermissions).append(" :state {:n 0} :actions {");
		functions.append(actions).append("} (panel#root))\n");
		App app(functions, true);
		const auto result = agentBinds(app, "f0");
		CHECK(result.status == RevisionStatus::Rejected);
		CHECK(result.error.contains("permission denied"));
	}
	// The same short chain, ending in a step that only sets, may be bound: it is what reaches that refused it
	const std::string quiet =
	    "(defn f0 \"\" [] (run \"a1\"))\n(defn f1 \"\" [] (run \"a2\"))\n(defn f2 \"\" [] (set \"state.n\" 1))\n";
	App app(quiet + "(defscreen main " + kPermissions +
	            R"( :state {:n 0} :actions {:a1 [{:lisp "f1"}] :a2 [{:lisp "f2"}]} (panel#root))
)",
	        true);
	CHECK(agentBinds(app, "f0").status == RevisionStatus::Applied);
}
