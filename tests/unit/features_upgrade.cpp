/// @file
/// @brief Unit tests for a feature's lifecycle through the `features` service: an upgrade keeps what was edited in
/// place (a three-way merge whose conflicts it lists), drops edits only in a proposal that names them when the base
/// release is gone, refuses as before, and `remove`, `enable`, `disable` and `status`; and a proof from the service
/// on a program's source. Each upgrade runs on a JSON feature and on a feature written as a program, from catalogues
/// and keys made in temporary folders.

#include "FeatureUpgrade.hpp"
#include "SignedCatalog.hpp"
#include "features/FeaturesService.hpp"

#include <affresco/format/Features.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::format::SigningKey;
using affresco::test::Catalog;
namespace format = affresco::format;

namespace {

/// @brief One kind of feature the upgrades run on: its host, its first release, and the parts the tests change.
struct Variant {
	std::string label;          ///< For the report.
	json host;                  ///< The app it installs into.
	json release;               ///< Its manifest, release 1.0.0.
	std::string edited;         ///< A pointer inside the record that the person edits in place.
	json local;                 ///< The person's value there.
	json upstream;              ///< A different value the new release gives it, for a conflict.
	std::string other;          ///< Another pointer the new release changes.
	json otherValue;            ///< Its value in the new release.
	std::string removable;      ///< A pointer the person edits and the new release removes.
	std::string removableInner; ///< The pointer inside it the person edits.
	json removableLocal;        ///< The person's value there.
};

/// @brief Forge's word count package, unsigned, with a style nothing uses (a part a release can remove).
/// @return Its manifest.
json jsonFeature() {
	std::ifstream file(std::string(AFFRESCOUI_EXAMPLES_DIRECTORY) + "/forge/json/features/word-count/feature.json");
	REQUIRE(file);
	json manifest = json::parse(file);
	manifest.erase("signature");
	manifest["provides"]["styles"] = {{"wordCount-badge", {{"color", "#ffffff"}}}};
	return manifest;
}

/// @brief A JSON app with the status bar and the export the word count needs.
/// @return The document.
json jsonHost() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"tokens": { "color.statusText": "#ffffff" },
		"state": { "text": "one two three" },
		"exports": { "editor.text": { "type": "string", "value": "{{state.text}}" } },
		"actions": { "save": [{ "set": "state.text", "to": "" }] },
		"menus": { "view": { "save": { "run": "save" } } },
		"root": { "type": "row", "id": "status-bar", "point": "statusBar.right" }
	})json");
}

/// @brief The word count written as a program: its state, a computed value, three views and a part.
/// @return Its manifest.
json programFeature() {
	const json definitions = {
	    {"state.wordCount", R"((defstate wordCount {:characters false} "The word count's state."))"},
	    {"computed.wordCount", R"((defcomputed wordCount "Its derived values." {:words (count app.editor.text)}))"},
	    {"wordCount-label",
		 R"((defview wordCount-label "The label." [] `(label#wordCount-label :text ~(str computed.wordCount.words))))"},
	    {"wordCount-hint", R"((defview wordCount-hint "The hint." [] `(label#wordCount-hint :text "words")))"},
	    {"wordCount-badge", R"((defview wordCount-badge "A badge." [] `(label#wordCount-badge :text "#")))"},
	    {"part.wordCount-views",
		 R"((defpart wordCount-views "Where its views go." :views {:statusBar.right {:wordCount-label ~(wordCount-label) :wordCount-hint ~(wordCount-hint)}}))"}};
	return {{"format", "affrescoui-feature"},
	        {"version", 2},
	        {"name", "wordCount"},
	        {"release", "1.0.0"},
	        {"title", "Word count"},
	        {"description", "Counts the words of the text being edited."},
	        {"license", "Apache-2.0"},
	        {"requires", {{"points", {"statusBar.right"}}, {"exports", {{"editor.text", {{"type", "string"}}}}}}},
	        {"provides", {{"program", {{"feature.lisp", definitions}}}}}};
}

/// @brief An app written as a program, with the status bar point and the export the word count needs.
/// @return Its source.
json programHost() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"permissions": { "allow": ["studio", "features"] },
		"program": { "app.lisp": { "after-save": "(defhook after-save \"Runs after a save.\")" } },
		"state": { "text": "one two three" },
		"points": { "statusBar.right": { "slot": "toolbar" } },
		"exports": { "editor.text": { "type": "string", "value": "{{state.text}}" } },
		"actions": { "save": [{ "set": "state.text", "to": "" }] },
		"root": { "type": "row", "id": "status-bar", "point": "statusBar.right" }
	})json");
}

