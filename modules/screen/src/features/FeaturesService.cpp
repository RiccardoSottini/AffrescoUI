/// @file
/// @brief The `features` provider: `catalog`, `install`, `upgrade` and `compose` (a feature from another app, with its
/// adapter and proof), each install or upgrade one revision.

#include "features/FeaturesService.hpp"
#include "CompositionProof.hpp"
#include "CompositionProofRevision.hpp"

namespace affresco::detail {

namespace {

using services::Call;
using services::Effect;
using services::MethodSpec;

/// The author of the revisions the service submits: the document's "permissions" decide whether they are reviewed.
constexpr std::string_view author = "features";

/// @brief What a submitted revision became, for the screen.
/// @param result The result.
/// @return `{status: "applied" | "proposed", revision, proposal}`.
json describe(const RevisionResult& result) {
	return {{"status", result.status == RevisionStatus::Applied ? "applied" : "proposed"},
	        {"revision", result.revision},
	        {"proposal", result.proposal}};
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
		const json& document = repository_.document();
		if(method.name == "catalog") {
			call->finish(features_.catalog(document));
			return std::nullopt;
		}
		json revision;
		try {
			if(method.name == "compose") {
				revision = composeRevision(with, document);
			} else {
				const std::string name = with.value("name", std::string());
				revision = method.name == "install"
				               ? features_.installRevision(document, name, with.value("version", std::string()), author)
				               : features_.upgradeRevision(document, name, author);
			}
		} catch(const DocumentError& error) {
			call->fail("refused", error.what());
			return std::nullopt;
		}
		if(revision.is_null()) {
			call->fail("unproven", unproven_);
			return std::nullopt;
		}
		// Submitted after the step that called it, never in the middle of one
		executor_.post([&repository = repository_, alive = alive_, revision = std::move(revision), call] {
			if(!screenAlive(alive)) return;
			const RevisionResult result = repository.submit(revision);
			if(result.status == RevisionStatus::Rejected) call->fail("rejected", result.error);
			else call->finish(describe(result));
		});
		return std::nullopt;
	}

private:
	/// @brief Proves a feature from another app on the document (every check but the scenarios, which the proposal's
	/// evidence plays when it is reviewed) and makes its install revision.
	/// @param with `{ manifest, adapter, source, host }`.
	/// @param document The document.
	/// @return The revision, or null with unproven_ set to the proof's reasons.
	/// @throws DocumentError If the manifest is too big.
	json composeRevision(const json& with, const json& document) {
		const json& manifest = with["manifest"];
		if(manifest.dump().size() > format::maxProofBytes) throw DocumentError("the manifest is too big");
		const format::ProofInputs inputs{document,
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
		return vcs::compositionRevision(proven.proof.value("feature", ""), proven.record, author, document);
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
		return "the features of the host's signed catalogues, installed and upgraded by revision";
	}
	/// @brief The JSON Schema of its configuration.
	/// @return An empty object: it takes none.
	json configurationSchema() const override {
		return {{"type", "object"}, {"additionalProperties", false}, {"properties", json::object()}};
	}
	/// @brief Its methods.
	/// @return catalog, install, upgrade and compose.
	std::vector<MethodSpec> methods(const json& /*configuration*/) const override {
		const json name = {{"type", "string"}, {"maxLength", 64}};
		// Installing changes the document, so a revision may not add a step that does it: only the screen's file
		return {{"catalog", Effect::Read, true, false,
		         {{"type", "object"}, {"properties", json::object()}, {"additionalProperties", false}},
		         "what the catalogues offer: [{name, version, publisher, title, description, installed, enabled}]"},
		        {"install", Effect::Write, false, false,
		         {{"type", "object"},
		          {"required", {"name"}},
		          {"properties", {{"name", name}, {"version", {{"type", "string"}, {"maxLength", 32}}}}},
		          {"additionalProperties", false}},
		         "installs a feature (its newest release, or \"version\") by revision: {status, revision, proposal}"},
		        {"upgrade", Effect::Write, false, false,
		         {{"type", "object"},
		          {"required", {"name"}},
		          {"properties", {{"name", name}}},
		          {"additionalProperties", false}},
		         "upgrades an installed feature to its newest release by revision: {status, revision, proposal}"},
		        {"compose", Effect::Write, false, false,
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
