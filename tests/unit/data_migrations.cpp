/// @file
/// @brief Migrations, property style: random records of a model, a migration and its inverse round-trip; a record
/// that fails leaves the store untouched; a destructive change archives what it drops and a revert brings it back;
/// a field the models stop declaring keeps its values; compareModels classifies changes like a schema registry.

#include <affresco/data/Changes.hpp>
#include <affresco/data/Store.hpp>

#include <doctest/doctest.h>

#include <random>
#include <string>

using affresco::json;
using affresco::data::ChangeClass;
using affresco::data::DataError;
using affresco::data::Store;

namespace {

/// @brief The expressions these tests' migrations use, evaluated without the screen format.
/// @param expression The expression.
/// @param variables `value` and `record`.
/// @return Its value.
json evaluate(const std::string& expression, const json& variables) {
	const json& value = variables["value"];
	if(expression == "{{value * 1000}}") return value.get<std::int64_t>() * 1000;
	if(expression == "{{value / 1000}}") return value.get<std::int64_t>() / 1000;
	if(expression == "{{number(value)}}") return std::stoll(value.get<std::string>());
	if(expression == "{{text(value)}}") return std::to_string(value.get<std::int64_t>());
	if(expression == "{{fail}}") throw std::runtime_error("no such thing");
	return nullptr;
}

/// @brief Version 1 of a generated songs model: seconds and an artist.
const json& version1() {
	static const json value = json::parse(R"json({ "songs": { "version": 1, "fields": {
	"title": { "type": "string", "required": true },
	"artist": { "type": "string" },
	"duration": { "type": "integer", "minimum": 0 },
	"legacy": { "type": "string" }
} } })json");
	return value;
}

/// @brief Version 2: "artist" renamed "by", "duration" in milliseconds, with its inverse.
const json& version2() {
	static const json value = json::parse(R"json({ "songs": { "version": 2, "fields": {
	"title": { "type": "string", "required": true },
	"by": { "type": "string" },
	"duration": { "type": "integer", "minimum": 0 },
	"legacy": { "type": "string" }
}, "migrations": [{ "from": 1, "to": 2, "steps": [
	{ "rename": "artist", "to": "by" },
	{ "convert": "duration", "with": "{{value * 1000}}", "inverse": "{{value / 1000}}" }
] }] } })json");
	return value;
}

/// @brief A store at version 1 with random records.
/// @param store The store.
/// @param seed The random seed.
/// @return The records as stored.
json randomSongs(Store& store, unsigned seed) {
	store.setEvaluator(evaluate);
	(void)store.follow(version1(), "test");
	std::mt19937 random(seed);
	std::uniform_int_distribution<int> pick(0, 1000);
	for(int i = 0; i < 50; ++i) {
		json fields = {{"title", "song " + std::to_string(pick(random))}};
		if(pick(random) % 3) fields["artist"] = "artist " + std::to_string(pick(random) % 7);
		if(pick(random) % 4) fields["duration"] = pick(random);
		if(pick(random) % 2) fields["legacy"] = "x";
		(void)store.insert("songs", fields, "app");
	}
	return store.run({{"from", "songs"}}, "q");
}

/// @brief The records without their times, which a migration changes.
/// @param records The records.
/// @return They.
json withoutTimes(json records) {
	for(json& record : records) {
		record.erase("updated");
		record.erase("created");
	}
	return records;
}

} // namespace

TEST_CASE("migrations: random records migrate and the inverse round-trips them") {
	for(unsigned seed = 1; seed <= 8; ++seed) {
		Store store;
		const json before = randomSongs(store, seed);
		const auto planned = store.plan(version2());
		CHECK(planned.ok());
		CHECK(store.run({{"from", "songs"}}, "q") == before); // the plan changed nothing here
		const auto report = store.follow(version2(), "agent/test");
		REQUIRE(report.collections.size() == 1);
		CHECK(report.collections[0].changed == planned.collections[0].changed);
		for(const json& song : store.run({{"from", "songs"}}, "q")) {
			CHECK_FALSE(song.contains("artist"));
			const auto old = std::ranges::find_if(before, [&](const json& b) { return b["id"] == song["id"]; });
			REQUIRE(old != before.end());
			if(old->contains("artist")) CHECK(song["by"] == (*old)["artist"]);
			if(old->contains("duration")) CHECK(song["duration"] == (*old)["duration"].get<int>() * 1000);
		}
		(void)store.follow(version1(), "person"); // the revert: the inverse runs
		CHECK(withoutTimes(store.run({{"from", "songs"}}, "q")) == withoutTimes(before));
	}
}

TEST_CASE("migrations: a record that fails leaves the store untouched") {
	Store store;
	const json before = randomSongs(store, 3);
	(void)store.insert("songs", {{"title", "huge"}, {"duration", 9007199254740}}, "app");
	json narrow = version2(); // a maximum the huge one breaks once converted
	narrow["songs"]["fields"]["duration"]["maximum"] = 1000000000;
	const auto planned = store.plan(narrow);
	CHECK_FALSE(planned.ok());
	CHECK(planned.collections[0].failed == 1);
	CHECK(planned.text().contains("fail"));
	CHECK_THROWS_AS((void)store.follow(narrow, "agent/test"), DataError);
	json now = store.run({{"from", "songs"}}, "q");
	now.erase(now.end() - 1);
	CHECK(now == before);
	CHECK(store.models().at("songs").version == 1);
	(void)store.insert("songs", {{"title", "still at 1"}, {"artist", "a"}}, "app"); // the old model still holds

	json broken = version2(); // an expression that fails
	broken["songs"]["migrations"][0]["steps"][1]["with"] = "{{fail}}";
	CHECK_THROWS_AS((void)store.follow(broken, "agent/test"), DataError);
	CHECK(store.models().at("songs").version == 1);
}

