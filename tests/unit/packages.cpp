/// @file
/// @brief Reading a package (Package.cpp, PackageFiles.hpp): a package of each kind read and written back the same,
/// each check refusing with its place and its reason, each limit at its edge, and the summary the person reviews.

#include <affresco/format/Package.hpp>
#include <affresco/format/PackageFiles.hpp>

#include "FeatureSignatures.hpp"

#include <affresco/core/Assets.hpp>
#include <affresco/core/Sha256.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
namespace format = affresco::format;

namespace {

/// @brief A version's document: generated test data only.
/// @return It.
json versionDocument() {
	return json::parse(R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "title": "Compact notes", "columns": ["To do", "Blocked", "Done"] },
	"program": { "app.lisp": { "count": "(defn count [] 2)", "blocked": "(defn blocked [] \"Blocked\")" } },
	"root": { "type": "label", "id": "title", "text": "{{state.title}}" }
	})json");
}

/// @brief The delta from versionDocument() to its base: the title and the "Blocked" column added, a definition changed
/// and one removed.
/// @return It.
json versionDelta() {
	return json::parse(R"json([
	{ "op": "replace", "path": "/state/title", "value": "Notes" },
	{ "op": "remove", "path": "/state/columns/1" },
	{ "op": "replace", "path": "/program/app.lisp/count", "value": "(defn count [] 1)" },
	{ "op": "remove", "path": "/program/app.lisp/blocked" },
	{ "op": "add", "path": "/program/app.lisp/old", "value": "(defn old [] nil)" }
	])json");
}

/// @brief A package of kind "changes" whose base is what it names.
/// @return It.
json changesPackage() {
	const json base = versionDocument().patch(versionDelta());
	return {{"format", "affrescoui-package"},
	        {"version", 1},
	        {"kind", "changes"},
	        {"name", "Flow with a Blocked column"},
	        {"release", "1.1.0"},
	        {"description", "A column for cards that wait on someone."},
	        {"author", "Ada"},
	        {"license", "Apache-2.0"},
	        {"engine", "1.0.0"},
	        {"base",
	         {{"app", "notes"},
	          {"name", "Notes"},
	          {"release", "1.0.0"},
	          {"notation", "lisp"},
	          {"tree", format::documentTree(base)},
	          {"delta", versionDelta()}}},
	        {"document", versionDocument()}};
}

/// @brief A package of kind "app": changesPackage() with the app's files.
/// @return It.
json appPackage() {
	json package = changesPackage();
	package["kind"] = "app";
	package["base"]["files"] = {
	    {"app.json",
		 {{"text",
		   R"({ "format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes", "entry": "index.screen.lisp" })"}}},
	    {"index.screen.lisp", {{"text", "(screen)"}}},
	    {"assets/icon.png", {{"data", "iVBORw0K"}}}};
	return package;
}

/// @brief Why a package is refused.
/// @param value The package.
/// @return The error, or "" if it is read.
std::string refusal(const json& value) {
	try {
		static_cast<void>(format::parsePackage(value));
		return "";
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
}

/// @brief Adds an asset to a package, listing it in its document.
/// @param package The package.
/// @param name The asset's name.
/// @param bytes Its bytes.
void addAsset(json& package, const std::string& name, const std::string& bytes) {
	package["assets"][name] = {{"type", "image/svg+xml"}, {"text", bytes}};
	package["document"]["assets"][name] = {
	    {"type", "image/svg+xml"}, {"size", bytes.size()}, {"sha256", affresco::sha256::hex(bytes)}};
	// the base doesn't have it
	package["base"]["delta"].push_back({{"op", "remove"}, {"path", "/assets"}});
}

/// A small SVG: generated test data only.
constexpr std::string_view dot = R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"/>)";

} // namespace

TEST_CASE("packages: a package of each kind is read and written back the same") {
	for(const json& value : {changesPackage(), appPackage()}) {
		const format::Package package = format::parsePackage(value);
		CHECK(format::packageJson(package) == value);
		CHECK(format::packageJson(format::readPackage(value.dump())) == value);
		CHECK(format::baseOf(package) == versionDocument().patch(versionDelta()));
	}
	json withExtras = changesPackage();
	withExtras["state"] = {{"blockedReason", "waiting"}};
	addAsset(withExtras, "dot.svg", std::string(dot));
	withExtras["proof"] = {{"scenarios", json::array()}};
	const format::Package package = format::parsePackage(withExtras);
	CHECK(format::packageJson(package) == withExtras);
	CHECK(package.kind == format::PackageKind::Changes);
	CHECK(package.author == "Ada");
	CHECK(package.base.notation == "lisp");
}

