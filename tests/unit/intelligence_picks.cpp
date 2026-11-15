/// @file
/// @brief What the person pointed at, as the in-process agent reads it (modules/intelligence/src/Picks.hpp): a
/// request's picks kept to their bounds, a first message that starts from the element and the definition that built
/// it instead of the whole screen, the rules for it only when there are picks, and the bytes it saves on the examples.
/// Driven like intelligence_agent.cpp, with a stand-in screen and claude-code answering from a list.

#include "../../modules/intelligence/src/Picks.hpp"

#include <affresco/intelligence/Agent.hpp>
#include <affresco/intelligence/ScreenQueries.hpp>

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <cctype>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using affresco::json;
namespace intelligence = affresco::intelligence;

namespace {

/// @brief An Executor safe from any thread, run by the test's thread.
class Tasks final : public affresco::Executor {
public:
	/// @brief Queues a task.
	/// @param task The task.
	void post(std::function<void()> task) override {
		{
			const std::scoped_lock lock(mutex_);
			tasks_.push_back(std::move(task));
		}
		wake_.notify_all();
	}
	/// @brief Runs tasks until a condition holds or ten seconds pass.
	/// @param done The condition, asked between tasks.
	/// @return Whether the condition holds.
	bool runUntil(const std::function<bool()>& done) {
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while(!done()) {
			std::function<void()> task;
			{
				std::unique_lock lock(mutex_);
				if(!wake_.wait_until(lock, end, [this] { return !tasks_.empty(); })) return done();
				task = std::move(tasks_.front());
				tasks_.pop_front();
			}
			task();
		}
		return true;
	}

private:
	std::mutex mutex_;                        ///< Guards tasks_.
	std::condition_variable wake_;            ///< Told of a task.
	std::deque<std::function<void()>> tasks_; ///< Waiting.
};

/// @brief The next number of a folder, so that two alive at once are apart.
/// @return It.
int nextFolder() {
	static int count = 0;
	return ++count;
}

/// @brief A data folder of its own, removed after the test.
struct Folder {
	/// @brief Creates it.
	Folder() { std::filesystem::create_directories(path); }
	Folder(const Folder&) = delete;
	Folder& operator=(const Folder&) = delete;
	/// @brief Removes it.
	~Folder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	/// The folder.
	std::filesystem::path path = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-picks-" + std::to_string(_getpid()) + "-" + std::to_string(nextFolder()));
#else
	                             ("affrescoui-picks-" + std::to_string(getpid()) + "-" + std::to_string(nextFolder()));
#endif
};

/// @brief The agent on claude-code, with a stand-in screen that answers it with a document.
struct Bench {
	Folder data;                                        ///< intelligence.json.
	Tasks tasks;                                        ///< The "main thread".
	json document;                                      ///< The screen's document.
	json state = {{"board", "Q3"}};                     ///< Its live state.
	std::vector<json> sent;                             ///< What the agent sent the screen.
	std::vector<json> revisions;                        ///< The revisions among them.
	std::vector<std::string> systems;                   ///< claude-code's system prompt, each time it ran.
	std::vector<std::string> inputs;                    ///< Its input, each time.
	std::vector<std::string> answers;                   ///< What it answers, in turn.
	std::size_t inlineDocument = std::size_t{32} * 1024;           ///< The agent's option.
	std::unique_ptr<intelligence::ProviderAgent> agent; ///< The agent.

	/// @brief Takes the document; start() starts the agent.
	/// @param screen The screen's document.
	explicit Bench(json screen) : document(std::move(screen)) {}
	Bench(const Bench&) = delete;
	Bench& operator=(const Bench&) = delete;
	Bench(Bench&&) = delete;
	Bench& operator=(Bench&&) = delete;
	/// @brief Stops the agent before the rest goes.
	~Bench() = default;

	/// @brief Starts the agent.
	void start() {
		intelligence::Settings settings;
		settings.provider = "claude-code";
		settings.model = "opus";
		settings.save(data.path);
		intelligence::AgentOptions options;
		options.dataDir = data.path;
		options.catalog = intelligence::Catalog::standard();
		options.guide = "# Format\nIntro.\n## Revisions\nCompact edits.\n";
		options.keys = std::make_shared<affresco::MemorySecretStore>();
		options.inlineDocument = inlineDocument;
		options.claude = [this](const std::string&, const std::string& system, const std::string& input,
		                        const std::function<bool()>&) -> intelligence::ClaudeAnswer {
			systems.push_back(system);
			inputs.push_back(input);
			const std::size_t turn = inputs.size() - 1;
			return {true, turn < answers.size() ? answers[turn] : R"({"reply": "OK"})"};
		};
		agent = std::make_unique<intelligence::ProviderAgent>(tasks, std::move(options));
		agent->onMessage.connect([this](const std::string& line) { answer(json::parse(line)); });
	}

