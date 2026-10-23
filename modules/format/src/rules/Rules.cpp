#include "affresco/format/Rules.hpp"

#include "affresco/format/Migrate.hpp"
#include "affresco/format/NodeIds.hpp"

#include "Checker.hpp"
#include "Composition.hpp"
#include "ContributionPoints.hpp"
#include "SectionRegistry.hpp"
#include "ServiceRules.hpp"
#include "Limits.hpp"
#include "RulesControl.hpp"
#include "StateTypes.hpp"

#include <optional>
#include <string_view>
#include <utility>

namespace affresco::format {

using expect::fail;

namespace {

/// The most findings a rejected revision lists; it says how many more there are.
constexpr std::size_t maxListed = 10;

/// @brief The screen a document builds: itself, or composed with its features (Composition.hpp).
/// @param document The document.
/// @param storage Holds the composed screen when there is one.
/// @return The screen.
const json& screenOf(const json& document, json& storage) {
	// NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter): callers pass documents they hold
	if(!needsComposition(document)) return document;
	storage = compose(document).screen;
	return storage;
}

/// @brief The state a document is checked against.
/// @param document The document, with the live state as its "state".
/// @return Its state, or an empty object.
const json& stateOf(const json& document) {
	static const json none = json::object();
	const auto it = document.find("state");
	return it != document.end() ? *it : none;
}

/// @brief Says which type a mismatch breaks, for its message.
/// @param mismatch The mismatch, at "state.<key>..." .
/// @return " (stateTypes.<key>)".
std::string typeNote(const Mismatch& mismatch) {
	const std::string_view place = std::string_view(mismatch.place).substr(std::string_view("state.").size());
	return " (stateTypes." + std::string(place.substr(0, place.find_first_of(".["))) + ")";
}

/// @brief Refuses a document larger than maxDocumentBytes without its "state", or whose "state" is larger than
/// maxStateBytes.
/// @param document The document.
/// @throws DocumentError If it is.
void checkDocumentSize(const json& document) {
	std::size_t bytes = 0;
	for(const auto& [key, value] : document.items())
		if(key != "state") bytes += key.size() + approximateBytes(value, maxDocumentBytes);
	if(bytes > maxDocumentBytes)
		fail("", "the screen is larger than " + std::to_string(maxDocumentBytes >> 20) + " MiB of JSON");
	if(approximateBytes(stateOf(document), maxStateBytes) > maxStateBytes)
		fail("state", "larger than " + std::to_string(maxStateBytes >> 20) + " MiB of JSON");
}

/// @brief Refuses a revision after which the live state is larger than maxStateBytes, and larger than before.
/// @param before The document with the live state in "state", before the revision.
/// @param after The same, after it.
/// @throws DocumentError If it would be.
void checkStateSize(const json& before, const json& after) {
	const std::size_t now = approximateBytes(stateOf(after), maxStateBytes);
	if(now > maxStateBytes && now > approximateBytes(stateOf(before)))
		fail("revision", "the state would be larger than " + std::to_string(maxStateBytes >> 20) + " MiB of JSON");
}

/// @brief A state key's value and declared type before a revision.
/// @param before The document with the live state in "state", before the revision.
/// @param key The key.
/// @return Its value and its type; nullptr for one it didn't have.
std::pair<const json*, const json*> typedBefore(const json& before, const std::string& key) {
	const json& state = stateOf(before);
	const auto value = state.find(key);
	const auto types = before.find("stateTypes");
	const json* type = nullptr;
	if(types != before.end() && types->is_object())
		if(const auto it = types->find(key); it != types->end()) type = &*it;
	return {value == state.end() ? nullptr : &*value, type};
}

/// @brief Refuses a revision after which the live state matches its types less than before: a value that no longer
/// matches, or a type the state doesn't match. Mismatches that were there before don't block it.
/// @param before The document with the live state in "state", before the revision.
/// @param after The same, after it.
/// @throws DocumentError If it brings a mismatch.
void checkTypedState(const json& before, const json& after) {
	const auto types = after.find("stateTypes");
	if(types == after.end() || !types->is_object()) return;
	const json& state = stateOf(after);
	// Cost follows the change: a key whose value and type are as before matches as it did
	for(const auto& [key, type] : types->items()) {
		const auto value = state.find(key);
		if(value == state.end()) continue;
		const auto [oldValue, oldType] = typedBefore(before, key);
		if(oldValue && oldType && *oldType == type && *oldValue == *value) continue;
		const std::optional<Mismatch> now = typeMismatch(*value, type, "state." + key);
		if(now && !(oldValue && oldType && typeMismatch(*oldValue, *oldType, "state." + key)))
			fail("revision", "the state wouldn't match its types: " + now->text() + typeNote(*now));
	}
}

} // namespace

Rules::Rules() : sections_(builtInSections()) {}

namespace {

/// @brief The lines for people of a migration's rewrites.
/// @param rewrites The rewrites.
/// @return Their messages.
std::vector<std::string> linesOf(const std::vector<Rewrite>& rewrites) {
	std::vector<std::string> lines;
	lines.reserve(rewrites.size());
	for(const Rewrite& rewrite : rewrites) lines.push_back(rewrite.message);
	return lines;
}

} // namespace

std::vector<std::string> Rules::upgrade(json& document) const { return linesOf(migrate(document)); }

std::vector<std::string> Rules::upgradeRevision(json& patch) const { return linesOf(migratePatch(patch)); }

void Rules::checkRevision(const json& patch) const {
	if(patch.is_array() && patch.size() > maxRevisionOperations)
		fail("revision.patch", std::to_string(patch.size()) + " operations, more than the " +
		                           std::to_string(maxRevisionOperations) + " a revision may have");
	if(approximateBytes(patch, maxRevisionBytes) > maxRevisionBytes)
		fail("revision.patch",
		     "more than " + std::to_string(maxRevisionBytes >> 20) + " MiB of JSON, more than a revision may have");
}

// The document's own "state" must match its types; the live state is checked by checkChange() and findings()
void Rules::validate(const json& doc) const {
	sections_->validate(doc);
	checkDocumentSize(doc);
	json composed;
	if(needsComposition(doc)) { // its features must fit, and what they merge must be valid too
		composed = composeOrFail(doc);
		sections_->validate(composed);
	}
	const json& screen = composed.is_null() ? doc : composed;
	const std::vector<Mismatch> mismatches = stateMismatches(screen, stateOf(screen));
	if(!mismatches.empty()) fail(mismatches.front().place, mismatches.front().problem + typeNote(mismatches.front()));
}

// In root, then screens, templates and components (NodeIds.hpp), then the features' views (Composition.hpp)
std::optional<std::string> Rules::findNode(const json& doc, std::string_view nodeId) const {
	if(auto found = findNodeWithId(doc, nodeId)) return found;
	return findFeatureNode(doc, nodeId);
}

namespace {

/// @brief Reads "permissions.review" (checked by the permissions section).
/// @param review The value, or null.
/// @return The policy; the defaults where a key is missing.
ReviewPolicy reviewPolicyOf(const json& review) {
	ReviewPolicy policy;
	if(!review.is_object()) return policy;
	policy.approvals = review.value("approvals", 1);
	policy.behaviourApprovals = review.value("behaviourApprovals", 0);
	policy.grantsLast = review.value("grantsLast", std::string());
	// A named copy: before C++23 (P2718) a temporary in the range expression ends before the loop's body
	for(const json owners = review.value("owners", json::object()); const auto& [pattern, names] : owners.items())
		policy.owners.emplace_back(pattern, names.get<std::vector<std::string>>());
	return policy;
}

/// @brief Reads a list of permission rules, such as "permissions.allow" (checked by the permissions section).
/// @param section "permissions".
/// @param key The list's key.
/// @param into Gets the rules.
void readRules(const json& section, const char* key, std::vector<PermissionRule>& into) {
	for(const json& text : section.value(key, json::array()))
		if(const auto rule = parsePermissionRule(text.get<std::string>())) into.push_back(*rule);
}

/// @brief Reads "permissions" (checked by the permissions section).
/// @param section Its value, an object.
/// @param result Gets its rules, reviewers and review policy.
void readPermissions(const json& section, Permissions& result) {
	result.configured = true;
	readRules(section, "allow", result.allow);
	readRules(section, "ask", result.ask);
	readRules(section, "deny", result.deny);
	for(const json& name : section.value("reviewers", json::array()))
		result.reviewers.push_back(name.get<std::string>());
	result.review = reviewPolicyOf(section.value("review", json()));
}

/// @brief Denies every node with "locked": true to everyone, in a node and its children.
/// @param node The node (its depth is bounded by the parser's).
/// @param deny Gets a rule for each.
void denyLocked(const json& node, std::vector<PermissionRule>& deny) {
	if(!node.is_object()) return;
	if(node.contains("locked") && node["locked"] == true && node.contains("id") && node["id"].is_string())
		if(auto rule = parsePermissionRule("*(#" + node["id"].get<std::string>() + ")"))
			deny.push_back(std::move(*rule));
	if(node.contains("children") && node["children"].is_array())
		for(const json& child : node["children"]) denyLocked(child, deny);
}

} // namespace

// "permissions" as rules, plus every node with "locked": true denied to everyone
Permissions Rules::permissions(const json& doc) const {
	Permissions result;
	if(doc.contains("permissions") && doc["permissions"].is_object()) readPermissions(doc["permissions"], result);
	if(doc.contains("root")) denyLocked(doc["root"], result.deny);
	for(const char* section : {"screens", "templates"})
		if(doc.contains(section) && doc[section].is_object())
			for(const auto& [name, node] : doc[section].items()) denyLocked(node, result.deny);
	return result;
}

// On the composed screens: what a feature brings is checked as if the app had it
void Rules::checkChange(const json& rawBefore, const json& rawAfter) const {
	for(const SectionType& section : sections_->sections()) {
		if(!section.capability) continue;
		if(rawBefore.value(section.name, json()) != rawAfter.value(section.name, json()))
			fail("revision", "permission denied: revisions can't change \"" + section.name + "\"");
	}
	json composedBefore, composedAfter;
	const json& before = screenOf(rawBefore, composedBefore);
	const json& after = screenOf(rawAfter, composedAfter);

	checkTypedState(before, after);
	checkStateSize(before, after);

	const auto granted = grantedActions(rawBefore); // the host's: a revision can't change what its points pass on
	if(granted != grantedActions(rawAfter))
		fail("revision", "permission denied: revisions can't change the actions a contribution point passes on");
	if(controlsIn(before, granted) != controlsIn(after, granted))
		fail("revision",
		     "permission denied: revisions can't add, change or move accept, reject, preview, endPreview, "
		     "restore, revert, undo, patch, call, send, connect or respond steps, nor run an action that has them, "
		     "a "
		     "native action or a computed one");

	checkServiceUses(before, after, services_);

	// What a revision refers to must exist, so a typo is rejected before a reviewer sees it. Findings that were there
	// before don't block it: the revision didn't bring them. (Most screens have none: then one check is enough.)
	auto now = check(after, stateOf(after), nativeActions_);
	if(now && now->empty()) return;
	auto old = now ? check(before, stateOf(before), nativeActions_) : std::nullopt;
	if(!old) { // too large to check: ids still are, as a node given a locked one's id first would pass for it
		now = duplicateIdFindings(after);
		old = duplicateIdFindings(before);
	}
	const std::vector<Finding> added = introduced(*old, *now);
	if(added.empty()) return;
	std::string message = "the checker found:";
	for(std::size_t i = 0; i < added.size() && i < maxListed; ++i) message += (i ? "; " : " ") + added[i].text();
	if(added.size() > maxListed) message += "; and " + std::to_string(added.size() - maxListed) + " more";
	fail("revision", message);
}

std::vector<std::string> Rules::findings(const json& document, const json& state) const {
	std::vector<std::string> lines;
	json composed;
	if(needsComposition(document)) { // features that don't fit are left out: say why
		Composition composition = compose(document);
		lines = std::move(composition.problems);
		composed = std::move(composition.screen);
	}
	const json& screen = composed.is_null() ? document : composed;
	for(const Mismatch& mismatch : stateMismatches(screen, state)) // a state kept on disk may predate its types
		lines.push_back(mismatch.text() + typeNote(mismatch));
	const auto found = check(screen, state, nativeActions_);
	if(!found) {
		lines.emplace_back("not checked: the screen is larger than the checker's limit");
		return lines;
	}
	for(const Finding& finding : *found) lines.push_back(finding.text());
	return lines;
}

void Rules::addNativeAction(std::string name) { nativeActions_.insert(std::move(name)); }

std::vector<std::string> Rules::reviewLines(const json& before, const json& after) const {
	return serviceReviewLines(before, after, services_);
}

} // namespace affresco::format
