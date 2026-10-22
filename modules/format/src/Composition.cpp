/// @file
/// @brief Composition: a document's enabled features merged into the screen the runtime builds, with RFC 0004's
/// checks (requirements, namespaces, conflicts) before each merge.

#include "Composition.hpp"
#include "FeatureManifest.hpp"

#include "affresco/format/NodeIds.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <utility>

namespace affresco::format {

namespace {

/// The sections a feature's declarations join under `<name>.<key>`.
constexpr auto prefixedSections = std::to_array<std::string_view>({"actions", "components", "styles", "settings"});

/// @brief Escapes a key for a JSON Pointer (RFC 6901).
/// @param key The key.
/// @return It, with '~' as "~0" and '/' as "~1".
std::string pointerKey(std::string_view key) {
	std::string text;
	for(const char c : key) text += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
	return text;
}

/// @brief Tells whether a character can be part of a name in an expression, so a match next to it is no whole name.
/// @param c The character.
/// @return true for a letter, a digit, '_' or '.'.
bool nameCharacter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
}

/// @brief Replaces every whole `app.<name>` in a text.
/// @param text The text.
/// @param name The export's name ("editor.text").
/// @param expression What it stands for.
/// @return The text, changed.
std::string replaceName(std::string text, const std::string& name, const std::string& expression) {
	const std::string target = "app." + name;
	for(std::size_t at = text.find(target); at != std::string::npos; at = text.find(target, at)) {
		const std::size_t end = at + target.size();
		if((at > 0 && nameCharacter(text[at - 1])) || (end < text.size() && nameCharacter(text[end]) && text[end] != '.') ||
		   (end < text.size() && text[end] == '.' && end + 1 < text.size() && !nameCharacter(text[end + 1]))) {
			at = end;
			continue;
		}
		text.replace(at, target.size(), expression);
		at += expression.size();
	}
	return text;
}

/// @brief The expression inside "{{...}}".
/// @param value "{{ expr }}".
/// @return "expr", or std::nullopt if the text isn't exactly one expression.
std::optional<std::string> soleExpression(const std::string& value) {
	const auto first = value.find_first_not_of(" \t\n");
	const auto last = value.find_last_not_of(" \t\n");
	if(first == std::string::npos || value.compare(first, 2, "{{") != 0 || last < first + 3 ||
	   value.compare(last - 1, 2, "}}") != 0)
		return std::nullopt;
	const std::string inner = value.substr(first + 2, last - 1 - (first + 2));
	if(inner.find("{{") != std::string::npos || inner.find("}}") != std::string::npos) return std::nullopt;
	return inner;
}

/// @brief Replaces `app.<name>` with the exports' expressions in every text of a value: `(expr)` inside "{{...}}",
/// the bare expression in a path ("in": "app.workspace.files").
/// @param value The value, changed in place.
/// @param exports The exports, longest names first.
/// @param depth How deep the walk is, bounded by maxNesting.
void replaceExports(json& value, const std::vector<std::pair<std::string, std::string>>& exports, int depth = 0) {
	if(depth > maxNesting) return;
	if(value.is_structured()) {
		for(auto& item : value) replaceExports(item, exports, depth + 1);
		return;
	}
	if(!value.is_string() || value.get_ref<const std::string&>().find("app.") == std::string::npos) return;
	std::string text = value.get<std::string>();
	const bool expression = text.find("{{") != std::string::npos;
	for(const auto& [name, replacement] : exports)
		text = replaceName(std::move(text), name, expression ? "(" + replacement + ")" : replacement);
	value = std::move(text);
}

/// @brief Gives every `"use": "<alias>.<component>"` in a value the feature's name instead of the alias.
/// @param value The value, changed in place.
/// @param aliases Alias to feature.
/// @param depth How deep the walk is, bounded by maxNesting.
void resolveImports(json& value, const std::map<std::string, std::string, std::less<>>& aliases, int depth = 0) {
	if(depth > maxNesting || !value.is_structured()) return;
	if(value.is_object())
		if(const auto use = value.find("use"); use != value.end() && use->is_string()) {
			const std::string& name = use->get_ref<const std::string&>();
			if(const auto dot = name.find('.'); dot != std::string::npos)
				if(const auto alias = aliases.find(std::string_view(name).substr(0, dot)); alias != aliases.end())
					*use = alias->second + name.substr(dot);
		}
	for(auto& item : value) resolveImports(item, aliases, depth + 1);
}

/// @brief Collects the ids in a node tree.
/// @param node The node.
/// @param ids Receives them.
/// @param depth How deep the walk is, bounded by maxNesting.
void idsIn(const json& node, std::vector<std::string>& ids, int depth = 0) {
	if(depth > maxNesting || !node.is_object()) return;
	if(const auto id = node.find("id"); id != node.end() && id->is_string()) ids.push_back(id->get<std::string>());
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(const auto& child : *children) idsIn(child, ids, depth + 1);
}

/// @brief Collects the contribution points of a node tree.
/// @param node The node.
/// @param pointer Its JSON Pointer.
/// @param points Receives point name to node pointer.
/// @param problems Receives a point declared twice.
/// @param depth How deep the walk is, bounded by maxNesting.
void pointsIn(const json& node, const std::string& pointer, std::map<std::string, std::string, std::less<>>& points,
              std::vector<std::string>& problems, int depth = 0) {
	if(depth > maxNesting || !node.is_object()) return;
	if(const auto point = node.find("point"); point != node.end() && point->is_string())
		if(!points.emplace(point->get<std::string>(), pointer).second)
			problems.push_back(std::format("{}.point: the contribution point '{}' is declared twice", placeOf(pointer),
			                               point->get<std::string>()));
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(std::size_t index = 0; index < children->size(); ++index)
			pointsIn((*children)[index], std::format("{}/children/{}", pointer, index), points, problems, depth + 1);
}

/// @brief Removes "point" from every node of a tree: it is only composition's to read.
/// @param node The node.
/// @param depth How deep the walk is, bounded by maxNesting.
void dropPoints(json& node, int depth = 0) {
	if(depth > maxNesting || !node.is_object()) return;
	node.erase("point");
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(auto& child : *children) dropPoints(child, depth + 1);
}

/// @brief Tells whether an object has a key starting with a prefix.
/// @param object The object (or anything else, which has none).
/// @param prefix The prefix.
/// @return The first such key, or std::nullopt.
std::optional<std::string> keyStartingWith(const json& object, const std::string& prefix) {
	if(!object.is_object()) return std::nullopt;
	for(const auto& [key, value] : object.items())
		if(key.starts_with(prefix)) return key;
	return std::nullopt;
}

/// @brief Merges a document's features into its screen, one at a time, checking each first.
class Composer {
public:
	/// @brief Starts from a document: the screen without the sections only composition reads.
	/// @param document The document.
	explicit Composer(const json& document) : document_(document), screen_(document) {
		screen_.erase("features");
		screen_.erase("exports");
		screen_.erase("imports");
		// Points may sit in components too: an app's status bar is often one (Forge's is).
		for(const auto& [pointer, tree] : nodeTrees(screen_)) pointsIn(*tree, pointer, points_, problems_);
		if(const auto exports = document.find("exports"); exports != document.end() && exports->is_object())
			exports_ = *exports;
		for(const std::string& id : nodeIds(screen_)) ids_.insert(id);
		if(const auto keys = screen_.find("keys"); keys != screen_.end() && keys->is_object())
			for(const auto& [key, handler] : keys->items()) keyOwners_.emplace(key, "the app");
	}