	/// @brief The stand-in screen answers what the agent sent.
	/// @param message The message.
	void answer(const json& message) {
		sent.push_back(message);
		const std::string type = message.value("type", "");
		json reply;
		if(type == "getDocument") {
			reply = {{"type", "document"}, {"revision", 41}, {"document", document}, {"state", state}};
		} else if(type == "revision") {
			revisions.push_back(message);
			reply = {{"type", "proposed"}, {"proposal", 9}};
		} else {
			return;
		}
		reply["id"] = message["id"];
		agent->send(reply.dump());
	}

	/// @brief The person asks from the screen's chat, with what they pointed at; waits for the final reply.
	/// @param text The request.
	/// @param picks Its "picks", or null for none.
	/// @return The final reply.
	json ask(const std::string& text, const json& picks = nullptr) {
		if(!agent) start();
		json request = {{"type", "request"}, {"id", "ask-1"}, {"text", text}, {"from", "user"}, {"source", "screen"}};
		if(!picks.is_null()) request["picks"] = picks;
		sent.clear();
		agent->send(request.dump());
		const auto finalReply = [this] {
			for(const json& message : sent)
				if(message.value("type", "") == "reply" && message.value("done", false)) return &message;
			return static_cast<const json*>(nullptr);
		};
		REQUIRE(tasks.runUntil([&] { return finalReply() != nullptr; }));
		return *finalReply();
	}

	/// @brief The first message of the last request.
	/// @return It.
	[[nodiscard]] const std::string& first() const { return inputs.front(); }
};

/// @brief Whether a text holds another.
/// @param text The text.
/// @param part The other.
/// @return Whether it does.
bool has(const std::string& text, const std::string& part) { return text.contains(part); }

/// @brief A program of many definitions, larger than the agent's inlineDocument when @p padding is large: a board
/// whose card title is built by one definition among them.
/// @param padding How many filler definitions.
/// @return The document.
json boardProgram(int padding) {
	json document = {
	    {"root",
		 {{"id", "page"},
		  {"children",
		   {{{"id", "cards"},
		     {"repeat", {{"for", "card"}, {"in", "state.cards"}}},
		     {"children", {{{"id", "cardTitle"}, {"type", "label"}, {"text", "{{card.title}}"}}}}}}}}},
	    {"program",
		 {{"columns.lisp",
		   {{"card-title", "(defview card-title [card]\n  `(label#cardTitle :text ~card.title))"},
		    {"column-name", "(defview column-name [column]\n  `(label#columnName :text ~column.name))"}}}}}};
	for(int i = 0; i < padding; ++i)
		document["program"]["filler.lisp"]["filler-" + std::to_string(i)] =
		    "(defview filler-" + std::to_string(i) + " []\n  `(node#filler" + std::to_string(i) + " :text \"" +
		    std::string(200, 'x') + "\"))";
	return document;
}

/// @brief A JSON screen with a card title among many other nodes.
/// @param padding How many other nodes.
/// @return The document.
json boardDocument(int padding) {
	json document = {
	    {"root",
		 {{"id", "page"},
		  {"children",
		   {{{"id", "drawer"},
		     {"children",
		      {{{"id", "cardTitle"}, {"type", "label"}, {"text", "{{card.title}}"}, {"onClick", "open"}}}}}}}}}};
	for(int i = 0; i < padding; ++i)
		document["root"]["children"].push_back(
		    {{"id", "other" + std::to_string(i)}, {"type", "label"}, {"text", "Something else " + std::to_string(i)}});
	return document;
}

/// @brief The pick of the card title in the board program.
/// @return The record.
json cardPick() {
	return {{"id", "pick-3"},
	        {"revision", 41},
	        {"node", "#cardTitle"},
	        {"type", "label"},
	        {"pointer", "/root/children/0/children/0"},
	        {"bounds", {412, 236, 180, 20}},
	        {"bindings", {{"text", "{{card.title}}"}}},
	        {"repeat", {{{"for", "card"}, {"in", "state.cards"}, {"index", 3}}}},
	        {"exact", true},
	        {"source",
	         {{"address", "/program/columns.lisp/card-title"},
	          {"kind", "defview"},
	          {"file", "columns.lisp"},
	          {"line", 2},
	          {"column", 3},
	          {"shared", false}}}};
}

/// @brief The pick of the card title in the JSON board.
/// @return The record.
json jsonPick() {
	return {{"node", "#cardTitle"},
	        {"type", "label"},
	        {"bindings", {{"text", "{{card.title}}"}}},
	        {"handlers", {"onClick"}},
	        {"exact", true},
	        {"source", {{"address", "#cardTitle"}, {"file", "board.json"}, {"shared", false}}}};
}

/// @brief The address of the definition that writes a node, as a type and its id ("(label#cardTitle"), not one that
/// only names it.
/// @param document The program's document.
/// @param node The node's address ("#cardTitle").
/// @return The address, or "".
std::string definitionOf(const json& document, const std::string& node) {
	for(const auto& [file, definitions] : document["program"].items())
		for(const auto& [name, text] : definitions.items()) {
			const std::string& written = text.get_ref<const std::string&>();
			for(std::size_t at = written.find(node); at != std::string::npos; at = written.find(node, at + 1)) {
				const std::size_t end = at + node.size();
				const bool typed = at > 0 && std::isalpha(static_cast<unsigned char>(written[at - 1])) != 0;
				const bool whole = end == written.size() || std::isalnum(static_cast<unsigned char>(written[end])) == 0;
				if(typed && whole) return std::format("/program/{}/{}", file, name);
			}
		}
	return "";
}

} // namespace

