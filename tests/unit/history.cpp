/// @file
/// @brief Unit tests for the vcs module's history on disk: save and load, a changed screen file, damage and the lock.
///
/// Each test works in its own temporary folder, removed at the end.

#include "Helpers.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include "Sha256.hpp" // modules/vcs/src: to forge a history with valid hashes

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::test::permissionsScreen;
using affresco::vcs::Repository;

namespace {

/// @brief A new, empty folder for one test, removed with everything in it when the test ends.
class TemporaryFolder {
public:
	/// @brief Creates the folder.
	TemporaryFolder() {
		static std::atomic<int> count = 0;
		path_ =
		    std::filesystem::temp_directory_path() /
		    ("affrescoui-history-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
			 "-" + std::to_string(count++));
		std::filesystem::create_directories(path_);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	/// @brief Removes the folder.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path_, ignored);
	}

	/// @brief The folder.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< The folder.
};

/// @brief Reads a whole file.
/// @param file The file.
/// @return Its text.
std::string readText(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

/// @brief Replaces a file's text.
/// @param file The file.
/// @param text The new text.
void writeText(const std::filesystem::path& file, const std::string& text) {
	std::ofstream(file, std::ios::binary | std::ios::trunc) << text;
}

/// @brief The file a commit is stored in.
/// @param history The history folder.
/// @param id The commit's id.
/// @return objects/<2>/<62>.
std::filesystem::path objectFile(const std::filesystem::path& history, const std::string& id) {
	return history / "objects" / id.substr(0, 2) / id.substr(2);
}

/// @brief Counts the files with a name ending in ".tmp" in a folder and below.
/// @param folder The folder.
/// @return How many there are.
int temporaryFiles(const std::filesystem::path& folder) {
	int count = 0;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(folder))
		if(entry.path().extension() == ".tmp") ++count;
	return count;
}

/// @brief Writes a commit object the way a program that ignores the rules could: well formed, with a valid id.
/// @param history The history folder.
/// @param commit The object: parents, author, committer, time, proposal, message, patch, document.
/// @return Its id.
std::string forgeCommit(const std::filesystem::path& history, const json& commit) {
	using affresco::vcs::detail::sha256;
	std::string object = "tree " + sha256(commit.at("document").dump()) + "\n";
	for(const json& parent : commit.at("parents")) object += "parent " + parent.get<std::string>() + "\n";
	object += "author " + commit.at("author").get<std::string>() + "\ncommitter " +
	          commit.at("committer").get<std::string>() + "\ntime " + commit.at("time").get<std::string>() +
	          "\npatch " + sha256(commit.at("patch").dump()) + "\n\n" + commit.at("message").get<std::string>();
	const std::string id = sha256(object);
	std::filesystem::create_directories(objectFile(history, id).parent_path());
	writeText(objectFile(history, id), commit.dump());
	return id;
}

/// @brief Rewrites the refs file the way a program that ignores the rules could, with a valid checksum.
/// @tparam Change A function that takes the refs as `json&`.
/// @param history The history folder.
/// @param change Changes the refs (without their checksum).
template <typename Change> void forgeRefs(const std::filesystem::path& history, Change change) {
	json refs = json::parse(readText(history / "refs.json"));
	refs.erase("checksum");
	change(refs);
	refs["checksum"] = affresco::vcs::detail::sha256(refs.dump());
	writeText(history / "refs.json", refs.dump(2));
}

/// @brief A renderer that can't show a screen titled "Unshowable", as one can't when a font file the screen names is
/// gone.
class FussyObserver final : public affresco::VersionObserver {
public:
	/// @brief Watches a repository.
	/// @param repository The repository; it outlives this observer.
	explicit FussyObserver(const Repository& repository) : repository_(repository) {}

	/// @brief Refuses the document if it is titled "Unshowable".
	/// @throws affresco::DocumentError Then.
	void documentChanged() override {
		if(repository_.document().dump().find("\"Unshowable\"") != std::string::npos)
			throw affresco::DocumentError("AffrescoUI screen: fonts.body: can't read 'gone.ttf'");
	}
	/// @brief Does nothing.
	void proposalsChanged() override {}

private:
	const Repository& repository_; ///< The repository watched.
};

/// @brief Fills a history with one of everything it keeps: commits on main, a merged proposal, a grant, an open
/// proposal with two commits, and a mode.
/// @param repository A repository with permissionsScreen().
/// @return The open proposal's id.
int fill(Repository& repository) {
	REQUIRE(
	    repository.submit({{"author", "studio"}, {"message", "Rename"}, {"set", {{"#title/text", "Done"}}}}).status ==
	    RevisionStatus::Applied);

	// The agent may change #board only: #title waits for review. Always allow it: a grant for agent(#title).
	const affresco::RevisionResult merged =
	    repository.submit({{"author", "agent"}, {"message", "Retitle"}, {"set", {{"#title/text", "Week"}}}});
	REQUIRE(merged.status == RevisionStatus::Proposed);
	REQUIRE_FALSE(repository.accept(merged.proposal, {"studio", merged.commit, true}));

	const affresco::RevisionResult open = repository.submit(
	    {{"author", "agent"}, {"message", "Recolour"}, {"set", {{"/colors", {{"accent", "#ff0000"}}}}}});
	REQUIRE(open.status == RevisionStatus::Proposed);
	REQUIRE(
	    repository.submit({{"author", "agent"}, {"proposal", open.proposal}, {"set", {{"/colors/accent", "#00ff00"}}}})
	        .status == RevisionStatus::Proposed);
	repository.setMode(affresco::Mode::AcceptEdits);
	return open.proposal;
}

} // namespace