	/// @brief Merges every enabled feature, then resolves the imports.
	/// @return The screen and the problems.
	Composition run() && {
		for(const std::string& name : mergeOrder()) {
			const json& record = document_["features"][name];
			const std::string place = "features." + name;
			const std::size_t before = problems_.size();
			checkRequirements(name, record, place);
			checkNamespace(name, record["provides"], place + ".provides");
			checkConflicts(name, record["provides"], place + ".provides");
			if(problems_.size() == before) merge(name, record);
		}
		resolve();
		for(const auto& [point, pointer] : points_) dropPoints(screen_[json::json_pointer(pointer)]);
		return {std::move(screen_), std::move(problems_)};
	}

private:
	/// @brief The enabled features in the order they merge: by "order", then name, each after the ones it requires.
	/// A dependency that isn't enabled, or a cycle, is a problem and leaves its features out.
	/// @return Their names.
	std::vector<std::string> mergeOrder() {
		std::vector<std::pair<std::pair<long long, std::string>, std::vector<std::string>>> waiting;
		if(const auto features = document_.find("features"); features != document_.end() && features->is_object())
			for(const auto& [name, record] : features->items()) {
				if(!record.value("enabled", false)) continue;
				std::vector<std::string> needs;
				if(const auto wanted = record.find("requires"); wanted != record.end() && wanted->contains("features"))
					for(const auto& [other, range] : (*wanted)["features"].items()) needs.push_back(other);
				waiting.push_back({{record.value("order", 0LL), name}, std::move(needs)});
			}
		std::ranges::sort(waiting);
		std::vector<std::string> order;
		std::set<std::string, std::less<>> placed;
		for(bool progress = true; progress && !waiting.empty();) {
			progress = false;
			for(auto it = waiting.begin(); it != waiting.end(); ++it)
				if(std::ranges::all_of(it->second, [&](const std::string& other) {
					   return placed.contains(other) || !enabled(other);
				   })) {
					placed.insert(it->first.second);
					order.push_back(it->first.second);
					waiting.erase(it);
					progress = true;
					break;
				}
		}
		for(const auto& [key, needs] : waiting)
			problems_.push_back(std::format("features.{}.requires.features: a dependency cycle", key.second));
		return order;
	}

