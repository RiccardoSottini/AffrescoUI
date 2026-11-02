/// @file
/// @brief Attacks from the independent review of RFC 0005 (SECURITY.md, "The Lisp language"), each failing on what was
/// there before its fix: the language's natives given what they don't expect, work that wasn't paid for, what advice
/// can keep or read, and who a hook's or an advice's run counts.

#include "LispApp.hpp"

#include <affresco/automation/Scenarios.hpp>
#include <affresco/lisp/Program.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using affresco::json;
using affresco::Key;
using affresco::RevisionStatus;
using lispapp::App;
namespace lisp = affresco::lisp;

namespace {

/// A text of 1 MiB, doubled from one character.
constexpr const char* kMegabyte = "(reduce (fn [text _] (str text text)) \"x\" (range 20))";

/// @brief What a text costs to evaluate.
/// @param interpreter The interpreter, with `big` defined.
/// @param text The text.
/// @return The operations it spent.
std::size_t operationsOf(lisp::Interpreter& interpreter, const std::string& text) {
	const std::size_t before = interpreter.budget().operations;
	static_cast<void>(interpreter.eval(text, ""));
	return before - interpreter.budget().operations;
}

} // namespace

TEST_CASE("lisp attack: the language's own % functions, called by a program") {
	for(const char* form :
	    {"(%nth)", "(%nth [1])", "(%nth [1] :a)", "(%drop)", "(%drop [1])", "(%drop [1] :a)", "(%sort-by-keys)",
	     "(%sort-by-keys [1])", "(%sort-by-keys [1 2 3] [])", "(%sort-by-keys [] [1 2 3])", "(%sort-by-keys 1 2)",
	     "(%sort-by-keys {:a 1} {:b 2})", "(%expression)", "(%binding 1)", "(%seq)", "(%get)", "(gensym 1)"}) {
		CAPTURE(form);
		lisp::Interpreter interpreter;
		try {
			static_cast<void>(interpreter.eval(form, ""));
		} catch(const lisp::LispError& error) {
			CHECK_FALSE(error.exhausted());
		}
	}
}

TEST_CASE("lisp attack: an integer overflow in range is no undefined behaviour") {
	lisp::Limits limits;
	limits.budget.bytes = std::size_t{1} << 30U;
	lisp::Interpreter interpreter(limits);
	const lisp::Value made =
	    interpreter.eval("(range -9223372036854775807 9223372036854775807 17592186044416)", ""); // 2^44 apart
	CHECK(interpreter.toTree(made).size() > 1000);
}

TEST_CASE("lisp attack: what a value's size makes slow costs its size") {
	lisp::Interpreter interpreter;
	static_cast<void>(interpreter.eval(std::string("(def big ") + kMegabyte + ")", ""));
	static_cast<void>(interpreter.eval("(def bigs [big big big big big big big big])", ""));
	static_cast<void>(
	    interpreter.eval(R"lisp((def far (assoc (zipmap (map str (range 40)) (range 40)) big 1)))lisp", ""));
	constexpr std::size_t atLeast = 4096; // of 1 MiB: 256 bytes for an operation, far less than a step's cost in time
	for(const char* form : {"(count big)", "(empty? big)", "(subs big 1000000)", "(= big (str big))", "(sort bigs)",
	                        "(get far big)", "(far big)", "(contains? far big)"}) {
		CAPTURE(form);
		CHECK(operationsOf(interpreter, form) >= atLeast);
	}
	// A big map viewed from JSON is searched member by member
	json object = json::object();
	for(int key = 0; key < 100000; ++key) object[std::to_string(key)] = key;
	const lisp::Value viewed = interpreter.viewJson(std::make_shared<const json>(std::move(object)));
	const std::size_t before = interpreter.budget().operations;
	const std::optional<lisp::Value> get = interpreter.global("get");
	REQUIRE(get.has_value());
	const std::vector<lisp::Value> arguments = {viewed, interpreter.makeString("nothing-here")};
	static_cast<void>(interpreter.call(*get, arguments));
	CHECK(before - interpreter.budget().operations >= atLeast);
}

