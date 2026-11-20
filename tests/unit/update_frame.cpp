/// @file
/// @brief An update does nothing that nothing asked for: the globals, the host's variables, the window-wide values and
/// the "persist" state are built only when read, and again only when a place they read changed.

#include "Headless.hpp"
#include "Templates.hpp"

#include <affresco/Screen.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A VersionControl that is a repository, counting the calls to log() (what the `history` global is built
/// from).
class CountingLog final : public affresco::VersionControl {
public:
	/// @brief A repository of a document.
	/// @param document The document.
	explicit CountingLog(json document) : repository_(rules_, std::move(document)) {}

	mutable int logs = 0; ///< Calls to log().

	/// @brief Forwards.
	/// @param observer The observer.
	void setObserver(affresco::VersionObserver* observer) override { repository_.setObserver(observer); }
	/// @brief Forwards.
	/// @return The document.
	[[nodiscard]] const json& document() const override { return repository_.document(); }
	/// @brief Forwards.
	/// @return The state.
	[[nodiscard]] json& state() override { return repository_.state(); }
	/// @brief Forwards.
	/// @return The state.
	[[nodiscard]] const json& state() const override { return repository_.state(); }
	/// @brief Forwards.
	/// @return The head.
	[[nodiscard]] affresco::CommitInfo head() const override { return repository_.head(); }
	/// @brief Forwards.
	/// @param revision The revision.
	/// @return The result.
	affresco::RevisionResult submit(const json& revision) override { return repository_.submit(revision); }
	/// @brief Forwards.
	/// @param revision The revision.
	/// @return The error.
	std::optional<std::string> commit(const json& revision) override { return repository_.commit(revision); }
	/// @brief Forwards.
	/// @param revision The revision.
	/// @return The commit.
	[[nodiscard]] affresco::CommitInfo resolve(const json& revision) const override {
		return repository_.resolve(revision);
	}
	/// @brief Forwards.
	/// @param revision The revision.
	/// @param author The author.
	/// @return The revision.
	[[nodiscard]] json restoreRevision(const json& revision, std::string_view author) const override {
		return repository_.restoreRevision(revision, author);
	}
	/// @brief Forwards.
	/// @param revision The revision.
	/// @param author The author.
	/// @return The revision.
	[[nodiscard]] json revertRevision(const json& revision, std::string_view author) const override {
		return repository_.revertRevision(revision, author);
	}
	/// @brief Forwards.
	/// @param from A revision.
	/// @param to Another.
	/// @return The diff.
	[[nodiscard]] json diff(const json& from, const json& to) const override { return repository_.diff(from, to); }
	/// @brief Forwards.
	/// @return The proposals.
	[[nodiscard]] std::vector<affresco::ProposalInfo> proposals() const override { return repository_.proposals(); }
	/// @brief Forwards.
	/// @param proposal The proposal.
	/// @return Its diff.
	[[nodiscard]] json proposalDiff(int proposal) const override { return repository_.proposalDiff(proposal); }
	/// @brief Forwards.
	/// @param proposal The proposal.
	/// @param approval The approval.
	/// @return The error.
	std::optional<std::string> accept(int proposal, const affresco::Approval& approval) override {
		return repository_.accept(proposal, approval);
	}
	/// @brief Forwards.
	/// @param proposal The proposal.
	/// @param reason Why.
	/// @return Whether there was one.
	bool reject(int proposal, std::string_view reason) override { return repository_.reject(proposal, reason); }
	/// @brief Forwards.
	/// @param proposal The proposal.
	/// @return The error.
	std::optional<std::string> preview(int proposal) override { return repository_.preview(proposal); }
	/// @brief Forwards.
	void endPreview() override { repository_.endPreview(); }
	/// @brief Forwards.
	/// @return The proposal previewed.
	[[nodiscard]] std::optional<int> previewing() const override { return repository_.previewing(); }
	/// @brief Forwards.
	/// @param author The author.
	/// @return Whether they review.
	[[nodiscard]] bool mayReview(std::string_view author) const override { return repository_.mayReview(author); }
	/// @brief Forwards.
	/// @param mode The mode.
	void setMode(affresco::Mode mode) override { repository_.setMode(mode); }
	/// @brief Forwards.
	/// @return The mode.
	[[nodiscard]] affresco::Mode mode() const override { return repository_.mode(); }
	/// @brief Forwards.
	/// @return The grants.
	[[nodiscard]] std::vector<affresco::Grant> grants() const override { return repository_.grants(); }
	/// @brief Forwards.
	/// @param rule The rule.
	/// @return Whether there was one.
	bool revokeGrant(std::string_view rule) override { return repository_.revokeGrant(rule); }
	/// @brief Forwards, and counts.
	/// @return The log.
	[[nodiscard]] std::vector<affresco::CommitInfo> log() const override {
		++logs;
		return repository_.log();
	}
	/// @brief Forwards.
	/// @param from A revision.
	/// @param name A name.
	/// @return The changes.
	[[nodiscard]] json exportChanges(const json& from, std::string_view name) const override {
		return repository_.exportChanges(from, name);
	}
	/// @brief Forwards.
	/// @param changes The changes.
	/// @return The error.
	std::optional<std::string> importChanges(const json& changes) override {
		return repository_.importChanges(changes);
	}

private:
	affresco::format::Rules rules_;        ///< The format's rules.
	affresco::vcs::Repository repository_; ///< The repository.
};