TEST_CASE("migrations: a drop archives the values, and the revert brings them back") {
	Store store;
	const json before = randomSongs(store, 5);
	json dropped = version1();
	dropped["songs"]["version"] = 2;
	dropped["songs"]["fields"].erase("legacy");
	dropped["songs"]["migrations"] = json::parse(R"([{ "from": 1, "to": 2, "steps": [{ "drop": "legacy" }] }])");
	const auto diff =
	    affresco::data::compareModels(affresco::data::parseModels(version1()), affresco::data::parseModels(dropped));
	CHECK(diff.worst() == ChangeClass::Destructive);
	const auto report = store.follow(dropped, "person");
	CHECK(report.collections[0].destructive);
	const std::size_t holding = static_cast<std::size_t>(
	    std::ranges::count_if(before, [](const json& song) { return song.contains("legacy"); }));
	CHECK(report.collections[0].archived == holding);
	CHECK(store.archived("songs").size() == holding);
	for(const json& song : store.run({{"from", "songs"}}, "q")) CHECK_FALSE(song.contains("legacy"));
	(void)store.follow(version1(), "person");
	CHECK(withoutTimes(store.run({{"from", "songs"}}, "q")) == withoutTimes(before));
}

TEST_CASE("migrations: without an inverse, the revert restores the archive") {
	Store store;
	const json before = randomSongs(store, 7);
	json textual = version1(); // durations become texts, with no way back written
	textual["songs"]["version"] = 2;
	textual["songs"]["fields"]["duration"] = {{"type", "string"}};
	textual["songs"]["migrations"] =
	    json::parse(R"([{ "from": 1, "to": 2, "steps": [{ "convert": "duration", "with": "{{text(value)}}" }] }])");
	(void)store.follow(textual, "person");
	(void)store.insert("songs", {{"title", "added since"}, {"duration", "12"}}, "app"); // fits only the new model
	const auto report = store.follow(version1(), "person");
	CHECK(report.collections[0].restoredArchive);
	CHECK(report.collections[0].archived == 1); // the one added since: kept in the archive, restorable
	CHECK(withoutTimes(store.run({{"from", "songs"}}, "q")) == withoutTimes(before));
	CHECK(store.archived("songs")[0].reason == "revert");
}

TEST_CASE("migrations: a field the models stop declaring keeps its values") {
	Store store;
	const json before = randomSongs(store, 9);
	json hidden = version1();
	hidden["songs"]["fields"].erase("legacy");
	(void)store.follow(hidden, "person");
	for(const json& song : store.run({{"from", "songs"}}, "q")) CHECK_FALSE(song.contains("legacy"));
	CHECK_THROWS_AS((void)store.insert("songs", {{"title", "t"}, {"legacy", "y"}}, "app"), DataError);
	(void)store.follow(version1(), "person");
	CHECK(withoutTimes(store.run({{"from", "songs"}}, "q")) == withoutTimes(before));
}

TEST_CASE("migrations: changes are classified compatible, transforming or destructive") {
	using affresco::data::parseModels;
	const auto classOf = [](const json& before, const json& after) {
		return affresco::data::compareModels(parseModels(before), parseModels(after)).worst();
	};
	json added = version1();
	added["songs"]["fields"]["year"] = {{"type", "integer"}};
	CHECK(classOf(version1(), added) == ChangeClass::Compatible);
	added["songs"]["fields"]["year"]["required"] = true;
	CHECK(classOf(version1(), added) == ChangeClass::Transforming);
	added["songs"]["fields"]["year"]["default"] = 2000;
	CHECK(classOf(version1(), added) == ChangeClass::Compatible);
	json retyped = version1();
	retyped["songs"]["fields"]["duration"] = {{"type", "number"}};
	CHECK(classOf(version1(), retyped) == ChangeClass::Compatible); // integer to number widens
	retyped["songs"]["fields"]["duration"] = {{"type", "string"}};
	CHECK(classOf(version1(), retyped) == ChangeClass::Transforming);
	json narrowed = version1();
	narrowed["songs"]["fields"]["duration"]["maximum"] = 10;
	CHECK(classOf(version1(), narrowed) == ChangeClass::Destructive);
	CHECK(classOf(version1(), version2()) == ChangeClass::Transforming); // the rename is explained by its migration
	json gone = version1();
	gone.erase("songs");
	gone["albums"] = {{"fields", {{"name", {{"type", "string"}}}}}};
	CHECK(classOf(version1(), gone) == ChangeClass::Destructive);
}

TEST_CASE("migrations: malformed models and migrations are refused with their place") {
	using affresco::data::parseModels;
	CHECK_THROWS_WITH_AS((void)parseModels(json::parse(R"({"a": {"fields": {"x": {"type": "colour"}}}})")),
	                     doctest::Contains("models.a.fields.x.type"), affresco::DocumentError);
	CHECK_THROWS_AS((void)parseModels(json::parse(R"({"a": {"fields": {"id": {"type": "string"}}}})")),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)parseModels(json::parse(R"({"a": {"fields": {"r": {"type": "reference", "to": "b"}}}})")),
	                affresco::DocumentError);
	Store store;
	json bad = version2();
	bad["songs"]["migrations"][0]["steps"][0] = {{"rename", "artist"}}; // no "to"
	CHECK_THROWS_AS((void)store.follow(bad, "test"), affresco::DocumentError);
}