TEST_CASE("intelligence picks: on a large program, the first message holds the picked definition, not the screen") {
	Bench bench(boardProgram(400));
	bench.inlineDocument = std::size_t{16} * 1024;
	REQUIRE(intelligence::compact(bench.document).size() > bench.inlineDocument);
	bench.ask("Make this bold", json::array({cardPick()}));
	const std::string& first = bench.first();
	CHECK(first.starts_with("The person asks: Make this bold\n\nThe person pointed at this on the screen:\n- "));
	CHECK(has(first, "\"node\":\"#cardTitle\""));
	CHECK(has(first, "It is built by this definition:\n(defview card-title [card]\n  `(label#cardTitle"));
	CHECK_FALSE(has(first, "column-name")); // another definition
	CHECK_FALSE(has(first, "filler-1"));
	CHECK_FALSE(has(first, intelligence::outline(bench.document).substr(0, 40))); // nor the outline
	CHECK_FALSE(has(first, "\"board\":\"Q3\""));                                  // nor the state
	CHECK(has(first, "(revision 41, 3 nodes, 402 definitions) isn't shown here: ask for what the change needs with "
	                 "read, find and guide."));
	CHECK(first.size() < 2000);
}

TEST_CASE("intelligence picks: on a JSON screen, the first message holds the picked node, not the rest") {
	Bench bench(boardDocument(600));
	bench.inlineDocument = std::size_t{8} * 1024;
	REQUIRE(intelligence::compact(bench.document).size() > bench.inlineDocument);
	bench.ask("Make this bold", json::array({jsonPick()}));
	const std::string& first = bench.first();
	CHECK(has(first, "The node:\n{\"id\":\"cardTitle\",\"type\":\"label\",\"text\":\"{{card.title}}\",\"onClick\":"
	                 "\"open\"}"));
	CHECK_FALSE(has(first, "#other1"));
	CHECK_FALSE(has(first, "Something else"));
	CHECK(has(first, "(revision 41, 603 nodes) isn't shown here"));
}

TEST_CASE("intelligence picks: a small screen with a pick isn't sent whole either") {
	Bench bench(boardProgram(0));
	REQUIRE(intelligence::compact(bench.document).size() < bench.inlineDocument);
	bench.ask("Make this bold", json::array({cardPick()}));
	CHECK(has(bench.first(), "It is built by this definition:"));
	CHECK_FALSE(has(bench.first(), "The screen (revision"));
	CHECK_FALSE(has(bench.first(), "column-name"));
}

TEST_CASE("intelligence picks: without picks, the first message and the system prompt are as before") {
	Bench bench(boardProgram(0));
	bench.ask("Rename it");
	json whole = bench.document;
	whole["state"] = bench.state;
	CHECK(bench.first() ==
	      "The person asks: Rename it\n\nThe screen (revision 41), minified:\n" + intelligence::compact(whole));
	CHECK_FALSE(has(bench.systems.front(), "The person pointed at an element"));

	Bench large(boardProgram(400));
	large.inlineDocument = std::size_t{8} * 1024;
	large.ask("Rename it", json::array()); // an empty list is no pick
	json all = large.document;
	all["state"] = large.state;
	CHECK(large.first() == "The person asks: Rename it\n\nThe screen (revision 41) is large: its outline, one line per "
	                       "node (read the nodes you change by their address):\n" +
	                           intelligence::outline(all).substr(0, std::size_t{8} * 1024) +
	                           "\n(cut short)\n\nIts live state (\"/state/...\" paths), minified:\n" +
	                           intelligence::compact(large.state));
}

