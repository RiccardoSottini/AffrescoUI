/// @file
/// @brief The `features` provider: `catalog` and `status`, then `install`, `upgrade`, `remove`, `enable`, `disable`
/// and `compose` (a feature from another app, with its adapter and proof), each one revision. An upgrade of a record
/// edited in place merges the three records (FeatureUpgrade.hpp), so the edits are kept.

#include "features/FeaturesService.hpp"
#include "CompositionProof.hpp"
#include "CompositionProofRevision.hpp"
#include "FeatureUpgrade.hpp"

#include <affresco/vcs/Merge.hpp>

#include <format>

namespace affresco::detail {

namespace {

using services::Call;
using services::Effect;
using services::MethodSpec;

/// The author of the revisions the service submits: the document's "permissions" decide whether they are reviewed.
constexpr std::string_view author = "features";

/// The keys of a record that say which release it is: an upgrade takes the new release's, whatever was edited.
constexpr std::array<std::string_view, 5> originKeys = {"release", "source", "digest", "publisher", "installed"};

/// @brief What a submitted revision became, for the screen.
/// @param result The result.
/// @return `{status: "applied" | "proposed", revision, proposal}`.
json describe(const RevisionResult& result) {
	return {{"status", result.status == RevisionStatus::Applied ? "applied" : "proposed"},
	        {"revision", result.revision},
	        {"proposal", result.proposal}};
}

/// @brief A place inside a feature's record as a place in the document.
/// @param name The feature.
/// @param path A JSON Pointer inside its record, "/" for all of it, or "#<id>/..." from a node with an id.
/// @return The document's place.
std::string documentPath(std::string_view name, const std::string& path) {
	if(path.starts_with('#')) return path;
	return std::format("/features/{}{}", name, path == "/" ? "" : path);
}

/// @brief A merge's reason, for the person upgrading: "main" is what they edited, the proposal the new release.
/// @param reason The merge's reason ("changed on main and in the proposal").
/// @param to The new release.
/// @return "changed locally and in 1.1.0".
std::string upgradeReason(std::string reason, std::string_view to) {
	const auto replace = [&reason](std::string_view from, const std::string& with) {
		if(const auto at = reason.find(from); at != std::string::npos) reason.replace(at, from.size(), with);
	};
	replace("on main", "locally");
	replace("in the proposal", "in " + std::string(to));
	return reason;
}

/// @brief Lines for a revision's message, at most format::maxDroppedEditsNamed and then how many more.
/// @param lines The lines.
/// @return "\n- <line>" for each, then "\n- and <n> more".
std::string listed(const std::vector<std::string>& lines) {
	std::string text;
	const std::size_t shown = std::min(lines.size(), format::maxDroppedEditsNamed);
	for(std::size_t i = 0; i < shown; ++i) text += "\n- " + lines[i];
	if(lines.size() > shown) text += std::format("\n- and {} more", lines.size() - shown);
	return text;
}

/// @brief A revision that replaces a feature's record.
/// @param name The feature.
/// @param message Its message.
/// @param record The record.
/// @return The revision.
json recordRevision(std::string_view name, std::string message, json record) {
	return {{"author", author},
	        {"message", std::move(message)},
	        {"set", {{"/features/" + std::string(name), std::move(record)}}}};
}

/// @brief What an upgrade submits.
struct Upgrade {
	json revision;                ///< The revision.
	json leftOut = json::array(); ///< The merge's conflicts, the local version kept: `[{path, reason, base, main,
	                              ///< proposal}]`.
	json dropped = json::array(); ///< The places of the edits it drops, when the base is gone.
	bool reviewed = false;        ///< Always a proposal only the person accepts, whatever the permissions say.
};

/// @brief The upgrade of an edited record whose base a catalogue still has: a three-way merge, the local version kept
/// where both changed the same part.
/// @param name The feature.
/// @param parts The three records.
/// @return The upgrade.
Upgrade mergedUpgrade(std::string_view name, const format::UpgradeParts& parts) {
	vcs::MergeResult merge = vcs::mergeDocuments(parts.base, parts.mine, parts.theirs);
	json record = std::move(merge.merged);
	// Which release it is stays the new one's, so the edits kept still count as edits
	for(const std::string_view key : originKeys)
		if(const auto value = parts.theirs.find(key); value != parts.theirs.end()) record[std::string(key)] = *value;
	Upgrade upgrade;
	std::vector<std::string> lines;
	for(json& conflict : merge.conflicts) {
		conflict["path"] = documentPath(name, conflict["path"].get<std::string>());
		conflict["reason"] = upgradeReason(conflict["reason"].get<std::string>(), parts.to);
		lines.push_back(conflict["path"].get<std::string>() + ": " + conflict["reason"].get<std::string>());
		upgrade.leftOut.push_back(std::move(conflict));
	}
	std::string message = std::format("Upgrade {} to {}, keeping local edits", name, parts.to);
	if(!lines.empty())
		message += std::format("; {} conflict{}, the local version kept:", lines.size(), lines.size() == 1 ? "" : "s") +
		           listed(lines);
	upgrade.revision = recordRevision(name, std::move(message), std::move(record));
	return upgrade;
}

/// @brief The upgrade of an edited record whose base no catalogue has: the new release's record, as a proposal that
/// names each place it drops, so nothing is lost without the person's yes.
/// @param name The feature.
/// @param parts The records.
/// @return The upgrade.
Upgrade droppingUpgrade(std::string_view name, const format::UpgradeParts& parts) {
	std::vector<std::string> places;
	for(const std::string& path : format::differingPaths(parts.mine, parts.theirs))
		places.push_back(documentPath(name, path));
	Upgrade upgrade;
	for(const std::string& place : places) upgrade.dropped.push_back(place);
	upgrade.reviewed = true;
	upgrade.revision = recordRevision(
	    name, std::format("Upgrade {} to {}, dropping local edits:", name, parts.to) + listed(places), parts.theirs);
	return upgrade;
}

/// @brief What upgrading a feature to its newest release submits.
/// @param features The catalogues.
/// @param document The document, as version control keeps it.
/// @param name The feature.
/// @return The upgrade: the new release's record if nothing was edited, else a merge, else a proposal that drops.
/// @throws DocumentError As format::upgradeParts().
Upgrade upgradeOf(const format::Features& features, const json& document, std::string_view name) {
	const format::UpgradeParts parts = format::upgradeParts(features, document, name);
	if(!format::edited(parts.mine, parts.baseKnown ? &parts.base : nullptr)) {
		Upgrade upgrade;
		upgrade.revision = recordRevision(name, std::format("Upgrade {} to {}", name, parts.to), parts.theirs);
		return upgrade;
	}
	return parts.baseKnown ? mergedUpgrade(name, parts) : droppingUpgrade(name, parts);
}

/// @brief The revision of `remove`, `enable` or `disable`.
/// @param method The method.
/// @param document The document, as version control keeps it.
/// @param name The feature.
/// @return The revision. A removal a definition still depends on is the document rules' to refuse.
/// @throws DocumentError If it isn't installed, or is already enabled or disabled.
json lifecycleRevision(std::string_view method, const json& document, const std::string& name) {
	const json* record = format::installedRecord(document, name);
	if(!record) throw DocumentError(std::format("features.{}: isn't installed", name));
	if(method == "remove")
		return {{"author", author}, {"message", "Remove " + name}, {"remove", json::array({"/features/" + name})}};
	const bool enable = method == "enable";
	if(record->value("enabled", false) == enable)
		throw DocumentError(std::format("features.{}: is {} already", name, enable ? "enabled" : "disabled"));
	return {
	    {"author", author},
	    {"message", std::format("{} {}", enable ? "Enable" : "Disable", name)},
	    {"patch", json::array({{{"op", "replace"}, {"path", "/features/" + name + "/enabled"}, {"value", enable}}})}};
}

/// @brief One `features` service.
class FeaturesService final : public services::Service {
public:
	/// @brief Serves a Screen's features.
	/// @param features The catalogues.
	/// @param repository The document.
	/// @param alive The Screen's guard.
	/// @param executor The main thread.
	FeaturesService(const format::Features& features, vcs::Repository& repository, std::weak_ptr<bool> alive,
	                Executor& executor)
	    : features_(features), repository_(repository), alive_(std::move(alive)), executor_(executor) {}

