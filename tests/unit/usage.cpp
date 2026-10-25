/// @file
/// @brief Unit tests for usage summaries (VERSION_CONTROL.md, "Usage"): counting per node id and action, the bounds,
/// the retention window, the file, that nothing is counted while off or without consent, and that an agent can't read
/// them.

#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Usage.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

using affresco::json;
using affresco::channel::Session;
using affresco::format::Usage;
using affresco::test::MemoryChannel;

namespace {

const std::int64_t start = Usage::now(); ///< "Now", for the tests.

/// @brief A fresh file for a test's usage summaries, in the system's temporary folder.
/// @param name The test's own name.
/// @return The file, which doesn't exist yet.
std::filesystem::path freshFile(const std::string& name) {
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / "affrescoui-usage-tests";
	std::filesystem::create_directories(folder);
	const std::filesystem::path file = folder / (name + ".usage.json");
	std::filesystem::remove(file);
	return file;
}

/// @brief A small document: two controls (a handler, a "bind") and a plain label.
/// @return The document.
json controls() {
	return json::parse(R"({"root": {"id": "root", "children": [
		{"id": "addCard", "onClick": "addCard"}, {"id": "search", "bind": "state.query"}, {"id": "title", "text": "T"}]}})");
}

/// @brief The Kanban template's document, its state in "state", as a host gives it to its repository.
/// @return The document.
json kanban() {
	const std::filesystem::path directory = std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "flow";
	const std::ifstream file(directory / "index.screen.json");
	REQUIRE(file);
	std::stringstream text;
	text << file.rdbuf();
	affresco::Screen screen = affresco::Screen::fromString(text.str(), directory);
	screen.setPlaceholderTypes(true);
	json document = json::parse(screen.documentJson());
	document["state"] = json::parse(screen.stateJson());
	return document;
}

/// @brief Sends a message on a channel and returns the answer to it.
/// @param channel The peer's channel.
/// @param message The message; given the id 1.
/// @return The last message sent back with that id.
json ask(MemoryChannel& channel, json message) {
	message["id"] = 1;
	channel.receive(message.dump());
	for(auto each = channel.sent.rbegin(); each != channel.sent.rend(); ++each)
		if(each->value("id", json()) == 1) return *each;
	return {};
}

} // namespace

TEST_CASE("usage: nothing is counted while off or without the person's consent") {
	Usage usage;
	usage.setFile(freshFile("off"));
	usage.ran("addCard", "onClick", false, start);
	usage.setEnabled(true); // the host's setting alone isn't enough
	CHECK_FALSE(usage.enabled());
	usage.ran("addCard", "onClick", false, start);
	usage.visited("board", start);
	CHECK(usage.summary(controls(), start)["actions"].empty());
	CHECK(usage.summary(controls(), start)["screens"].empty());

	usage.giveConsent();
	CHECK(usage.enabled());
	usage.setEnabled(false); // consent without the setting isn't either
	usage.ran("addCard", "onClick", false, start);
	CHECK(usage.summary(controls(), start)["actions"].empty());
	CHECK(usage.consented());
}

TEST_CASE("usage: counts runs and errors per node and action, screens, and the controls never used") {
	const std::filesystem::path file = freshFile("counting");
	{
		Usage usage;
		usage.setFile(file);
		usage.giveConsent();
		usage.setEnabled(true);
		usage.ran("addCard", "onClick:addCard", false, start);
		usage.ran("addCard", "onClick:addCard", true, start + 5);
		usage.visited("settings", start + 6);
		const json summary = usage.summary(controls(), start + 10);
		REQUIRE(summary["actions"].size() == 1);
		const json& count = summary["actions"][0];
		CHECK(count["node"] == "addCard");
		CHECK(count["action"] == "onClick:addCard");
		CHECK(count["runs"] == 2);
		CHECK(count["errors"] == 1);
		CHECK(count["first"] == start);
		CHECK(count["last"] == start + 5);
		CHECK(summary["screens"][0]["screen"] == "settings");
		CHECK(summary["untouched"] == json{"search"});
		CHECK(summary["enabled"] == true);
		CHECK_FALSE(summary.contains("consent"));
	}
	// Kept in its file, with the consent; nothing but ids, counts and times
	Usage reloaded;
	reloaded.setFile(file);
	reloaded.setEnabled(true);
	CHECK(reloaded.enabled());
	const json summary = reloaded.summary(controls(), start + 10);
	CHECK(summary["actions"][0]["runs"] == 2);
	// Cleared: the counts go, the consent stays
	reloaded.clear();
	CHECK(reloaded.summary(controls(), start + 10)["actions"].empty());
	CHECK(reloaded.consented());
	// Older than the window: dropped
	reloaded.ran("addCard", "onClick", false, start);
	CHECK(reloaded.summary(controls(), start + Usage::retentionSeconds + 1)["actions"].empty());
}