/// @brief A document built into a runtime, on a headless window.
struct Built {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the runtime.
	CountingLog versions;                                                             ///< The document.
	affresco::format::Runtime runtime;                                                ///< The panels.

	/// @brief Builds a document.
	/// @param document The document.
	/// @param base Where its files are.
	explicit Built(json document, const std::filesystem::path& base = ".")
	    : versions(std::move(document)), runtime(versions, base) {
		runtime.build(*window);
		window->render();
	}

	/// @brief Runs an action and the update it asks for.
	/// @param name The action.
	/// @return What the update cost.
	const affresco::format::UpdateStatistics& run(std::string_view name) {
		runtime.runAction(name, window->root(), json::object());
		window->render();
		return runtime.lastUpdate();
	}

	/// @brief A label's text.
	/// @param id The label's id.
	/// @return Its text.
	[[nodiscard]] std::string text(const std::string& id) {
		const auto* label = dynamic_cast<const affresco::Label*>(runtime.find(id));
		REQUIRE(label);
		return label ? label->text() : std::string();
	}
};

/// @brief A file's text.
/// @param path The file.
/// @return Its text.
std::string readFile(const std::filesystem::path& path) {
	const std::ifstream file(path, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

/// @brief A template's document as the host opens it, with its state.
/// @param name The template.
/// @return The document.
json templateDocument(const std::string& name) {
	const std::filesystem::path folder = templateDirectory(AFFRESCOUI_EXAMPLES_DIRECTORY, name);
	const bool app = std::filesystem::exists(folder / "app.json") || std::filesystem::exists(folder / "app.lisp");
	const affresco::Screen screen = app ? affresco::Screen::fromApp(folder, {}, affresco::Screen::History::InMemory)
	                                    : affresco::Screen::fromString(readFile(folder / "index.screen.json"), folder);
	json document = json::parse(screen.documentJson());
	document["state"] = json::parse(screen.stateJson());
	return document;
}

/// @brief A screen whose labels read one value each.
constexpr const char* readers = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "unread": 0, "dirty": false, "same": "a", "list": [1, 2] },
	"actions": {
		"unread": [{ "increment": "state.unread" }],
		"dirty": [{ "toggle": "state.dirty" }],
		"same": [{ "set": "state.same", "to": "a" }],
		"sameList": [{ "set": "state.list", "to": [1, 2] }],
		"zero": [{ "increment": "state.unread", "by": 0 }],
		"save": { "title": "Save", "category": "File", "enablement": "{{state.dirty}}", "do": [] }
	},
	"root": { "type": "panel", "id": "root", "children": [
		{ "type": "label", "id": "plain", "text": "plain", "width": 100, "height": 20 },
		{ "type": "label", "id": "focus", "text": "{{context.focus}}", "y": 20, "width": 100, "height": 20 },
		{ "type": "label", "id": "width", "text": "{{window.width}}", "y": 40, "width": 100, "height": 20 },
		{ "type": "label", "id": "same", "text": "{{state.same}}", "y": 60, "width": 100, "height": 20 },
		{ "type": "label", "id": "save", "text": "{{commands[0].enabled}}", "y": 80, "width": 100, "height": 20 },
		{ "type": "toggle", "id": "button", "y": 100, "width": 100, "height": 20 }
	] }
})json";

} // namespace

TEST_CASE("update frame: on every template, a change nothing reads evaluates nothing and builds nothing") {
	for(const std::string& name : templateNames(AFFRESCOUI_EXAMPLES_DIRECTORY)) {
		CAPTURE(name);
		json document = templateDocument(name);
		document["state"]["updateFrameUnread"] = 0;
		document["actions"]["updateFrameUnread"] = json::array({{{"increment", "state.updateFrameUnread"}}});
		Built built(std::move(document), templateDirectory(AFFRESCOUI_EXAMPLES_DIRECTORY, name));
		built.run("updateFrameUnread"); // the first may still settle what the build left (a focus, a measure)
		const std::uint64_t updates = built.runtime.lastUpdate().updates;
		const affresco::format::UpdateStatistics& statistics = built.run("updateFrameUnread");
		CHECK(statistics.updates == updates + 1u);
		CHECK(statistics.nodesEvaluated == 0);
		CHECK(statistics.variablesBuilt == 0);
		CHECK(statistics.alwaysReaders == 0);
		CHECK_FALSE(statistics.full);
	}
}

