/// @file
/// @brief Unit tests for catalogue v0 (RFC 0004, "The catalogue"): a signed feature installs and upgrades from a local
/// catalogue, and a tampered one is refused with the reason. The keys are made when the tests run, the catalogue in a
/// temporary folder.

#include "FeatureSignatures.hpp"

#include <affresco/format/Features.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <string_view>

using affresco::json;
using affresco::RevisionStatus;
using affresco::format::SigningKey;

namespace {

/// @brief Forge's example catalogue (examples/forge/catalog), signed by a key whose private half was discarded.
/// @return Its folder.
std::filesystem::path exampleCatalog() {
	return std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "forge" / "catalog";
}

/// @brief The example catalogue's publisher, as Forge trusts it (Forge.hpp).
constexpr std::string_view examplePublisher = "ed25519:wvzomDMyk9aPCGQCvTzNPugq4saaDmJdO_8LaeLFYQc";

/// @brief Forge's word count package, unsigned.
/// @return Its manifest.
json wordCount() {
	std::ifstream file(std::string(AFFRESCOUI_EXAMPLES_DIRECTORY) + "/forge/features/word-count/feature.json");
	REQUIRE(file);
	json manifest = json::parse(file);
	manifest.erase("signature");
	return manifest;
}

/// @brief An app with the status bar and the export word count needs.
/// @return The document.
json app() {
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

/// @brief Writes a file.
/// @param file The file.
/// @param text Its bytes.
void write(const std::filesystem::path& file, const std::string& text) {
	std::filesystem::create_directories(file.parent_path());
	std::ofstream out(file, std::ios::binary);
	out << text;
	REQUIRE(out);
}

/// @brief A local catalogue in a temporary folder, removed with it.
struct Catalog {
	std::filesystem::path folder; ///< Its folder.
	json index;                   ///< Its index, unsigned.

	/// @brief Makes an empty catalogue.
	Catalog() {
		std::random_device random;
		folder = std::filesystem::temp_directory_path() / ("affrescoui-catalog-" + std::to_string(random()));
		std::filesystem::create_directories(folder);
		index = {{"format", "affrescoui-catalog"},
		         {"version", 2},
		         {"name", "test"},
		         {"publishers", json::object()},
		         {"features", json::object()}};
	}
	~Catalog() {
		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}
	Catalog(const Catalog&) = delete;
	Catalog& operator=(const Catalog&) = delete;

	/// @brief Adds a release: its manifest signed and written, listed in the index with its digest and size.
	/// @param manifest The manifest.
	/// @param key Who signs it.
	/// @return The manifest's file.
	std::filesystem::path add(json manifest, const SigningKey& key) {
		const std::string name = manifest["name"], release = manifest["release"];
		const std::string url = name + "/" + release + ".json";
		const std::string bytes = key.sign(std::move(manifest)).dump(1, '\t');
		write(folder / url, bytes);
		index["publishers"][key.publisher()] = {{"name", "Test publisher"}};
		index["features"][name]["title"] = "Word count";
		index["features"][name]["releases"].push_back({{"release", release},
		                                               {"url", url},
		                                               {"digest", affresco::format::digestOf(bytes)},
		                                               {"publisher", key.publisher()},
		                                               {"size", bytes.size()}});
		return folder / url;
	}

	/// @brief Signs the index and writes it.
	/// @param key Who signs it.
	/// @return The index's file.
	std::filesystem::path publish(const SigningKey& key) const {
		write(folder / "index.json", key.sign(index).dump(1, '\t'));
		return folder / "index.json";
	}
};

/// @brief The reason something is refused, or "" if it isn't.
/// @param run What to run.
/// @return The reason.
template <typename Run>
std::string refusal(Run run) {
	try {
		run();
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return {};
}

/// @brief Replaces one byte of a file, keeping its size.
/// @param file The file.
/// @param from The text to find.
/// @param to What replaces it, as long.
void tamper(const std::filesystem::path& file, const std::string& from, const std::string& to) {
	std::ifstream in(file, std::ios::binary);
	std::string bytes((std::istreambuf_iterator<char>(in)), {});
	in.close();
	const std::size_t at = bytes.find(from);
	REQUIRE(at != std::string::npos);
	bytes.replace(at, from.size(), to);
	write(file, bytes);
}

} // namespace

TEST_CASE("catalog: a signed feature installs from a local catalogue, and upgrades") {
	const SigningKey publisher = SigningKey::generate();
	Catalog catalog;
	catalog.add(wordCount(), publisher);
	affresco::format::Features features;
	features.trustPublisher(publisher.publisher());
	CHECK(features.addCatalog(catalog.publish(publisher)) == "test");

	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, app());
	const json row = features.catalog(repository.document())[0];
	CHECK(row["name"] == "wordCount");
	CHECK(row["version"] == "1.0.0");
	CHECK(row["publisher"] == "Test publisher");
	CHECK(row["installed"] == false);

	const json install = features.installRevision(repository.document(), "wordCount", "", "features");
	const affresco::RevisionResult installed = repository.submit(install);
	REQUIRE_MESSAGE(installed.status == RevisionStatus::Applied, installed.error);
	const json& record = repository.document()["features"]["wordCount"];
	CHECK(record["source"] == "catalog:test");
	CHECK(record["publisher"] == publisher.publisher());
	CHECK(record["digest"].get<std::string>().starts_with("sha512:"));
	CHECK(features.catalog(repository.document())[0]["installed"] == true);
	CHECK(refusal([&] { (void)features.installRevision(repository.document(), "wordCount", "", "features"); })
	          .find("installed already") != std::string::npos);
	CHECK(refusal([&] { (void)features.upgradeRevision(repository.document(), "wordCount", "features"); })
	          .find("up to date") != std::string::npos);

	// Disabled, then upgraded: it stays disabled
	REQUIRE(repository.submit({{"author", "studio"}, {"set", {{"/features/wordCount/enabled", false}}}}).status ==
	        RevisionStatus::Applied);
	json newer = wordCount();
	newer["release"] = "1.1.0";
	catalog.add(newer, publisher);
	features.addCatalog(catalog.publish(publisher));
	const affresco::RevisionResult upgraded =
	    repository.submit(features.upgradeRevision(repository.document(), "wordCount", "features"));
	REQUIRE_MESSAGE(upgraded.status == RevisionStatus::Applied, upgraded.error);
	CHECK(repository.document()["features"]["wordCount"]["release"] == "1.1.0");
	CHECK(repository.document()["features"]["wordCount"]["enabled"] == false);
}

TEST_CASE("catalog: an install goes after the last feature, whatever order a peer wrote") {
	const SigningKey publisher = SigningKey::generate();
	Catalog catalog;
	catalog.add(wordCount(), publisher);
	affresco::format::Features features;
	features.trustPublisher(publisher.publisher());
	features.addCatalog(catalog.publish(publisher));
	for(const json& order : {json(std::numeric_limits<int>::max()), json(std::numeric_limits<std::int64_t>::max()),
	                         json("first"), json(-5)}) {
		INFO(order.dump());
		json document = app();
		document["features"]["other"] = {{"order", order}};
		const json install = features.installRevision(document, "wordCount", "", "features");
		const json& record = install["set"]["/features/wordCount"];
		CHECK(record["order"].get<int>() >= 10); // after the last one, never wrapped around (an overflow once)
	}
}

TEST_CASE("catalog: a tampered package is refused with the reason") {
	const SigningKey publisher = SigningKey::generate();
	Catalog catalog;
	const std::filesystem::path manifest = catalog.add(wordCount(), publisher);
	affresco::format::Features features;
	features.trustPublisher(publisher.publisher());
	const std::filesystem::path index = catalog.publish(publisher);
	const json document = app();
	const auto install = [&] { (void)features.installRevision(document, "wordCount", "", "features"); };

	SUBCASE("its manifest changed, its size kept") {
		features.addCatalog(index);
		tamper(manifest, "\"Word count\"", "\"Word_count\"");
		CHECK(refusal(install).find("its digest isn't the catalogue's (the package was changed)") != std::string::npos);
	}
	SUBCASE("its manifest grew") {
		features.addCatalog(index);
		tamper(manifest, "\"Word count\"", "\"Word counts\"");
		CHECK(refusal(install).find("its size isn't the catalogue's") != std::string::npos);
	}
	SUBCASE("its manifest re-signed by another key, the index's digests updated by the same") {
		const SigningKey stranger = SigningKey::generate();
		json changed = wordCount();
		changed["description"] = "Something else";
		catalog.index["features"].clear();
		catalog.add(changed, stranger);
		features.addCatalog(catalog.publish(publisher));
		CHECK(refusal(install).find("which isn't a trusted publisher") != std::string::npos);
	}
	SUBCASE("its manifest signed by a trusted key, but not the one the index names") {
		const SigningKey other = SigningKey::generate();
		features.trustPublisher(other.publisher());
		const std::string bytes = other.sign(wordCount()).dump(1, '\t');
		write(manifest, bytes);
		catalog.index["features"]["wordCount"]["releases"][0]["digest"] = affresco::format::digestOf(bytes);
		catalog.index["features"]["wordCount"]["releases"][0]["size"] = bytes.size();
		features.addCatalog(catalog.publish(publisher));
		CHECK(refusal(install).find("not by the publisher the catalogue names") != std::string::npos);
	}
	SUBCASE("the index changed after signing") {
		tamper(index, "\"test\"", "\"tset\"");
		CHECK(refusal([&] { (void)features.addCatalog(index); }).find("it was changed after signing") !=
		      std::string::npos);
	}
	SUBCASE("the index signed by a key the host doesn't trust") {
		const std::filesystem::path forged = catalog.publish(SigningKey::generate());
		CHECK(refusal([&] { (void)features.addCatalog(forged); }).find("which isn't a trusted publisher") !=
		      std::string::npos);
	}
	SUBCASE("a URL out of the catalogue's folder") {
		catalog.index["features"]["wordCount"]["releases"][0]["url"] = "../index.json";
		CHECK(refusal([&] { (void)features.addCatalog(catalog.publish(publisher)); })
		          .find("expected a path inside the catalogue's folder") != std::string::npos);
	}
	SUBCASE("nothing trusted") {
		affresco::format::Features untrusting;
		CHECK(refusal([&] { (void)untrusting.addCatalog(index); }).find("which isn't a trusted publisher") !=
		      std::string::npos);
	}
}

TEST_CASE("catalog: an upgrade never changes the publisher") {
	const SigningKey first = SigningKey::generate(), second = SigningKey::generate();
	Catalog catalog;
	catalog.add(wordCount(), first);
	affresco::format::Features features;
	features.trustPublisher(first.publisher());
	features.trustPublisher(second.publisher());
	features.addCatalog(catalog.publish(first));
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, app());
	REQUIRE(repository.submit(features.installRevision(repository.document(), "wordCount", "", "features")).status ==
	        RevisionStatus::Applied);
	json newer = wordCount();
	newer["release"] = "2.0.0";
	catalog.add(newer, second);
	features.addCatalog(catalog.publish(first));
	CHECK(refusal([&] { (void)features.upgradeRevision(repository.document(), "wordCount", "features"); })
	          .find("another publisher") != std::string::npos);
}

