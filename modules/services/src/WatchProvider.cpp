/// @file
/// @brief The `watch` provider: changes under a granted folder, debounced, as a stream of batches.

#include "Providers.hpp"
#include "Watcher.hpp"

#include <algorithm>
#include <stdexcept>

namespace affresco::services {

namespace {

constexpr std::size_t mostSubscriptions = 4; ///< Streams at once, per service; each watches the whole root.
constexpr int shortestDebounce = 20;         ///< `debounce`, in milliseconds, at least.
constexpr int longestDebounce = 2000;        ///< `debounce`, in milliseconds, at most.

/// @brief Turns a batch into the event a subscriber hears: {changes: [{path, kind}], rescan}.
/// @param root The root, to tell a changed path from a removed one.
/// @param batch The batch.
/// @return The event.
[[nodiscard]] json batchEvent(const std::filesystem::path& root, const WatchBatch& batch) {
	json changes = json::array();
	for(const std::string& path : batch.paths) {
		std::error_code error;
		const bool there = std::filesystem::symlink_status(root / pathFromUtf8(path), error).type() !=
		                   std::filesystem::file_type::not_found;
		changes.push_back({{"path", path}, {"kind", there && !error ? "changed" : "removed"}});
	}
	return {{"changes", std::move(changes)}, {"rescan", batch.rescan}};
}

/// @brief One subscription: its call and the watcher feeding it.
struct Subscription {
	std::shared_ptr<Call> call;       ///< Where the batches go.
	std::unique_ptr<Watcher> watcher; ///< Watches the root for it.
};

/// @brief One `watch` service: a root and its subscriptions.
class WatchService final : public Service {
public:
	/// @brief Watches a root.
	/// @param root The root (canonical).
	/// @param options The debounce and the ignore globs.
	WatchService(std::filesystem::path root, WatchOptions options)
	    : root_(std::move(root)), options_(std::move(options)) {}

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call>) override {
		return "'" + method.name + "' is a stream: subscribe to it";
	}
	std::optional<std::string> subscribe(const std::string& stream, std::shared_ptr<Call> call) override {
		if(stream != "changes") return "it has no stream '" + stream + "'";
		std::erase_if(subscriptions_, [](const Subscription& each) { return each.call->stopToken().stop_requested(); });
		if(subscriptions_.size() >= mostSubscriptions) return "the service has 4 subscriptions already";
		Subscription subscription{call, nullptr};
		try {
			subscription.watcher = std::make_unique<Watcher>(root_, options_, [call, root = root_](WatchBatch batch) {
				if(!call->stopToken().stop_requested()) call->event(batchEvent(root, batch));
			});
		} catch(const std::exception&) {
			return "the root couldn't be watched";
		}
		subscriptions_.push_back(std::move(subscription));
		return std::nullopt;
	}

private:
	std::filesystem::path root_;              ///< See the constructor.
	WatchOptions options_;                    ///< See the constructor.
	std::vector<Subscription> subscriptions_; ///< Those started (last, so their watchers stop first).
};

/// @brief The `watch` provider.
class WatchProvider final : public Provider {
public:
	std::string_view name() const override { return "watch"; }
	std::string_view about() const override { return "changes under a folder the host grants"; }
	json configurationSchema() const override {
		const json glob = {{"type", "string"}, {"maxLength", 200}};
		return {{"type", "object"},
		        {"required", {"root"}},
		        {"additionalProperties", false},
		        {"properties",
		         {{"root", {{"type", "string"}}},
		          {"ignore", {{"type", "array"}, {"maxItems", 64}, {"items", glob}}},
		          {"debounce", {{"type", "integer"}, {"minimum", shortestDebounce}, {"maximum", longestDebounce}}}}}};
	}
	std::vector<MethodSpec> methods(const json&) const override {
		return {
		    {"changes",
			 Effect::Read,
			 true,
			 true,
			 {{"type", "object"}, {"additionalProperties", false}},
			 "streams what changed under the root, debounced: each event {changes: [{path, kind: changed|removed}], "
			 "rescan} (rescan: too much changed to list, read the folder again)"}};
	}
	std::unique_ptr<Service> create(const json& configuration, ServiceContext context) override {
		WatchOptions options;
		options.debounce = std::chrono::milliseconds(configuration.value("debounce", 100));
		options.ignore = configuration.value("ignore", std::vector<std::string>());
		return std::make_unique<WatchService>(context.grant.root, std::move(options));
	}
};

} // namespace

std::unique_ptr<Provider> makeWatchProvider() { return std::make_unique<WatchProvider>(); }

} // namespace affresco::services
