/// @file
/// @brief The checker's passes, findings and hints, and the shapes it learns from the state (Checker.hpp).

#include "Checker.hpp"

#include "BuiltInComponents.hpp"
#include "CheckerImpl.hpp"

#include "affresco/format/NodeIds.hpp"

#include <algorithm>
#include <iterator>
#include <span>
#include <unordered_map>

namespace affresco::format {

std::string Finding::text() const {
	return (place.empty() ? "" : place + ": ") + problem + (hint.empty() ? "" : " (" + hint + ")");
}

namespace checker {

/* Shapes */

namespace {

/// Keys an object has before the checker finds them by name rather than one after another (as an ordered object does).
constexpr std::size_t indexedEntries = 16;

/// @brief Merges the keys of many into an object, finding each by name: an ordered object would look for each key
/// among all those before it.
/// @param into An object's shape.
/// @param other Another's.
void mergeWide(Shape& into, const Shape& other) {
	Shape::object_t& fields = into.get_ref<Shape::object_t&>();
	fields.reserve(fields.size() + other.size()); // the names the index views stay where they are
	std::unordered_map<std::string_view, Shape*> shapes;
	for(auto& [key, shape] : fields) shapes.emplace(key, &shape);
	for(const auto& [key, shape] : other.items()) {
		const auto it = shapes.find(key);
		if(it == shapes.end()) fields.emplace_back(key, shape); // other's keys are all different
		else mergeInto(*it->second, shape);
	}
}

} // namespace

void mergeInto(Shape& into, const Shape& other) {
	if(into.is_null()) return;
	if(other.is_null() || into.type() != other.type()) {
		into = anything;
	} else if(into.is_array()) {
		if(other.empty()) return;
		if(into.empty()) into = other;
		else mergeInto(into[0], other[0]);
	} else if(into.is_object() && other.size() >= indexedEntries) {
		mergeWide(into, other);
	} else if(into.is_object()) {
		for(const auto& [key, shape] : other.items()) {
			const auto it = into.find(key);
			if(it == into.end()) into[key] = shape;
			else mergeInto(*it, shape);
		}
	}
}

bool Work::pay(const Shape& shape) {
	if(over_) return false;
	std::size_t nodes = 0;
	std::vector<const Shape*> pending{&shape};
	while(!pending.empty() && !over_) {
		const Shape* node = pending.back();
		pending.pop_back();
		over_ = ++nodes > left_;
		if(node->is_structured())
			for(const Shape& inside : *node) pending.push_back(&inside);
	}
	left_ -= over_ ? left_ : nodes;
	return !over_;
}

Shape merge(Shape a, const Shape& b) {
	mergeInto(a, b);
	return a;
}

void addItem(Shape& list, const Shape& item) {
	if(list.empty()) list.push_back(item);
	else mergeInto(list[0], item);
}

namespace {

/// @brief The kinds of value from the state that a shape tells apart.
enum class DataKind {
	unknown, ///< null, or an empty object (most likely a map, filled later)
	object,  ///< An object with keys.
	list,    ///< A list.
	single   ///< Text, a number or a boolean.
};

/// @brief Tells what kind of value a value from the state is.
/// @param data The value.
/// @return Its kind.
DataKind kindOfData(const json& data) {
	if(data.is_object()) return data.empty() ? DataKind::unknown : DataKind::object;
	if(data.is_array()) return DataKind::list;
	return data.is_null() ? DataKind::unknown : DataKind::single;
}

/// @brief Describes what a place in the state may hold from the values it holds: one value, the items of a list, or
/// the values a key has in those items. Each value is looked at once: merging items one after another into a shape
/// would look for each of their keys among all the keys so far.
/// @param values The values, in order (at least one).
/// @return Their shape: anything if one is null or an empty object, or they are of different kinds.
Shape shapeOfValues(std::span<const json* const> values) {
	const DataKind kind = kindOfData(*values.front());
	if(kind == DataKind::unknown ||
	   std::ranges::any_of(values, [&](const json* value) { return kindOfData(*value) != kind; }))
		return anything;
	if(kind == DataKind::single) return single;
	if(kind == DataKind::list) {
		std::vector<const json*> items;
		for(const json* value : values)
			for(const json& item : *value) items.push_back(&item);
		return items.empty() ? Shape::array() : Shape::array({shapeOfValues(items)});
	}
	Shape shape = Shape::object();
	Shape::object_t& fields = shape.get_ref<Shape::object_t&>();
	if(values.size() == 1) { // its keys are all different: each goes at the end, without looking for it
		for(const auto& [key, value] : values.front()->items()) {
			const json* const one = &value;
			fields.emplace_back(key, shapeOfValues({&one, 1}));
		}
		return shape;
	}
	// Each key's values, the keys in the order they first come (the views name keys in the values, which stay)
	std::vector<std::pair<std::string_view, std::vector<const json*>>> keys;
	std::unordered_map<std::string_view, std::size_t> positions;
	for(const json* object : values)
		for(const auto& [key, value] : object->items()) {
			const auto [it, added] = positions.try_emplace(key, keys.size());
			if(added) keys.emplace_back(key, std::vector<const json*>());
			keys[it->second].second.push_back(&value);
		}
	for(const auto& [key, inside] : keys) fields.emplace_back(key, shapeOfValues(inside));
	return shape;
}

} // namespace

Shape shapeOfData(const json& data) {
	const json* const one = &data;
	return shapeOfValues({&one, 1});
}

const Shape& itemOf(const Shape& list) { return list.is_array() && !list.empty() ? list[0] : anything; }

/* Hints */

namespace {

/// @brief Counts the edits that turn one word into another: insertions, deletions, substitutions, and swaps of two
/// letters next to each other, the commonest typo ("opne").
/// @param a A word.
/// @param b Another.
/// @return The distance (optimal string alignment).
std::size_t distance(std::string_view a, std::string_view b) {
	const std::size_t width = b.size() + 1;
	std::vector<std::size_t> d((a.size() + 1) * width);
	for(std::size_t i = 0; i <= a.size(); ++i) d[i * width] = i;
	for(std::size_t j = 0; j <= b.size(); ++j) d[j] = j;
	for(std::size_t i = 1; i <= a.size(); ++i)
		for(std::size_t j = 1; j <= b.size(); ++j) {
			const std::size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
			std::size_t best =
			    std::min({d[(i - 1) * width + j] + 1, d[i * width + j - 1] + 1, d[(i - 1) * width + j - 1] + cost});
			if(i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
				best = std::min(best, d[(i - 2) * width + j - 2] + 1);
			d[i * width + j] = best;
		}
	return d[a.size() * width + b.size()];
}

/// The longest name a hint compares: distance() costs the product of the lengths, and a peer chooses them.
constexpr std::size_t maxHintLength = 64;

/// The most names a hint compares a name with: a peer chooses how many keys the state has.
constexpr std::size_t maxHintNames = 1000;

/// @brief Finds the name nearest to a mistyped one, if one is near enough to be what was meant.
/// @param word The name as written.
/// @param names The names that exist.
/// @return The nearest name within one edit (two for names of five letters or more), or std::nullopt; none for
/// names longer than maxHintLength. Only the first maxHintNames names are compared.
std::optional<std::string> closest(std::string_view word, const std::vector<std::string>& names) {
	if(word.size() > maxHintLength) return std::nullopt;
	std::size_t best = word.size() < 5 ? 2 : 3; // one more than allowed
	std::optional<std::string> found;
	for(const std::string& name : names | std::views::take(maxHintNames)) {
		if(name.size() > word.size() + best || word.size() > name.size() + best) continue; // too far apart to matter
		if(const std::size_t d = distance(word, name); d < best) {
			best = d;
			found = name;
		}
	}
	return found;
}

} // namespace

std::string hintFor(std::string_view word, const std::vector<std::string>& names, std::string_view prefix,
                    std::string_view listIntro) {
	if(const auto near = closest(word, names)) return "did you mean '" + std::string(prefix) + *near + "'?";
	if(listIntro.empty()) return "";
	if(names.empty()) return std::string(listIntro) + " none";
	constexpr std::size_t shown = 12;
	std::string list(listIntro);
	for(std::size_t i = 0; i < names.size() && i < shown; ++i) list += (i ? ", " : " ") + names[i];
	return names.size() > shown ? list + ", ..." : list;
}

std::vector<std::string> keysOf(const json& object) {
	std::vector<std::string> keys;
	if(object.is_object())
		for(const auto& [key, value] : object.items()) keys.push_back(key);
	return keys;
}

std::string describe(const json& tree) {
	if(tree.is_number_integer()) return tree.dump();
	if(!tree.is_object() || tree.size() != 1) return "...";
	if(const auto value = tree.find("$value"); value != tree.end()) return value->dump();
	const auto get = tree.find("$get");
	if(get == tree.end() || !get->is_array() || get->empty() || !(*get)[0].is_string()) return "...";
	std::string text = (*get)[0].get<std::string>();
	for(std::size_t i = 1; i < get->size(); ++i) {
		const json& key = (*get)[i];
		text +=
		    key.is_string() ? "." + key.get<std::string>() : "[" + (key.is_number_integer() ? key.dump() : "...") + "]";
	}
	return text;
}

/* The checker */

Checker::Checker(const json& document, const json& state, const std::set<std::string, std::less<>>& nativeActions)
    : document_(document), nativeActions_(nativeActions), state_(Shape::object()) {
	if(state.is_object())
		for(const auto& [key, value] : state.items()) // as in shapeOfData()
			state_.get_ref<Shape::object_t&>().emplace_back(key, shapeOfData(value));
	learnTypes();
}

std::optional<std::vector<Finding>> Checker::run() {
	// A pass that learnt nothing read every path against what the state finally holds. What a pass learns can only
	// take findings away, unless a list got its first known items: a pass without findings needs no other then.
	for(int i = 0; i < maxPasses; ++i) {
		learnt_ = Learning::nothing;
		pass();
		if(exhausted_) return std::nullopt;
		if(learnt_ == Learning::nothing || (learnt_ == Learning::more && findings_.empty())) break;
	}
	return std::vector<Finding>(findings_.begin(), findings_.end());
}

void Checker::pass() {
	findings_.clear();
	walked_.clear();
	actionsReached_.clear();
	componentsReached_.clear();
	computedShapes_.clear();
	computing_.clear();
	hints_ = 0;

	walkNodes();
	walkHandlers();
	walkUnreached();
}

void Checker::walkNodes() {
	const Scope none;
	if(const json* computed = section("computed"))
		for(const auto& [name, value] : computed->items()) (void)computedShape(name);
	if(const auto theme = document_.find("theme"); theme != document_.end()) read(*theme, none, "theme");
	if(const auto root = document_.find("root"); root != document_.end()) walkNode(*root, "root", none, nullptr, false);
	for(const char* name : {"screens", "templates"})
		if(const json* nodes = section(name))
			for(const auto& [key, node] : nodes->items())
				walkNode(node, std::string(name) + "." + key, none, nullptr, false);
}

void Checker::walkHandlers() {
	const Scope none;
	if(const json* keys = section("keys")) walkKeys(*keys, none, "keys");
	if(const auto onLoad = document_.find("onLoad"); onLoad != document_.end()) walkHandler(*onLoad, none, "onLoad");
	if(const json* watch = section("watch"))
		for(const auto& [path, handler] : watch->items()) {
			(void)shapeOfPath(path, none, "watch." + path);
			walkHandler(handler, none, "watch." + path);
		}
}

// What no node or handler reaches is checked with every variable allowed: its callers (native code, channels, a
// future revision) aren't known
void Checker::walkUnreached() {
	Scope open;
	open.vars[std::string(openScope)] = anything;
	if(const json* styles = section("styles"))
		for(const auto& [name, style] : styles->items()) {
			if(!style.is_object()) continue;
			Prepared prepared = prepare(style, "styles." + name);
			applyStyles(prepared, 0);
			walkProperties(prepared, open);
		}
	if(const json* components = section("components"))
		for(const auto& [name, component] : components->items())
			if(!componentsReached_.contains(name))
				walkNode(json{{"use", name}}, "components." + name, open, nullptr, true);
	if(const json* actions = section("actions"))
		for(const auto& [name, steps] : actions->items())
			if(!actionsReached_.contains(name)) callAction(name, open, "actions." + name);
}

/* Findings and limits */

void Checker::report(const std::string& place, std::string problem, std::string hint) {
	if(place.starts_with("components.")) { // a built-in's own body is the library's, tested there
		const std::string name = place.substr(11, place.find_first_of(".[", 11) - 11);
		if(!definition("components", name) && builtInComponent(name)) return;
	}
	Finding finding{place, std::move(problem), std::move(hint)};
	if(finding.hint.empty() && hints_ >= maxHints) { // the same finding may have been reported with its hint
		const auto same = findings_.lower_bound(finding);
		if(same != findings_.end() && same->place == finding.place && same->problem == finding.problem) return;
	}
	findings_.insert(std::move(finding));
}

bool Checker::visit() {
	if(++visits_ > maxVisits) exhausted_ = true;
	return !exhausted_;
}

const json* Checker::section(const char* name) const {
	const auto it = document_.find(name);
	return it != document_.end() && it->is_object() ? &*it : nullptr;
}

const json* Checker::definition(const char* sectionName, const std::string& name) const {
	const json* entries = section(sectionName);
	if(!entries) return nullptr;
	if(entries->size() < indexedEntries) {
		const auto it = entries->find(name);
		return it != entries->end() ? &*it : nullptr;
	}
	const auto [index, added] = entries_.try_emplace(sectionName);
	if(added)
		for(const auto& [key, entry] : entries->items()) index->second.emplace(key, &entry);
	const auto it = index->second.find(name);
	return it != index->second.end() ? it->second : nullptr;
}

std::vector<std::string> Checker::definedNames(const char* sectionName) const {
	const json* entries = section(sectionName);
	return entries ? keysOf(*entries) : std::vector<std::string>{};
}

void Checker::refer(std::string_view kind, const char* sectionName, const std::string& name, std::string_view prefix,
                    const std::string& place) {
	if(definition(sectionName, name)) return;
	if(std::string_view(sectionName) == "colors" && definition("tokens", name)) return; // a colour token
	report(place, "unknown " + std::string(kind) + " '" + std::string(prefix) + name + "'", hint([&] {
		       std::vector<std::string> names = definedNames(sectionName);
		       if(std::string_view(sectionName) == "colors") // the colour tokens are named too
			       for(std::string& token : definedNames("tokens"))
				       if(token.starts_with("color.")) names.push_back(std::move(token));
		       return hintFor(name, names, prefix, "defined:");
	       }));
}

} // namespace checker

std::optional<std::vector<Finding>> check(const json& document, const json& state,
                                          const std::set<std::string, std::less<>>& nativeActions) {
	checker::Checker checker(document, state, nativeActions);
	std::optional<std::vector<Finding>> found = checker.run();
	if(!found) return found;
	std::ranges::move(duplicateIdFindings(document), std::back_inserter(*found));
	std::ranges::sort(*found);
	return found;
}

// An id names one node: "#id" paths, rules and anchors would find only the first
std::vector<Finding> duplicateIdFindings(const json& document) {
	std::vector<Finding> found;
	for(const DuplicateId& duplicate : duplicateIds(document))
		for(std::size_t i = 1; i < duplicate.pointers.size(); ++i)
			found.push_back({placeOf(duplicate.pointers[i]),
			                 "id '" + duplicate.id + "' is taken by " + placeOf(duplicate.pointers[0]),
			                 "an id names one node: give this one another"});
	return found;
}

std::vector<Finding> introduced(const std::vector<Finding>& before, const std::vector<Finding>& after) {
	const auto at = [](const Finding& f) { return std::pair(std::cref(f.place), std::cref(f.problem)); };
	std::set<std::pair<std::string, std::string>> exact, now;
	for(const Finding& f : before) exact.emplace(at(f));
	for(const Finding& f : after) now.emplace(at(f));

	// Findings that moved: their problem is still there, at another place
	std::multiset<std::string> moved;
	for(const Finding& f : before)
		if(!now.contains(at(f))) moved.insert(f.problem);

	std::vector<Finding> result;
	for(const Finding& f : after) {
		if(exact.contains(at(f))) continue;
		if(const auto it = moved.find(f.problem); it != moved.end()) moved.erase(it);
		else result.push_back(f);
	}
	return result;
}

} // namespace affresco::format