TEST_CASE("vcs: a history kept in a folder comes back whole after a restart") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";

	std::vector<affresco::CommitInfo> log;
	json document, proposalDiff;
	int open = 0;
	{
		Repository first(rules, permissionsScreen());
		const Repository::OpenedHistory opened = first.openHistory(history);
		CHECK_FALSE(opened.loaded);
		CHECK(opened.memoryOnly.empty());
		CHECK(opened.setAside.empty());
		open = fill(first);
		log = first.log();
		document = first.document();
		proposalDiff = first.proposalDiff(open);
	} // the host quits

	CHECK(std::filesystem::is_regular_file(history / "refs.json"));
	CHECK(std::filesystem::is_regular_file(objectFile(history, log.back().id)));
	CHECK(temporaryFiles(history) == 0); // every write was renamed into place

	Repository second(rules, permissionsScreen());
	const Repository::OpenedHistory opened = second.openHistory(history);
	CHECK(opened.loaded);
	CHECK(opened.memoryOnly.empty());

	// main: the same commits, the same ids
	const std::vector<affresco::CommitInfo> restored = second.log();
	REQUIRE(restored.size() == log.size());
	for(std::size_t i = 0; i < log.size(); ++i) {
		CAPTURE(i);
		CHECK(restored[i].id == log[i].id);
		CHECK(restored[i].number == log[i].number);
		CHECK(restored[i].parents == log[i].parents);
		CHECK(restored[i].committer == log[i].committer);
	}
	CHECK(second.document().dump() == document.dump());
	CHECK(second.resolve("HEAD~1").id == log[log.size() - 2].id);

	// The open proposal, its branch, the grant and the mode
	REQUIRE(second.proposals().size() == 1);
	const affresco::ProposalInfo proposal = second.proposals().front();
	CHECK(proposal.id == open);
	CHECK(proposal.commits == 2);
	CHECK(proposal.operations == 2);
	CHECK(proposal.author == "agent");
	CHECK(second.proposalDiff(open) == proposalDiff);
	REQUIRE(second.grants().size() == 1);
	CHECK(second.grants().front().rule == "agent(#title)");
	CHECK(affresco::toString(second.mode()) == "acceptEdits");

	// It goes on from there: the grant still applies, ids aren't reused, the restored proposal can be merged
	CHECK(second.submit({{"author", "agent"}, {"set", {{"#title/text", "Month"}}}}).status == RevisionStatus::Applied);
	REQUIRE_FALSE(second.accept(open, {"studio", proposal.head, false}));
	CHECK(second.document().at(json::json_pointer("/colors/accent")) == "#00ff00");
	second.setMode(affresco::Mode::Default);
	const affresco::RevisionResult next =
	    second.submit({{"author", "agent"}, {"set", {{"/colors/accent", "#0000ff"}}}});
	REQUIRE(next.status == RevisionStatus::Proposed);
	CHECK(next.proposal == open + 1);
}

TEST_CASE("vcs: a history that began with another version of the screen is set aside") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";
	std::string oldHead;
	{
		Repository first(rules, permissionsScreen());
		first.openHistory(history);
		REQUIRE(first.submit({{"author", "studio"}, {"set", {{"#title/text", "Done"}}}}).status ==
		        RevisionStatus::Applied);
		oldHead = first.head().id;
	}

	json changed = permissionsScreen(); // the screen file was edited since
	changed["root"]["children"][1]["text"] = "Backlog";
	Repository second(rules, changed);
	const Repository::OpenedHistory opened = second.openHistory(history);
	CHECK_FALSE(opened.loaded);
	CHECK(opened.memoryOnly.empty());
	REQUIRE_FALSE(opened.setAside.empty());
	CHECK(opened.setAside.filename().string().starts_with("board.history.replaced-"));

	// The new history starts from the file as it is; the old one is kept, whole
	CHECK(second.head().number == 0);
	CHECK(second.document() == changed);
	CHECK(std::filesystem::is_regular_file(history / "refs.json"));
	CHECK(std::filesystem::is_regular_file(objectFile(opened.setAside, oldHead)));
	CHECK(readText(opened.setAside / "refs.json").find(oldHead) != std::string::npos);

	// And it is the one that loads next time
	Repository third(rules, changed);
	CHECK(third.openHistory(history).loaded);
	CHECK(third.head().id == second.head().id);
}

