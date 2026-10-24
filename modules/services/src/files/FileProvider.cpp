/// @file
/// @brief The `fs` provider: text files under a granted folder, read, written and listed on worker threads, and its
/// entries made, moved, deleted, copied and described (FileEntries.hpp).

#include "FileEntries.hpp"
#include "Providers.hpp"

#include <cerrno>
#include <fstream>
#include <iterator>
#include <random>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace affresco::services {

namespace {

constexpr std::uintmax_t largestFile = std::uintmax_t{8} << 20; ///< A file read or written, at most 8 MiB.
constexpr std::size_t mostEntries = 10000;                      ///< A listing, at most 10,000 entries.
constexpr std::size_t mostAtOnce = 16;                          ///< Calls running at once, per service.

/// @brief Reads a text file.
/// @param path The file (under the root).
/// @param call Where its text goes.
void readFile(const std::filesystem::path& path, Call& call) {
	std::error_code error;
	if(!std::filesystem::is_regular_file(path, error)) return call.fail("notFound", "no such file");
	if(std::filesystem::file_size(path, error) > largestFile || error)
		return call.fail("tooLarge", "the file is over 8 MiB");
	std::ifstream file(path, std::ios::binary);
	std::string text(std::istreambuf_iterator<char>(file), {});
	if(!file && !file.eof()) return call.fail("failed", "the file couldn't be read");
	if(text.size() > largestFile) return call.fail("tooLarge", "the file is over 8 MiB");
	if(!isUtf8(text)) return call.fail("notText", "the file isn't UTF-8 text");
	call.finish({{"text", std::move(text)}});
}

#ifndef _WIN32
/// @brief Writes all of a text to a file, resuming after a signal.
/// @param descriptor The file, open for writing.
/// @param text Its bytes.
/// @return Whether all of them were written.
bool writeAll(int descriptor, const std::string& text) {
	for(std::size_t at = 0; at < text.size();) {
		const ssize_t count = ::write(descriptor, text.data() + at, text.size() - at);
		if(count > 0) at += static_cast<std::size_t>(count);
		else if(count == 0 || errno != EINTR) return false;
	}
	return true;
}
#endif

/// @brief Creates a new file and writes it; fails rather than open what is already there, a link included.
/// @param file The new file.
/// @param text Its bytes.
/// @param permissions Its permissions.
/// @return Whether it was created and written; if it was created but not written, it is removed.
bool createWith(const std::filesystem::path& file, const std::string& text, std::filesystem::perms permissions) {
	std::error_code error;
#ifdef _WIN32
	if(std::filesystem::exists(std::filesystem::symlink_status(file, error))) return false;
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if(!out || !out.write(text.data(), static_cast<std::streamsize>(text.size())) || !out.flush()) {
		out.close();
		std::filesystem::remove(file, error);
		return false;
	}
	out.close();
	std::filesystem::permissions(file, permissions, error);
	return true;
#else
	const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if(descriptor < 0) return false;
	bool written = ::fchmod(descriptor, static_cast<mode_t>(permissions) & 07777) == 0 && writeAll(descriptor, text);
	written = ::close(descriptor) == 0 && written;
	if(!written) std::filesystem::remove(file, error);
	return written;
#endif
}

/// @brief Writes a text file, replacing it, through a new file beside it so a reader never sees half of it. The new
/// file's name is drawn at random and it is created only if nothing is there: a fixed name let a link planted beside
/// the file (a cloned repository can hold one) have the text written wherever it pointed, outside the root.
/// @param path The file (under the root); its folder must exist.
/// @param text Its text.
/// @param call Where the result goes.
void writeFile(const std::filesystem::path& path, const std::string& text, Call& call) {
	using std::filesystem::perms;
	std::error_code error;
	if(std::filesystem::is_directory(path, error)) return call.fail("failed", "the path is a folder");
	// A file replaced keeps its permissions (a private one stays private); a new one is readable, as before
	const std::filesystem::file_status status = std::filesystem::status(path, error);
	const perms permissions =
	    error ? perms::owner_read | perms::owner_write | perms::group_read | perms::others_read : status.permissions();
	std::random_device random;
	for(int attempt = 0; attempt < 8; ++attempt) {
		std::filesystem::path temporary = path;
		temporary += ".affrescoui-writing-" + std::to_string(random());
		if(!createWith(temporary, text, permissions)) {
			if(std::filesystem::exists(std::filesystem::symlink_status(temporary, error))) continue; // name taken
			break;
		}
		std::filesystem::rename(temporary, path, error);
		if(!error) return call.finish({{"bytes", text.size()}});
		std::filesystem::remove(temporary, error);
		break;
	}
	call.fail("failed", "the file couldn't be written");
}

/// @brief Lists a folder: {entries: [{name, kind, size}]}, kind "file", "folder" or "other".
/// @param path The folder (under the root).
/// @param stop Says to give up.
/// @param call Where the listing goes.
void listFolder(const std::filesystem::path& path, const std::stop_token& stop, Call& call) {
	std::error_code error;
	std::filesystem::directory_iterator entry(path, error);
	if(error) return call.fail("notFound", "no such folder");
	json entries = json::array();
	for(; entry != std::filesystem::directory_iterator() && !stop.stop_requested(); entry.increment(error)) {
		if(entries.size() == mostEntries) return call.fail("tooLarge", "the folder has over 10000 entries");
		const std::string name = entry->path().filename().string();
		if(!isUtf8(name)) continue; // a name the document can't hold
		const bool file = entry->is_regular_file(error), folder = entry->is_directory(error);
		entries.push_back({{"name", name},
		                   {"kind", file     ? "file"
		                            : folder ? "folder"
		                                     : "other"},
		                   {"size", file ? json(entry->file_size(error)) : json()}});
		if(error) break;
	}
	if(error) return call.fail("failed", "the folder couldn't be listed");
	call.finish({{"entries", std::move(entries)}});
}

/// @brief A path for an error message, cut if it is long.
/// @param path The path.
/// @return It, or an ellipsis.
std::string shortened(const std::string& path) { return path.size() > 200 ? "…" : path; }

/// @brief One `fs` service: a root and its worker threads.
class FileService final : public Service {
public:
	/// @brief Serves a root.
	/// @param root The root (canonical).
	/// @param readOnly Whether writing is refused.
	/// @param now Whether each call's work runs before invoke() returns (ServiceContext::now).
	/// @param sources The other roots it copies from, by name (canonical).
	FileService(std::filesystem::path root, bool readOnly,
	            std::map<std::string, std::filesystem::path, std::less<>> sources, bool now)
	    : root_(std::move(root)), readOnly_(readOnly), sources_(std::move(sources)), tasks_(mostAtOnce, now) {}

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		const json& with = call->arguments();
		const std::string& name = method.name;
		if(readOnly_ && name != "read" && name != "list" && name != "stat") return "the service is read-only";
		// A copy from another root reads its path there; that root is only ever read
		std::filesystem::path from = root_;
		if(const std::string source = with.is_object() ? with.value("from", std::string()) : ""; !source.empty()) {
			const auto granted = sources_.find(source);
			if(name != "copy" || granted == sources_.end())
				return "with.from: '" + shortened(source) + "' isn't a folder this service copies from (copyFrom)";
			from = granted->second;
		}
		// An entry's own path (a link is moved, deleted or copied as a link); a text's path follows links
		const bool entry = name == "rename" || name == "delete" || name == "copy";
		const std::string relative = with.is_object() ? with.value("path", std::string(".")) : ".";
		const std::optional<std::filesystem::path> path =
		    entry ? entryUnderRoot(from, relative) : underRoot(root_, relative);
		if(!path) return "with.path: must be a path inside the root '" + shortened(relative) + "'";
		std::optional<std::filesystem::path> to;
		if(name == "rename" || name == "copy") {
			const std::string target = with.value("to", std::string());
			to = entryUnderRoot(root_, target);
			if(!to) return "with.to: must be a path inside the root '" + shortened(target) + "'";
		}
		const bool plain = from != root_; // from another root: no link, no execute bit
		const bool started = tasks_.run([root = root_, name, path = *path, to, plain, call](const std::stop_token&) {
			const json& arguments = call->arguments();
			if(name == "read") readFile(path, *call);
			else if(name == "write") writeFile(path, arguments.value("text", std::string()), *call);
			else if(name == "list") listFolder(path, call->stopToken(), *call);
			else if(name == "mkdir") makeFolder(root, path, *call);
			else if(name == "rename") moveEntry(root, path, *to, *call);
			else if(name == "delete") removeEntry(path, {}, call->stopToken(), *call);
			else if(name == "copy")
				copyEntry(root, path, *to,
				          {arguments.value("free", false), arguments.value("merge", false),
				           arguments.value("exclude", std::vector<std::string>{}), plain},
				          {}, call->stopToken(), *call);
			else describeEntry(path, *call);
		});
		return started ? std::nullopt : std::optional<std::string>("the service is busy (16 calls at once)");
	}

private:
	std::filesystem::path root_;                                        ///< See the constructor.
	bool readOnly_;                                                     ///< See the constructor.
	std::map<std::string, std::filesystem::path, std::less<>> sources_; ///< See the constructor.
	Tasks tasks_; ///< The calls running (last, so they end first).
};

