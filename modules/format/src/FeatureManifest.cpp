/// @file
/// @brief Feature manifests: releases and ranges, the manifest's shape, the "features", "exports" and "imports"
/// sections (rfcs/0004-features.md).

#include "FeatureManifest.hpp"
#include "Limits.hpp"
#include "affresco/format/Migrate.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <set>

namespace affresco::format {

namespace {

using expect::fail;

/// The keys a manifest may have.
constexpr auto manifestKeys =
    std::to_array<std::string_view>({"format", "version", "name", "release", "title", "description", "license",
                                     "publisher", "homepage", "note", "requires", "provides", "scenarios", "signature"});

/// The keys a record of the "features" section may have: the manifest's parts the runtime merges, and its origin.
constexpr auto recordKeys = std::to_array<std::string_view>(
    {"release", "title", "source", "digest", "publisher", "enabled", "order", "requires", "provides"});

/// The keys of "requires".
constexpr auto requiresKeys =
    std::to_array<std::string_view>({"format", "points", "exports", "models", "services", "connections", "features"});

/// The keys of "provides".
constexpr auto providesKeys = std::to_array<std::string_view>(
    {"state", "computed", "actions", "components", "styles", "keys", "menus", "settings", "views", "exports"});

/// The parts of "provides" that hold data, never steps: a key there named like a control step is a name.
constexpr auto dataParts = std::to_array<std::string_view>({"state", "settings", "styles", "exports"});

/// The keys of an export.
constexpr auto exportKeys = std::to_array<std::string_view>({"type", "value", "description", "note"});

/// Longest name a feature, a local declaration or an export may have.
constexpr std::size_t maxNameLength = 64;

/// @brief Tells whether a list of names holds one.
/// @param names The names.
/// @param name The name.
/// @return true if it does.
template<std::size_t N> bool among(const std::array<std::string_view, N>& names, std::string_view name) {
	return std::ranges::find(names, name) != names.end();
}

/// @brief Tells whether a character can start an identifier.
/// @param c The character.
/// @return true for an ASCII letter or '_'.
bool identifierStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

/// @brief Tells whether a character can go on an identifier.
/// @param c The character.
/// @return true for an ASCII letter, digit or '_'.
bool identifierPart(char c) { return identifierStart(c) || (c >= '0' && c <= '9'); }

/// @brief Tells whether a text is one identifier, as expressions read them.
/// @param text The text.
/// @return true if it is, and at most maxNameLength long.
bool identifier(std::string_view text) {
	return !text.empty() && text.size() <= maxNameLength && identifierStart(text.front()) &&
	       std::ranges::all_of(text.substr(1), identifierPart);
}

/// @brief Tells whether a text is identifiers joined by dots ("editor.text"), the way settings and exports are named.
/// @param text The text.
/// @return true if it is.
bool dottedName(std::string_view text) {
	if(text.empty() || text.size() > maxNameLength) return false;
	for(std::size_t start = 0;;) {
		const auto dot = text.find('.', start);
		if(!identifier(text.substr(start, dot - start))) return false;
		if(dot == std::string_view::npos) return true;
		start = dot + 1;
	}
}

/// @brief Tells whether a text can be a local declaration's name: an identifier, then letters, digits, '_', '.' or
/// '-' ("toggle", "status-strong").
/// @param text The text.
/// @return true if it can.
bool localName(std::string_view text) {
	return !text.empty() && text.size() <= maxNameLength && identifierStart(text.front()) &&
	       std::ranges::all_of(text.substr(1), [](char c) { return identifierPart(c) || c == '.' || c == '-'; });
}

/// @brief Refuses a key a part doesn't know.
/// @param value The part (an object).
/// @param keys The keys it may have.
/// @param place Its place.
/// @throws DocumentError On an unknown key.
template<std::size_t N>
void knownKeys(const json& value, const std::array<std::string_view, N>& keys, const std::string& place) {
	for(const auto& [key, item] : value.items())
		if(!among(keys, key)) fail(place + "." + key, "unknown key");
}

/// @brief Reads an object of names, each checked.
/// @param value The value.
/// @param place Its place.
/// @param valid Whether a name is allowed.
/// @param what What a name must be, for the error.
/// @throws DocumentError If it isn't an object or a name isn't allowed.
void checkNames(const json& value, const std::string& place, bool (*valid)(std::string_view), const char* what) {
	expect::object(value, place);
	for(const auto& [key, item] : value.items())
		if(!valid(key)) fail(place + "." + key, std::format("must be {}", what));
}

/// @brief Reads a list of distinct strings.
/// @param value The value.
/// @param place Its place.
/// @throws DocumentError If it isn't one.
void checkStringList(const json& value, const std::string& place) {
	if(!value.is_array()) fail(place, "expected a list of names");
	std::set<std::string_view> seen;
	for(std::size_t index = 0; index < value.size(); ++index) {
		const std::string at = std::format("{}[{}]", place, index);
		if(!value[index].is_string() || value[index].get_ref<const std::string&>().empty())
			fail(at, "expected a name");
		if(!seen.insert(value[index].get_ref<const std::string&>()).second) fail(at, "listed twice");
	}
}

/// @brief Reads a type as exports write it: a JSON Schema type name, or a list of them.
/// @param value The value.
/// @param place Its place.
/// @throws DocumentError If it isn't one.
void checkTypeName(const json& value, const std::string& place) {
	constexpr auto types =
	    std::to_array<std::string_view>({"string", "number", "integer", "boolean", "array", "object", "null"});
	const auto known = [&](const json& name) { return name.is_string() && among(types, name.get<std::string>()); };
	if(known(value)) return;
	if(value.is_array() && !value.empty() && std::ranges::all_of(value, known)) return;
	fail(place, "expected a type: string, number, integer, boolean, array, object or null");
}

/// @brief Checks "requires".
/// @param needs The part.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkRequires(const json& needs, const std::string& place) {
	expect::object(needs, place);
	knownKeys(needs, requiresKeys, place);
	if(const auto format = needs.find("format"); format != needs.end()) expect::integer(*format, place + ".format");
	if(const auto points = needs.find("points"); points != needs.end()) checkStringList(*points, place + ".points");
	if(const auto exports = needs.find("exports"); exports != needs.end()) {
		checkNames(*exports, place + ".exports", dottedName, "a name like editor.text");
		for(const auto& [name, wanted] : exports->items()) {
			const std::string at = place + ".exports." + name;
			expect::object(wanted, at);
			if(!wanted.contains("type")) fail(at, "missing \"type\"");
			checkTypeName(wanted["type"], at + ".type");
		}
	}
	if(const auto models = needs.find("models"); models != needs.end())
		checkNames(*models, place + ".models", identifier, "a model's name");
	for(const char* part : {"services", "connections"})
		if(const auto uses = needs.find(part); uses != needs.end()) {
			checkNames(*uses, place + "." + part, identifier, "a name");
			for(const auto& [name, methods] : uses->items()) checkStringList(methods, place + "." + part + "." + name);
		}
	if(const auto features = needs.find("features"); features != needs.end()) {
		checkNames(*features, place + ".features", validFeatureName, "a feature's name");
		for(const auto& [name, range] : features->items())
			if(!range.is_string() || !validRange(range.get_ref<const std::string&>()))
				fail(place + ".features." + name, "expected a range: 1.2.3, ^1.2.3, ~1.2.3 or >=1.2.3");
	}
}

/// @brief Checks the ids of a view's nodes: each in the feature's namespace ("wordCount-label").
/// @param node A node.
/// @param prefix The namespace's prefix ("wordCount-").
/// @param place Its place.
/// @throws DocumentError On an id outside it.
void checkViewIds(const json& node, const std::string& prefix, const std::string& place) {
	expect::object(node, place);
	if(const auto id = node.find("id"); id != node.end())
		if(!id->is_string() || !id->get_ref<const std::string&>().starts_with(prefix))
			fail(place + ".id", std::format("must start with '{}'", prefix));
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(std::size_t index = 0; index < children->size(); ++index)
			checkViewIds((*children)[index], prefix, std::format("{}.children[{}]", place, index));
}

/// @brief Checks "provides.views": `"<point>": { "<id>": <node> }`, every id in the feature's namespace.
/// @param views The part.
/// @param feature The feature's name.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkViews(const json& views, const std::string& feature, const std::string& place) {
	expect::object(views, place);
	const std::string prefix = feature + "-";
	for(const auto& [point, nodes] : views.items()) {
		const std::string at = place + "." + point;
		if(point.empty()) fail(at, "expected a contribution point's name");
		expect::object(nodes, at);
		for(const auto& [id, node] : nodes.items()) {
			if(!id.starts_with(prefix) || id.size() == prefix.size())
				fail(at + "." + id, std::format("id '{}' must start with '{}'", id, prefix));
			checkViewIds(node, prefix, at + "." + id);
			if(node.contains("id")) fail(at + "." + id + ".id", "the view's key is its id");
			if(node.contains("point")) fail(at + "." + id + ".point", "a feature's view can't be a contribution point");
		}
	}
}

