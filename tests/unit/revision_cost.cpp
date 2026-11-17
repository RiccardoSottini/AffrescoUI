/// @file
/// @brief Unit tests for what a revision costs: the version control says what it changed (DocumentChange), and the
/// runtime does each thing once, and only for what was touched (modules/format/src/runtime/EngineVersions.cpp,
/// CompiledCache::carryOver()). Counted exactly, never timed.

#include "CompiledNode.hpp" // modules/format/src/nodes
#include "Headless.hpp"
#include "LispApp.hpp"
#include "ProgramRules.hpp" // modules/format/src/rules

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using affresco::DocumentChange;
using affresco::DocumentError;
using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::revisionCounters;
using affresco::format::RevisionCounters;

namespace {

/// @brief A screen of three labels, a colour and a font.
constexpr const char* labelsScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "count": 0 },
	"colors": { "ink": "#203040" },
	"root": { "id": "root", "type": "panel", "children": [
		{ "id": "logo", "type": "label", "text": "Logo", "width": 200, "height": 30, "color": "@ink" },
		{ "id": "count", "type": "label", "text": "{{state.count}}", "y": 40, "width": 200, "height": 30 },
		{ "id": "other", "type": "label", "text": "Other", "y": 80, "width": 200, "height": 30 }
	] }
})json";