/// @brief The two kinds of feature.
/// @return A JSON feature in a JSON app, and a feature with a program in a program.
std::vector<Variant> variants() {
	constexpr std::string_view program = "/provides/program/feature.lisp/";
	return {
	    {"a JSON feature", jsonHost(), jsonFeature(), "/provides/views/statusBar.right/wordCount-label/textColor",
		 "#ff0000", "#00ff00", "/provides/actions/toggle/title", "Toggle the count", "/provides/styles/wordCount-badge",
		 "/provides/styles/wordCount-badge/color", "#000000"},
	    {"a feature with a program", programHost(), programFeature(), std::string(program) + "wordCount-label",
		 R"((defview wordCount-label "The label, here." [] `(label#wordCount-label :text ~(str computed.wordCount.words))))",
		 R"((defview wordCount-label "The label, upstream." [] `(label#wordCount-label :text ~(str computed.wordCount.words))))",
		 std::string(program) + "wordCount-hint",
		 R"((defview wordCount-hint "The hint." [] `(label#wordCount-hint :text "words counted")))",
		 std::string(program) + "wordCount-badge", std::string(program) + "wordCount-badge",
		 R"((defview wordCount-badge "A badge, here." [] `(label#wordCount-badge :text "!")))"}};
}

/// @brief A later release of a manifest, with changes.
/// @param manifest The manifest.
/// @param changes `{pointer: value}`; a null value removes it.
/// @return Release 1.1.0.
json released(json manifest, const json& changes) {
	manifest["release"] = "1.1.0";
	for(const auto& [pointer, value] : changes.items()) {
		const json::json_pointer at(pointer);
		if(value.is_null()) manifest[at.parent_pointer()].erase(at.back());
		else manifest[at] = value;
	}
	return manifest;
}

/// @brief An executor whose tasks the test runs by hand.
class QueuedExecutor final : public affresco::Executor {
public:
	/// @brief Queues a task.
	/// @param task The task.
	void post(std::function<void()> task) override { tasks_.push_back(std::move(task)); }
	/// @brief Runs every task queued, and those they queue.
	void run() {
		while(!tasks_.empty()) {
			const std::function<void()> task = std::move(tasks_.front());
			tasks_.pop_front();
			task();
		}
	}

private:
	std::deque<std::function<void()>> tasks_; ///< The tasks, in order.
};

/// @brief How a call to the service ended.
struct Answer {
	json result;         ///< What it finished with, or null.
	std::string code;    ///< Why it failed ("refused", "rejected"), or "".
	std::string message; ///< The failure's message.
};

/// @brief A call the test reads.
class RecordedCall final : public affresco::services::Call {
public:
	/// @brief A call with arguments.
	/// @param arguments Its `with`.
	explicit RecordedCall(json arguments) : arguments_(std::move(arguments)) {}
	/// @copydoc affresco::services::Call::arguments
	const json& arguments() const noexcept override { return arguments_; }
	/// @copydoc affresco::services::Call::stopToken
	std::stop_token stopToken() const noexcept override { return {}; }
	/// @copydoc affresco::services::Call::progress
	void progress(json /*progress*/) override {}
	/// @copydoc affresco::services::Call::event
	void event(json /*event*/) override {}
	/// @copydoc affresco::services::Call::finish
	void finish(json result) override { answer.result = std::move(result); }
	/// @copydoc affresco::services::Call::fail
	void fail(std::string code, std::string message, json /*data*/) override {
		answer.code = std::move(code);
		answer.message = std::move(message);
	}

	Answer answer; ///< How it ended.

private:
	json arguments_; ///< See the constructor.
};

