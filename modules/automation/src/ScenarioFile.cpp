/// @file
/// @brief ScenarioFile: reads a scenarios file and checks every scenario and step before anything is played, so a
/// typo is reported with its place instead of half-way through a run.

#include "affresco/automation/Scenarios.hpp"

#include "Internal.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/format/Migrate.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <initializer_list>
#include <set>
#include <sstream>

namespace affresco::automation {

namespace {

using detail::fail;

/// @brief Refuses keys a value doesn't take.
/// @param value An object.
/// @param allowed The keys it may have.
/// @param place Its place.
/// @throws DocumentError If it has another.
void onlyKeys(const json& value, std::initializer_list<std::string_view> allowed, const std::string& place) {
	for(const auto& [key, member] : value.items())
		if(std::ranges::find(allowed, key) == allowed.end())
			fail(place.empty() ? key : std::format("{}.{}", place, key), "unknown key here");
}

/// @brief Checks a node found by what it is to a person: { "role", "name"? }.
/// @param target The object.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkByRole(const json& target, const std::string& place) {
	onlyKeys(target, {"role", "name"}, place);
	if(!target.contains("role") || !target["role"].is_string() || target["role"].get<std::string>().empty())
		fail(place + ".role", R"(expected a role: "button", "checkbox", "textbox", ...)");
	if(target.contains("name") && !target["name"].is_string()) fail(place + ".name", "expected a string");
}

/// @brief Checks a target: [x, y] in window units, "#id", or { "role", "name"? }.
/// @param target The target.
/// @param place Its place.
/// @throws DocumentError If it is none of them.
void checkTarget(const json& target, const std::string& place) {
	if(target.is_array() && target.size() == 2 && target[0].is_number() && target[1].is_number()) return;
	if(target.is_string() && target.get<std::string>().size() > 1 && target.get<std::string>().front() == '#') return;
	if(target.is_object()) return checkByRole(target, place);
	fail(place, R"(expected [x, y] in window units, "#id", or {"role": ..., "name": ...})");
}

/// @brief Checks "nth", the match a "#id" picks among several (0, the first, by default).
/// @param step The step or expectation.
/// @param place Its place.
/// @throws DocumentError If it isn't a whole number from 0.
void checkNth(const json& step, const std::string& place) {
	if(step.contains("nth") && (!step["nth"].is_number_integer() || step["nth"].get<long long>() < 0 ||
	                            step["nth"].get<long long>() > 1000000))
		fail(place + ".nth", "expected a whole number from 0");
}

/// @brief Checks a name that becomes part of a file's name (a capture, an image).
/// @param value The name.
/// @param place Its place.
/// @throws DocumentError If it can't be part of a file's name.
void checkFileName(const json& value, const std::string& place) {
	if(!value.is_string() || !detail::isSafeFileName(value.get<std::string>()))
		fail(place, "expected a name of letters, digits, '-', '_' and '.'");
}

/// @brief Checks which node an expectation is about.
/// @param node "#id", or {"role": ..., "name": ...}.
/// @param place Its place.
/// @throws DocumentError If it is neither.
void checkNodeName(const json& node, const std::string& place) {
	if(node.is_object()) checkByRole(node, place);
	else if(!node.is_string() || node.get<std::string>().size() < 2 || node.get<std::string>().front() != '#')
		fail(place, R"(expected "#id", or {"role": ..., "name": ...})");
}

/// @brief Checks what an expectation of a node expects: `exists`, `text`, or `property` with `is`.
/// @param expect The expectation.
/// @param place Its place.
/// @throws DocumentError If it expects none of them, two, or a malformed one.
void checkNodeClaim(const json& expect, const std::string& place) {
	const int what = static_cast<int>(expect.contains("exists")) + static_cast<int>(expect.contains("text")) +
	                 static_cast<int>(expect.contains("property"));
	if(what != 1) fail(place, R"(expected one of "exists", "text", or "property" with "is")");
	if(expect.contains("exists") && !expect["exists"].is_boolean()) fail(place + ".exists", "expected true or false");
	if(expect.contains("text") && !expect["text"].is_string()) fail(place + ".text", "expected a string");
	if(!expect.contains("property")) {
		if(expect.contains("is")) fail(place + ".is", R"(only "state" and "property" take "is")");
		return;
	}
	if(!expect["property"].is_string() || !detail::isReadableProperty(expect["property"].get<std::string>()))
		fail(place + ".property", "expected one of " + detail::readableProperties());
	if(!expect.contains("is")) fail(place, R"("property" needs "is": the value expected)");
}

/// @brief Checks the node part of an expectation: `node`, `nth`, then `exists`, `text`, or `property` with `is`.
/// @param expect The expectation.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkNodeExpectation(const json& expect, const std::string& place) {
	onlyKeys(expect, {"node", "nth", "exists", "text", "property", "is", "note"}, place);
	checkNodeName(expect["node"], place + ".node");
	checkNth(expect, place);
	checkNodeClaim(expect, place);
}

/// @brief Checks an expectation: of a state value, of a node, or of a picture.
/// @param expect The expectation.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkExpectation(const json& expect, const std::string& place) {
	if(!expect.is_object()) fail(place, R"(expected an object: {"state": ...}, {"node": ...} or {"image": ...})");
	const int subjects = static_cast<int>(expect.contains("state")) + static_cast<int>(expect.contains("node")) +
	                     static_cast<int>(expect.contains("image")) + static_cast<int>(expect.contains("data"));
	if(subjects != 1) fail(place, R"(expected exactly one of "state", "data", "node" or "image")");
	if(expect.contains("data")) {
		onlyKeys(expect, {"data", "is", "note"}, place);
		const std::string path = expect["data"].is_string() ? expect["data"].get<std::string>() : "";
		if(path.starts_with("data.") || !detail::parseStatePath(path))
			fail(place + ".data",
			     R"(expected a path into the records, without 'data.': "notes[0].title", "notes.length")");
		if(!expect.contains("is")) fail(place, R"("data" needs "is": the value expected)");
	} else if(expect.contains("node")) {
		checkNodeExpectation(expect, place);
	} else if(expect.contains("image")) {
		onlyKeys(expect, {"image", "note"}, place);
		checkFileName(expect["image"], place + ".image");
	} else {
		onlyKeys(expect, {"state", "is", "note"}, place);
		const std::string path = expect["state"].is_string() ? expect["state"].get<std::string>() : "";
		if(path.starts_with("state.") || path.starts_with("computed."))
			fail(place + ".state", R"(a path into the live state, without 'state.' ("cards.length"); computed values )"
			                       "aren't read: expect the text of a node that shows one");
		if(!detail::parseStatePath(path))
			fail(place + ".state", "expected a path into the state, as expressions write it without 'state.': "
			                       R"("cards[0].title", "cards.length")");
		if(!expect.contains("is")) fail(place, R"("state" needs "is": the value expected)");
	}
}

/// @brief Checks a pointer step: move, click, press or release, with a target.
/// @param step The step.
/// @param kind Its kind.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkPointer(const json& step, std::string_view kind, const std::string& place) {
	onlyKeys(step, {kind, "nth", "note"}, place);
	checkTarget(step[std::string(kind)], place + "." + std::string(kind));
}

