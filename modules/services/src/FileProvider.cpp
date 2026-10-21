/// @file
/// @brief The `fs` provider: text files under a granted folder, read, written and listed on worker threads.

#include "Providers.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

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

/// @brief Writes a text file, replacing it, through a file beside it so a reader never sees half of it.
/// @param path The file (under the root); its folder must exist.
/// @param text Its text.
/// @param call Where the result goes.
void writeFile(const std::filesystem::path& path, const std::string& text, Call& call) {
	std::error_code error;
	if(std::filesystem::is_directory(path, error)) return call.fail("failed", "the path is a folder");
	std::filesystem::path temporary = path;
	temporary += ".affrescoui-writing";
	{
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		if(!file.flush()) {
			std::filesystem::remove(temporary, error);
			return call.fail("failed", "the file couldn't be written");
		}
	}
	std::filesystem::rename(temporary, path, error);
	if(error) {
		std::filesystem::remove(temporary, error);
		return call.fail("failed", "the file couldn't be written");
	}
	call.finish({{"bytes", text.size()}});
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
		const bool started = tasks_.run([name = method.name, path = *path, call](std::stop_token) {
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