	/// @brief Starts a call to one of its methods. Main thread; returns at once.
	/// @param method The method (its arguments already checked).
	/// @param call Where the result goes.
	/// @return An error if the call can't start, else std::nullopt.
	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		const json& with = call->arguments();
		// What revisions change: for a program, its source, not what it builds
		const json& source = repository_.sourceDocument();
		const std::string name = with.value("name", std::string());
		try {
			if(method.name == "catalog") {
				call->finish(features_.catalog(repository_.document()));
			} else if(method.name == "status") {
				call->finish(format::featureStatus(features_, source, name));
			} else if(method.name == "upgrade") {
				submitLater(upgradeOf(features_, source, name), std::move(call));
			} else if(json revision = revisionOf(method.name, with, source); revision.is_null()) {
				call->fail("unproven", unproven_);
			} else {
				Upgrade submitted;
				submitted.revision = std::move(revision);
				submitLater(std::move(submitted), std::move(call));
			}
		} catch(const DocumentError& error) {
			call->fail("refused", error.what());
		}
		return std::nullopt;
	}

private:
	/// @brief The revision of `install`, `compose`, `remove`, `enable` or `disable`.
	/// @param method The method.
	/// @param with Its arguments.
	/// @param source The document, as version control keeps it.
	/// @return The revision, or null for a proof that failed (unproven_).
	/// @throws DocumentError With the reason it is refused.
	json revisionOf(std::string_view method, const json& with, const json& source) {
		const std::string name = with.value("name", std::string());
		if(method == "compose") return composeRevision(with, source);
		if(method == "install")
			return features_.installRevision(source, name, with.value("version", std::string()), author);
		return lifecycleRevision(method, source, name);
	}