/// @brief Checks a drag: from a target to another, in a number of moves.
/// @param step The step.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkDrag(const json& step, const std::string& place) {
	onlyKeys(step, {"drag", "to", "nth", "toNth", "moves", "note"}, place);
	checkTarget(step["drag"], place + ".drag");
	if(!step.contains("to")) fail(place, R"("drag" needs "to": where to drop)");
	checkTarget(step["to"], place + ".to");
	if(step.contains("toNth") && (!step["toNth"].is_number_integer() || step["toNth"].get<long long>() < 0 ||
	                              step["toNth"].get<long long>() > 1000000))
		fail(place + ".toNth", "expected a whole number from 0");
	if(step.contains("moves") && (!step["moves"].is_number_integer() || step["moves"].get<long long>() < 1 ||
	                              step["moves"].get<long long>() > detail::maxDragMoves))
		fail(place + ".moves", "expected a whole number from 1 to " + std::to_string(detail::maxDragMoves));
}

/// @brief Checks a scroll: a target, and dx and dy.
/// @param step The step.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkScroll(const json& step, const std::string& place) {
	onlyKeys(step, {"scroll", "dx", "dy", "nth", "note"}, place);
	checkTarget(step["scroll"], place + ".scroll");
	for(const char* axis : {"dx", "dy"})
		if(step.contains(axis) && !step[axis].is_number()) fail(place + "." + axis, "expected a number");
}