TEST_CASE("lisp attack: join and %binding are charged as they grow, not after") {
	lisp::Limits limits;
	limits.budget.operations = lisp::maxBuildOperations;
	limits.budget.bytes = lisp::maxBuildBytes;
	for(const char* last : {"(join big (range 3000))", "(apply %binding (map (fn [_] big) (range 3000)))"}) {
		CAPTURE(last);
		lisp::Interpreter interpreter(limits);
		static_cast<void>(interpreter.eval(std::string("(def big ") + kMegabyte + ")", ""));
		const std::size_t before = interpreter.budget().bytes;
		CHECK_THROWS_AS(static_cast<void>(interpreter.eval(last, "")), lisp::LispError);
		// It stopped as it reached its budget: what is left is what it had, less the budget's worth at the most
		CHECK(interpreter.budget().bytes < before);
		CHECK(interpreter.budget().bytes <= std::size_t{1} << 20U);
	}
}

TEST_CASE("lisp attack: a function with more locals than anyone writes costs its compiling") {
	std::string text = "(defn f [] (let [";
	for(int at = 0; at < 60000; ++at) text += "a" + std::to_string(at) + " 1 ";
	text += ']';
	for(int at = 0; at < 60000; ++at) text += " a" + std::to_string(at);
	text += "))";
	lisp::Program program;
	program.addFile(text, "app.lisp");
	lisp::Limits limits;
	limits.budget.operations = lisp::maxBuildOperations;
	limits.budget.bytes = lisp::maxBuildBytes;
	try {
		static_cast<void>(program.build(limits));
		FAIL("it should have run out");
	} catch(const lisp::LispError& error) {
		CHECK(error.exhausted());
	}
}

TEST_CASE("lisp attack: sort with what can't be ordered") {
	lisp::Interpreter interpreter;
	static_cast<void>(interpreter.eval("(def nan (let [i (* 1e308 10)] (- i i)))", ""));
	const json sorted = interpreter.toTree(interpreter.eval("(sort [3 nan 1 nan 2 nan 0 nan 5 4])", ""));
	REQUIRE(sorted.size() == 10);
	CHECK(sorted[0] == 0); // what has an order is in it; what has none comes last
	CHECK(sorted[5] == 5);
}