TEST_CASE("catalog: an oversized index is refused before it is read") {
	const SigningKey publisher = SigningKey::generate();
	Catalog catalog;
	catalog.index["padding"] = std::string(9u << 20, ' ');
	affresco::format::Features features;
	features.trustPublisher(publisher.publisher());
	CHECK(refusal([&] { (void)features.addCatalog(catalog.publish(publisher)); }).find("larger than") !=
	      std::string::npos);
}

TEST_CASE("catalog: Forge's example catalogue verifies with its public key, and installs") {
	affresco::format::Features features;
	const auto add = [&] { (void)features.addCatalog(exampleCatalog() / "index.json"); };
	CHECK(refusal(add).find("which isn't a trusted publisher") != std::string::npos);
	features.trustPublisher(examplePublisher);
	CHECK(features.addCatalog(exampleCatalog() / "index.json") == "forge-examples");
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, app());
	const json rows = features.catalog(repository.document());
	REQUIRE(rows.size() == 2);
	CHECK(rows[1]["name"] == "wordCount");
	CHECK(rows[1]["publisher"] == "AffrescoUI examples");
	const affresco::RevisionResult installed =
	    repository.submit(features.installRevision(repository.document(), "wordCount", "", "features"));
	REQUIRE_MESSAGE(installed.status == RevisionStatus::Applied, installed.error);
	CHECK(repository.document()["features"]["wordCount"]["source"] == "catalog:forge-examples");
}

TEST_CASE("catalog: a tampered copy of Forge's example catalogue is refused") {
	Catalog copy;
	std::filesystem::copy(exampleCatalog(), copy.folder, std::filesystem::copy_options::recursive);
	affresco::format::Features features;
	features.trustPublisher(examplePublisher);
	const json document = app();

	SUBCASE("a manifest") {
		features.addCatalog(copy.folder / "index.json");
		tamper(copy.folder / "wordCount" / "1.0.0.json", "\"Word count\"", "\"Word_count\"");
		const auto install = [&] { (void)features.installRevision(document, "wordCount", "", "features"); };
		CHECK(refusal(install).find("its digest isn't the catalogue's") != std::string::npos);
	}
	SUBCASE("the index") {
		tamper(copy.folder / "index.json", "\"1.2.0\"", "\"1.2.1\"");
		const auto add = [&] { (void)features.addCatalog(copy.folder / "index.json"); };
		CHECK(refusal(add).find("it was changed after signing") != std::string::npos);
	}
}
