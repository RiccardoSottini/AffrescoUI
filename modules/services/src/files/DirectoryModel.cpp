/// @file
/// @brief DirectoryModel: the first listing and rescans on a worker thread, the watcher's changes resolved there too,
/// and applied to the rows on the main thread as ranges.

#include "affresco/services/DirectoryModel.hpp"

#include "Providers.hpp"
#include "files/Watcher.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iterator>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <thread>
#include <unordered_set>
#include <utility>

namespace affresco::services {

namespace {

using Entry = DirectoryModel::Entry;
using Kind = DirectoryModel::Kind;

constexpr std::size_t mostJobs = 256; ///< Jobs waiting at most; past them the queue becomes one rescan.
constexpr std::size_t mostRuns = 64;  ///< Ranges one change emits at most; past them it emits a Reset.

/// @brief The name rows use for a kind.
/// @param kind The kind.
/// @return "folder", "file" or "other".
[[nodiscard]] std::string_view kindName(Kind kind) noexcept {
	return kind == Kind::Folder ? "folder" : kind == Kind::File ? "file" : "other";
}

/// @brief Describes a directory entry. A link is its target's kind, except that a link to a folder is "other", so
/// it is never listed into (it may lead outside the root).
/// @param item The entry.
/// @param relative Its path, relative to the root.
/// @return It.
[[nodiscard]] Entry describe(const std::filesystem::directory_entry& item, std::string relative) {
	std::error_code error;
	Entry entry{std::move(relative), Kind::Other, 0};
	const bool link = item.is_symlink(error);
	if(item.is_regular_file(error)) {
		entry.kind = Kind::File;
		entry.size = item.file_size(error);
		if(error) entry.size = 0;
	} else if(item.is_directory(error) && !link) entry.kind = Kind::Folder;
	return entry;
}

/// @brief What is at a path now.
/// @param root The root.
/// @param relative The path, relative to it.
/// @return It, or std::nullopt if nothing is.
[[nodiscard]] std::optional<Entry> describePath(const std::filesystem::path& root, const std::string& relative) {
	std::error_code error;
	const std::filesystem::directory_entry item(root / pathFromUtf8(relative), error);
	if(error || !(item.is_symlink(error) || item.exists(error))) return std::nullopt;
	return describe(item, relative);
}

/// @brief A listing: entries in the model's order, and whether some were left out.
struct Listing {
	std::vector<Entry> entries; ///< The entries.
	bool truncated = false;     ///< Whether some were left out.
};

/// @brief Lists everything under a folder (not the folder itself), skipping ignored paths, in the model's order.
/// @param root The root.
/// @param under The folder, relative to the root ("" for the root).
/// @param options What to ignore, how many at most.
/// @param stop Says to give up.
/// @return The listing.
[[nodiscard]] Listing listTree(const std::filesystem::path& root, const std::string& under,
                               const DirectoryOptions& options, const std::stop_token& stop) {
	Listing listing;
	std::error_code error;
	const std::filesystem::path folder = under.empty() ? root : root / pathFromUtf8(under);
	if(!under.empty() && std::filesystem::is_symlink(folder, error)) return listing;
	std::filesystem::recursive_directory_iterator item(
	    folder, std::filesystem::directory_options::skip_permission_denied, error);
	for(const std::filesystem::recursive_directory_iterator end; !error && item != end && !stop.stop_requested();
	    item.increment(error)) {
		std::string relative = genericUtf8(item->path().lexically_relative(root));
		if(!isUtf8(relative) || ignoredBy(options.ignore, relative)) {
			item.disable_recursion_pending();
			continue;
		}
		if(listing.entries.size() == options.mostEntries) {
			listing.truncated = true;
			break;
		}
		listing.entries.push_back(describe(*item, std::move(relative)));
	}
	std::ranges::sort(listing.entries, DirectoryModel::before);
	return listing;
}

/// @brief Posts work on a model to the main thread, if the model is still there then.
/// @param executor The main thread.
/// @param model The model.
/// @param work The work.
template <typename Work> void postTo(Executor& executor, std::weak_ptr<DirectoryModel> model, Work work) {
	// NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
	executor.post([model = std::move(model), work = std::move(work)]() mutable {
		if(const std::shared_ptr<DirectoryModel> self = model.lock()) work(*self);
	});
}

/// @brief Whether a new entry has a place in a model's rows: at the root, or in a folder the rows have or that
/// comes in with it. An entry whose folder is gone (or became a file) is dropped.
/// @param model The model.
/// @param path The entry's path.
/// @param keptFolders The folders coming in with it.
/// @return Whether it has.
[[nodiscard]] bool hasPlace(const DirectoryModel& model, const std::string& path,
                            const std::unordered_set<std::string>& keptFolders) {
	const std::size_t slash = path.rfind('/');
	if(slash == std::string::npos || slash == 0) return true;
	const std::string folder = path.substr(0, slash);
	if(keptFolders.contains(folder)) return true;
	const std::optional<std::size_t> parent = model.find(folder);
	return parent && model.entry(*parent).kind == Kind::Folder;
}

} // namespace

/// @brief The model's thread, its queue of jobs, and its watcher.
struct DirectoryModel::Worker {
	using Job = std::function<void(const std::stop_token&)>; ///< Work for the thread.