TEST_CASE("usage: bounded names, entries and counts, also when read from a damaged file") {
	Usage usage;
	usage.setFile(freshFile("bounds"));
	usage.giveConsent();
	usage.setEnabled(true);
	usage.ran(std::string(Usage::maxNameBytes + 1, 'n'), "onClick", false, start);
	usage.ran("line\nbreak", "onClick", false, start);
	usage.ran("addCard", "", false, start);
	CHECK(usage.summary(controls(), start)["actions"].empty());
	for(std::size_t i = 0; i < Usage::maxEntries + 10; ++i)
		usage.ran("node" + std::to_string(i), "onClick", false, start);
	for(std::size_t i = 0; i < Usage::maxScreens + 10; ++i) usage.visited("screen" + std::to_string(i), start);
	const json summary = usage.summary(controls(), start);
	CHECK(summary["actions"].size() == Usage::maxEntries);
	CHECK(summary["screens"].size() == Usage::maxScreens);

	const std::filesystem::path file = freshFile("damaged");
	std::ofstream(file) << R"({"consent": 1, "since": -5, "actions": [
		{"node": "a", "action": "onClick", "runs": 99999999999, "errors": 99999999999999, "first": 0, "last": )"
	                    << start << R"(}, {"node": 5, "action": "onClick"}, "junk"]})";
	Usage damaged;
	damaged.setFile(file);
	damaged.setEnabled(true);
	const json read = damaged.summary(controls(), start);
	REQUIRE(read["actions"].size() == 1);
	CHECK(read["actions"][0]["runs"] == Usage::maxCount);
	CHECK(read["actions"][0]["errors"] == Usage::maxCount);
	CHECK(read["since"].get<std::int64_t>() >= 0);
}

#ifndef _WIN32
// Found in a security review: the file was made with the process's default mode (0644 with the usual
// umask), so other users of the device could read how the app is used, though the history and the state are private
TEST_CASE("usage: the file is readable by its owner only") {
	const std::filesystem::path file = freshFile("private");
	const std::filesystem::path temporary = file.string() + ".tmp";
	std::ofstream(temporary) << "{}"; // one a run that stopped while writing left behind, with the default mode
	std::filesystem::permissions(temporary, std::filesystem::perms::all);
	Usage usage;
	usage.setFile(file);
	usage.giveConsent();
	using std::filesystem::perms;
	CHECK((std::filesystem::status(file).permissions() & perms::all) == (perms::owner_read | perms::owner_write));
}
#endif

TEST_CASE("usage: an agent can never read the usage summaries; a reviewer can, only while they are on") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, kanban());
	MemoryChannel studio;
	MemoryChannel agent;
	Session studioSession(studio, repository, "studio", std::function<json()>{});
	Session agentSession(agent, repository, "agent", std::function<json()>{});
	bool on = false;
	int cleared = 0;
	const auto summaries = [&](bool clear) -> std::optional<json> {
		if(!on) return std::nullopt;
		cleared += clear ? 1 : 0;
		return json{{"actions", json::array()}};
	};
	studioSession.setUsage(summaries);
	agentSession.setUsage(summaries);

	CHECK(ask(studio, {{"type", "getUsage"}})["error"].get<std::string>().find("off") != std::string::npos);
	on = true;
	const json refused = ask(agent, {{"type", "getUsage"}});
	CHECK(refused["type"] == "rejected");
	CHECK(refused["error"].get<std::string>().find("only a reviewer") != std::string::npos);
	CHECK(ask(agent, {{"type", "clearUsage"}})["type"] == "rejected");
	CHECK(cleared == 0);
	CHECK(ask(studio, {{"type", "getUsage"}})["type"] == "usage");
	CHECK(ask(studio, {{"type", "clearUsage"}})["type"] == "usage");
	CHECK(cleared == 1);
}
