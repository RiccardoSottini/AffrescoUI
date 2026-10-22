/// @file
/// @brief The `fs` provider: text files under a granted folder, read, written and listed on worker threads.

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
constexpr std::size_t mostEntries = 10000;                       ///< A listing, at most 10,000 entries.
constexpr std::size_t mostAtOnce = 16;                           ///< Calls running at once, per service.

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
		if(entries.size() == mostEntries)
			return call.fail("tooLarge", "the folder has over 10000 entries");
		const std::string name = entry->path().filename().string();
		if(!isUtf8(name)) continue; // a name the document can't hold
		const bool file = entry->is_regular_file(error), folder = entry->is_directory(error);
		entries.push_back({{"name", name},
		                   {"kind", file ? "file" : folder ? "folder" : "other"},
		                   {"size", file ? json(entry->file_size(error)) : json()}});
		if(error) break;
	}
	if(error) return call.fail("failed", "the folder couldn't be listed");
	call.finish({{"entries", std::move(entries)}});
}

/// @brief One `fs` service: a root and its worker threads.
class FileService final : public Service {
public:
	/// @brief Serves a root.
	/// @param root The root (canonical).
	/// @param readOnly Whether writing is refused.
	FileService(std::filesystem::path root, bool readOnly) : root_(std::move(root)), readOnly_(readOnly) {}

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		const json& with = call->arguments();
		const std::string relative = with.is_object() ? with.value("path", std::string(".")) : ".";
		const std::optional<std::filesystem::path> path = underRoot(root_, relative);
		if(!path) return "with.path: must be a path inside the root '" + std::string(relative.size() > 200 ? "…" : relative) + "'";
		if(method.name == "write" && readOnly_) return "the service is read-only";
		const bool started = tasks_.run([name = method.name, path = *path, call](const std::stop_token&) {
			if(name == "read") readFile(path, *call);
			else if(name == "write") writeFile(path, call->arguments().value("text", std::string()), *call);
			else listFolder(path, call->stopToken(), *call);
		});
		return started ? std::nullopt : std::optional<std::string>("the service is busy (16 calls at once)");
	}

private:
	std::filesystem::path root_; ///< See the constructor.
	bool readOnly_;              ///< See the constructor.
	Tasks tasks_{mostAtOnce};    ///< The calls running (last, so they end first).
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
		        {"properties", {{"root", {{"type", "string"}}}, {"readOnly", {{"type", "boolean"}}}}}};
	}
	std::vector<MethodSpec> methods(const json& configuration) const override {
		const json path = {{"type", "string"}, {"maxLength", 4096}};
		std::vector<MethodSpec> methods = {
		    {"read", Effect::Read, true, false,
		     {{"type", "object"}, {"required", {"path"}}, {"properties", {{"path", path}}}, {"additionalProperties", false}},
		     "reads a UTF-8 text file under the root (at most 8 MiB): {text}"},
		    {"list", Effect::Read, true, false,
		     {{"type", "object"}, {"properties", {{"path", path}}}, {"additionalProperties", false}},
		     "lists a folder under the root: {entries: [{name, kind, size}]}"}};
		if(!configuration.value("readOnly", false))
			methods.push_back({"write", Effect::Write, true, false,
			                   {{"type", "object"},
			                    {"required", {"path", "text"}},
			                    {"properties", {{"path", path}, {"text", {{"type", "string"}, {"maxLength", largestFile}}}}},
			                    {"additionalProperties", false}},
			                   "replaces a text file under the root: {bytes}"});
		return methods;
	}
	std::unique_ptr<Service> create(const json& configuration, ServiceContext context) override {
		return std::make_unique<FileService>(context.grant.root, configuration.value("readOnly", false));
	}
};

} // namespace

std::unique_ptr<Provider> makeFileProvider() { return std::make_unique<FileProvider>(); }

} // namespace affresco::services