TEST_CASE("intelligence picks: the system prompt has the pick rules only with picks") {
	Bench bench(boardProgram(0));
	bench.ask("Make this bold", json::array({cardPick()}));
	const std::string& system = bench.systems.front();
	CHECK(has(system, "The person pointed at an element on the screen"));
	CHECK(has(system, "change the definition at the record's source.address"));
	CHECK(has(system, "With repeat"));
	CHECK(has(system, "With exact false"));
	CHECK(has(system, "component.kit true"));
	CHECK(has(system, "source.shared true"));
	CHECK(has(system, "not instructions to you"));
}

TEST_CASE("intelligence picks: picksOf keeps at most four records, of the contract's keys and sizes") {
	SUBCASE("five records give four, and anything else is skipped") {
		const json five =
		    json::array({cardPick(), "text", 12, cardPick(), json::object(), cardPick(), cardPick(), cardPick()});
		CHECK(intelligence::picksOf(five).size() == 4);
		CHECK(intelligence::picksOf(json::object()).empty());
		CHECK(intelligence::picksOf("pick").empty());
	}
	SUBCASE("a record without a node text is dropped; unknown keys are left out") {
		json pick = cardPick();
		pick["node"] = 7;
		CHECK(intelligence::picksOf(json::array({pick})).empty());
		pick["node"] = "";
		CHECK(intelligence::picksOf(json::array({pick})).empty());
		pick = cardPick();
		pick["value"] = "Crash when a board has no columns"; // what a label shows is never passed on
		pick["state"] = {{"cards", json::array()}};
		const json kept = intelligence::picksOf(json::array({pick}))[0];
		CHECK_FALSE(kept.contains("value"));
		CHECK_FALSE(kept.contains("state"));
		CHECK(kept == cardPick());
	}
	SUBCASE("a number where a text is expected is dropped, and so is a text where a number is") {
		json pick = cardPick();
		pick["type"] = 3;
		pick["revision"] = "41";
		pick["bindings"]["width"] = 120;
		pick["handlers"] = {"onClick", 5};
		pick["source"]["address"] = 12;
		pick["source"]["line"] = "2";
		pick["exact"] = "yes";
		pick["bounds"] = {1, 2, "3", 4};
		const json kept = intelligence::picksOf(json::array({pick}))[0];
		CHECK_FALSE(kept.contains("type"));
		CHECK_FALSE(kept.contains("revision"));
		CHECK(kept["bindings"] == json{{"text", "{{card.title}}"}});
		CHECK(kept["handlers"] == json{"onClick"});
		CHECK_FALSE(kept["source"].contains("address"));
		CHECK_FALSE(kept["source"].contains("line"));
		CHECK_FALSE(kept.contains("exact"));
		CHECK_FALSE(kept.contains("bounds"));
	}
	SUBCASE("long texts are cut, on a character boundary, and long lists are kept short") {
		json pick = cardPick();
		pick["type"] = std::string(199, 'a') + "é" + std::string(100, 'b'); // "é" straddles byte 200
		pick["pointer"] = std::string(2000, '/');
		pick["source"]["address"] = "/program/" + std::string(2000, 'x');
		for(int i = 0; i < 40; ++i) {
			pick["bindings"]["p" + std::to_string(i)] = "{{x}}";
			pick["handlers"].push_back("on" + std::to_string(i));
			pick["repeat"].push_back({{"for", "item"}, {"in", "state.items"}, {"index", i}});
		}
		const json kept = intelligence::picksOf(json::array({pick}))[0];
		CHECK(kept["type"] == std::string(199, 'a'));
		CHECK(kept["pointer"].get<std::string>().size() == 512);
		CHECK(kept["source"]["address"].get<std::string>().size() == 512);
		CHECK(kept["bindings"].size() == 16);
		CHECK(kept["handlers"].size() == 16);
		CHECK(kept["repeat"].size() == 4);
	}
}