/// @brief A host with a signed catalogue of the feature, the feature installed from it.
struct Installed {
	SigningKey key = SigningKey::generate();               ///< The publisher.
	Catalog catalog;                                       ///< Its catalogue.
	format::Features features;                             ///< The host's catalogues.
	format::Rules rules;                                   ///< The document rules.
	std::unique_ptr<affresco::vcs::Repository> repository; ///< The document.

	/// @brief Installs a feature's first release.
	/// @param variant The kind of feature.
	explicit Installed(const Variant& variant) {
		catalog.add(variant.release, key);
		features.trustPublisher(key.publisher());
		features.addCatalog(catalog.publish(key));
		repository = std::make_unique<affresco::vcs::Repository>(rules, variant.host);
		const Answer installed = call("install", {{"name", "wordCount"}});
		REQUIRE_MESSAGE(installed.code.empty(), installed.message);
		REQUIRE(installed.result["status"] == "applied");
	}

	/// @brief Publishes another release.
	/// @param manifest Its manifest.
	void publish(const json& manifest) {
		catalog.add(manifest, key);
		features.addCatalog(catalog.publish(key));
	}

	/// @brief Edits the installed record in place, as a person in the studio would.
	/// @param pointer A pointer inside the record.
	/// @param value Its new value.
	void edit(const std::string& pointer, const json& value) const {
		const affresco::RevisionResult result =
		    repository->submit({{"author", "studio"}, {"set", {{"/features/wordCount" + pointer, value}}}});
		REQUIRE_MESSAGE(result.status == RevisionStatus::Applied, result.error);
	}

	/// @brief The installed record.
	/// @return It, or null.
	[[nodiscard]] json record() const {
		const json& document = repository->sourceDocument();
		return document.contains("features") && document["features"].contains("wordCount")
		           ? document["features"]["wordCount"]
				   : json();
	}

	/// @brief Calls a method of the `features` service and runs what it queues.
	/// @param method The method.
	/// @param arguments Its arguments.
	/// @return How it ended.
	Answer call(const std::string& method, json arguments) const {
		QueuedExecutor executor;
		const auto alive = std::make_shared<bool>(true);
		const std::unique_ptr<affresco::services::Provider> provider =
		    affresco::detail::makeFeaturesProvider(features, *repository, alive);
		const std::unique_ptr<affresco::services::Service> service =
		    provider->create(json::object(), {executor, {}, {}, {}});
		const std::vector<affresco::services::MethodSpec> methods = provider->methods(json::object());
		const auto spec = std::ranges::find(methods, method, &affresco::services::MethodSpec::name);
		REQUIRE(spec != methods.end());
		const auto recorded = std::make_shared<RecordedCall>(std::move(arguments));
		REQUIRE_FALSE(service->invoke(*spec, recorded).has_value());
		executor.run();
		return recorded->answer;
	}

	/// @brief Upgrades the feature through the service.
	/// @return How it ended.
	[[nodiscard]] Answer upgrade() const { return call("upgrade", {{"name", "wordCount"}}); }

	/// @brief Undoes the last commit on main.
	void undo() const {
		const affresco::RevisionResult result = repository->submit(repository->revertRevision("HEAD", "studio"));
		REQUIRE_MESSAGE(result.status == RevisionStatus::Applied, result.error);
	}
};

/// @brief A value at a pointer inside a record.
/// @param record The record.
/// @param pointer The pointer.
/// @return It, or null.
json at(const json& record, const std::string& pointer) {
	const json::json_pointer path(pointer);
	return record.contains(path) ? record[path] : json();
}

/// @brief The last commit's message.
/// @param installed The host.
/// @return It.
std::string lastMessage(const Installed& installed) { return installed.repository->log().back().message; }

} // namespace

