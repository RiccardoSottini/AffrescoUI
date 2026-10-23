/// @file
/// @brief Forge's search feature, native side (Search.hpp).

#include "features/search/Search.hpp"

#include "features/palette/FileIndex.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/services/Search.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <format>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace forge {

namespace fs = std::filesystem;
using affresco::json;

namespace {

constexpr std::size_t mostPattern = 4096;                     ///< The longest pattern.
constexpr std::size_t mostGlobs = 16384;                      ///< The longest include or exclude list.
constexpr std::size_t mostReplacement = 65536;                ///< The longest replacement.
constexpr auto partialEvery = std::chrono::milliseconds(150); ///< How often partial results are streamed.
/// @brief Always excluded: the folders the file index skips (FileIndex.cpp), build output and dependencies.
constexpr std::string_view skippedFolders = "build, node_modules, out, target, dist, __pycache__";

/// @brief A text argument.
/// @param arguments The arguments (an object).
/// @param name Its name.
/// @param most Its longest.
/// @return It, or "" if it is missing.
/// @throws std::runtime_error If it isn't text, or is too long.
std::string textOf(const json& arguments, const char* name, std::size_t most) {
	const auto found = arguments.find(name);
	if(found == arguments.end() || found->is_null()) return {};
	if(!found->is_string()) throw std::runtime_error(std::string(name) + " must be text");
	if(found->get_ref<const std::string&>().size() > most) throw std::runtime_error(std::string(name) + " is too long");
	return found->get<std::string>();
}

/// @brief The pattern options of the arguments.
/// @param arguments The arguments (an object).
/// @return Them; anything but true is false.
affresco::PatternOptions optionsOf(const json& arguments) {
	const auto flag = [&arguments](const char* name) { return arguments.value(name, json(false)) == json(true); };
	return {.regex = flag("regex"), .wholeWord = flag("wholeWord"), .caseSensitive = flag("caseSensitive")};
}

/// @brief The arguments, as an object.
/// @param arguments The arguments.
/// @return Them.
/// @throws std::runtime_error If they aren't an object.
const json& objectOf(const json& arguments) {
	if(!arguments.is_object()) throw std::runtime_error("expected an object of arguments");
	// NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter): its callers pass the call's arguments, which outlive it
	return arguments;
}

/// @brief A file's name and folder in a path.
/// @param path The path, with '/'.
/// @return Its name, and its folder ("" at the root).
std::pair<std::string, std::string> nameAndFolder(const std::string& path) {
	const std::size_t slash = path.rfind('/');
	if(slash == std::string::npos) return {path, ""};
	return {path.substr(slash + 1), path.substr(0, slash)};
}

/// @brief The results tree of some files' matches.
/// @param files The files, sorted by path.
/// @param summary What the search did.
/// @param done Whether it has finished.
/// @return The results, as searchFolder() returns them.
json resultsOf(const std::vector<affresco::services::SearchFile>& files,
               const affresco::services::SearchSummary& summary, bool done) {
	json tree = json::array(), hits = json::array();
	std::size_t matches = 0;
	for(const affresco::services::SearchFile& file : files) {
		const auto [name, folder] = nameAndFolder(file.path);
		json children = json::array();
		for(const affresco::services::SearchMatch& match : file.matches) {
			const std::size_t first = std::min(match.text.find_first_not_of(" \t"), match.at);
			children.push_back({{"id", "hit:" + std::to_string(hits.size())},
			                    {"label", std::to_string(match.line) + ": " + match.text.substr(first)}});
			hits.push_back({{"file", file.path}, {"label", name}, {"line", match.line}, {"column", match.column}});
		}
		matches += file.matches.size();
		const std::string count = std::to_string(file.matches.size());
		tree.push_back({{"id", "file:" + file.path},
		                {"label", folder.empty() ? std::format("{}  ({})", name, count)
		                                         : std::format("{}  {}  ({})", name, folder, count)},
		                {"icon", "file"},
		                {"children", std::move(children)}});
	}
	return {
	    {"tree", std::move(tree)},    {"hits", std::move(hits)},        {"files", files.size()}, {"matches", matches},
	    {"skipped", summary.skipped}, {"truncated", summary.truncated}, {"done", done}};
}

/// @brief The files a search found so far, filled from its threads and read from the calling one.
struct Gathered {
	std::mutex mutex;                                  ///< Guards the rest.
	std::condition_variable changed;                   ///< Told when finished.
	std::vector<affresco::services::SearchFile> files; ///< Found so far.
	bool fresh = false;                                ///< Whether files changed since the last partial result.
	bool finished = false;                             ///< Whether the search returned.
};

/// @brief The files found so far, sorted by path.
/// @param gathered The files (its mutex held).
/// @return A sorted copy.
std::vector<affresco::services::SearchFile> sorted(const Gathered& gathered) {
	std::vector<affresco::services::SearchFile> files = gathered.files;
	std::ranges::sort(files, {}, &affresco::services::SearchFile::path);
	return files;
}

/// @brief Replaces the matches in one file of the folder.
/// @param root The folder.
/// @param file The file, relative to it.
/// @param pattern The pattern.
/// @param replacement The replacement.
/// @param limits The caps.
/// @param count Gets the replacements made.
/// @return An error, or std::nullopt.
std::optional<std::string> replaceInFile(const fs::path& root, const std::string& file,
                                         const affresco::TextPattern& pattern, const std::string& replacement,
                                         const SearchLimits& limits, std::size_t& count) {
	count = 0;
	const fs::path path = insideFolder(root, file);
	if(path.empty()) return "the file isn't inside the open folder";
	std::error_code error;
	if(!fs::is_regular_file(path, error) || fs::is_symlink(fs::symlink_status(path, error))) return "not a file";
	if(fs::file_size(path, error) > limits.fileBytes || error) return "the file is too large";
	std::ifstream in(path, std::ios::binary);
	if(!in) return "the file can't be read";
	const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	affresco::services::Replaced replaced;
	if(auto failed = affresco::services::replaceAll(text, pattern, replacement, replaced)) return failed;
	count = replaced.count;
	return count ? writeInFolder(path, replaced.text) : std::nullopt;
}

/// @brief A list of paths from the arguments.
/// @param arguments The arguments (an object).
/// @param name The list's name.
/// @param most Its most entries.
/// @return Its paths.
/// @throws std::runtime_error If it isn't a list of text, or is too long.
std::vector<std::string> pathsOf(const json& arguments, const char* name, std::size_t most) {
	const auto found = arguments.find(name);
	if(found == arguments.end() || found->is_null()) return {};
	if(!found->is_array() || found->size() > most)
		throw std::runtime_error(std::string(name) + " must be a short list");
	std::vector<std::string> paths;
	for(const json& path : *found) {
		if(!path.is_string()) throw std::runtime_error(std::string(name) + " must list paths");
		paths.push_back(path.get<std::string>());
	}
	return paths;
}

} // namespace