	/// @brief Submits a revision after the step that called it, never in the middle of one, and answers the call.
	/// @param submitted The revision, whether it must be reviewed, and what the result adds (an upgrade's leftOut).
	/// @param call Where the result goes.
	void submitLater(Upgrade submitted, std::shared_ptr<Call> call) {
		executor_.post(
		    [&repository = repository_, alive = alive_, submitted = std::move(submitted), call = std::move(call)] {
			    if(!screenAlive(alive)) return;
			    const RevisionResult result =
			        submitted.reviewed ? repository.proposeForPerson(submitted.revision)
			                                 : repository.submit(submitted.revision);
			    if(result.status == RevisionStatus::Rejected) {
				    call->fail("rejected", result.error);
				    return;
			    }
			    json answer = describe(result);
			    if(!submitted.leftOut.empty()) answer["leftOut"] = submitted.leftOut;
			    if(!submitted.dropped.empty()) answer["dropped"] = submitted.dropped;
			    call->finish(std::move(answer));
		    });
	}

	/// @brief Proves a feature from another app on the document (every check but the scenarios, which the proposal's
	/// evidence plays when it is reviewed) and makes its install revision.
	/// @param with `{ manifest, adapter, source, host }`.
	/// @param source The document as version control keeps it: a program's source, which the proof builds itself.
	/// @return The revision, or null with unproven_ set to the proof's reasons.
	/// @throws DocumentError If the manifest is too big.
	json composeRevision(const json& with, const json& source) {
		const json& manifest = with["manifest"];
		if(manifest.dump().size() > format::maxProofBytes) throw DocumentError("the manifest is too big");
		const format::ProofInputs inputs{source,
		                                 manifest,
		                                 with.value("adapter", json()),
		                                 with.value("source", std::string()),
		                                 with.value("host", std::string("host")),
		                                 repository_.head().id,
		                                 "",
		                                 ""};
		const format::ProvenComposition proven =
		    format::proveComposition(inputs, [](const json&, const json&) { return std::vector<format::ScenarioOutcome>(); });
		if(!proven.proof.value("passed", false)) {
			unproven_.clear();
			for(const std::string& line : format::proofReasons(proven.proof)) unproven_ += line + "\n";
			return nullptr;
		}
		return vcs::compositionRevision(proven.proof.value("feature", ""), proven.record, author, source);
	}