TEST_CASE("packages: each check refuses with its place and its reason") {
	const auto with = [](auto change) {
		json value = changesPackage();
		change(value);
		return refusal(value);
	};
	CHECK(with([](json& v) { v["colour"] = "red"; }).contains("AffrescoUI package: colour: not a key of a package"));
	CHECK(with([](json& v) { v["base"]["shade"] = 1; }).contains("base.shade: not a key"));
	CHECK(with([](json& v) { v["format"] = "affrescoui-app"; }).contains("format: expected \"affrescoui-package\""));
	CHECK(with([](json& v) { v["version"] = 2; }).contains("version: expected 1"));
	CHECK(with([](json& v) { v["kind"] = "theme"; }).contains("kind: expected"));
	CHECK(with([](json& v) { v.erase("engine"); }).contains("engine: missing"));
	CHECK(with([](json& v) { v["release"] = "1.2"; }).contains("release: expected three numbers"));
	CHECK(with([](json& v) { v["name"] = 7; }).contains("name: expected text"));
	CHECK(with([](json& v) { v["base"]["app"] = "Notes"; }).contains("base.app: expected an app's id"));
	CHECK(with([](json& v) { v["base"]["notation"] = "yaml"; }).contains("base.notation"));
	CHECK(with([](json& v) { v["base"]["tree"] = "sha1:00"; }).contains("base.tree: expected \"sha256:\""));
	CHECK(with([](json& v) { v["document"] = json::array(); }).contains("document: expected a source document"));
	// the delta: only add, remove and replace, as a delta writes them
	CHECK(with([](json& v) {
		      v["base"]["delta"][0]["op"] = "move";
	      }).contains("base.delta[0].op: expected \"add\", \"remove\" or \"replace\""));
	CHECK(with([](json& v) { v["base"]["delta"][1]["value"] = 1; }).contains("base.delta[1]: a remove has no value"));
	CHECK(with([](json& v) { v["base"]["delta"][0].erase("value"); }).contains("base.delta[0]: missing its value"));
	CHECK(with([](json& v) { v["base"]["delta"][0]["path"] = "state"; }).contains("base.delta[0].path"));
	CHECK(with([](json& v) {
		      v["base"]["delta"][0]["path"] = "/nowhere/at/all";
	      }).contains("base.delta: doesn't apply to the document"));
	// the base the delta gives must be the one the tree names
	CHECK(with([](json& v) {
		      v["base"]["delta"][0]["value"] = "Other notes";
	      }).contains("the file doesn't hold the base it names"));
	CHECK(with([](json& v) {
		      v["document"]["root"]["id"] = "heading";
	      }).contains("the file doesn't hold the base it names"));
	// files belong to a whole app only, which must have them
	CHECK(with([](json& v) { v["base"]["files"] = json::object(); }).contains("base.files: only a package of kind"));
	CHECK(with([](json& v) { v["kind"] = "app"; }).contains("base.files: a package of kind \"app\" holds"));
	// state: keys the base hasn't
	CHECK(with([](json& v) { v["state"] = {{"title", "x"}}; }).contains("state.title: the base has this key"));
	CHECK(with([](json& v) { v["state"] = json::array(); }).contains("state: expected"));
	// proof and signature
	CHECK(with([](json& v) { v["proof"] = {{"scenarios", 1}}; }).contains("proof: expected"));
	const std::string publisher = format::SigningKey::generate().publisher();
	CHECK(with([&publisher](json& v) {
		      v["signature"] = {{"publisher", publisher}, {"value", std::string(86, 'B')}};
	      }).contains("signature: doesn't verify"));
	CHECK(with([&publisher](json& v) {
		      v["signature"] = {{"publisher", publisher}};
	      }).contains("signature: doesn't verify"));
}