TEST_CASE("lisp attack: an agent's :around keeps what goes on with a function, and calls it after") {
	App app(R"lisp(
(defn target "" [] 1)
(defn keep-it "The file's, for now." [proceed] nil)
(defn keep-too "An agent's." [proceed] (proceed))
(advice target :around keep-too)
(advice target :around keep-it)
(defn caller "" [] (let [kept (target)] (when kept (kept))))
(defcommand c "" :title "C" [] (caller))
(defkey "ctrl+c" c)
(defscreen main (panel#root))
)lisp");
	app.write("keep-it", R"lisp((defn keep-it "An agent's." [proceed] proceed))lisp");
	app.errors.clear();
	app.press(Key::C);
	CHECK(app.reported("has ended"));
}

TEST_CASE("lisp attack: an agent's hook on a person's function, run from a JSON step") {
	App app(R"lisp(
(defhook point "A hook point.")
(defn reach "The person's, with a control step." [] (call "hit"))
(defn quiet "" [] 1)
(defcommand c "" :title "C" [] (run "from-json"))
(defkey "ctrl+c" c)
(defscreen main :actions {:from-json [{:runHooks "point"}]} (panel#root))
)lisp");
	app.write("add-hook point reach", "(add-hook point reach)");
	app.errors.clear();
	app.runtime->runAction("from-json", app.window->root(), json::object());
	CHECK(app.hits == 0);
	CHECK(app.reported("agent/x"));
}

TEST_CASE("lisp attack: an agent's advice on a JSON action names a person's function") {
	for(const char* how : {":before", ":after", ":around"}) {
		CAPTURE(how);
		App app(R"lisp(
(defn reach "The person's, with a control step." [& more] (call "hit"))
(defscreen main :actions {:plain [{:set "state.out" :to 1}]} :state {:out 0} (panel#root))
)lisp");
		const std::string address = "advice plain " + std::string(how).substr(1) + " reach";
		app.write(address, "(advice plain " + std::string(how) + " reach)");
		app.errors.clear();
		app.runtime->runAction("plain", app.window->root(), json::object());
		CHECK(app.hits == 0);
		CHECK(app.reported("agent/x"));
	}
}

TEST_CASE("lisp attack: a stranger's advice reads what it wasn't given, through the variables around it") {
	App app(R"lisp(
(defn pay "" [card] (set "state.done" true))
(defn spy "The file's, for now." [] 1)
(advice pay :before spy)
(defscreen main :state {:stolen nil :event nil :done false}
  :actions {:start [{:run "pay" :with {:card {:number "4242"}}}]}
  (panel#root))
)lisp");
	app.write(
	    "spy",
	    R"lisp((defn spy "An agent's." [] (set "state.stolen" card.number) (set "state.event" event.secret)))lisp");
	app.runtime->runAction("start", app.window->root(), json{{"secret", "hidden"}});
	CHECK(app.state("done") == true);
	CHECK(app.state("stolen").is_null());
	CHECK(app.state("event").is_null());
}

TEST_CASE("lisp attack: a JSON app gains no code by a revision") {
	const json document = {{"format", "affrescoui-screen"},
	                       {"version", 2},
	                       {"root", {{"id", "root"}, {"type", "panel"}}},
	                       {"actions", {{"plain", json::array({{{"set", "state.out"}, {"to", 1}}})}}}};
	const json program = {{"app.lisp", {{"f", "(defn f [] 1)"}}}};
	const lispapp::WithoutTheScan rules(true);
	affresco::vcs::Repository repository(rules, document);
	const json add = {{"author", "agent/x"},
	                  {"message", "m"},
	                  {"patch", {{{"op", "add"}, {"path", "/program"}, {"value", program}}}}};
	const auto submitted = repository.submit(add);
	CHECK(submitted.status == RevisionStatus::Rejected);
	CHECK(submitted.error.contains("static"));
	CHECK(repository.propose(add).status == RevisionStatus::Rejected);
	CHECK(repository.commit(add).has_value());
	json person = add;
	person["author"] = "user";
	CHECK(repository.commitOwnInput(person).has_value());
	CHECK_FALSE(repository.sourceDocument().contains("program"));
	// A change set brings none either
	const json changes = {{"format", "affrescoui-changes"},
	                      {"version", 1},
	                      {"name", "x"},
	                      {"revisions", json::array({{{"message", "m"}, {"patch", add["patch"]}}})}};
	CHECK(repository.importChanges(changes).has_value());
	CHECK_FALSE(repository.sourceDocument().contains("program"));
	// A program may still take a definition out, and be one
	App app("(defn f [] 1) (defn g [] 2) (defscreen main (panel#root))");
	app.commit({{"author", "agent/x"}, {"message", "m"}, {"remove", json::array({"/program/app.lisp/g"})}});
	CHECK(app.repository.sourceDocument().contains("program"));
}

TEST_CASE("lisp attack: a program's channels, built by it, are pointed nowhere when a scenario plays") {
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / "affresco-gate-scenario";
	std::filesystem::remove_all(folder);
	std::filesystem::create_directories(folder);
	const std::string document = lisp::sourceDocument(R"lisp(
(defscreen main
  :channels {:out {:socket "/tmp/affresco-a-reviewers.sock"}}
  :state {:closed nil}
  :actions {"out:closed" [{:set "state.closed" :to "{{event.message}}"}]}
  (panel#root))
)lisp",
	                                                  "app.lisp")
	                                 .dump();
	/// @brief Runs what is posted, at once.
	class NoPeers final : public affresco::Executor {
	public:
		/// @copydoc Executor::post
		void post(std::function<void()> task) override { task(); }
	} noPeers;
	const affresco::detail::ManualClock clock;
	const std::unique_ptr<affresco::Window> window = affresco::detail::createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::automation::unconnectedScreen(document, folder, folder / "no-host");
	screen.build(*window);
	screen.startChannels(noPeers);
	const json state = json::parse(screen.stateJson());
	CHECK(state["closed"].get<std::string>().contains("isn't allowed"));
	std::filesystem::remove_all(folder);
}

TEST_CASE("lisp attack: what a limited reader can't write, it can't write through the root") {
	App app(R"lisp(
(defn secret-fn "" [] 1)
(defscreen main :state {:secret "hunter2"}
  :permissions {:allow ["reader"] :reviewers ["studio"] :data [{:author "reader" :read ["/state/secret"]}]}
  (panel#root))
)lisp");
	json whole = app.repository.sourceDocument();
	whole["program"]["app.lisp"]["secret-fn"] = R"lisp((defn secret-fn "Mine." [] 2))lisp";
	const auto result = app.repository.submit(
	    {{"author", "reader"}, {"message", "m"}, {"patch", {{{"op", "replace"}, {"path", ""}, {"value", whole}}}}});
	CHECK(result.status == RevisionStatus::Rejected);
	CHECK(app.repository.sourceDocument()["program"]["app.lisp"]["secret-fn"].get<std::string>().find("Mine") ==
	      std::string::npos);
}

TEST_CASE("lisp attack: a person's undo of many definitions doesn't take the main thread") {
	// An agent adds them, takes them away, and the person's undo brings them all back: each is the agent's, at one walk
	App app("(defn f [] 1) (defscreen main (panel#root))");
	json some = json::object();
	for(int at = 0; at < 2000; ++at) {
		const std::string name = "many" + std::to_string(at);
		some[name] = "(defn " + name + R"lisp( "" [] 1))lisp";
	}
	app.commit({{"author", "agent/x"}, {"message", "m"}, {"set", {{"/program/more.lisp", some}}}});
	app.commit({{"author", "agent/x"}, {"message", "m"}, {"remove", json::array({"/program/more.lisp"})}});
	app.commit(app.repository.revertRevision("HEAD", "user"));
	const json authors = app.repository.programAuthors();
	CHECK(authors["more.lisp"]["many0"] == "agent/x");
	CHECK(authors["more.lisp"]["many1999"] == "agent/x");
}

TEST_CASE("lisp attack: native re-entry through steps is bounded, and closures don't outlive what they hold") {
	App app(R"lisp(
(def base "A vector that lasts." [1 2 3])
(defn again "" [] (run "again"))
(defn grow "" []
  (let [more (conj base 4)
        tail (rest base)
        keep (fn [] (conj tail 5))]
    (set "state.seen" [(count more) (count (keep)) (count base)])))
(defcommand a "" :title "A" [] (again))
(defcommand g "" :title "G" [] (grow))
(defkey "ctrl+a" a)
(defkey "ctrl+g" g)
(defscreen main :state {:seen nil} (panel#root))
)lisp");
	app.errors.clear();
	app.press(Key::A);
	CHECK_FALSE(app.errors.empty());
	for(int turn = 0; turn < 5; ++turn) app.press(Key::G); // each event gives back what it made, and sees base whole
	CHECK(app.state("seen") == json::array({4, 3, 3}));
}

TEST_CASE("lisp attack: reading a long text is linear, on one line and form by form") {
	using Clock = std::chrono::steady_clock;
	std::string words = "(def x '[";
	for(int at = 0; at < 180000; ++at) words += "a ";
	words += "])";
	const auto started = Clock::now();
	lisp::Program one;
	one.addFile(words, "app.lisp");
	CHECK(Clock::now() - started < std::chrono::seconds(10));
	std::string forms;
	for(int at = 0; at < 500000; ++at) forms += "(a)";
	const auto again = Clock::now();
	CHECK(lisp::formsOf(forms, "").size() == 500000);
	CHECK(Clock::now() - again < std::chrono::seconds(10));
}