	const format::Features& features_; ///< See the constructor.
	vcs::Repository& repository_;      ///< See the constructor.
	std::weak_ptr<bool> alive_;        ///< See the constructor.
	Executor& executor_;               ///< See the constructor.
	std::string unproven_;             ///< The reasons of the last proof that failed.
};

/// @brief The `features` provider.
class FeaturesProvider final : public services::Provider {
public:
	/// @brief Serves a Screen's features.
	/// @param features The catalogues.
	/// @param repository The document.
	/// @param alive The Screen's guard.
	FeaturesProvider(const format::Features& features, vcs::Repository& repository, std::weak_ptr<bool> alive)
	    : features_(features), repository_(repository), alive_(std::move(alive)) {}

	/// @brief The name declarations use.
	/// @return "features".
	std::string_view name() const override { return "features"; }
	/// @brief One sentence for the catalog.
	/// @return It.
	std::string_view about() const override {
		return "the features of the host's signed catalogues, installed, upgraded, enabled, disabled and removed by "
		       "revision";
	}
	/// @brief The JSON Schema of its configuration.
	/// @return An empty object: it takes none.
	json configurationSchema() const override {
		return {{"type", "object"}, {"additionalProperties", false}, {"properties", json::object()}};
	}
	/// @brief Its methods.
	/// @return catalog, status, install, upgrade, remove, enable, disable and compose.
	std::vector<MethodSpec> methods(const json& /*configuration*/) const override {
		const json name = {{"type", "string"}, {"maxLength", 64}};
		const json named = {{"type", "object"},
		                    {"required", {"name"}},
		                    {"properties", {{"name", name}}},
		                    {"additionalProperties", false}};
		// Installing changes the document, so a revision may not add a step that does it: only the screen's file
		return {
		    {"catalog",
			 Effect::Read,
			 true,
			 false,
			 {{"type", "object"}, {"properties", json::object()}, {"additionalProperties", false}},
			 "what the catalogues offer: [{name, version, publisher, title, description, installed, enabled}]"},
		    {"status", Effect::Read, true, false, named, "one feature: {installed, release, newest, edited, enabled}"},
		    {"install",
			 Effect::Write,
			 false,
			 false,
			 {{"type", "object"},
			  {"required", {"name"}},
			  {"properties", {{"name", name}, {"version", {{"type", "string"}, {"maxLength", 32}}}}},
			  {"additionalProperties", false}},
			 "installs a feature (its newest release, or \"version\") by revision: {status, revision, proposal}"},
		    {"upgrade", Effect::Write, false, false, named,
			 "upgrades an installed feature to its newest release by revision, merging its local edits: {status, "
			 "revision, proposal, leftOut, dropped}"},
		    {"remove", Effect::Write, false, false, named, "removes an installed feature by revision"},
		    {"enable", Effect::Write, false, false, named, "turns an installed feature on by revision"},
		    {"disable", Effect::Write, false, false, named, "turns an installed feature off by revision"},
		    {"compose",
			 Effect::Write,
			 false,
			 false,
			 {{"type", "object"},
			  {"required", {"manifest", "source"}},
			  {"properties",
			   {{"manifest", {{"type", "object"}}},
			    {"adapter", {{"type", {"object", "null"}}}},
			    {"source", {{"type", "string"}, {"maxLength", 128}}},
			    {"host", {{"type", "string"}, {"maxLength", 64}}}}},
			  {"additionalProperties", false}},
			 "installs a feature from another app with an adapter, once proven, by revision: {status, revision, "
			 "proposal}; refused \"unproven\" with the proof's reasons"}};
	}
	/// @brief Creates the service. Main thread.
	/// @param context What it works with: its executor.
	/// @return The service.
	std::unique_ptr<services::Service> create(const json& /*configuration*/,
	                                          services::ServiceContext context) override {
		return std::make_unique<FeaturesService>(features_, repository_, alive_, context.executor);
	}

private:
	const format::Features& features_; ///< See the constructor.
	vcs::Repository& repository_;      ///< See the constructor.
	std::weak_ptr<bool> alive_;        ///< See the constructor.
};

} // namespace

std::unique_ptr<services::Provider> makeFeaturesProvider(const format::Features& features,
                                                         vcs::Repository& repository, std::weak_ptr<bool> alive) {
	return std::make_unique<FeaturesProvider>(features, repository, std::move(alive));
}

} // namespace affresco::detail