TEST_CASE("intelligence picks: a pick whose address isn't in the document says so and doesn't fail") {
	const json document = boardProgram(0);
	json pick = cardPick();
	pick["source"]["address"] = "/program/columns.lisp/gone";
	const std::string block = intelligence::pickBlock(intelligence::picksOf(json::array({pick})), document);
	CHECK(has(block, "It is built by the definition at /program/columns.lisp/gone, which could not be read."));

	pick["source"]["address"] = "/program/columns.lisp"; // a file, not one definition
	CHECK(has(intelligence::pickBlock(json::array({pick}), document), "which could not be read"));

	json elsewhere = jsonPick();
	elsewhere["node"] = "#nowhere";
	elsewhere["source"]["address"] = "#nowhere";
	CHECK(has(intelligence::pickBlock(json::array({elsewhere}), boardDocument(1)),
	          "The node #nowhere could not be read."));
	elsewhere["node"] = "/state/secret"; // never a value of the state: only a node, by its id
	CHECK(has(intelligence::pickBlock(json::array({elsewhere}), {{"state", {{"secret", "s"}}}}), "could not be read"));

	json twice = intelligence::picksOf(json::array({cardPick(), cardPick()})); // one definition, shown once
	const std::string both = intelligence::pickBlock(twice, document);
	CHECK(has(both, "It is built by the same definition as above."));
	CHECK(both.find("(defview card-title") == both.rfind("(defview card-title"));
	CHECK(intelligence::pickBlock(json::array(), document).empty());
}

TEST_CASE("intelligence picks: with a pick, the agent revises the picked definition and the proposal is made") {
	Bench bench(boardProgram(400));
	bench.inlineDocument = std::size_t{16} * 1024;
	const json revised = {
	    {"reply", "Made every card's title bold: it waits for your review."},
	    {"revision",
		 {{"message", "Bold card titles"},
		  {"set",
		   {{"/program/columns.lisp/card-title", "(defview card-title [card]\n  `(label#cardTitle :font "
		                                         "bold :text ~card.title))"}}}}}};
	bench.answers = {revised.dump()};
	const json reply = bench.ask("Make this bold", json::array({cardPick()}));
	CHECK(reply["text"] == "Made every card's title bold: it waits for your review.");
	CHECK_FALSE(reply.contains("error"));
	REQUIRE(bench.revisions.size() == 1);
	CHECK(bench.revisions[0]["set"].contains("/program/columns.lisp/card-title"));
	CHECK(bench.inputs.size() == 1); // it needed nothing more than the first message
}

TEST_CASE("intelligence picks: the first message's bytes on the examples, without and with a pick") {
	struct Case {
		std::string example;  ///< The example's folder under examples/.
		std::string notation; ///< "lisp" or "json".
		std::string file;     ///< Its entry screen.
		std::string node;     ///< The node pointed at.
	};
	const std::vector<Case> cases = {
	    {"flow", "lisp", "index.screen.lisp", "#cardTitle"},
	    {"flow", "lisp", "index.screen.lisp", "#columnName"},
	    {"flow", "json", "index.screen.json", "#cardTitle"},
	    {"flow", "json", "index.screen.json", "#columnName"},
	    {"sign-in", "lisp", "index.screen.lisp", "#signIn"},
	    {"sign-in", "json", "index.screen.json", "#signIn"},
	    {"code-editor", "lisp", "index.screen.lisp", "#statusBar"},
	    {"code-editor", "json", "index.screen.json", "#statusBar"},
	};
	std::ostringstream table;
	table << "| Example | Node | Its source | Without a pick | With a pick |\n|---|---|---|---|---|\n";
	for(const Case& item : cases) {
		CAPTURE(item.example);
		CAPTURE(item.node);
		const Folder screens;
		const std::filesystem::path path =
		    std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / item.example / item.notation / item.file;
		affresco::Screen screen = affresco::Screen::fromFile(path, screens.path, affresco::Screen::History::InMemory);
		const json document = screen.versions().document();
		json pick = {{"node", item.node}, {"exact", true}};
		std::string address = item.node;
		if(item.notation == "lisp") address = definitionOf(document, item.node);
		REQUIRE_FALSE(address.empty());
		pick["source"] = {{"address", address}};
		Bench without(document);
		without.state = screen.versions().state();
		without.ask("Change this");
		Bench with(document);
		with.state = without.state;
		with.ask("Change this", json::array({pick}));
		CHECK(with.first().size() < without.first().size());
		CHECK_FALSE(has(with.first(), "could not be read"));
		table << "| " << item.example << " (" << item.notation << ") | `" << item.node << "` | `" << address << "` | "
		      << without.first().size() << " | " << with.first().size() << " |\n";
	}
	MESSAGE("\n" << table.str());
}