	/// @brief Tells whether the document has a feature, enabled.
	/// @param name Its name.
	/// @return true if it does.
	bool enabled(const std::string& name) const {
		const auto features = document_.find("features");
		return features != document_.end() && features->contains(name) && (*features)[name].value("enabled", false);
	}

	/// @brief Adds a problem.
	/// @param place Its place.
	/// @param message What is wrong.
	void problem(const std::string& place, const std::string& message) { problems_.push_back(place + ": " + message); }

	/// @brief Checks what a feature requires (check 2): points, exports, services, connections, features, format.
	/// @param name The feature.
	/// @param record Its record.
	/// @param place Its place.
	void checkRequirements(const std::string& name, const json& record, const std::string& place) {
		(void)name;
		const json needs = record.value("requires", json::object());
		const std::string at = place + ".requires";
		if(needs.contains("format") && needs["format"] != document_.value("version", json()))
			problem(at + ".format", std::format("needs format {}", needs["format"].dump()));
		for(const auto& point : needs.value("points", json::array())) checkPoint(point.get<std::string>(), at + ".points");
		for(const auto& [exported, wanted] : needs.value("exports", json::object()).items()) {
			if(!exports_.contains(exported)) problem(at + ".exports." + exported, "the app doesn't export it");
			else if(exports_[exported].value("type", json()) != wanted["type"])
				problem(at + ".exports." + exported, std::format("needs a {}, the app exports a {}", wanted["type"].dump(),
				                                                 exports_[exported].value("type", json()).dump()));
		}
		for(const char* section : {"services", "connections"})
			for(const auto& [declared, methods] : needs.value(section, json::object()).items())
				if(!document_.contains(section) || !document_[section].contains(declared))
					problem(std::format("{}.{}.{}", at, section, declared),
					        std::format("the app declares no {} '{}' (a host must grant it: a revision can't)",
					                    std::string_view(section).substr(0, std::string_view(section).size() - 1),
					                    declared));
		for(const auto& [other, range] : needs.value("features", json::object()).items()) {
			if(!merged_.contains(other))
				problem(at + ".features." + other, enabled(other) ? "can't be merged" : "not installed, or disabled");
			else if(!satisfies(merged_.at(other), range.get<std::string>()))
				problem(at + ".features." + other,
				        std::format("needs {}, {} is installed", range.get<std::string>(), merged_.at(other)));
		}
	}

	/// @brief Checks that a point a feature requires exists: a node's point, or a menu.
	/// @param point The point.
	/// @param place Its place.
	void checkPoint(const std::string& point, const std::string& place) {
		if(points_.contains(point) || (screen_.contains("menus") && screen_["menus"].contains(point))) return;
		std::string offered;
		for(const auto& [name, pointer] : points_) offered += (offered.empty() ? "" : ", ") + name;
		problem(place, std::format("the app has no contribution point '{}' (it has: {})", point,
		                           offered.empty() ? "none" : offered));
	}

	/// @brief Checks a feature's namespace (check 3): the app uses none of its names, and its ids are free.
	/// @param name The feature.
	/// @param provides What it provides.
	/// @param place Its place.
	void checkNamespace(const std::string& name, const json& provides, const std::string& place) {
		for(const char* section : {"state", "computed"})
			if(screen_.contains(section) && screen_[section].is_object() && screen_[section].contains(name))
				problem(place, std::format("the namespace '{}' is taken: the app's {} has '{}'", name, section, name));
		for(const std::string_view section : prefixedSections)
			if(const auto key = keyStartingWith(screen_.value(section, json()), name + "."))
				problem(place, std::format("the namespace '{}' is taken: the app's {} has '{}'", name, section, *key));
		for(const auto& [point, nodes] : provides.value("views", json::object()).items()) {
			if(!points_.contains(point))
				problem(place + ".views." + point, std::format("the app has no contribution point '{}'", point));
			for(const auto& [id, node] : nodes.items()) {
				std::vector<std::string> ids = {id};
				idsIn(node, ids);
				for(const std::string& each : ids)
					if(ids_.contains(each)) problem(std::format("{}.views.{}.{}", place, point, id), std::format("id '{}' is taken", each));
			}
		}
	}

