/// @file
/// @brief The `features` provider: `catalog`, `install` and `upgrade`, each install or upgrade one revision.

#include "FeaturesService.hpp"

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

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		const json& with = call->arguments();
		const json& document = repository_.document();
		if(method.name == "catalog") {
			call->finish(features_.catalog(document));
			return std::nullopt;
		}
		json revision;
		try {
			const std::string name = with.value("name", std::string());
			revision = method.name == "install"
			               ? features_.installRevision(document, name, with.value("version", std::string()), author)
			               : features_.upgradeRevision(document, name, author);
		} catch(const DocumentError& error) {
			call->fail("refused", error.what());
			return std::nullopt;
		}
		// Submitted after the step that called it, never in the middle of one
		executor_.post([&repository = repository_, alive = alive_, revision = std::move(revision), call] {
			if(alive.expired()) return;
			const RevisionResult result = repository.submit(revision);
			if(result.status == RevisionStatus::Rejected) call->fail("rejected", result.error);
			else call->finish(describe(result));
		});
		return std::nullopt;
	}

private:
	const format::Features& features_; ///< See the constructor.
	vcs::Repository& repository_;      ///< See the constructor.
	std::weak_ptr<bool> alive_;        ///< See the constructor.
	Executor& executor_;               ///< See the constructor.
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

	std::string_view name() const override { return "features"; }
	std::string_view about() const override {
		return "the features of the host's signed catalogues, installed and upgraded by revision";
	}
	json configurationSchema() const override {
		return {{"type", "object"}, {"additionalProperties", false}, {"properties", json::object()}};
	}
	std::vector<MethodSpec> methods(const json&) const override {
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
		         "upgrades an installed feature to its newest release by revision: {status, revision, proposal}"}};
	}
	std::unique_ptr<services::Service> create(const json&, services::ServiceContext context) override {
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
