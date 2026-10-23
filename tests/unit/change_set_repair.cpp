/// @file
/// @brief Unit tests for self-repairing change sets (vcs::ChangeSetRepairs, VERSION_CONTROL.md): the "Blocked column"
/// change set on the Kanban template, installed, broken by a change to the template, repaired by
/// tests/repair/fake_agent.py through a ProcessChannel, and accepted by a reviewer; and what a repair may never do.

#include "Helpers.hpp"

#include <affresco/Screen.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/channel/Channels.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/ChangeSetRepair.hpp>
#include <affresco/vcs/Repository.hpp>

#include "FeatureSignatures.hpp" // modules/format/src: a change set's signature covers its repair fields

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using affresco::json;
using affresco::vcs::ChangeSetRepairs;
using affresco::vcs::RepairStatus; // compared in double parentheses: doctest would print it with vcs::toString
using namespace std::chrono_literals;

namespace {

/// @brief An Executor whose tasks, posted from a channel's reading thread, run on the test's thread when it asks.
class ThreadedExecutor final : public affresco::Executor {
public:
	/// @brief Queues a task.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		ready_.notify_one();
	}

	/// @brief Runs tasks as they arrive until @p done says so, or until @p timeout passes.
	/// @param done Checked before each task.
	/// @param timeout How long to wait in all.
	/// @return Whether @p done became true in time.
	bool runUntil(const std::function<bool()>& done, std::chrono::milliseconds timeout = 20s) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while(!done()) {
			std::unique_lock lock(mutex_);
			if(!ready_.wait_until(lock, deadline, [this] { return !tasks_.empty(); })) return false;
			const std::function<void()> task = std::move(tasks_.front());
			tasks_.pop_front();
			lock.unlock();
			task();
		}
		return true;
	}

private:
	std::mutex mutex_;                        ///< Guards tasks_.
	std::condition_variable ready_;           ///< Signalled when a task is queued.
	std::deque<std::function<void()>> tasks_; ///< Waiting to run, oldest first.
};

/// @brief The shipped screens' folder.
/// @return It.
std::filesystem::path screens() { return std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "flow"; }

/// @brief A JSON file.
/// @param path Its path.
/// @return Its document.
json readJson(const std::filesystem::path& path) {
	const std::ifstream in(path);
	std::stringstream text;
	text << in.rdbuf();
	return json::parse(text.str());
}

/// @brief The Kanban template, as shipped or changed, with its include merged in as Screen loads it (its own entries
/// win), since a Repository takes a whole document.
/// @param change Changes it (may be empty).
/// @return The document.
json kanban(const std::function<void(json&)>& change = {}) {
	json document = readJson(screens() / "kanban.screen.json");
	for(const json& file : document["include"]) {
		const json part = readJson(screens() / file.get<std::string>()); // a loop over a temporary's items dangles
		for(const auto& [section, entries] : part.items())
			if(entries.is_object())
				for(const auto& [name, entry] : entries.items())
					if(!document[section].contains(name)) document[section][name] = entry;
	}
	document.erase("include");
	if(change) change(document);
	return document;
}

/// @brief The "Blocked column" change set: a Blocked column before Done, with its request, base and proof.
/// @return It.
json blockedColumn() {
	const json changes = {{"format", "affrescoui-changes"},
	                      {"version", 2},
	                      {"name", "blocked-column"},
	                      {"revisions",
	                       {{{"message", "A Blocked column before Done"},
	                         {"patch",
	                          {{{"op", "test"}, {"path", "/state/columns/3/name"}, {"value", "Done"}},
	                           {{"op", "add"},
	                            {"path", "/state/columns/3"},
	                            {"value", {{"name", "Blocked"}, {"color", "#dc2626"}, {"cards", json::array()}}}}}}}}}};
	const json proof = {
	    {"scenarios",
	     {{{"name", "blocked-column"},
	       {"steps",
	        {{{"expect", {{"state", "columns[3].name"}, {"is", "Blocked"}}}},
	         {{"expect", {{"state", "columns.length"}, {"is", 5}}}}}}}}}};
	return ChangeSetRepairs::describe(changes, "Add a Blocked column before the last one",
	                                  {{"name", "kanban.screen.json"}, {"commit", "original"}, {"tree", "original"}},
	                                  proof);
}