	/// @brief Checks a feature's conflicts (check 5): keys bound already, menus missing or holding its items.
	/// @param name The feature.
	/// @param provides What it provides.
	/// @param place Its place.
	void checkConflicts(const std::string& name, const json& provides, const std::string& place) {
		for(const auto& [key, handler] : provides.value("keys", json::object()).items())
			if(const auto owner = keyOwners_.find(key); owner != keyOwners_.end())
				problem(place + ".keys." + key, owner->second + " binds it already (give one a \"when\")");
		for(const auto& [menu, items] : provides.value("menus", json::object()).items()) {
			if(!screen_.contains("menus") || !screen_["menus"].contains(menu)) {
				problem(place + ".menus." + menu, std::format("the app has no menu '{}'", menu));
				continue;
			}
			for(const auto& [item, entry] : (items.contains("items") ? items["items"] : items).items())
				if(screen_["menus"][menu].contains(name + "." + item))
					problem(place + ".menus." + menu + "." + item, "the menu has that item already");
		}
	}

	/// @brief Merges one feature that passed its checks.
	/// @param name The feature.
	/// @param record Its record.
	void merge(const std::string& name, const json& record) {
		json provides = record["provides"];
		replaceExports(provides, exportsByLength());
		if(provides.contains("state")) screen_["state"][name] = provides["state"];
		if(provides.contains("computed") && !provides["computed"].empty()) mergeComputed(name, provides["computed"]);
		for(const std::string_view section : prefixedSections)
			for(const auto& [key, value] : provides.value(section, json::object()).items())
				screen_[section][name + "." + key] = value;
		for(const auto& [key, handler] : provides.value("keys", json::object()).items()) {
			screen_["keys"][key] = handler;
			keyOwners_.emplace(key, std::format("feature '{}'", name));
		}
		for(const auto& [menu, items] : provides.value("menus", json::object()).items())
			for(const auto& [item, entry] : (items.contains("items") ? items["items"] : items).items()) {
				json merged = entry;
				if(!merged.contains("run") && !merged.contains("submenu")) merged["run"] = name + "." + item;
				screen_["menus"][menu][name + "." + item] = std::move(merged);
			}
		for(const auto& [point, nodes] : provides.value("views", json::object()).items())
			for(const auto& [id, node] : nodes.items()) mergeView(point, id, node);
		for(const auto& [key, entry] : provides.value("exports", json::object()).items())
			exports_[name + "." + key] = entry;
		merged_.emplace(name, record["release"].get<std::string>());
	}

	/// @brief Merges a feature's computed values as one object, `computed.<name>.<key>`.
	/// @param name The feature.
	/// @param computed Its computed values, each one "{{...}}".
	void mergeComputed(const std::string& name, const json& computed) {
		std::string fields;
		for(const auto& [key, value] : computed.items()) {
			const auto expression = soleExpression(value.get<std::string>());
			if(!expression) {
				problem(std::format("features.{}.provides.computed.{}", name, key), "expected one expression, \"{{...}}\"");
				return;
			}
			fields += std::format("{}{}: ({})", fields.empty() ? "" : ", ", key, *expression);
		}
		screen_["computed"][name] = "{{ {" + fields + "} }}";
	}

	/// @brief Appends a view to its point's children, after the app's own and the earlier features'.
	/// @param point The point.
	/// @param id The view's id.
	/// @param node The view.
	void mergeView(const std::string& point, const std::string& id, const json& node) {
		json view = {{"id", id}};
		for(const auto& [key, value] : node.items()) view[key] = value;
		std::vector<std::string> ids;
		idsIn(view, ids);
		ids_.insert(ids.begin(), ids.end());
		json& children = screen_[json::json_pointer(points_.at(point))]["children"];
		if(!children.is_array()) children = json::array();
		children.push_back(std::move(view));
	}