TEST_CASE("feature upgrade: with no local edit, the record is exactly the new release's") {
	for(const Variant& variant : variants()) {
		CAPTURE(variant.label);
		Installed installed(variant);
		CHECK_FALSE(format::edited(installed.record(), nullptr));
		installed.edit("/enabled", false);
		installed.publish(released(variant.release, {{variant.other, variant.otherValue}}));
		const Answer upgraded = installed.upgrade();
		REQUIRE_MESSAGE(upgraded.code.empty(), upgraded.message);
		CHECK(upgraded.result["status"] == "applied");
		CHECK_FALSE(upgraded.result.contains("leftOut"));
		json expected = installed.features.releaseRecord("wordCount", "1.1.0", installed.record().value("order", 0));
		expected["enabled"] = false;
		CHECK(installed.record() == expected);
		CHECK(lastMessage(installed) == "Upgrade wordCount to 1.1.0");
	}
}

TEST_CASE("feature upgrade: a local edit survives an upgrade that changes another part") {
	for(const Variant& variant : variants()) {
		CAPTURE(variant.label);
		Installed installed(variant);
		installed.edit(variant.edited, variant.local);
		CHECK(format::edited(installed.record(), nullptr));
		installed.publish(released(variant.release, {{variant.other, variant.otherValue}}));
		const Answer upgraded = installed.upgrade();
		REQUIRE_MESSAGE(upgraded.code.empty(), upgraded.message);
		CHECK(upgraded.result["status"] == "applied");
		const json record = installed.record();
		CHECK(record["release"] == "1.1.0");
		CHECK(at(record, variant.edited) == variant.local);
		CHECK(at(record, variant.other) == variant.otherValue);
		CHECK(record["installed"] == installed.features.releaseRecord(
		                                 "wordCount", "1.1.0", installed.record().value("order", 0))["installed"]);
		CHECK(format::edited(record, nullptr)); // the kept edit is still an edit
		CHECK(lastMessage(installed) == "Upgrade wordCount to 1.1.0, keeping local edits");
	}
}

TEST_CASE("feature upgrade: both changed the same part: it applies, keeps the local one, and lists the conflict") {
	for(const Variant& variant : variants()) {
		CAPTURE(variant.label);
		Installed installed(variant);
		const json original = at(installed.record(), variant.edited);
		installed.edit(variant.edited, variant.local);
		installed.publish(
		    released(variant.release, {{variant.edited, variant.upstream}, {variant.other, variant.otherValue}}));
		const Answer upgraded = installed.upgrade();
		REQUIRE_MESSAGE(upgraded.code.empty(), upgraded.message);
		CHECK(upgraded.result["status"] == "applied");
		const json record = installed.record();
		CHECK(at(record, variant.edited) == variant.local);
		CHECK(at(record, variant.other) == variant.otherValue);
		const json& leftOut = upgraded.result["leftOut"];
		REQUIRE(leftOut.size() == 1);
		const std::string path = "/features/wordCount" + variant.edited;
		CHECK(leftOut[0]["path"] == path);
		CHECK(leftOut[0]["reason"] == "changed locally and in 1.1.0");
		CHECK(leftOut[0]["base"] == original);
		CHECK(leftOut[0]["main"] == variant.local);
		CHECK(leftOut[0]["proposal"] == variant.upstream);
		CHECK(lastMessage(installed) == "Upgrade wordCount to 1.1.0, keeping local edits; 1 conflict, the local "
		                                "version kept:\n- " +
		                                    path + ": changed locally and in 1.1.0");
	}
}

TEST_CASE("feature upgrade: a part the new release removed, edited locally, is a conflict and stays") {
	for(const Variant& variant : variants()) {
		CAPTURE(variant.label);
		Installed installed(variant);
		installed.edit(variant.removableInner, variant.removableLocal);
		installed.publish(released(variant.release, {{variant.removable, nullptr}}));
		const Answer upgraded = installed.upgrade();
		REQUIRE_MESSAGE(upgraded.code.empty(), upgraded.message);
		CHECK(at(installed.record(), variant.removableInner) == variant.removableLocal);
		const json& leftOut = upgraded.result["leftOut"];
		REQUIRE(leftOut.size() == 1);
		CHECK(leftOut[0]["path"] == "/features/wordCount" + variant.removable);
		CHECK(leftOut[0]["reason"] == "changed locally, removed in 1.1.0");
		CHECK(leftOut[0]["proposal"].is_null());
	}
}