/// @brief Checks "provides.menus": `"<menu>": { "items": { "<item>": {...} } }`, or the items directly.
/// @param menus The part.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkProvidedMenus(const json& menus, const std::string& place) {
	expect::object(menus, place);
	for(const auto& [menu, items] : menus.items()) {
		const std::string at = place + "." + menu;
		expect::object(items, at);
		const json& list = items.contains("items") ? items["items"] : items;
		const std::string listAt = items.contains("items") ? at + ".items" : at;
		checkNames(list, listAt, localName, "a local name");
		for(const auto& [item, entry] : list.items()) expect::object(entry, listAt + "." + item);
	}
}

/// @brief Checks "provides".
/// @param provides The part.
/// @param feature The feature's name.
/// @param place Its place.
/// @throws DocumentError If it is malformed, or brings a capability or a control step.
void checkProvides(const json& provides, const std::string& feature, const std::string& place) {
	expect::object(provides, place);
	for(const auto& [key, part] : provides.items())
		if(among(capabilitySections, key))
			fail(place + "." + key, "a capability: only the host can grant it, never a revision");
	knownKeys(provides, providesKeys, place);
	if(const auto state = provides.find("state"); state != provides.end())
		checkNames(*state, place + ".state", identifier, "an identifier");
	if(const auto computed = provides.find("computed"); computed != provides.end()) {
		checkNames(*computed, place + ".computed", identifier, "an identifier");
		for(const auto& [name, value] : computed->items()) expect::string(value, place + ".computed." + name);
	}
	for(const char* part : {"actions", "components", "styles"})
		if(const auto declared = provides.find(part); declared != provides.end())
			checkNames(*declared, place + "." + part, localName, "a local name");
	if(const auto keys = provides.find("keys"); keys != provides.end()) expect::object(*keys, place + ".keys");
	if(const auto menus = provides.find("menus"); menus != provides.end()) checkProvidedMenus(*menus, place + ".menus");
	if(const auto settings = provides.find("settings"); settings != provides.end()) {
		checkNames(*settings, place + ".settings", dottedName, "a name like showCharacters");
		for(const auto& [name, type] : settings->items()) expect::object(type, place + ".settings." + name);
	}
	if(const auto views = provides.find("views"); views != provides.end()) checkViews(*views, feature, place + ".views");
	if(const auto exports = provides.find("exports"); exports != provides.end())
		checkExportsSection(*exports, place + ".exports");
	if(const auto steps = controlStepsIn(provides, place); !steps.empty())
		fail(steps.front().substr(0, steps.front().find(": ")),
		     steps.front().substr(steps.front().find(": ") + 2) +
		         " is a control step: a feature that uses one is installed by the host, not by a revision");
}