	/// @brief The exports with their expressions, longest names first so "editor.textLength" wins over "editor.text".
	/// @return Name and expression.
	std::vector<std::pair<std::string, std::string>> exportsByLength() const {
		std::vector<std::pair<std::string, std::string>> found;
		for(const auto& [name, entry] : exports_.items())
			if(const auto expression = soleExpression(entry.value("value", std::string())))
				found.emplace_back(name, *expression);
		std::ranges::stable_sort(found, std::ranges::greater{}, [](const auto& item) { return item.first.size(); });
		return found;
	}

	/// @brief Resolves the imports: each alias names a merged feature in its range, and its components' uses are
	/// renamed.
	void resolve() {
		const auto imports = document_.find("imports");
		if(imports == document_.end() || !imports->is_object() || imports->empty()) return;
		std::map<std::string, std::string, std::less<>> aliases;
		for(const auto& [alias, text] : imports->items()) {
			const auto parts = splitImport(text.get<std::string>());
			if(!parts || !merged_.contains(parts->first)) {
				problem("imports." + alias, std::format("the feature '{}' isn't installed", parts ? parts->first : ""));
				continue;
			}
			if(!satisfies(merged_.at(parts->first), parts->second)) {
				problem("imports." + alias, std::format("needs {}, {} is installed", parts->second, merged_.at(parts->first)));
				continue;
			}
			aliases.emplace(alias, parts->first);
		}
		for(const char* section : {"root", "screens", "templates", "components"})
			if(screen_.contains(section)) resolveImports(screen_[section], aliases);
	}

	const json& document_;                                         ///< The document.
	json screen_;                                                  ///< The screen being composed.
	json exports_ = json::object();                                ///< The app's exports, then the features'.
	std::map<std::string, std::string, std::less<>> points_;       ///< Point to node pointer.
	std::set<std::string, std::less<>> ids_;                       ///< Node ids taken.
	std::map<std::string, std::string, std::less<>> keyOwners_;    ///< Key to who binds it, for messages.
	std::map<std::string, std::string, std::less<>> merged_;       ///< Merged feature to its release.
	std::vector<std::string> problems_;                            ///< See Composition::problems.
};

/// @brief Finds a node with an id in a view, depth first.
/// @param node The node.
/// @param pointer Its JSON Pointer.
/// @param id The id.
/// @param depth How deep the walk is, bounded by maxNesting.
/// @return The pointer of the node, or std::nullopt.
std::optional<std::string> findInView(const json& node, const std::string& pointer, std::string_view id, int depth) {
	if(depth > maxNesting || !node.is_object()) return std::nullopt;
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(std::size_t index = 0; index < children->size(); ++index) {
			const json& child = (*children)[index];
			const std::string at = std::format("{}/children/{}", pointer, index);
			if(child.is_object() && child.value("id", json()) == id) return at;
			if(auto found = findInView(child, at, id, depth + 1)) return found;
		}
	return std::nullopt;
}

} // namespace

bool needsComposition(const json& document) {
	for(const char* section : {"features", "imports"})
		if(const auto found = document.find(section); found != document.end() && found->is_object() && !found->empty())
			return true;
	return false;
}

Composition compose(const json& document) { return Composer(document).run(); }

json composeOrFail(const json& document) {
	Composition composed = compose(document);
	if(!composed.problems.empty()) expect::fail("", composed.problems.front());
	return std::move(composed.screen);
}

std::optional<std::string> findFeatureNode(const json& document, std::string_view id) {
	const auto features = document.find("features");
	if(features == document.end() || !features->is_object()) return std::nullopt;
	for(const auto& [name, record] : features->items()) {
		if(!record.is_object() || !record.contains("provides") || !record["provides"].contains("views")) continue;
		for(const auto& [point, nodes] : record["provides"]["views"].items()) {
			if(!nodes.is_object()) continue;
			const std::string base = std::format("/features/{}/provides/views/{}", pointerKey(name), pointerKey(point));
			for(const auto& [key, node] : nodes.items()) {
				const std::string at = base + "/" + pointerKey(key);
				if(key == id) return at;
				if(auto found = findInView(node, at, id, 0)) return found;
			}
		}
	}
	return std::nullopt;
}

json featureStates(const json& document) {
	json states = json::object();
	const auto features = document.find("features");
	if(features == document.end() || !features->is_object()) return states;
	for(const auto& [name, record] : features->items())
		if(record.is_object() && record.value("enabled", false) && record.contains("provides") &&
		   record["provides"].contains("state"))
			states[name] = record["provides"]["state"];
	return states;
}

} // namespace affresco::format