TEST_CASE("packages: a whole app's files are checked: inside its folder, text or data, with its own manifest") {
	const auto with = [](auto change) {
		json value = appPackage();
		change(value);
		return refusal(value);
	};
	CHECK(with([](json&) {}).empty());
	CHECK(with([](json& v) {
		      v["base"]["files"]["../outside.json"] = {{"text", ""}};
	      }).contains("base.files.../outside.json: not a file inside the app's folder"));
	CHECK(with([](json& v) {
		      v["base"]["files"]["/etc/hosts"] = {{"text", ""}};
	      }).contains("base.files./etc/hosts: not a file inside"));
	CHECK(with([](json& v) {
		      v["base"]["files"]["seed/tasks.csv"] = {{"text", "a"}, {"data", "YQ=="}};
	      }).contains("AffrescoUI package: base.files.seed/tasks.csv: expected {\"text\"} or {\"data\"}"));
	CHECK(with([](json& v) {
		      v["base"]["files"]["seed/tasks.csv"] = {{"data", "not base64!"}};
	      }).contains("base.files.seed/tasks.csv.data: not base64"));
	CHECK(with([](json& v) { v["base"]["files"].erase("app.json"); }).contains("no app.json or app.lisp"));
	CHECK(
	    with([](json& v) {
		    v["base"]["files"]["app.json"]["text"] =
		        R"({ "format": "affrescoui-app", "version": 1, "id": "other", "name": "Other", "entry": "index.screen.lisp" })";
	    }).contains("base.files.app.json: the app's id is 'other', not the base's 'notes'"));
	CHECK(with([](json& v) {
		      v["base"]["files"]["app.json"]["text"] = "{ nope";
	      }).contains("base.files.app.json: not JSON"));
	CHECK(with([](json& v) {
		      v["base"]["files"].erase("index.screen.lisp");
	      }).contains("its entry 'index.screen.lisp' isn't among the files"));
}

TEST_CASE("packages: an asset is one the document lists, with its bytes' size and SHA-256") {
	json value = changesPackage();
	addAsset(value, "dot.svg", std::string(dot));
	CHECK(refusal(value).empty());
	json unlisted = value;
	unlisted["document"]["assets"].erase("dot.svg");
	unlisted["base"]["delta"].back() = {{"op", "remove"}, {"path", "/assets"}};
	CHECK(refusal(unlisted).contains("assets.dot.svg: the document's \"assets\" doesn't list it"));
	json other = value;
	other["assets"]["dot.svg"]["text"] = std::string(dot) + " ";
	CHECK(refusal(other).contains("assets.dot.svg: not the bytes the document's \"assets\" lists"));
	for(const char* field : {"sha256", "type"}) { // a listing of another type is a mismatch, not an error
		CAPTURE(field);
		json numbered = value;
		numbered["document"]["assets"]["dot.svg"][field] = 7;
		CHECK(refusal(numbered).contains("assets.dot.svg: not the bytes the document's \"assets\" lists"));
	}
	json badName = value;
	badName["assets"]["Dot.svg"] = badName["assets"]["dot.svg"];
	CHECK(refusal(badName).contains("assets.Dot.svg:"));
	json badType = value;
	badType["assets"]["dot.svg"]["type"] = "application/x-sh";
	CHECK(refusal(badType).contains("assets.dot.svg.type:"));
}

TEST_CASE("packages: a signed package verifies, and one changed after it was signed doesn't") {
	const format::SigningKey key = format::SigningKey::generate();
	const json signedValue = key.sign(changesPackage());
	const format::Package package = format::parsePackage(signedValue);
	CHECK(package.signature["publisher"] == key.publisher());
	CHECK(format::packageJson(package) == signedValue);
	json changed = signedValue;
	changed["name"] = "Flow with a Waiting column";
	CHECK(refusal(changed).contains("signature: doesn't verify"));
}