/// @brief Collects the control steps in a value: objects with a key that names one.
/// @param value The value.
/// @param place Its place.
/// @param found Receives "place: 'step'" for each.
/// @param depth How deep the walk is, bounded by maxNesting.
void collectControlSteps(const json& value, const std::string& place, std::vector<std::string>& found, int depth) {
	if(depth > maxNesting) return;
	if(value.is_object()) {
		for(const auto& [key, item] : value.items())
			if(among(controlSteps, key)) found.push_back(std::format("{}: '{}'", place, key));
		for(const auto& [key, item] : value.items())
			if(item.is_structured()) collectControlSteps(item, place + "." + key, found, depth + 1);
	} else if(value.is_array()) {
		for(std::size_t index = 0; index < value.size(); ++index)
			if(value[index].is_structured())
				collectControlSteps(value[index], std::format("{}[{}]", place, index), found, depth + 1);
	}
}

/// @brief Refuses a value too big or too deep for a manifest.
/// @param value The manifest or record.
/// @param place Its place.
/// @throws DocumentError If it is.
void checkLimits(const json& value, const std::string& place) {
	if(approximateBytes(value, maxManifestBytes + 1) > maxManifestBytes)
		fail(place, std::format("larger than a feature may be ({} bytes)", maxManifestBytes));
	if(nestingDepth(value) > maxDocumentNesting)
		fail(place, std::format("nests deeper than {} levels", maxDocumentNesting));
}