/// @brief Plays a proof's scenarios on a document, headless, as affrescoui_host does.
/// @param document The document with its state, as JSON text.
/// @param proof The proof, as JSON text.
/// @return The failures, or "".
std::string playProof(const std::string& document, const std::string& proof) {
	namespace automation = affresco::automation;
	const json file = {{"format", "affrescoui-scenarios"},
	                   {"version", 2},
	                   {"screen", "kanban.screen.json"},
	                   {"scenarios", json::parse(proof).at("scenarios")}};
	automation::Options options;
	options.document = document;
	options.pictures = false;
	std::string failed;
	const auto scenarios = automation::ScenarioFile::parse(file.dump(), screens() / "proof.scenarios.json");
	for(const automation::ScenarioResult& result : automation::run(scenarios, options).scenarios)
		for(const automation::Failure& failure : result.failures)
			failed += (failed.empty() ? "" : "; ") + result.name + ": " + failure.problem;
	return failed;
}

/// @brief A screen's repairs, read back.
/// @param screen The screen.
/// @return Its first change set's row.
json firstRepair(const affresco::Screen& screen) { return json::parse(screen.repairs())["changeSets"][0]; }

/// @brief The Kanban template with Done renamed: the Blocked column's patch no longer applies.
/// @param document The template.
void renameDone(json& document) { document["state"]["columns"][3]["name"] = "Shipped"; }

/// @brief A repository on the changed template, with the proof check, for tests below the Screen.
struct Repairs {
	affresco::format::Rules rules;                                ///< The screen format.
	affresco::vcs::Repository repository{rules, kanban(renameDone)}; ///< The changed template.
	ChangeSetRepairs repairs{repository, "kanban.screen.json"};   ///< Under test.
	std::vector<json> broadcast;                                  ///< What it told every peer.

	/// @brief Sets the proof check and records the broadcasts.
	Repairs() {
		repairs.setProofCheck([](const json& document, const json& proof) -> std::optional<std::string> {
			std::string failed = playProof(document.dump(), proof.dump());
			if(failed.empty()) return std::nullopt;
			return failed;
		});
		repairs.onMessage.connect([this](const json& message) { broadcast.push_back(message); });
	}

	/// @brief The open repair request's id.
	/// @return It.
	[[nodiscard]] std::string requestId() const { return repairs.openRequest().value_or(json()).at("id").get<std::string>(); }

	/// @brief Sends the agent's answer to the open request.
	/// @param patch The revision's patch.
	/// @param from The channel's author.
	/// @return What the host answered.
	json answer(const json& patch, const std::string& from = "agent") {
		json answered;
		repairs.handle({{"type", "repair"}, {"from", from}, {"request", requestId()}, {"revision", {{"patch", patch}}}},
		               [&](const json& message) { answered = message; });
		return answered;
	}
};

/// @brief The fake agent's right repair, on the changed template.
/// @return Its patch.
json blockedBeforeShipped() {
	return {{{"op", "add"},
	         {"path", "/state/columns/3"},
	         {"value", {{"name", "Blocked"}, {"color", "#dc2626"}, {"cards", json::array()}}}}};
}

} // namespace

TEST_CASE("change set repair: the Blocked column applies on the template it was recorded on") {
	affresco::Screen screen = affresco::Screen::fromString(kanban().dump(), screens());
	screen.setProofCheck(playProof);
	CHECK(screen.installChanges(blockedColumn().dump()) == "applied");
	CHECK(screen.versions().state()["columns"][3]["name"] == "Blocked");
	CHECK(screen.hasImported(blockedColumn().dump()));
}