/// @brief Checks a resize: [width, height].
/// @param value The size.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkSize(const json& value, const std::string& place) {
	const auto side = [](const json& length) {
		return length.is_number_integer() && length.get<long long>() >= 1 &&
		       length.get<long long>() <= detail::maxWindowSide;
	};
	if(!value.is_array() || value.size() != 2 || !std::ranges::all_of(value, side))
		fail(place, "expected [width, height], whole numbers from 1 to " + std::to_string(detail::maxWindowSide));
}

/// @brief Checks an action step: its name, and the event it runs with.
/// @param step The step.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkAction(const json& step, const std::string& place) {
	onlyKeys(step, {"action", "event", "note"}, place);
	if(!step["action"].is_string() || step["action"].get<std::string>().empty())
		fail(place + ".action", "expected the name of an action");
	if(step.contains("event") && !step["event"].is_object()) fail(place + ".event", "expected an object");
}

/// @brief Checks the text a `type` step types.
/// @param value The text.
/// @param at Its place.
/// @throws DocumentError If it isn't text, or is too long.
void checkTyped(const json& value, const std::string& at) {
	if(!value.is_string() || value.get<std::string>().size() > detail::maxTypedBytes)
		fail(at, "expected text, at most " + std::to_string(detail::maxTypedBytes) + " bytes");
}

/// @brief Checks the key a `key` step presses.
/// @param value The key's name, with modifiers.
/// @param at Its place.
/// @throws DocumentError If it isn't a key.
void checkKeyName(const json& value, const std::string& at) {
	if(!value.is_string() || !detail::parseKey(value.get<std::string>()))
		fail(at, "expected a key: " + detail::keyNames());
}

/// @brief Checks how long a `wait` step waits.
/// @param value The milliseconds.
/// @param at Its place.
/// @throws DocumentError If it isn't a number of milliseconds within the limit.
void checkWait(const json& value, const std::string& at) {
	if(!value.is_number() || value.get<double>() < 0.0 || value.get<double>() > detail::maxWaitMs)
		fail(at, "expected milliseconds, from 0 to " + std::to_string(static_cast<int>(detail::maxWaitMs)));
}

/// @brief Checks the file a `choose` step answers dialogs with.
/// @param value A file name in the run's own folder, or null to cancel.
/// @param at Its place.
/// @throws DocumentError If it is neither.
void checkChosen(const json& value, const std::string& at) {
	if(!value.is_null() && (!value.is_string() || !detail::isSafeFileName(value.get<std::string>())))
		fail(at, "expected a file name of letters, digits, '-', '_' and '.', or null to cancel");
}

/// @brief Checks the steps whose one value says it all: type, key, revision, resize, wait, capture, choose, expect.
/// @param step The step.
/// @param kind Its kind.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkValueStep(const json& step, std::string_view kind, const std::string& place) {
	onlyKeys(step, {kind, "note"}, place);
	const json& value = step[std::string(kind)];
	const std::string at = place + "." + std::string(kind);
	if(kind == "type") {
		checkTyped(value, at);
	} else if(kind == "key") {
		checkKeyName(value, at);
	} else if(kind == "revision") {
		if(!value.is_object()) fail(at, R"(expected a revision: {"message": ..., "set": {...}})");
	} else if(kind == "resize") {
		checkSize(value, at);
	} else if(kind == "wait") {
		checkWait(value, at);
	} else if(kind == "capture") {
		checkFileName(value, at);
	} else if(kind == "choose") {
		checkChosen(value, at);
	} else {
		checkExpectation(value, at);
	}
}