json searchFolder(const fs::path& root, const json& arguments, const std::function<bool()>& cancelled,
                  const std::function<void(json)>& onPartial, const SearchLimits& limits) {
	if(root.empty()) throw std::runtime_error("no folder is open");
	const json& with = objectOf(arguments);
	const affresco::services::SearchQuery query{.pattern = textOf(with, "pattern", mostPattern),
	                                            .options = optionsOf(with),
	                                            .include = textOf(with, "include", mostGlobs),
	                                            .exclude = std::string(skippedFolders) + ", " +
	                                                       textOf(with, "exclude", mostGlobs),
	                                            .maxResults = limits.results};
	Gathered gathered;
	affresco::services::SearchSummary summary;
	std::optional<std::string> error;
	std::stop_source stop;
	std::jthread worker([&] {
		error = affresco::services::searchFiles(
		    root, query, stop.get_token(),
		    [&gathered](affresco::services::SearchFile&& file) {
			    const std::scoped_lock lock(gathered.mutex);
			    gathered.files.push_back(std::move(file));
			    gathered.fresh = true;
		    },
		    summary);
		const std::scoped_lock lock(gathered.mutex);
		gathered.finished = true;
		gathered.changed.notify_all();
	});
	// CallProgress is used only here, on the calling thread: polled to stop the search, told the files found so far
	std::unique_lock lock(gathered.mutex);
	while(!gathered.changed.wait_for(lock, partialEvery, [&gathered] { return gathered.finished; })) {
		if(cancelled()) stop.request_stop();
		if(!gathered.fresh) continue;
		gathered.fresh = false;
		const std::vector<affresco::services::SearchFile> files = sorted(gathered);
		lock.unlock();
		onPartial(resultsOf(files, {}, false));
		lock.lock();
	}
	lock.unlock();
	worker.join();
	if(error) throw std::runtime_error(*error);
	return resultsOf(sorted(gathered), summary, true);
}

json replaceInFolder(const fs::path& root, const json& arguments, const SearchLimits& limits) {
	if(root.empty()) throw std::runtime_error("no folder is open");
	const json& with = objectOf(arguments);
	affresco::TextPattern pattern;
	if(auto error = pattern.compile(textOf(with, "pattern", mostPattern), optionsOf(with)))
		throw std::runtime_error(*error);
	const std::string replacement = textOf(with, "replacement", mostReplacement);
	const std::vector<std::string> files = pathsOf(with, "files", limits.files);
	const std::vector<std::string> skipList = pathsOf(with, "skip", limits.files);
	const std::set<std::string> skip(skipList.begin(), skipList.end());
	json changed = json::array(), skipped = json::array(), failed = json::array();
	std::size_t replaced = 0;
	for(const std::string& file : std::set<std::string>(files.begin(), files.end())) {
		if(skip.contains(file)) {
			skipped.push_back(file);
			continue;
		}
		std::size_t count = 0;
		if(auto error = replaceInFile(root, file, pattern, replacement, limits, count)) {
			failed.push_back({{"file", file}, {"error", *error}});
			continue;
		}
		replaced += count;
		if(count) changed.push_back(file);
	}
	return {{"replaced", replaced},
	        {"changed", std::move(changed)},
	        {"skipped", std::move(skipped)},
	        {"failed", std::move(failed)}};
}

void registerSearch(affresco::Screen& screen, const std::shared_ptr<Workspace>& workspace) {
	// The folder the next search looks in, taken on the main thread, where openFolder changes the workspace
	struct Scope {
		std::mutex mutex; ///< Guards root.
		fs::path root;    ///< The folder.
	};
	const auto scope = std::make_shared<Scope>();
	screen.onCall("forge.searchScope", [scope, workspace](const std::string&) {
		const std::scoped_lock lock(scope->mutex);
		scope->root = workspace->root;
		return json{{"open", !scope->root.empty()}}.dump();
	});
	screen.onAsyncCall("forge.search", [scope](const std::string& arguments, affresco::CallProgress& call) {
		const fs::path root = [&scope] {
			const std::scoped_lock lock(scope->mutex);
			return scope->root;
		}();
		return searchFolder(
		           root, affresco::parseUntrusted(arguments), [&call] { return call.cancelled(); },
		           [&call](json partial) { call.partial(std::move(partial)); })
		    .dump();
	});
	screen.onCall("forge.replace", [workspace](const std::string& arguments) {
		return replaceInFolder(workspace->root, affresco::parseUntrusted(arguments)).dump();
	});
}

} // namespace forge