TEST_CASE("change set repair: a peer that may read only some paths gets the request without the revisions") {
	affresco::test::MemoryChannel studio, reader;
	affresco::Screen screen = affresco::Screen::fromString(kanban([](json& document) {
		                                                       renameDone(document);
		                                                       document["permissions"]["data"] = {
		                                                           {{"author", "reader"}, {"read", {"/state/theme"}}}};
	                                                       }).dump(),
	                                                       screens());
	screen.setProofCheck(playProof);
	screen.connect(studio, "studio");
	screen.connect(reader, "reader");
	CHECK(screen.installChanges(blockedColumn().dump()) == "repairing");
	const auto requestIn = [](const affresco::test::MemoryChannel& channel) {
		const auto found =
		    std::ranges::find_if(channel.sent, [](const json& m) { return m.value("kind", "") == "repair"; });
		REQUIRE(found != channel.sent.end());
		return (*found)["repair"];
	};
	CHECK(requestIn(studio).contains("revisions"));
	const json hidden = requestIn(reader); // its revisions and proof can show values the reader can't see
	CHECK(hidden["request"] == "Add a Blocked column before the last one");
	for(const char* key : {"revisions", "proof", "failed"}) CHECK_FALSE(hidden.contains(key));
	for(const json& message : reader.sent)
		if(message.value("type", "") == "repairs")
			for(const json& row : message["changeSets"]) CHECK_FALSE(row.contains("reason"));
}

TEST_CASE("change set repair: the template changes, the fake agent repairs it, the reviewer accepts") {
	ThreadedExecutor executor;
	// The channels first: a connected channel must outlive the screen, which says goodbye on it when it closes
	affresco::test::MemoryChannel studio;
	const std::string agentScript = (screens() / "../../tests/repair/fake_agent.py").string();
	affresco::channel::ProcessChannel agent(executor, "python3 \"" + agentScript + "\"");
	affresco::Screen screen = affresco::Screen::fromString(kanban(renameDone).dump(), screens());
	screen.setProofCheck(playProof);
	screen.versions().setMode(affresco::Mode::AcceptEdits); // a repair is reviewed even so
	screen.connect(studio, "studio");
	screen.connect(agent, "agent");
	const json before = screen.versions().state();

	// Left off, the rest untouched, a repair asked for
	CHECK(screen.installChanges(blockedColumn().dump()) == "repairing");
	CHECK(screen.versions().state() == before);
	CHECK(firstRepair(screen)["reason"].get<std::string>().starts_with("the patch no longer applies"));
	const auto requested = std::ranges::find_if(studio.sent, [](const json& m) { return m.value("kind", "") == "repair"; });
	REQUIRE(requested != studio.sent.end());
	CHECK((*requested)["repair"]["request"] == "Add a Blocked column before the last one");
	CHECK((*requested)["repair"]["newBase"]["name"] == "screen");

	// The agent's revision becomes a proposal, not a commit
	REQUIRE(executor.runUntil([&] { return !screen.proposals().empty(); }));
	CHECK(screen.versions().state() == before);
	const affresco::ProposalInfo proposal = screen.proposals().front();
	CHECK(proposal.author == "agent");
	CHECK(screen.versions().accept(proposal.id, {"agent", proposal.head, false}).has_value()); // not its own reviewer

	// The reviewer accepts: re-recorded on the new base, and its proof passes
	REQUIRE_FALSE(screen.versions().accept(proposal.id, {"studio", proposal.head, false}).has_value());
	const json row = firstRepair(screen);
	CHECK(row["status"] == "repaired");
	CHECK(row["repairedFrom"]["proposal"] == proposal.id);
	CHECK(screen.versions().state()["columns"][3]["name"] == "Blocked");
	CHECK(screen.versions().state()["columns"][4]["name"] == "Shipped");
	json view = screen.versions().document();
	view["state"] = screen.versions().state();
	CHECK(playProof(view.dump(), blockedColumn()["proof"].dump()).empty());
}