/// @brief The step kinds: the key that says what a step does. Every step has exactly one, or is only a "note".
constexpr std::array<std::string_view, 15> stepKinds = {"capture", "move",   "click", "press",  "release",
                                                        "drag",    "scroll", "type",  "key",    "revision",
                                                        "action",  "resize", "wait",  "choose", "expect"};

/// @brief Finds what a step does: the one key of stepKinds it has.
/// @param step The step.
/// @param place Its place.
/// @return The kind, or std::nullopt for a step that is only a "note".
/// @throws DocumentError If it isn't an object, or has no kind or two.
std::optional<std::string_view> kindOf(const json& step, const std::string& place) {
	if(!step.is_object()) fail(place, R"(expected a step: {"click": "#id"}, {"type": "text"}, ...)");
	std::vector<std::string_view> kinds;
	for(const std::string_view kind : stepKinds)
		if(step.contains(std::string(kind))) kinds.push_back(kind);
	if(kinds.empty()) {
		if(step.size() == 1 && step.contains("note")) return std::nullopt;
		fail(place, "unknown step " + toText(step).substr(0, 80) +
		                ": expected one of capture, move, click, press, release, drag, scroll, type, key, revision, "
		                "action, resize, wait, choose, expect");
	}
	if(kinds.size() > 1)
		fail(place, "one step does one thing: '" + std::string(kinds[0]) + "' and '" + std::string(kinds[1]) + "'");
	return kinds[0];
}

/// @brief Checks one step, and tells whether it draws.
/// @param step The step.
/// @param place Its place.
/// @return Whether it needs a picture (capture, expect image).
/// @throws DocumentError If it is malformed.
bool checkStep(const json& step, const std::string& place) {
	const std::optional<std::string_view> found = kindOf(step, place);
	if(!found) return false; // a comment
	const std::string_view kind = *found;
	checkNth(step, place);
	if(kind == "move" || kind == "click" || kind == "press" || kind == "release") checkPointer(step, kind, place);
	else if(kind == "drag") checkDrag(step, place);
	else if(kind == "scroll") checkScroll(step, place);
	else if(kind == "action") checkAction(step, place);
	else checkValueStep(step, kind, place);
	return kind == "capture" || (kind == "expect" && step["expect"].contains("image"));
}

/// @brief Reads a scenario's scale.
/// @param value The scale.
/// @param place Its place.
/// @return The scale, from 1 to the limit.
/// @throws DocumentError If it isn't such a whole number.
int readScale(const json& value, const std::string& place) {
	const int scale = expect::integer(value, place);
	if(scale < 1 || scale > detail::maxScale) fail(place, "expected 1 to " + std::to_string(detail::maxScale));
	return scale;
}

/// @brief Checks a scenario's steps and adds them to it.
/// @param item The scenario's JSON.
/// @param place Its place.
/// @param scenario The scenario, which gets the steps, and whether they need pictures.
/// @throws DocumentError If there are too many steps, or one is malformed.
void readSteps(const json& item, const std::string& place, Scenario& scenario) {
	const json& steps = item.contains("steps") ? item["steps"] : json();
	if(!steps.is_array() || steps.size() > detail::maxSteps)
		fail(place + ".steps", "expected a list of at most " + std::to_string(detail::maxSteps) + " steps");
	for(std::size_t s = 0; s < steps.size(); ++s) {
		if(checkStep(steps[s], place + ".steps[" + std::to_string(s) + "]")) scenario.needsPictures = true;
		scenario.steps.push_back(steps[s]);
	}
}

/// @brief Reads one scenario.
/// @param item The scenario's JSON.
/// @param place Its place.
/// @param screens The folder its screen is in.
/// @param defaultScreen The file's screen, if it names one.
/// @return The scenario.
/// @throws DocumentError If it is malformed.
Scenario readScenario(const json& item, const std::string& place, const std::filesystem::path& screens,
                      const std::optional<std::string>& defaultScreen) {
	if(!item.is_object()) fail(place, R"(expected a scenario: {"name": ..., "steps": [...]})");
	onlyKeys(item, {"name", "screen", "scale", "note", "steps"}, place);
	Scenario scenario;
	scenario.name = item.contains("name") ? expect::string(item["name"], place + ".name") : "";
	if(!detail::isSafeFileName(scenario.name))
		fail(place + ".name", "expected a name of letters, digits, '-', '_' and '.'");
	const std::optional<std::string> screen =
	    item.contains("screen") ? std::optional(expect::string(item["screen"], place + ".screen")) : defaultScreen;
	if(!screen) fail(place, R"(no "screen": give one here or for the whole file)");
	scenario.screen = screens / *screen;
	if(item.contains("scale")) scenario.scale = readScale(item["scale"], place + ".scale");
	readSteps(item, place, scenario);
	return scenario;
}

/// @brief Reads the scenarios of a file.
/// @param document The file's JSON.
/// @param folder Where the file is.
/// @return The scenarios.
/// @throws DocumentError If one is malformed.
std::vector<Scenario> readScenarios(const json& document, const std::filesystem::path& folder) {
	const std::filesystem::path screens =
	    folder / (document.contains("screens") ? expect::string(document["screens"], "screens") : std::string("."));
	const std::optional<std::string> defaultScreen =
	    document.contains("screen") ? std::optional(expect::string(document["screen"], "screen")) : std::nullopt;

	const json& list = document.contains("scenarios") ? document["scenarios"] : json();
	if(!list.is_array() || list.empty() || list.size() > detail::maxScenarios)
		fail("scenarios", "expected a list of 1 to " + std::to_string(detail::maxScenarios) + " scenarios");
	std::vector<Scenario> scenarios;
	std::set<std::string, std::less<>> names;
	for(std::size_t i = 0; i < list.size(); ++i) {
		const std::string place = "scenarios[" + std::to_string(i) + "]";
		Scenario scenario = readScenario(list[i], place, screens, defaultScreen);
		if(!names.insert(scenario.name).second)
			fail(place + ".name", "'" + scenario.name + "' is the name of another scenario");
		scenarios.push_back(std::move(scenario));
	}
	return scenarios;
}

/// @brief Puts the file's name in front of an error's place, as the screen loader does.
/// @param path The file.
/// @param message The error, with or without the module's prefix.
/// @return "AffrescoUI scenarios: <file>: <place>: <message>".
std::string inFile(const std::filesystem::path& path, std::string message) {
	for(const std::string_view prefix : {"AffrescoUI scenarios: ", "AffrescoUI screen: "})
		if(message.starts_with(prefix)) message.erase(0, prefix.size());
	return "AffrescoUI scenarios: " + path.filename().string() + ": " + message;
}

} // namespace