TEST_CASE("packages: each limit at its edge") {
	json value = changesPackage();
	value["name"] = std::string(format::maxPackageNameLength, 'n');
	CHECK(refusal(value).empty());
	value["name"] = std::string(format::maxPackageNameLength + 1, 'n');
	CHECK(refusal(value).contains("name: longer than 80 characters"));
	value["name"] = [] { // characters, not bytes: 80 two-byte ones
		std::string text;
		for(std::size_t i = 0; i < format::maxPackageNameLength; ++i) text += "\xc3\xa8";
		return text;
	}();
	CHECK(refusal(value).empty());
	value = changesPackage();
	value["description"] = std::string(format::maxPackageDescriptionLength, 'd');
	CHECK(refusal(value).empty());
	value["description"] = std::string(format::maxPackageDescriptionLength + 1, 'd');
	CHECK(refusal(value).contains("description: longer than 2000 characters"));

	// files: at most maxPackageFiles, each at most maxPackageFileBytes
	json app = appPackage();
	for(std::size_t i = app["base"]["files"].size(); i < format::maxPackageFiles; ++i)
		app["base"]["files"]["seed/" + std::to_string(i) + ".csv"] = {{"text", ""}};
	CHECK(refusal(app).empty());
	app["base"]["files"]["seed/one-more.csv"] = {{"text", ""}};
	CHECK(refusal(app).contains("base.files: more than 1024 files"));
	app = appPackage();
	app["base"]["files"]["seed/big.csv"] = {{"text", std::string(format::maxPackageFileBytes, 'x')}};
	CHECK(refusal(app).empty());
	app["base"]["files"]["seed/big.csv"] = {{"text", std::string(format::maxPackageFileBytes + 1, 'x')}};
	CHECK(refusal(app).contains("base.files.seed/big.csv: larger than 4 MiB"));
	app["base"]["files"]["seed/big.csv"] = {
	    {"data", affresco::encodeBase64(std::string(format::maxPackageFileBytes + 1, 'x'))}};
	CHECK(refusal(app).contains("base.files.seed/big.csv.data: not base64, or larger than 4 MiB"));

	// nesting: the document's, as a screen's
	value = changesPackage();
	json deep = 1;
	for(int i = 0; i < affresco::maxDocumentNesting; ++i) deep = json::array({deep});
	value["document"]["state"]["deep"] = deep;
	CHECK(refusal(value).contains("document: nested more than 200 levels"));

	// the file: at most maxPackageBytes, refused before it is parsed
	std::string text = changesPackage().dump();
	text.append(format::maxPackageBytes - text.size(), ' ');
	CHECK_NOTHROW(static_cast<void>(format::readPackage(text)));
	text += ' ';
	CHECK_THROWS_WITH_AS(static_cast<void>(format::readPackage(text)), doctest::Contains("file: larger than 16 MiB"),
	                     affresco::DocumentError);
	CHECK_THROWS_AS(static_cast<void>(format::readPackage(std::string(20, '{'))), json::parse_error);
}

TEST_CASE("packages: the summary names its parts, how its base compares, and how its signature stands") {
	json value = changesPackage();
	value["state"] = {{"blockedReason", "waiting"}};
	addAsset(value, "dot.svg", std::string(dot));
	const format::Package package = format::parsePackage(value);
	const json base = format::baseOf(package);

	json summary = format::packageSummary(package, json(), json::object());
	CHECK(summary["kind"] == "changes");
	CHECK(summary["name"] == "Flow with a Blocked column");
	CHECK(summary["release"] == "1.1.0");
	CHECK(summary["author"] == "Ada");
	CHECK(summary["engine"] == "1.0.0");
	CHECK(summary["base"] == json{{"app", "notes"},
	                              {"name", "Notes"},
	                              {"release", "1.0.0"},
	                              {"exact", nullptr},
	                              {"installedRelease", nullptr}});
	CHECK(summary["bytes"] == format::packageJson(package).dump().size());
	CHECK(summary["files"] == 0);
	CHECK(summary["assets"] == json::array({{{"name", "dot.svg"}, {"type", "image/svg+xml"}, {"size", dot.size()}}}));
	CHECK(summary["state"] == json{{"blockedReason", "waiting"}});
	CHECK(summary["definitions"]["added"] == json::array({"app.lisp/blocked"}));
	CHECK(summary["definitions"]["changed"] == json::array({"app.lisp/count"}));
	CHECK(summary["definitions"]["removed"] == json::array({"app.lisp/old"}));
	CHECK(summary["signature"] == "none");

	CHECK(format::packageSummary(package, base, json::object())["base"]["exact"] == true);
	json other = base;
	other["state"]["title"] = "My notes";
	CHECK(format::packageSummary(package, other, json::object())["base"]["exact"] == false);
	CHECK(format::packageSummary(format::parsePackage(appPackage()), json(), json::object())["files"] == 3);

	const format::SigningKey key = format::SigningKey::generate();
	const format::Package signedPackage = format::parsePackage(key.sign(value));
	CHECK(format::packageSummary(signedPackage, json(), {{key.publisher(), "Ada's studio"}})["signature"] ==
	      "trusted:Ada's studio");
	const std::string unknown = format::packageSummary(signedPackage, json(), json::object())["signature"];
	CHECK(unknown == "unknown:" + key.publisher().substr(8, 16));
}
