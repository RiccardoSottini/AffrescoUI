/// @file
/// @brief The checker's passes, findings and hints, and the shapes it learns from the state (Checker.hpp).

#include "Checker.hpp"

#include "CheckerImpl.hpp"

#include "affresco/format/NodeIds.hpp"

namespace affresco::format {

std::string Finding::text() const {
	return (place.empty() ? "" : place + ": ") + problem + (hint.empty() ? "" : " (" + hint + ")");
}

namespace checker {

/* Shapes */

void mergeInto(Shape& into, const Shape& other) {
	if(into.is_null()) return;
	if(other.is_null() || into.type() != other.type()) {
		into = anything;
	} else if(into.is_array()) {
		if(other.empty()) return;
		if(into.empty()) into = other;
		else mergeInto(into[0], other[0]);
	} else if(into.is_object()) {
		for(const auto& [key, shape] : other.items()) {
			const auto it = into.find(key);
			if(it == into.end()) into[key] = shape;
			else mergeInto(*it, shape);
		}
	}
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

/// @brief Tells whether a value from the state is of the kind a shape describes.
/// @param shape The shape.
/// @param data The value.
/// @return true for a non-empty object and an object shape, a list and a list shape, a single value and its shape.
bool sameKind(const Shape& shape, const json& data) {
	if(data.is_object()) return !data.empty() && shape.is_object();
	if(data.is_array()) return shape.is_array();
	return !data.is_null() && shape.is_boolean();
}

/// @brief Merges what a value from the state holds into a shape, in place: a list of thousands of items costs no
/// copies.
/// @param shape The shape so far.
/// @param data The value.
void absorb(Shape& shape, const json& data) {
	if(shape.is_null()) return;
	if(!sameKind(shape, data)) {
		shape = anything;
	} else if(data.is_object()) {
		for(const auto& [key, value] : data.items()) {
			const auto it = shape.find(key);
			if(it == shape.end()) shape[key] = shapeOfData(value);
			else absorb(*it, value);
		}
	} else if(data.is_array()) {
		for(const json& item : data) {
			if(shape.empty()) shape.push_back(shapeOfData(item));
			else absorb(shape[0], item);
			if(shape[0].is_null()) break; // nothing more can be known
		}
	}
}

} // namespace

Shape shapeOfData(const json& data) {
	if(data.is_array()) {
		Shape items = Shape::array();
		absorb(items, data);
		return items;
	}
	if(data.is_object()) {
		if(data.empty()) return anything; // most likely a map, filled later
		Shape fields = Shape::object();
		for(const auto& [key, value] : data.items()) fields[key] = shapeOfData(value);
		return fields;
	}
	return data.is_null() ? anything : single;
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
		for(const auto& [key, value] : state.items()) state_[key] = shapeOfData(value);
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
	if(const json* keys = section("keys"))
		walkKeys(*keys, none, "keys");
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
	const auto it = entries->find(name);
	return it != entries->end() ? &*it : nullptr;
}

std::vector<std::string> Checker::definedNames(const char* sectionName) const {
	const json* entries = section(sectionName);
	return entries ? keysOf(*entries) : std::vector<std::string>{};
}

void Checker::refer(std::string_view kind, const char* sectionName, const std::string& name, std::string_view prefix,
                    const std::string& place) {
	if(definition(sectionName, name)) return;
	if(std::string_view(sectionName) == "colors" && definition("tokens", name)) return; // a colour token
	report(place, "unknown " + std::string(kind) + " '" + std::string(prefix) + name + "'",
	       hint([&] { return hintFor(name, definedNames(sectionName), prefix, "defined:"); }));
}

} // namespace checker

std::optional<std::vector<Finding>> check(const json& document, const json& state,
                                          const std::set<std::string, std::less<>>& nativeActions) {
	checker::Checker checker(document, state, nativeActions);
	std::optional<std::vector<Finding>> found = checker.run();
	if(!found) return found;
	// An id names one node: "#id" paths, rules and anchors would find only the first
	for(const DuplicateId& duplicate : duplicateIds(document))
		for(std::size_t i = 1; i < duplicate.pointers.size(); ++i)
			found->push_back({placeOf(duplicate.pointers[i]),
			                  "id '" + duplicate.id + "' is taken by " + placeOf(duplicate.pointers[0]),
			                  "an id names one node: give this one another"});
	std::ranges::sort(*found);
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