/// @brief A screen on a program: two views, each building a label.
constexpr const char* programScreen = R"lisp(
(defview logo "The logo." [] `(label#logo :text "Logo" :width 200 :height 30))
(defview other "Another label." [] `(label#other :text "Other" :y 40 :width 200 :height 30))
(defscreen main :state {:count 0}
  (panel#root ~(logo) ~(other) (label#count :text ~state.count :y 80 :width 200 :height 30)))
)lisp";

/// @brief Keeps what the version control said, and refuses a version when asked to.
class Recorder final : public affresco::VersionObserver {
public:
	/// @brief A version landed, and may have changed anything.
	void documentChanged() override {
		changes.emplace_back();
		befores.push_back(false);
	}
	/// @brief A version landed, and what it changed.
	/// @param change It.
	void documentChangedWith(const DocumentChange& change) override {
		changes.push_back(change);
		befores.push_back(change.before != nullptr);
		if(refuse) {
			refuse = false;
			throw DocumentError("refused");
		}
	}
	/// @brief The proposals changed.
	void proposalsChanged() override {}

	std::vector<DocumentChange> changes; ///< What each version said, in order (everything for documentChanged()).
	std::vector<bool> befores;           ///< Whether each one came with the document before.
	bool refuse = false;                 ///< Whether the next version is refused.
};

/// @brief A document under version control with a recorder as its observer.
struct Recorded {
	affresco::format::Rules rules;                                          ///< The format's rules.
	affresco::vcs::Repository repository{rules, json::parse(labelsScreen)}; ///< The history.
	Recorder recorder;                                                      ///< What it said.

	/// @brief Observes the history.
	Recorded() { repository.setObserver(&recorder); }

	/// @brief Commits a revision, which must apply.
	/// @param patch Its patch.
	void commit(const json& patch) {
		const std::optional<std::string> error =
		    repository.commit({{"author", "agent/x"}, {"message", "m"}, {"patch", patch}});
		INFO(error.value_or(""));
		REQUIRE_FALSE(error.has_value());
	}

	/// @brief What the last version said.
	/// @return It.
	[[nodiscard]] const DocumentChange& last() const { return recorder.changes.back(); }
};

/// @brief A screen built in a headless window, with its own history.
struct Built {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(400, 300, clock); ///< Outlives the runtime.
	affresco::format::Rules rules;                                                    ///< The format's rules.
	affresco::vcs::Repository repository;                                             ///< The document.
	affresco::format::Runtime runtime;                                                ///< The panels.

	/// @brief Builds a screen.
	/// @param document It.
	explicit Built(const json& document = json::parse(labelsScreen))
	    : repository(rules, document), runtime(repository, ".") {
		runtime.build(*window);
		window->render();
	}

	/// @brief Commits a revision.
	/// @param patch Its patch.
	/// @return The error, if it was refused.
	std::optional<std::string> commit(const json& patch) {
		std::optional<std::string> error =
		    repository.commit({{"author", "agent/x"}, {"message", "m"}, {"patch", patch}});
		window->render();
		return error;
	}

	/// @brief What a label shows.
	/// @param id Its id.
	/// @return Its text, or "" if there is none.
	[[nodiscard]] std::string text(const char* id) {
		const auto* label = dynamic_cast<const affresco::Label*>(runtime.find(id));
		return label ? label->text() : "";
	}
};

/// @brief What the counters moved by since a moment.
struct Since {
	RevisionCounters start = revisionCounters(); ///< Where they were.

	/// @brief Nodes compiled since.
	[[nodiscard]] std::size_t compiled() const { return revisionCounters().nodesCompiled - start.nodesCompiled; }
	/// @brief Compiled nodes kept since.
	[[nodiscard]] std::size_t carried() const { return revisionCounters().nodesCarried - start.nodesCarried; }
	/// @brief Times colours and fonts were read since.
	[[nodiscard]] std::size_t derived() const { return revisionCounters().derivedReads - start.derivedReads; }
};

} // namespace

TEST_CASE("revision cost: a label's text set says its place, its section, and keeps the document before") {
	Recorded recorded;
	recorded.commit(json::parse(R"([{"op": "replace", "path": "/root/children/0/text", "value": "Brand"}])"));
	const DocumentChange& change = recorded.last();
	CHECK_FALSE(change.everything);
	CHECK(change.document == std::vector<std::string>{"/root/children/0/text"});
	CHECK(change.sections == std::vector<std::string>{"root"});
	CHECK(change.state.empty());
	CHECK(recorded.recorder.befores.back());
}

TEST_CASE("revision cost: a label's text set compiles that label alone and reads no colour or font again") {
	Built built;
	const Since since;
	REQUIRE_FALSE(
	    built.commit(json::parse(R"([{"op": "replace", "path": "/root/children/0/text", "value": "Brand"}])")));
	CHECK(built.text("logo") == "Brand");
	CHECK(built.text("other") == "Other");
	CHECK(since.compiled() == 1U); // the label; the root and its siblings kept what they had
	CHECK(since.carried() >= 3U);
	CHECK(since.derived() == 0U);
}

TEST_CASE("revision cost: a revision of the state alone is a state update, with nothing compiled") {
	Recorded recorded;
	recorded.commit(json::parse(R"([{"op": "replace", "path": "/state/count", "value": 5}])"));
	CHECK_FALSE(recorded.last().everything);
	CHECK(recorded.last().document.empty());
	CHECK(recorded.last().state == std::vector<std::string>{"/count"});

	Built built;
	const Since since;
	REQUIRE_FALSE(built.commit(json::parse(R"([{"op": "replace", "path": "/state/count", "value": 5}])")));
	CHECK(built.text("count") == "5");
	CHECK_FALSE(built.runtime.lastUpdate().full);
	CHECK(since.compiled() == 0U);
	CHECK(since.derived() == 0U);
}

TEST_CASE("revision cost: a revision of the state alone shows the version's number and the history it makes") {
	json screen = json::parse(labelsScreen);
	screen["root"]["children"][0]["text"] = "{{revision.number}} of {{length(history)}}";
	Built built(screen);
	REQUIRE(built.text("logo") == "0 of 1");
	REQUIRE_FALSE(built.commit(json::parse(R"([{"op": "replace", "path": "/state/count", "value": 5}])")));
	CHECK(built.text("logo") == "1 of 2");
	CHECK(built.text("count") == "5");
}

TEST_CASE("revision cost: a definition changed builds its program once, and the version before not again") {
	lispapp::App app(programScreen);
	const std::size_t builds = affresco::format::programBuildCount();
	const Since since;
	app.write("logo", R"((defview logo "The logo." [] `(label#logo :text "Brand" :width 200 :height 30)))");
	app.settle();
	CHECK(affresco::format::programBuildCount() - builds == 1U);
	CHECK(dynamic_cast<const affresco::Label*>(app.runtime->find("logo"))->text() == "Brand");
	CHECK(dynamic_cast<const affresco::Label*>(app.runtime->find("other"))->text() == "Other");
	CHECK(since.compiled() == 1U);

	const std::size_t second = affresco::format::programBuildCount();
	app.write("other",
	          R"((defview other "Another label." [] `(label#other :text "Else" :y 40 :width 200 :height 30)))");
	app.settle();
	CHECK(affresco::format::programBuildCount() - second == 1U);
	CHECK(dynamic_cast<const affresco::Label*>(app.runtime->find("other"))->text() == "Else");
}

TEST_CASE("revision cost: adding a style compiles everything again, and the screen is right") {
	Built built;
	const Since since;
	REQUIRE_FALSE(built.commit(json::parse(R"([{"op": "add", "path": "/styles", "value": {"big": {"x": 40}}},
		{"op": "add", "path": "/root/children/2/style", "value": "big"}])")));
	CHECK(since.carried() == 0U);
	CHECK(since.compiled() >= 4U);
	CHECK(built.runtime.find("other")->x().value == doctest::Approx(40));
	CHECK(built.text("logo") == "Logo");
}

TEST_CASE("revision cost: a version the observer refuses is taken back, and the observer hears everything") {
	Recorded recorded;
	const json before = recorded.repository.document();
	recorded.recorder.refuse = true;
	const std::optional<std::string> error = recorded.repository.commit(
	    {{"author", "agent/x"},
		 {"message", "m"},
		 {"patch", json::parse(R"([{"op": "replace", "path": "/root/children/0/text", "value": "B"}])")}});
	CHECK(error.value_or("") == "refused");
	CHECK(recorded.repository.document() == before);
	CHECK(recorded.repository.head().number == 0);
	REQUIRE(recorded.recorder.changes.size() == 2U);
	CHECK(recorded.recorder.changes.back().everything);
}

TEST_CASE("revision cost: after a version the runtime refuses, what stays compiled is the version shown") {
	Built built;
	REQUIRE(
	    built.commit(json::parse(R"([{"op": "replace", "path": "/root/children/0/type", "value": "noSuchType"}])")));
	CHECK(built.text("logo") == "Logo");
	REQUIRE_FALSE(
	    built.commit(json::parse(R"([{"op": "replace", "path": "/root/children/2/text", "value": "Next"}])")));
	CHECK(built.text("logo") == "Logo");
	CHECK(built.text("other") == "Next");
}

TEST_CASE("revision cost: a restore, a revert, a preview, its end and an import change everything") {
	Recorded recorded;
	recorded.commit(json::parse(R"([{"op": "replace", "path": "/root/children/0/text", "value": "B"}])"));
	const auto everything = [&recorded] {
		REQUIRE_FALSE(recorded.recorder.changes.empty());
		return recorded.last().everything;
	};

	REQUIRE_FALSE(recorded.repository.commit(recorded.repository.restoreRevision(0, "studio")));
	CHECK(everything());
	REQUIRE_FALSE(recorded.repository.commit(recorded.repository.revertRevision("HEAD", "studio")));
	CHECK(everything());

	const affresco::RevisionResult proposed = recorded.repository.propose(
	    {{"author", "agent/x"},
		 {"message", "m"},
		 {"patch", json::parse(R"([{"op": "replace", "path": "/root/children/2/text", "value": "P"}])")}});
	REQUIRE(proposed.status == affresco::RevisionStatus::Proposed);
	recorded.recorder.changes.clear();
	REQUIRE_FALSE(recorded.repository.preview(proposed.proposal));
	CHECK(everything());
	recorded.recorder.changes.clear();
	recorded.repository.endPreview();
	CHECK(everything());

	Recorded source;
	source.commit(json::parse(R"([{"op": "replace", "path": "/root/children/2/text", "value": "I"}])"));
	Recorded target;
	REQUIRE_FALSE(target.repository.importChanges(source.repository.exportChanges(0, "import")));
	CHECK(target.last().everything);
}

TEST_CASE("revision cost: a definition changed shows the version's number, read by a label the program builds") {
	lispapp::App app(R"lisp(
(defview logo "The logo." [] `(label#logo :text "Logo" :width 200 :height 30))
(defscreen main (panel#root ~(logo) (label#number :text ~(str "Revision " revision.number) :y 40 :width 200 :height 30)))
)lisp");
	const auto number = [&app] { return dynamic_cast<const affresco::Label*>(app.runtime->find("number"))->text(); };
	REQUIRE(number() == "Revision 0");
	app.write("logo", R"((defview logo "The logo." [] `(label#logo :text "Brand" :width 200 :height 30)))");
	app.settle();
	CHECK(number() == "Revision 1");
}

TEST_CASE("revision cost: a revision of the document shows the version's number, through a component's parameter too") {
	json screen = json::parse(labelsScreen);
	screen["components"] = json::parse(R"({"bar": {"params": {"items": []}, "node": {"type": "panel", "children": [
		{"repeat": {"for": "item", "in": "items", "index": "i"}, "id": "item{{i}}", "type": "label", "text": "{{item.text}}",
		 "width": 200, "height": 30}]}}})");
	screen["root"]["children"].push_back(json::parse(R"({"id": "bar", "use": "bar", "y": 120,
		"with": {"items": [{"text": "{{'Revision ' + revision.number}}"}]}})"));
	screen["root"]["children"][0]["text"] = "{{revision.number}} of {{length(history)}}";
	Built built(screen);
	REQUIRE(built.text("logo") == "0 of 1");
	REQUIRE(built.text("item0") == "Revision 0");
	REQUIRE_FALSE(built.commit(json::parse(R"([{"op": "replace", "path": "/colors/ink", "value": "#405060"}])")));
	CHECK(built.text("logo") == "1 of 2");
	CHECK(built.text("item0") == "Revision 1");
}
