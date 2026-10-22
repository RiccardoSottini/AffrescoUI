/// @file
/// @brief vcs::ChangeSetRepairs: installing change sets that carry a proof, and repairing them through review.

#include "affresco/vcs/ChangeSetRepair.hpp"

#include "permissions/Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include <algorithm>
#include <utility>

namespace affresco::vcs {

using expect::fail;

namespace {

/// @brief A change set's name, as importChanges() names its author.
/// @param changes The change set.
/// @return Its "name", or "changes".
[[nodiscard]] std::string nameOf(const json& changes) { return changes.value("name", std::string("changes")); }

/// @brief Checks the repair fields of a change set, where present.
/// @param changes The change set.
/// @throws DocumentError Naming the field.
void checkFields(const json& changes) {
	if(changes.contains("request") &&
	   (!changes["request"].is_string() ||
	    changes["request"].get_ref<const std::string&>().size() > ChangeSetRepairs::maxRequestBytes))
		fail("changes.request", "expected words, at most " + std::to_string(ChangeSetRepairs::maxRequestBytes) +
		                            " bytes");
	if(changes.contains("template") && !changes["template"].is_object())
		fail("changes.template", "expected {name, commit, tree}");
	if(changes.contains("proof") &&
	   (!changes["proof"].is_object() || !changes["proof"].contains("scenarios") ||
	    !changes["proof"]["scenarios"].is_array()))
		fail("changes.proof", "expected {\"scenarios\": [...]}");
}

} // namespace

std::string_view toString(RepairStatus status) noexcept {
	switch(status) {
	case RepairStatus::Applied: return "applied";
	case RepairStatus::NeedsRepair: return "needsRepair";
	case RepairStatus::Repairing: return "repairing";
	case RepairStatus::Repaired: return "repaired";
	case RepairStatus::GaveUp: return "gaveUp";
	}
	return "applied";
}

ChangeSetRepairs::ChangeSetRepairs(Repository& repository, std::string templateName)
	: repository_(repository), templateName_(std::move(templateName)),
	  templateTree_(detail::sha256(detail::view(repository.document(), repository.state()).dump())),
	  templateCommit_(repository.resolve(json(0)).id) {
	decisions_ = repository_.onDecision.connect([this](const Decision& decision) { decided(decision); });
}

ChangeSetRepairs::~ChangeSetRepairs() { repository_.onDecision.disconnect(decisions_); }

json ChangeSetRepairs::currentBase() const {
	return {{"name", templateName_}, {"commit", templateCommit_}, {"tree", templateTree_}};
}

json ChangeSetRepairs::describe(json changes, std::string_view request, const json& base, const json& proof) {
	changes["request"] = std::string(request);
	changes["template"] = base;
	if(!proof.is_null()) changes["proof"] = proof;
	return changes;
}

std::optional<std::string> ChangeSetRepairs::checkProof(const json& changes, const json& document) const {
	if(!changes.contains("proof")) return std::nullopt;
	if(!check_) return "no proof check";
	return check_(document, changes["proof"]);
}

std::optional<std::string> ChangeSetRepairs::problemWith(const json& changes) const {
	json document;
	try {
		document = repository_.tryChanges(changes);
	} catch(const DocumentError& error) {
		return "the patch no longer applies: " + detail::reasonOf(error);
	}
	if(const auto failed = checkProof(changes, document)) return "its scenarios fail: " + *failed;
	return std::nullopt;
}

RepairStatus ChangeSetRepairs::install(const json& changes) {
	expect::object(changes, "changes");
	checkFields(changes);
	InstalledChangeSet* installed = find(nameOf(changes));
	if(!installed) {
		if(installed_.size() >= maxChangeSets)
			fail("changes", "at most " + std::to_string(maxChangeSets) + " change sets per screen");
		installed = &installed_.emplace_back();
	}
	installed->changes = changes;
	installed->reason.clear();
	installed->status = RepairStatus::Applied;
	if(repository_.hasImported(changes)) return RepairStatus::Applied;

	if(const auto problem = problemWith(changes)) {
		needsRepair(*installed, *problem);
		return installed->status;
	}
	if(const auto error = repository_.importChanges(changes)) {
		needsRepair(*installed, "the patch no longer applies: " + *error);
		return installed->status;
	}
	onMessage(status());
	return RepairStatus::Applied;
}

void ChangeSetRepairs::needsRepair(InstalledChangeSet& installed, std::string reason) {
	installed.status = RepairStatus::NeedsRepair;
	installed.reason = std::move(reason);
	installed.request.clear();
	installed.proposal = 0;
	installed.proposed = nullptr;
	askNext();
}

void ChangeSetRepairs::askNext() {
	if(std::ranges::any_of(installed_, [](const auto& i) { return i.status == RepairStatus::Repairing; })) {
		onMessage(status());
		return;
	}
	for(InstalledChangeSet& installed : installed_) {
		if(installed.status != RepairStatus::NeedsRepair) continue;
		if(installed.attempts >= maxAttempts) {
			installed.status = RepairStatus::GaveUp;
			installed.reason += " (gave up after " + std::to_string(maxAttempts) + " repairs)";
			continue;
		}
		++installed.attempts;
		installed.status = RepairStatus::Repairing;
		installed.request = "repair-" + nameOf(installed.changes) + "-" + std::to_string(installed.attempts);
		onMessage(request(installed));
		break;
	}
	onMessage(status());
}

json ChangeSetRepairs::request(const InstalledChangeSet& installed) const {
	const json& changes = installed.changes;
	const std::string name = nameOf(changes);
	const std::string words = changes.value("request", std::string());
	return {{"type", "request"},
	        {"kind", "repair"},
	        {"id", installed.request},
	        {"from", "host"},
	        {"text", "Repair the change set '" + name + "'" + (words.empty() ? "" : ": " + words)},
	        {"repair",
	         {{"name", name},
	          {"request", words},
	          {"oldBase", changes.value("template", json())},
	          {"newBase", currentBase()},
	          {"failed", installed.reason},
	          {"revisions", changes["revisions"]},
	          {"proof", changes.value("proof", json())},
	          {"attempt", installed.attempts},
	          {"maxAttempts", maxAttempts}}}};
}

std::optional<json> ChangeSetRepairs::openRequest() const {
	for(const InstalledChangeSet& installed : installed_)
		if(installed.status == RepairStatus::Repairing && installed.proposal == 0) return request(installed);
	return std::nullopt;
}

json ChangeSetRepairs::status() const {
	json rows = json::array();
	for(const InstalledChangeSet& installed : installed_)
		rows.push_back({{"name", nameOf(installed.changes)},
		                {"status", toString(installed.status)},
		                {"reason", installed.reason},
		                {"attempts", installed.attempts},
		                {"proposal", installed.proposal},
		                {"request", installed.changes.value("request", std::string())},
		                {"repairedFrom", installed.changes.value("repairedFrom", json())}});
	return {{"type", "repairs"}, {"changeSets", rows}};
}

void ChangeSetRepairs::handle(const json& message, const std::function<void(const json&)>& answer) {
	if(!message.is_object()) return;
	const std::function<void(const json&)> reply = [&](const json& out) { // carries the asker's id back
		json echoed = out;
		if(message.contains("id")) echoed["id"] = message["id"];
		answer(echoed);
	};
	if(message.contains("revision")) return propose(message, reply);
	if(message.contains("retry")) return retry(message, reply);
	reply(status());
}

void ChangeSetRepairs::propose(const json& message, const std::function<void(const json&)>& answer) {
	const std::string id = message.value("request", std::string());
	const auto open = std::ranges::find_if(installed_, [&](const InstalledChangeSet& i) {
		return i.status == RepairStatus::Repairing && i.proposal == 0 && i.request == id;
	});
	if(open == installed_.end() || !message["revision"].is_object())
		return answer({{"type", "error"}, {"message", "repair: no open repair request '" + id + "'"}});

	json revision = message["revision"];
	revision["author"] = message.value("from", std::string("agent")); // the channel's, never the message's
	revision.erase("parent");
	revision.erase("proposal");
	json candidate = {{"format", "affrescoui-changes"}, {"version", 2}, {"name", nameOf(open->changes)},
	                  {"revisions", json::array({revision})}};
	for(const char* field : {"request", "proof"})
		if(open->changes.contains(field)) candidate[field] = open->changes[field];
	if(const auto problem = problemWith(candidate)) {
		answer({{"type", "error"}, {"message", "repair: " + *problem}});
		return needsRepair(*open, "its repair failed: " + *problem);
	}

	const RevisionResult result = repository_.propose(revision);
	if(result.status != RevisionStatus::Proposed) {
		answer({{"type", "error"}, {"message", "repair: " + result.error}});
		return needsRepair(*open, "its repair was refused: " + result.error);
	}
	open->proposal = result.proposal;
	answer({{"type", "proposed"}, {"request", id}, {"proposal", result.proposal}, {"commit", result.commit}});
	onMessage(status());
}

void ChangeSetRepairs::retry(const json& message, const std::function<void(const json&)>& answer) {
	const std::string from = message.value("from", std::string());
	InstalledChangeSet* installed = message["retry"].is_string() ? find(message["retry"].get<std::string>()) : nullptr;
	if(!repository_.mayReview(from) || !installed || installed->status == RepairStatus::Repairing)
		return answer({{"type", "error"}, {"message", "repair: only a reviewer retries a change set that is off"}});
	installed->attempts = 0;
	const json changes = installed->changes;
	install(changes);
	answer(status());
}

void ChangeSetRepairs::decided(const Decision& decision) {
	const auto it = std::ranges::find_if(installed_, [&](const InstalledChangeSet& i) {
		return i.status == RepairStatus::Repairing && i.proposal == decision.proposal;
	});
	if(it == installed_.end() || decision.proposal == 0) return;
	if(decision.error) return needsRepair(*it, "the reviewer rejected its repair: " + *decision.error);

	const json old = it->changes;
	json repaired = repository_.exportChanges(json(decision.revision - 1), nameOf(old));
	for(const char* field : {"request", "proof"})
		if(old.contains(field)) repaired[field] = old[field];
	repaired["template"] = currentBase();
	repaired["repairedFrom"] = {{"name", nameOf(old)},
	                            {"template", old.value("template", json())},
	                            {"changes", detail::sha256(old.dump())},
	                            {"proposal", decision.proposal}};
	it->changes = std::move(repaired);
	it->request.clear();
	if(const auto failed = checkProof(it->changes, detail::view(repository_.document(), repository_.state()))) {
		it->attempts = maxAttempts; // landed by a reviewer, yet failing: a person looks at it next
		return needsRepair(*it, "its scenarios fail after its repair: " + *failed);
	}
	repository_.recordImported(it->changes);
	it->status = RepairStatus::Repaired;
	it->reason.clear();
	it->attempts = 0;
	askNext();
}

InstalledChangeSet* ChangeSetRepairs::find(std::string_view name) {
	const auto it = std::ranges::find_if(installed_, [&](const auto& i) { return nameOf(i.changes) == name; });
	return it == installed_.end() ? nullptr : &*it;
}

} // namespace affresco::vcs