TEST_CASE("vcs: a damaged history is never loaded, and the error names the file") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";
	std::string head;
	{
		Repository first(rules, permissionsScreen());
		first.openHistory(history);
		fill(first);
		head = first.log()[1].id; // "Rename"
	}

	std::filesystem::path damaged;
	std::string expected;
	SUBCASE("a commit whose content changed") {
		damaged = objectFile(history, head);
		std::string text = readText(damaged);
		const std::size_t at = text.find("Rename");
		REQUIRE(at != std::string::npos);
		text.replace(at, 6, "Renamf"); // still JSON, still a commit: only the hash can tell
		writeText(damaged, text);
		expected = "doesn't match its SHA-256";
	}
	SUBCASE("a commit that is missing") {
		damaged = objectFile(history, head);
		std::filesystem::remove(damaged);
		expected = "can't be read";
	}
	SUBCASE("a commit that is cut short") {
		damaged = objectFile(history, head);
		const std::string text = readText(damaged);
		writeText(damaged, text.substr(0, text.size() / 2));
	}
	SUBCASE("refs whose content changed") {
		damaged = history / "refs.json";
		std::string text = readText(damaged);
		const std::size_t at = text.find("\"acceptEdits\"");
		REQUIRE(at != std::string::npos);
		text.replace(at, 13, "\"plan\"");
		writeText(damaged, text);
		expected = "doesn't match its checksum";
	}
	INFO("damaged: " << damaged.string());

	Repository second(rules, permissionsScreen());
	std::string error;
	try {
		second.openHistory(history);
	} catch(const affresco::DocumentError& failure) {
		error = failure.what();
	}
	INFO("error: " << error);
	CHECK(error.starts_with("AffrescoUI history: " + damaged.string() + ": "));
	CHECK(error.find(expected) != std::string::npos);
	CHECK(error.find("move " + history.string() + " aside") != std::string::npos);

	// Nothing was loaded: the screen as the file has it, and the folder as it was
	CHECK(second.head().number == 0);
	CHECK(second.proposals().empty());
	CHECK(second.grants().empty());
	CHECK(affresco::toString(second.mode()) == "default");
	CHECK(second.document() == permissionsScreen());
	CHECK(std::filesystem::is_regular_file(history / "refs.json"));
}

TEST_CASE("vcs: a whole history whose last revision can't be shown isn't loaded, and isn't called damaged") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";
	{
		Repository first(rules, permissionsScreen());
		first.openHistory(history);
		REQUIRE(first.submit({{"author", "studio"}, {"set", {{"#title/text", "Unshowable"}}}}).status ==
		        RevisionStatus::Applied);
	}

	Repository second(rules, permissionsScreen());
	FussyObserver observer(second);
	second.setObserver(&observer);
	std::string error;
	try {
		second.openHistory(history);
	} catch(const affresco::DocumentError& failure) {
		error = failure.what();
	}
	INFO("error: " << error);
	CHECK(error.starts_with("AffrescoUI history: " + history.string() +
	                        ": its last revision can't be shown ("
	                        "fonts.body: can't read 'gone.ttf')"));
	CHECK(error.find("damaged") == std::string::npos);
	CHECK(second.head().number == 0);
	CHECK(second.document() == permissionsScreen());
	CHECK(std::filesystem::is_regular_file(history / "refs.json"));
}

TEST_CASE("vcs: while one program writes a history, another only reads it") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";

	auto writer = std::make_unique<Repository>(rules, permissionsScreen());
	REQUIRE(writer->openHistory(history).memoryOnly.empty());
	REQUIRE(writer->submit({{"author", "studio"}, {"set", {{"#title/text", "Done"}}}}).status ==
	        RevisionStatus::Applied);
	const std::string refs = readText(history / "refs.json");

	Repository reader(rules, permissionsScreen());
	const Repository::OpenedHistory opened = reader.openHistory(history);
	CHECK(opened.loaded); // it shows the same history
	CHECK(opened.memoryOnly.find("in use by another program") != std::string::npos);
	CHECK(reader.head().id == writer->head().id);

	// Its own changes stay in memory: the writer's history is untouched
	REQUIRE(reader.submit({{"author", "studio"}, {"set", {{"#title/text", "Later"}}}}).status ==
	        RevisionStatus::Applied);
	CHECK_FALSE(std::filesystem::exists(objectFile(history, reader.head().id)));
	CHECK(readText(history / "refs.json") == refs);

	// The lock goes with the program that held it
	writer.reset();
	Repository next(rules, permissionsScreen());
	const Repository::OpenedHistory reopened = next.openHistory(history);
	CHECK(reopened.loaded);
	CHECK(reopened.memoryOnly.empty());
	CHECK(next.head().number == 1);
	CHECK(next.document().at(json::json_pointer("/root/children/1/text")) == "Done");
}

