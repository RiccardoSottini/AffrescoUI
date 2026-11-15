/// @file
/// @brief Plays what CIDER, Calva and Conjure send (tests/nrepl/clients/*.json) to the nREPL protocol, and checks each
/// reply's keys and statuses. Each file is a list of steps: `send` (the request; a text starting with `$` is a session
/// id captured earlier), `expect` (the replies the step gives, in order: `keys` present, `absent` keys, `status`
/// exactly, `value`, `ns`, `id`, and `capture` to keep its `new-session`), `hold` (leave the pairing question
/// unanswered; otherwise every question is answered yes), and `questions` and `evaluations` (how many questions were
/// asked, and evaluations reached the program, by the end of the step).

#ifndef __EMSCRIPTEN__ // a page can't listen for an editor: the protocol isn't built there

#include "nrepl_fakes.hpp"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>

using namespace nrepl_test;

namespace {

/// @brief The client files' folder, by an absolute path (the examples' folder is beside tests/).
const std::filesystem::path clientsFolder =
    std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY).parent_path() / "tests" / "nrepl" / "clients";

/// @brief Converts a step's JSON to a message. @param json It. @param captured The session ids kept, by name.
/// @return The message.
Value toValue(const nlohmann::json& json, const std::map<std::string, std::string>& captured) {
	if(json.is_number_integer()) return Value(json.get<std::int64_t>());
	if(json.is_string()) {
		const auto text = json.get<std::string>();
		if(text.starts_with("$")) {
			const auto found = captured.find(text.substr(1));
			REQUIRE_MESSAGE(found != captured.end(), "no session captured as " << text);
			return Value(found->second);
		}
		return Value(text);
	}
	if(json.is_array()) {
		Value::List items;
		for(const auto& item : json) items.push_back(toValue(item, captured));
		return Value(std::move(items));
	}
	Value::Dictionary entries;
	for(const auto& [key, item] : json.items()) entries.emplace(key, toValue(item, captured));
	return Value(std::move(entries));
}

/// @brief Checks one reply against its expectation. @param reply It. @param expect The expectation.
/// @param captured Where a `capture` keeps the new session.
void checkReply(const Value& reply, const nlohmann::json& expect, std::map<std::string, std::string>& captured) {
	for(const auto& key : expect.value("keys", nlohmann::json::array()))
		CHECK_MESSAGE(reply.find(key.get<std::string>()) != nullptr, "missing " << key.get<std::string>());
	for(const auto& key : expect.value("absent", nlohmann::json::array())) {
		const auto name = key.get<std::string>();
		bool nested = false; // `clojure` is looked for inside `versions` too
		if(const Value* versions = reply.find("versions"); versions != nullptr)
			nested = versions->find(name) != nullptr;
		CHECK_MESSAGE((reply.find(name) == nullptr && !nested), "unexpected " << name);
	}
	if(expect.contains("status")) CHECK(statusOf(reply) == expect["status"].get<std::vector<std::string>>());
	for(const char* key : {"value", "ns", "id"})
		if(expect.contains(key)) CHECK(textOf(reply, key) == expect[key].get<std::string>());
	if(expect.contains("capture")) captured[expect["capture"].get<std::string>()] = textOf(reply, "new-session");
}

/// @brief Plays one client's file. @param file Its name in tests/nrepl/clients.
void play(const std::string& file) {
	std::ifstream input(clientsFolder / file);
	REQUIRE_MESSAGE(input.good(), "can't read " << (clientsFolder / file).string());
	const nlohmann::json steps = nlohmann::json::parse(input);
	REQUIRE(steps.is_array());
	FakeHost fake;
	fake.console.known = {{.name = "add-item",
	                       .kind = "defn",
	                       .doc = "Adds an item.",
	                       .file = "main.lisp",
	                       .line = 1,
	                       .arguments = {"[item]"}}};
	Connection connection(fake);
	std::map<std::string, std::string> captured;
	std::size_t step = 0;
	for(const auto& entry : steps) {
		++step;
		CAPTURE(step);
		const std::size_t before = connection.link.sent.size();
		const std::size_t asked = fake.questions.size();
		connection.peer->receive(toValue(entry.at("send"), captured));
		if(!entry.value("hold", false) && fake.questions.size() > asked) fake.answer(true);
		const std::vector<Value> replies(connection.link.sent.begin() + static_cast<std::ptrdiff_t>(before),
		                                 connection.link.sent.end());
		const nlohmann::json& expect = entry.at("expect");
		REQUIRE(replies.size() == expect.size());
		for(std::size_t index = 0; index < replies.size(); ++index) checkReply(replies[index], expect[index], captured);
		if(entry.contains("questions")) CHECK(fake.questions.size() == entry["questions"].get<std::size_t>());
		if(entry.contains("evaluations"))
			CHECK(fake.console.requests.size() == entry["evaluations"].get<std::size_t>());
	}
	CHECK_FALSE(connection.link.closed);
}

} // namespace

TEST_CASE("nrepl_clients: CIDER's sequence") { play("cider.json"); }

TEST_CASE("nrepl_clients: Calva's sequence") { play("calva.json"); }

TEST_CASE("nrepl_clients: Conjure's sequence") { play("conjure.json"); }

#endif