TEST_CASE("feature upgrade: when the base release is gone, a proposal names the edits it drops") {
	for(const Variant& variant : variants()) {
		CAPTURE(variant.label);
		Installed installed(variant);
		installed.edit(variant.edited, variant.local);
		installed.catalog.index["features"]["wordCount"]["releases"] = json::array(); // 1.0.0 is gone
		installed.publish(released(variant.release, {{variant.other, variant.otherValue}}));
		const json before = installed.record();
		const Answer upgraded = installed.upgrade();
		REQUIRE_MESSAGE(upgraded.code.empty(), upgraded.message);
		CHECK(upgraded.result["status"] == "proposed");
		CHECK(installed.record() == before); // nothing lands without the person's yes
		const json& dropped = upgraded.result["dropped"];
		CHECK(std::ranges::find(dropped, json("/features/wordCount" + variant.edited)) != dropped.end());
		const std::vector<affresco::ProposalInfo> proposals = installed.repository->proposals();
		REQUIRE(proposals.size() == 1);
		CHECK(proposals[0].message.starts_with("Upgrade wordCount to 1.1.0, dropping local edits:"));
		CHECK(proposals[0].message.contains("/features/wordCount" + variant.edited));
	}
}

TEST_CASE("feature upgrade: a record without its digest is edited if it differs from its release; a bad digest is "
          "refused") {
	const Variant variant = variants().front();
	Installed installed(variant);
	json record = installed.record();
	record.erase("installed");
	const json base = installed.features.releaseRecord("wordCount", "1.0.0", installed.record().value("order", 0));
	CHECK_FALSE(format::edited(record, &base));
	CHECK(format::edited(record, nullptr)); // no base to tell: kept as an edit, so nothing is dropped silently
	record[json::json_pointer(variant.edited)] = variant.local;
	CHECK(format::edited(record, &base));
	for(const json& malformed : {json("sha256:00"), json(7), json("md5:" + std::string(64, '0'))}) {
		CAPTURE(malformed.dump());
		const affresco::RevisionResult refused = installed.repository->submit(
		    {{"author", "studio"}, {"set", {{"/features/wordCount/installed", malformed}}}});
		CHECK(refused.status != RevisionStatus::Applied);
		CHECK(refused.error.contains("installed"));
	}
}

TEST_CASE("feature upgrade: refused as before: not installed, up to date, another publisher") {
	const Variant variant = variants().front();
	Installed installed(variant);
	const Answer missing = installed.call("upgrade", {{"name", "other"}});
	CHECK(missing.code == "refused");
	CHECK(missing.message.contains("isn't installed"));
	const Answer current = installed.upgrade();
	CHECK(current.code == "refused");
	CHECK(current.message.contains("up to date"));
	const SigningKey stranger = SigningKey::generate();
	installed.features.trustPublisher(stranger.publisher());
	installed.catalog.add(released(variant.release, {}), stranger);
	installed.features.addCatalog(installed.catalog.publish(installed.key));
	const Answer another = installed.upgrade();
	CHECK(another.code == "refused");
	CHECK(another.message.contains("another publisher"));
}

TEST_CASE("features service: remove, enable, disable and status, each undone") {
	for(const Variant& variant : variants()) {
		CAPTURE(variant.label);
		Installed installed(variant);
		installed.publish(released(variant.release, {}));
		Answer status = installed.call("status", {{"name", "wordCount"}});
		CHECK(
		    status.result ==
		    json{{"installed", true}, {"release", "1.0.0"}, {"newest", "1.1.0"}, {"edited", false}, {"enabled", true}});

		CHECK(installed.call("enable", {{"name", "wordCount"}}).message.contains("enabled already"));
		const Answer disabled = installed.call("disable", {{"name", "wordCount"}});
		REQUIRE_MESSAGE(disabled.code.empty(), disabled.message);
		CHECK(disabled.result["status"] == "applied");
		CHECK(installed.record()["enabled"] == false);
		CHECK(lastMessage(installed) == "Disable wordCount");
		installed.undo();
		CHECK(installed.record()["enabled"] == true);
		installed.edit("/enabled", false);
		CHECK(installed.call("enable", {{"name", "wordCount"}}).code.empty());
		CHECK(installed.record()["enabled"] == true);
		installed.undo();
		CHECK(installed.record()["enabled"] == false);

		installed.edit(variant.edited, variant.local);
		status = installed.call("status", {{"name", "wordCount"}});
		CHECK(status.result["edited"] == true);
		CHECK(status.result["enabled"] == false);

		const Answer removed = installed.call("remove", {{"name", "wordCount"}});
		REQUIRE_MESSAGE(removed.code.empty(), removed.message);
		CHECK(installed.record().is_null());
		CHECK(installed.call("status", {{"name", "wordCount"}}).result ==
		      json{{"installed", false}, {"release", ""}, {"newest", "1.1.0"}, {"edited", false}, {"enabled", false}});
		CHECK(installed.call("remove", {{"name", "wordCount"}}).message.contains("isn't installed"));
		installed.undo();
		CHECK(at(installed.record(), variant.edited) == variant.local);
	}
}