TEST_CASE("vcs: a history folder is opened before the first change, once") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;

	Repository changed(rules, permissionsScreen());
	REQUIRE(changed.submit({{"author", "studio"}, {"set", {{"#title/text", "Done"}}}}).status ==
	        RevisionStatus::Applied);
	CHECK_THROWS_WITH_AS(changed.openHistory(folder.path() / "a.history"),
	                     doctest::Contains("open the history before the first change"), affresco::DocumentError);

	Repository twice(rules, permissionsScreen());
	twice.openHistory(folder.path() / "b.history");
	CHECK_THROWS_WITH_AS(twice.openHistory(folder.path() / "c.history"), doctest::Contains("already has a folder"),
	                     affresco::DocumentError);
	CHECK_FALSE(std::filesystem::exists(folder.path() / "a.history"));
	CHECK_FALSE(std::filesystem::exists(folder.path() / "c.history"));
}

TEST_CASE("vcs: a history written by hand can't give the screen what no revision can") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";
	std::string root;
	{
		Repository first(rules, permissionsScreen());
		first.openHistory(history);
		root = first.head().id;
	}

	// A commit on main's head with valid hashes, as only a program that ignores the rules could write it
	json document = permissionsScreen();
	std::string expected;
	SUBCASE("a harmless change loads (the forging is right)") { document["root"]["children"][1]["text"] = "Forged"; }
	SUBCASE("a new channel") {
		document["channels"] = {{"outside", {{"socket", "outside.sock"}}}};
		expected = "revisions can't change \"channels\"";
	}
	SUBCASE("other permissions") {
		document["permissions"]["allow"].push_back("agent");
		expected = "revisions can't change \"permissions\"";
	}
	SUBCASE("a control step") {
		document["actions"] = {{"approve", json::array({{{"accept", 1}}})}};
		expected = "revisions can't add or change accept";
	}
	const std::string forged = forgeCommit(history, {{"parents", {root}},
	                                                 {"author", "agent"},
	                                                 {"committer", "agent"},
	                                                 {"time", "2026-10-19T00:00:00Z"},
	                                                 {"proposal", 0},
	                                                 {"message", "Forged"},
	                                                 {"patch", json::array()},
	                                                 {"document", document}});
	forgeRefs(history, [&](json& refs) { refs["main"] = forged; });

	Repository second(rules, permissionsScreen());
	if(expected.empty()) {
		CHECK(second.openHistory(history).loaded);
		CHECK(second.head().id == forged);
		return;
	}
	std::string error;
	try {
		second.openHistory(history);
	} catch(const affresco::DocumentError& failure) {
		error = failure.what();
	}
	INFO("error: " << error);
	CHECK(error.starts_with("AffrescoUI history: " + objectFile(history, forged).string() + ": "));
	CHECK(error.find(expected) != std::string::npos);
	CHECK(second.head().id == root);
	CHECK(second.document() == permissionsScreen());
}

TEST_CASE("vcs: a history written by hand can't grant more than \"don't ask again\" does") {
	const affresco::format::Rules rules;
	const TemporaryFolder folder;
	const std::filesystem::path history = folder.path() / "board.history";
	{
		Repository first(rules, permissionsScreen());
		first.openHistory(history);
		fill(first); // one grant, agent(#title), and an open proposal
	}

	std::string rule, place = "grants[0].rule";
	SUBCASE("a grant for everything") { rule = "agent"; }
	SUBCASE("a grant for everyone") { rule = "*(#title)"; }
	SUBCASE("a grant for the whole layout") { rule = "agent(/root)"; }
	SUBCASE("a rule a proposal would grant") {
		rule = "agent";
		place = "proposals[0].remember";
	}
	forgeRefs(history, [&](json& refs) {
		if(place == "grants[0].rule") refs["grants"][0]["rule"] = rule;
		else refs["proposals"][0]["remember"] = {rule};
	});

	Repository second(rules, permissionsScreen());
	std::string error;
	try {
		second.openHistory(history);
	} catch(const affresco::DocumentError& failure) {
		error = failure.what();
	}
	INFO("error: " << error);
	CHECK(error.find(place + ": '" + rule + "' is not a rule that \"don't ask again\" grants") != std::string::npos);
	CHECK(second.grants().empty());
	CHECK(second.proposals().empty());
}