/// @brief Checks a manifest's "signature": `{ "publisher": "ed25519:<key>", "value": "<signature>" }` (their
/// encodings are FeatureSignatures' to check).
/// @param signature The part.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkSignatureShape(const json& signature, const std::string& place) {
	expect::object(signature, place);
	for(const auto& [key, item] : signature.items())
		if(key != "publisher" && key != "value") fail(place + "." + key, "unknown key");
	if(!signature.contains("publisher") || !signature["publisher"].is_string() ||
	   !signature["publisher"].get_ref<const std::string&>().starts_with("ed25519:"))
		fail(place + ".publisher", "expected \"ed25519:<public key>\"");
	if(!signature.contains("value")) fail(place, "missing \"value\"");
	expect::string(signature["value"], place + ".value");
}

/// @brief Reads a text key if it is there.
/// @param object The object.
/// @param key The key.
/// @param place The object's place.
/// @throws DocumentError If it is there and isn't a text.
void optionalText(const json& object, const char* key, const std::string& place) {
	if(const auto value = object.find(key); value != object.end()) expect::string(*value, place + "." + key);
}

} // namespace

std::optional<Release> parseRelease(std::string_view text) {
	Release release;
	std::array<int*, 3> parts = {&release.major, &release.minor, &release.patch};
	for(std::size_t index = 0; index < parts.size(); ++index) {
		const auto end = index + 1 < parts.size() ? text.find('.') : text.size();
		if(end == std::string_view::npos) return std::nullopt;
		const std::string_view digits = text.substr(0, end);
		if(digits.empty() || digits.size() > 9 || !std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; }))
			return std::nullopt;
		std::from_chars(digits.data(), digits.data() + digits.size(), *parts[index]);
		text.remove_prefix(std::min(text.size(), end + 1));
		if(index + 1 == parts.size() && end != digits.size()) return std::nullopt;
	}
	return release;
}

bool validRange(std::string_view range) {
	if(range.starts_with(">=")) range.remove_prefix(2);
	else if(range.starts_with('^') || range.starts_with('~')) range.remove_prefix(1);
	return parseRelease(range).has_value();
}

bool satisfies(std::string_view release, std::string_view range) {
	const auto have = parseRelease(release);
	if(!have || !validRange(range)) return false;
	if(range.starts_with(">=")) return *have >= *parseRelease(range.substr(2));
	if(range.starts_with('^')) {
		const Release low = *parseRelease(range.substr(1));
		const Release high = low.major > 0 ? Release{low.major + 1, 0, 0} : Release{0, low.minor + 1, 0};
		return low <= *have && *have < high;
	}
	if(range.starts_with('~')) {
		const Release low = *parseRelease(range.substr(1));
		return low <= *have && *have < Release{low.major, low.minor + 1, 0};
	}
	return *have == *parseRelease(range);
}

bool validFeatureName(std::string_view name) {
	return !name.empty() && name.size() <= maxNameLength && name.front() >= 'a' && name.front() <= 'z' &&
	       std::ranges::all_of(name, [](char c) {
		       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
	       });
}

std::vector<std::string> controlStepsIn(const json& provides, const std::string& place) {
	std::vector<std::string> found;
	if(!provides.is_object()) return found;
	for(const auto& [key, part] : provides.items())
		if(!among(dataParts, key) && part.is_structured()) collectControlSteps(part, place + "." + key, found, 1);
	return found;
}

void checkManifest(const json& manifest, const std::string& place) {
	expect::object(manifest, place);
	checkLimits(manifest, place);
	knownKeys(manifest, manifestKeys, place);
	if(manifest.value("format", json()) != manifestFormat)
		fail(place + ".format", std::format("expected \"{}\"", manifestFormat));
	if(const json version = manifest.value("version", json()); version != 1 && version != formatVersion)
		fail(place + ".version", "expected 1 or 2");
	if(!manifest.contains("name") || !manifest["name"].is_string() ||
	   !validFeatureName(manifest["name"].get_ref<const std::string&>()))
		fail(place + ".name", "must be one camelCase word, starting with a lower-case letter");
	if(!manifest.contains("release") || !manifest["release"].is_string() ||
	   !parseRelease(manifest["release"].get_ref<const std::string&>()))
		fail(place + ".release", "expected a release like 1.2.3");
	if(!manifest.contains("title")) fail(place, "missing \"title\"");
	for(const char* key : {"title", "description", "license", "publisher", "homepage", "note"})
		optionalText(manifest, key, place);
	if(const auto needs = manifest.find("requires"); needs != manifest.end())
		checkRequires(*needs, place + ".requires");
	if(!manifest.contains("provides")) fail(place, "missing \"provides\"");
	checkProvides(manifest["provides"], manifest["name"].get<std::string>(), place + ".provides");
	if(const auto scenarios = manifest.find("scenarios"); scenarios != manifest.end() && !scenarios->is_structured())
		fail(place + ".scenarios", "expected a list or an object of scenarios");
	if(const auto signature = manifest.find("signature"); signature != manifest.end())
		checkSignatureShape(*signature, place + ".signature");
}