ScenarioFile ScenarioFile::parse(std::string_view text, const std::filesystem::path& path) {
	try {
		json document = parseUntrusted(text);
		if(!document.is_object()) fail("", R"(expected an object with "scenarios")");
		onlyKeys(document, {"$schema", "format", "version", "note", "screen", "screens", "scenarios"}, "");
		if(document.contains("format") && document["format"] != "affrescoui-scenarios")
			fail("format", R"(expected "affrescoui-scenarios")");
		if(!document.contains("version") || (document["version"] != 1 && document["version"] != format::formatVersion))
			fail("version", "expected 1 or 2");
		if(!format::migrate(document).empty())
			log(LogLevel::Info, path.filename().string() + ": version 1, read as version 2 (affrescoui migrate --write "
			                                                "rewrites the file)");
		return {path, readScenarios(document, path.parent_path())};
	} catch(const json::exception& error) { // not JSON, or what parseUntrusted didn't turn into a DocumentError
		throw DocumentError(inFile(path, error.what()));
	} catch(const DocumentError& error) {
		throw DocumentError(inFile(path, error.what()));
	}
}

ScenarioFile ScenarioFile::load(const std::filesystem::path& path) {
	const std::ifstream file(path, std::ios::binary);
	if(!file) throw DocumentError("AffrescoUI scenarios: could not read " + path.string());
	std::stringstream text;
	text << file.rdbuf();
	return parse(text.str(), path);
}

} // namespace affresco::automation