TEST_CASE("features service: a proof from the service works on a program's source; a JSON host is unchanged") {
	const json sections = json::parse(R"json({
		"permissions": { "allow": ["studio", "features"] },
		"state": { "pages": [{ "id": "a", "name": "Sky" }] },
		"exports": { "doc.pages": { "type": "array", "kind": "list", "value": "{{state.pages}}",
			"item": { "id": "string", "name": "string" } } },
		"points": { "panels.dock": { "slot": "dock", "receives": ["doc.pages"] } },
		"features": {}
	})json");
	json program = affresco::lisp::sourceDocument(R"lisp((defscreen main "The window and its dock."
  (row#main (column#panels :point panels.dock)))
)lisp",
	                                              "app.lisp");
	program.update(sections);
	json written = {{"format", "affrescoui-screen"}, {"version", 2}};
	written.update(sections);
	written["root"] = json::parse(
	    R"json({ "type": "row", "id": "main", "children": [{ "type": "column", "id": "panels", "point": "panels.dock" }] })json");
	const json lister = json::parse(R"json({
		"format": "affrescoui-feature", "version": 2, "name": "pageList", "release": "1.0.0", "title": "Page list",
		"requires": { "format": 2 },
		"needs": { "pages": { "kind": "list", "item": { "id": "string", "name": "string" } },
		           "dock": { "kind": "slot", "slot": "dock" } },
		"provides": { "views": { "dock": { "pageList-count": { "type": "label", "text": "{{length(app.pages)}} pages" } } } }
	})json");
	const json binding = json::parse(R"json({
		"format": "affrescoui-adapter", "version": 1, "feature": "pageList", "host": "editor",
		"bind": { "pages": { "value": "{{app.doc.pages}}" } }
	})json");
	for(const json& host : {program, written}) {
		CAPTURE(host.contains("program"));
		const format::Rules rules;
		affresco::vcs::Repository repository(rules, host);
		const format::Features features;
		QueuedExecutor executor;
		const auto alive = std::make_shared<bool>(true);
		const auto provider = affresco::detail::makeFeaturesProvider(features, repository, alive);
		const auto service = provider->create(json::object(), {executor, {}, {}, {}});
		const auto methods = provider->methods(json::object());
		const auto compose = std::ranges::find(methods, "compose", &affresco::services::MethodSpec::name);
		REQUIRE(compose != methods.end());
		const auto call = std::make_shared<RecordedCall>(
		    json{{"manifest", lister}, {"adapter", binding}, {"source", "app:other"}, {"host", "editor"}});
		REQUIRE_FALSE(service->invoke(*compose, call).has_value());
		executor.run();
		REQUIRE_MESSAGE(call->answer.code.empty(), call->answer.message);
		CHECK(call->answer.result["status"] == "applied");
		const json& source = repository.sourceDocument();
		const json& record = source["features"]["pageList"];
		CHECK(record["proof"]["passed"] == true);
		CHECK(source.contains("program") == host.contains("program")); // the record went in beside the program
		CHECK(source.contains("root") == host.contains("root"));
		// The digest version control writes is the format's: the record isn't an edit
		REQUIRE(record.contains("installed"));
		CHECK(record["installed"] == format::installedDigest(record));
	}
}