void checkFeatureRecord(const std::string& name, const json& record, const std::string& place) {
	if(!validFeatureName(name)) fail(place, "a feature's name is one camelCase word, starting with a lower-case letter");
	expect::object(record, place);
	checkLimits(record, place);
	knownKeys(record, recordKeys, place);
	if(!record.contains("release") || !record["release"].is_string() ||
	   !parseRelease(record["release"].get_ref<const std::string&>()))
		fail(place + ".release", "expected a release like 1.2.3");
	for(const char* key : {"title", "source"}) optionalText(record, key, place);
	for(const char* key : {"digest", "publisher"})
		if(const auto value = record.find(key); value != record.end() && !value->is_null())
			expect::string(*value, place + "." + key);
	if(!record.contains("enabled")) fail(place, "missing \"enabled\"");
	expect::boolean(record["enabled"], place + ".enabled");
	if(const auto order = record.find("order"); order != record.end()) expect::integer(*order, place + ".order");
	if(const auto needs = record.find("requires"); needs != record.end())
		checkRequires(*needs, place + ".requires");
	if(!record.contains("provides")) fail(place, "missing \"provides\"");
	checkProvides(record["provides"], name, place + ".provides");
}

void checkFeaturesSection(const json& section, const std::string& name) {
	expect::object(section, name);
	for(const auto& [feature, record] : section.items()) checkFeatureRecord(feature, record, name + "." + feature);
}

void checkExportsSection(const json& section, const std::string& name) {
	checkNames(section, name, dottedName, "a name like editor.text");
	for(const auto& [key, entry] : section.items()) {
		const std::string at = name + "." + key;
		expect::object(entry, at);
		knownKeys(entry, exportKeys, at);
		if(!entry.contains("type")) fail(at, "missing \"type\"");
		checkTypeName(entry["type"], at + ".type");
		if(!entry.contains("value")) fail(at, "missing \"value\"");
		const std::string value = expect::string(entry["value"], at + ".value");
		if(!value.starts_with("{{") || !value.ends_with("}}") || value.size() < 5)
			fail(at + ".value", "expected one expression, \"{{...}}\"");
		optionalText(entry, "description", at);
		optionalText(entry, "note", at);
	}
}

void checkImportsSection(const json& section, const std::string& name) {
	checkNames(section, name, identifier, "an identifier");
	for(const auto& [alias, entry] : section.items()) {
		const std::string text = expect::string(entry, name + "." + alias);
		const auto parts = splitImport(text);
		if(!parts || !validFeatureName(parts->first) || !validRange(parts->second))
			fail(name + "." + alias, "expected \"<feature>@<range>\", e.g. \"workbench@^1.0.0\"");
	}
}

std::optional<std::pair<std::string, std::string>> splitImport(std::string_view text) {
	const auto at = text.find('@');
	if(at == std::string_view::npos || at == 0 || at + 1 == text.size()) return std::nullopt;
	std::string range(text.substr(at + 1));
	// "^1" is how people write "^1.0.0" (RFC 0004's own example): complete the missing parts.
	for(auto dots = std::ranges::count(range, '.'); dots < 2; ++dots) range += ".0";
	return std::pair{std::string(text.substr(0, at)), std::move(range)};
}

json recordFor(const json& manifest, const std::string& source, const std::string& digest,
               const std::string& publisher, int order) {
	json record = {{"release", manifest["release"]}, {"title", manifest.value("title", "")}, {"source", source}};
	record["digest"] = digest.empty() ? json() : json(digest);
	record["publisher"] = publisher.empty() ? json() : json(publisher);
	record["enabled"] = true;
	record["order"] = order;
	record["requires"] = manifest.value("requires", json::object());
	record["provides"] = manifest["provides"];
	return record;
}

} // namespace affresco::format