TEST_CASE("change set repair: a change set whose scenarios fail is left off, with the reason") {
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository{rules, kanban([](json& document) {
		document["state"]["columns"].push_back({{"name", "Archived"}, {"color", "#64748b"}, {"cards", json::array()}});
	})};
	ChangeSetRepairs repairs{repository, "kanban.screen.json"};
	repairs.setProofCheck([](const json& document, const json& proof) -> std::optional<std::string> {
		std::string failed = playProof(document.dump(), proof.dump());
		if(failed.empty()) return std::nullopt;
		return failed;
	});
	const json before = repository.state();
	CHECK((repairs.install(blockedColumn()) == RepairStatus::Repairing));
	CHECK(repairs.changeSets()[0].reason.starts_with("its scenarios fail"));
	CHECK(repository.state() == before);
}

TEST_CASE("change set repair: an answer must match the open request, and its author is the channel's") {
	Repairs fixture;
	REQUIRE((fixture.repairs.install(blockedColumn()) == RepairStatus::Repairing));
	json answered;
	fixture.repairs.handle({{"type", "repair"}, {"from", "agent"}, {"request", "repair-other-1"},
	                        {"revision", {{"patch", blockedBeforeShipped()}}}},
	                       [&](const json& message) { answered = message; });
	CHECK(answered["type"] == "error");

	// An author written into the revision is the channel's anyway
	json forged;
	fixture.repairs.handle({{"type", "repair"}, {"from", "agent"}, {"request", fixture.requestId()},
	                        {"revision", {{"author", "studio"}, {"patch", blockedBeforeShipped()}}}},
	                       [&](const json& message) { forged = message; });
	REQUIRE(forged["type"] == "proposed");
	CHECK(fixture.repository.proposals().front().author == "agent");
	CHECK_FALSE(fixture.repairs.openRequest().has_value()); // one repair at a time: this one waits for review
}

TEST_CASE("change set repair: a repair can't touch what the rules deny, and attempts are limited") {
	Repairs fixture;
	REQUIRE((fixture.repairs.install(blockedColumn()) == RepairStatus::Repairing));
	const json denied = {{{"op", "replace"}, {"path", "#statusBar/color"}, {"value", "#000000"}}};
	for(int attempt = 1; attempt <= ChangeSetRepairs::maxAttempts; ++attempt) {
		CHECK(fixture.answer(denied)["type"] == "error");
		CHECK(fixture.repository.proposals().empty());
	}
	CHECK((fixture.repairs.changeSets()[0].status == RepairStatus::GaveUp));
	CHECK_FALSE(fixture.repairs.openRequest().has_value());

	// Only a reviewer retries
	json answered;
	fixture.repairs.handle({{"type", "repair"}, {"from", "agent"}, {"retry", "blocked-column"}},
	                       [&](const json& message) { answered = message; });
	CHECK(answered["type"] == "error");
	fixture.repairs.handle({{"type", "repair"}, {"from", "studio"}, {"retry", "blocked-column"}},
	                       [&](const json& message) { answered = message; });
	CHECK((fixture.repairs.changeSets()[0].status == RepairStatus::Repairing));
}

TEST_CASE("change set repair: a rejected repair is asked for again") {
	Repairs fixture;
	REQUIRE((fixture.repairs.install(blockedColumn()) == RepairStatus::Repairing));
	REQUIRE(fixture.answer(blockedBeforeShipped())["type"] == "proposed");
	CHECK(fixture.repository.reject(fixture.repository.proposals().front().id, "not now"));
	CHECK(fixture.repairs.changeSets()[0].reason.starts_with("the reviewer rejected its repair"));
	CHECK(fixture.repairs.openRequest().value_or(json())["repair"]["attempt"] == 2);
}

TEST_CASE("change set repair: a change set's signature covers its request, base and proof") {
	const affresco::format::SigningKey key = affresco::format::SigningKey::generate();
	json changes = key.sign(blockedColumn());
	CHECK_NOTHROW((void)affresco::format::verifySignature(changes, "changes"));
	changes["request"] = "Delete every column";
	CHECK_THROWS_AS((void)affresco::format::verifySignature(changes, "changes"), affresco::DocumentError);
}