/// @brief The `fs` provider.
class FileProvider final : public Provider {
public:
	std::string_view name() const override { return "fs"; }
	std::string_view about() const override { return "text files under a folder the host grants"; }
	json configurationSchema() const override {
		return {{"type", "object"},
		        {"required", {"root"}},
		        {"additionalProperties", false},
		        {"properties",
		         {{"root", {{"type", "string"}}},
		          {"readOnly", {{"type", "boolean"}}},
		          {"copyFrom", {{"type", "array"}, {"maxItems", 16}, {"items", {{"type", "string"}}}}}}}};
	}
	std::vector<MethodSpec> methods(const json& configuration) const override {
		const json path = {{"type", "string"}, {"maxLength", 4096}};
		// An arguments schema: an object of these properties, the required ones named
		const auto object = [](json properties, json required) {
			return json{{"type", "object"},
			            {"required", std::move(required)},
			            {"properties", std::move(properties)},
			            {"additionalProperties", false}};
		};
		std::vector<MethodSpec> methods = {
		    {"read",
			 Effect::Read,
			 true,
			 false,
			 {{"type", "object"},
			  {"required", {"path"}},
			  {"properties", {{"path", path}}},
			  {"additionalProperties", false}},
			 "reads a UTF-8 text file under the root (at most 8 MiB): {text}"},
		    {"list",
			 Effect::Read,
			 true,
			 false,
			 {{"type", "object"}, {"properties", {{"path", path}}}, {"additionalProperties", false}},
			 "lists a folder under the root: {entries: [{name, kind, size}]}"}};
		methods.push_back({"stat", Effect::Read, true, false, object({{"path", path}}, {"path"}),
		                   "describes what is at a path under the root: {exists, kind, size, modified}"});
		if(configuration.value("readOnly", false)) return methods;
		methods.push_back(
		    {"write", Effect::Write, true, false,
			 object({{"path", path}, {"text", {{"type", "string"}, {"maxLength", largestFile}}}}, {"path", "text"}),
			 "replaces a text file under the root: {bytes}"});
		methods.push_back({"mkdir", Effect::Write, true, false, object({{"path", path}}, {"path"}),
		                   "makes a folder under the root, and the folders missing above it: {path}"});
		methods.push_back(
		    {"rename", Effect::Write, true, false, object({{"path", path}, {"to", path}}, {"path", "to"}),
			 "moves or renames a file or folder under the root (fails if the new path is taken): {path}"});
		methods.push_back({"delete", Effect::Write, true, false, object({{"path", path}}, {"path"}),
		                   "deletes a file, or a folder with all it holds (at most 10,000 entries): {removed}"});
		const json exclude = {{"type", "array"}, {"maxItems", 64}, {"items", {{"type", "string"}, {"maxLength", 256}}}};
		methods.push_back({"copy", Effect::Write, true, false,
		                   object({{"path", path},
		                           {"to", path},
		                           {"free", {{"type", "boolean"}}},
		                           {"merge", {{"type", "boolean"}}},
		                           {"exclude", exclude},
		                           {"from", {{"type", "string"}, {"maxLength", 64}}}},
		                          {"path", "to"}),
		                   "copies a file or folder (at most 10,000 entries, 256 MiB) to a path under the root, from "
		                   "the root or from a root of copyFrom (from: its links left out, no file copied executable); "
		                   "with free, a taken path becomes \"<name> copy<ext>\"; with merge, a folder merges into the "
		                   "one there (what is missing or older is copied); exclude: name globs left out: {path}"});
		return methods;
	}
	std::unique_ptr<Service> create(const json& configuration, ServiceContext context) override {
		return std::make_unique<FileService>(context.grant.root,
		                                     configuration.value("readOnly", false) || context.grant.rootReadOnly,
		                                     context.grant.sources, context.now);
	}
};

} // namespace

std::unique_ptr<Provider> makeFileProvider() { return std::make_unique<FileProvider>(); }

} // namespace affresco::services