	std::mutex mutex;                  ///< Guards jobs.
	std::condition_variable_any woken; ///< Signalled when a job is queued.
	std::deque<Job> jobs;              ///< Waiting to run.
	Job rescan;                        ///< Lists everything and resets the rows.
	std::jthread thread;               ///< Runs the jobs.
	std::unique_ptr<Watcher> watcher;  ///< Feeds it changes (last, so it stops first).

	/// @brief Queues a job; a full queue becomes one rescan.
	/// @param job The job.
	void push(Job job) {
		const std::scoped_lock lock(mutex);
		if(jobs.size() >= mostJobs) {
			jobs.clear();
			job = rescan;
		}
		jobs.push_back(std::move(job));
		woken.notify_one();
	}
	/// @brief Runs jobs until stopped.
	/// @param stop Says to stop.
	void run(const std::stop_token& stop) {
		std::unique_lock lock(mutex);
		while(woken.wait(lock, stop, [this] { return !jobs.empty(); })) {
			const Job job = std::move(jobs.front());
			jobs.pop_front();
			lock.unlock();
			job(stop);
			lock.lock();
		}
	}
};

std::shared_ptr<DirectoryModel> DirectoryModel::open(Executor& executor, std::filesystem::path root,
                                                     DirectoryOptions options) {
	auto model = std::make_shared<DirectoryModel>(Key{}, executor, std::move(root), std::move(options));
	model->start();
	return model;
}

DirectoryModel::DirectoryModel(Key /*key*/, Executor& executor, std::filesystem::path root, DirectoryOptions options)
    : executor_(executor), root_(std::move(root)), options_(std::move(options)) {
	std::error_code error;
	const std::filesystem::path canonical = std::filesystem::canonical(root_, error);
	if(!error) root_ = canonical;
}

DirectoryModel::~DirectoryModel() = default;

void DirectoryModel::start() {
	worker_ = std::make_unique<Worker>();
	Worker& worker = *worker_;
	worker.rescan = [model = weak_from_this(), &executor = executor_, root = root_,
	                 options = options_](const std::stop_token& stop) {
		postTo(executor, model, [listing = listTree(root, "", options, stop)](DirectoryModel& self) mutable {
			self.reset(std::move(listing.entries), listing.truncated);
		});
	};
	const auto resolve = [&worker, model = weak_from_this(), &executor = executor_, root = root_](WatchBatch batch) {
		if(batch.rescan) return worker.push(worker.rescan);
		worker.push([model, &executor, root, paths = std::move(batch.paths)](const std::stop_token&) {
			std::vector<Update> updates;
			updates.reserve(paths.size());
			for(const std::string& path : paths) updates.push_back({path, describePath(root, path)});
			postTo(executor, model,
			       [updates = std::move(updates)](DirectoryModel& self) mutable { self.apply(std::move(updates)); });
		});
	};
	if(options_.watch) try {
			worker.watcher =
			    std::make_unique<Watcher>(root_, WatchOptions{options_.debounce, options_.ignore}, resolve);
		} catch(const std::exception&) { // NOLINT(bugprone-empty-catch): listed once, not watched: watching() says so
		}
	worker.thread = std::jthread([&worker](const std::stop_token& stop) { worker.run(stop); });
	worker.push(worker.rescan);
}

json DirectoryModel::row(std::size_t index) const {
	if(index >= entries_.size()) return nullptr;
	const Entry& entry = entries_[index];
	const std::size_t slash = entry.path.rfind('/');
	return {{"path", entry.path},
	        {"name", slash == std::string::npos ? entry.path : entry.path.substr(slash + 1)},
	        {"kind", kindName(entry.kind)},
	        {"depth", std::ranges::count(entry.path, '/')},
	        {"size", entry.kind == Kind::File ? json(entry.size) : json()}};
}

std::optional<std::size_t> DirectoryModel::find(std::string_view path) const {
	for(const Kind kind : {Kind::Folder, Kind::File}) {
		const Entry key{std::string(path), kind, 0};
		const auto found = std::ranges::lower_bound(entries_, key, before);
		if(found != entries_.end() && found->path == path) return static_cast<std::size_t>(found - entries_.begin());
	}
	return std::nullopt;
}

std::size_t DirectoryModel::subtreeEnd(std::size_t index) const noexcept {
	const std::string_view folder = entries_[index].path;
	if(entries_[index].kind != Kind::Folder) return index + 1;
	const auto inside = [folder](const Entry& entry) {
		return entry.path.size() > folder.size() && entry.path[folder.size()] == '/' && entry.path.starts_with(folder);
	};
	const auto first = entries_.begin() + static_cast<std::ptrdiff_t>(index) + 1;
	return static_cast<std::size_t>(std::partition_point(first, entries_.end(), inside) - entries_.begin());
}

bool DirectoryModel::watching() const noexcept { return worker_ && worker_->watcher && worker_->watcher->complete(); }

void DirectoryModel::rescan() { worker_->push(worker_->rescan); }

// NOLINTNEXTLINE(bugprone-exception-escape): substr(0, n) never throws
bool DirectoryModel::before(const Entry& first, const Entry& second) noexcept {
	std::string_view one = first.path, other = second.path;
	for(;;) {
		const std::size_t oneEnd = one.find('/'), otherEnd = other.find('/');
		const bool oneLast = oneEnd == std::string_view::npos, otherLast = otherEnd == std::string_view::npos;
		const bool oneFolder = !oneLast || first.kind == Kind::Folder;
		const bool otherFolder = !otherLast || second.kind == Kind::Folder;
		if(oneFolder != otherFolder) return oneFolder;
		const std::string_view oneName = one.substr(0, oneEnd), otherName = other.substr(0, otherEnd);
		if(oneName != otherName) return oneName < otherName;
		if(oneLast || otherLast) return oneLast && !otherLast; // a folder before what it holds
		one.remove_prefix(oneEnd + 1);
		other.remove_prefix(otherEnd + 1);
	}
}

void DirectoryModel::reset(std::vector<Entry> entries, bool truncated) {
	entries_ = std::move(entries);
	truncated_ = truncated;
	ready_ = true;
	onChange.emit({ModelChange::Kind::Reset, 0, entries_.size()});
}

void DirectoryModel::apply(std::vector<Update> updates) {
	if(!ready_) return; // the first listing, still to come, has them
	std::vector<std::string> gone;
	std::vector<Entry> added;
	for(Update& update : updates) {
		const std::optional<std::size_t> index = find(update.path);
		if(!update.entry) {
			if(index) gone.push_back(std::move(update.path));
		} else if(!index) added.push_back(std::move(*update.entry));
		else if(entries_[*index].kind != update.entry->kind) { // a file became a folder, or the other way
			gone.push_back(std::move(update.path));
			added.push_back(std::move(*update.entry));
		} else if(entries_[*index] != *update.entry) {
			entries_[*index] = std::move(*update.entry);
			onChange.emit({ModelChange::Kind::Changed, *index, 1});
		}
	}
	removeAll(gone);
	std::vector<std::string> folders;
	for(const Entry& entry : added)
		if(entry.kind == Kind::Folder) folders.push_back(entry.path);
	insertAll(std::move(added));
	for(const std::string& folder : folders)
		if(find(folder)) listLater(folder);
}

void DirectoryModel::removeAll(const std::vector<std::string>& paths) {
	std::vector<std::pair<std::size_t, std::size_t>> ranges, merged;
	for(const std::string& path : paths)
		if(const std::optional<std::size_t> index = find(path)) ranges.emplace_back(*index, subtreeEnd(*index));
	std::ranges::sort(ranges);
	for(const auto& range : ranges) {
		if(!merged.empty() && range.first <= merged.back().second)
			merged.back().second = std::max(merged.back().second, range.second);
		else merged.push_back(range);
	}
	const bool many = merged.size() > mostRuns;
	for(const auto& range : std::views::reverse(merged)) {
		const auto begin = entries_.begin();
		entries_.erase(begin + static_cast<std::ptrdiff_t>(range.first),
		               begin + static_cast<std::ptrdiff_t>(range.second));
		if(!many) onChange.emit({ModelChange::Kind::Removed, range.first, range.second - range.first});
	}
	if(many) onChange.emit({ModelChange::Kind::Reset, 0, entries_.size()});
}

void DirectoryModel::insertAll(std::vector<Entry> entries) {
	std::ranges::sort(entries, before);
	std::unordered_set<std::string> keptFolders;
	std::vector<std::pair<std::size_t, std::vector<Entry>>> runs; // where in the rows, and what goes there
	for(Entry& entry : entries) {
		if(!hasPlace(*this, entry.path, keptFolders) || find(entry.path)) continue;
		if(entries_.size() + keptFolders.size() + runs.size() >= options_.mostEntries) {
			truncated_ = true;
			break;
		}
		if(entry.kind == Kind::Folder) keptFolders.insert(entry.path);
		const auto index =
		    static_cast<std::size_t>(std::ranges::lower_bound(entries_, entry, before) - entries_.begin());
		if(runs.empty() || runs.back().first != index) runs.emplace_back(index, std::vector<Entry>());
		runs.back().second.push_back(std::move(entry));
	}
	const bool many = runs.size() > mostRuns;
	for(auto& run : std::views::reverse(runs)) {
		entries_.insert(entries_.begin() + static_cast<std::ptrdiff_t>(run.first),
		                std::make_move_iterator(run.second.begin()), std::make_move_iterator(run.second.end()));
		if(!many) onChange.emit({ModelChange::Kind::Inserted, run.first, run.second.size()});
	}
	if(many) onChange.emit({ModelChange::Kind::Reset, 0, entries_.size()});
}

void DirectoryModel::listLater(const std::string& path) {
	worker_->push([model = weak_from_this(), &executor = executor_, root = root_, options = options_,
	               folder = path](const std::stop_token& stop) {
		postTo(executor, model, [listing = listTree(root, folder, options, stop)](DirectoryModel& self) mutable {
			if(listing.truncated) self.truncated_ = true;
			self.insertAll(std::move(listing.entries));
		});
	});
}

} // namespace affresco::services