TEST_CASE("update frame: a document that never reads the history never builds it") {
	Built built(json::parse(readers));
	built.run("unread");
	built.run("dirty");
	built.window->setSize(500, 300);
	built.window->render();
	CHECK(built.versions.logs == 0);
}

TEST_CASE("update frame: a focus change evaluates what reads the context, a resize what reads the window") {
	Built built(json::parse(readers));
	built.window->setFocus(built.runtime.find("button"));
	const affresco::format::UpdateStatistics& focused = built.run("unread");
	CHECK(focused.nodesEvaluated == 1);
	CHECK(built.text("focus") == "button");

	built.window->setSize(500, 300);
	built.window->render();
	CHECK_FALSE(built.runtime.lastUpdate().full);
	CHECK(built.runtime.lastUpdate().nodesEvaluated == 1);
	CHECK(built.text("width") == "500");
}

TEST_CASE("update frame: the commands' list changes when what their enablement reads does, and only then") {
	Built built(json::parse(readers));
	CHECK(built.text("save") == "false");
	const affresco::format::UpdateStatistics& unread = built.run("unread");
	CHECK(unread.variablesBuilt == 0);
	CHECK(unread.nodesEvaluated == 0);
	const affresco::format::UpdateStatistics& dirty = built.run("dirty");
	CHECK(dirty.variablesBuilt >= 1);
	CHECK(dirty.nodesEvaluated == 1);
	CHECK(built.text("save") == "true");
}

TEST_CASE("update frame: a write of the value a place holds runs no update") {
	Built built(json::parse(readers));
	const std::uint64_t updates = built.runtime.lastUpdate().updates;
	built.run("same");
	built.run("sameList");
	built.run("zero");
	CHECK(built.runtime.lastUpdate().updates == updates);
	CHECK(built.text("same") == "a");
	built.run("unread");
	CHECK(built.runtime.lastUpdate().updates == updates + 1u);
}

TEST_CASE("update frame: the state file is written when a persisted place changes, and not built otherwise") {
	const std::filesystem::path folder = std::filesystem::temp_directory_path() / "affrescoui-update-frame";
	std::filesystem::remove_all(folder);
	std::filesystem::create_directories(folder);
	json document = json::parse(readers);
	document["persist"] = json::array({"state.dirty"});
	const std::filesystem::path file = folder / "persist.screen.json";
	std::ofstream(file) << document.dump();

	const ManualClock clock;
	const auto window = createHeadlessWindow(400, 300, clock);
	affresco::Screen screen = affresco::Screen::fromFile(file, folder / "data", affresco::Screen::History::InMemory);
	screen.build(*window);
	window->render();
	const std::filesystem::path stateFile = folder / "data" / "persist.state.json";
	std::filesystem::remove(stateFile);

	screen.runAction("unread", window->root());
	window->render();
	CHECK_FALSE(std::filesystem::exists(stateFile)); // nothing persisted changed: not even compared

	screen.runAction("dirty", window->root());
	window->render();
	REQUIRE(std::filesystem::exists(stateFile));
	CHECK(json::parse(readFile(stateFile)) == json{{"/dirty", true}});
	std::error_code ignored; // Windows keeps a file the screen still has open: the next run empties the folder first
	std::filesystem::remove_all(folder, ignored);
}

TEST_CASE("update frame: the keybindings list changes when a panel's keys change, even to as many keys") {
	Built built(json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"actions": { "go": [] },
		"root": { "type": "panel", "id": "root", "children": [
			{ "type": "panel", "id": "keyed", "keys": { "ctrl+a": "go" }, "width": 10, "height": 10 },
			{ "type": "label", "id": "shown", "text": "{{keybindings[0].key}}", "y": 20, "width": 100, "height": 20 }
		] }
	})json"));
	CHECK(built.text("shown") == "ctrl+a");
	const affresco::RevisionResult applied = built.versions.submit(
	    {{"author", "app"}, {"message", "m"}, {"set", {{"/root/children/0/keys", {{"ctrl+b", "go"}}}}}});
	REQUIRE(applied.status == affresco::RevisionStatus::Applied);
	built.window->render();
	built.window->render(); // the update the list's change asks for
	CHECK(built.text("shown") == "ctrl+b");
}
