/// @file
/// @brief The `search` provider (SearchProvider.hpp): the search engine (affresco/services/Search.hpp) over a granted
/// folder, on worker threads.

#include "search/SearchProvider.hpp"

#include "Providers.hpp"
#include "affresco/services/Search.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

namespace affresco::services {

namespace {

constexpr std::size_t mostAtOnce = 4;                           ///< Calls running at once, per service.
constexpr std::uintmax_t largestFile = std::uintmax_t{8} << 20; ///< A file replaced in, at most 8 MiB.

/// @brief The pattern options of a call's arguments.
/// @param with The arguments (checked against the schema).
/// @return The options.
PatternOptions optionsOf(const json& with) {
	return {.regex = with.value("regex", false),
	        .wholeWord = with.value("wholeWord", false),
	        .caseSensitive = with.value("caseSensitive", false)};
}

/// @brief The match as the document sees it.
/// @param match The match.
/// @return {line, column, length, text, at}.
json matchJson(const SearchMatch& match) {
	return {{"line", match.line},
	        {"column", match.column},
	        {"length", match.length},
	        {"text", match.text},
	        {"at", match.at}};
}

/// @brief Runs a `find` call: one event {file, matches} per file with matches, then {files, matches, searched,
/// skipped, truncated}.
/// @param root The root.
/// @param stop The worker's stop token.
/// @param call The call.
void find(const std::filesystem::path& root, const std::stop_token& stop, Call& call) {
	const json& with = call.arguments();
	const SearchQuery query{.pattern = with.value("pattern", std::string()),
	                  .options = optionsOf(with),
	                  .include = with.value("include", std::string()),
	                  .exclude = with.value("exclude", std::string()),
	                  .folder = with.value("folder", std::string(".")),
	                  .maxResults = with.value("maxResults", std::size_t{2000}),
	                  .gitignore = with.value("gitignore", true)};
	std::stop_source source;
	const EitherStop stopping(call.stopToken(), stop, [&source] { source.request_stop(); });
	SearchSummary summary;
	const auto onFile = [&call](SearchFile file) {
		json matches = json::array();
		for(const SearchMatch& match : file.matches) matches.push_back(matchJson(match));
		call.event({{"file", std::move(file.path)}, {"matches", std::move(matches)}});
	};
	if(auto error = searchFiles(root, query, source.get_token(), onFile, summary)) return call.fail("invalid", *error);
	if(summary.cancelled) return call.fail("cancelled", "the search was stopped");
	call.finish({{"files", summary.files},
	             {"matches", summary.matches},
	             {"searched", summary.searched},
	             {"skipped", summary.skipped},
	             {"truncated", summary.truncated}});
}

/// @brief Runs a `replace` call: the file's text with every match replaced, {text, replacements}; nothing is written.
/// @param path The file (under the root).
/// @param call The call.
void replace(const std::filesystem::path& path, Call& call) {
	std::error_code error;
	if(!std::filesystem::is_regular_file(path, error)) return call.fail("notFound", "no such file");
	if(std::filesystem::file_size(path, error) > largestFile || error)
		return call.fail("tooLarge", "the file is over 8 MiB");
	std::ifstream file(path, std::ios::binary);
	const std::string text(std::istreambuf_iterator<char>(file), {});
	if(text.size() > largestFile) return call.fail("tooLarge", "the file is over 8 MiB");
	if(!isUtf8(text)) return call.fail("notText", "the file isn't UTF-8 text");
	const json& with = call.arguments();
	TextPattern pattern;
	if(auto problem = pattern.compile(with.value("pattern", std::string()), optionsOf(with)))
		return call.fail("invalid", "pattern: " + *problem);
	Replaced replaced;
	if(auto problem = replaceAll(text, pattern, with.value("replacement", std::string()), replaced))
		return call.fail("invalid", *problem);
	if(replaced.text.size() > largestFile) return call.fail("tooLarge", "the new text is over 8 MiB");
	call.finish({{"text", std::move(replaced.text)}, {"replacements", replaced.count}});
}

/// @brief One `search` service: a root and its worker threads.
class SearchService final : public Service {
public:
	/// @brief Serves a root.
	/// @param root The root (canonical).
	explicit SearchService(std::filesystem::path root) : root_(std::move(root)) {}

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		const json& with = call->arguments();
		std::filesystem::path path = root_;
		if(method.name == "replace") {
			const std::string relative = with.value("path", std::string());
			const std::optional<std::filesystem::path> file = underRoot(root_, relative);
			if(!file) return "with.path: must be a path inside the root";
			path = *file;
		}
		const bool started =
		    tasks_.run([name = method.name, path = std::move(path), call](const std::stop_token& stop) {
			    if(name == "find") find(path, stop, *call);
			    else replace(path, *call);
		    });
		return started ? std::nullopt : std::optional<std::string>("the service is busy (4 calls at once)");
	}

private:
	std::filesystem::path root_; ///< See the constructor.
	Tasks tasks_{mostAtOnce};    ///< The calls running (last, so they end first).
};

/// @brief The `search` provider.
class SearchProvider final : public Provider {
public:
	std::string_view name() const override { return "search"; }
	std::string_view about() const override { return "text searched and replaced in a folder the host grants"; }
	json configurationSchema() const override {
		return {{"type", "object"},
		        {"required", {"root"}},
		        {"additionalProperties", false},
		        {"properties", {{"root", {{"type", "string"}}}}}};
	}
	std::vector<MethodSpec> methods(const json& /*configuration*/) const override {
		const json text = {{"type", "string"}, {"maxLength", TextPattern::maxPatternBytes}};
		const json globs = {{"type", "string"}, {"maxLength", 16384}};
		const json flag = {{"type", "boolean"}};
		const json pattern = {{"pattern", text}, {"regex", flag}, {"wholeWord", flag}, {"caseSensitive", flag}};
		json find = pattern, replace = pattern;
		find.update({{"include", globs},
		             {"exclude", globs},
		             {"folder", {{"type", "string"}, {"maxLength", 4096}}},
		             {"maxResults", {{"type", "integer"}, {"minimum", 1}, {"maximum", SearchQuery::mostResults}}},
		             {"gitignore", flag}});
		replace.update({{"path", {{"type", "string"}, {"maxLength", 4096}}},
		                {"replacement", {{"type", "string"}, {"maxLength", 65536}}}});
		return {
		    {"find",
			 Effect::Read,
			 true,
			 false,
			 {{"type", "object"}, {"required", {"pattern"}}, {"properties", find}, {"additionalProperties", false}},
			 "searches the text files under the root, one event {file, matches: [{line, column, length, text, "
			 "at}]} per file: {files, matches, searched, skipped, truncated}"},
		    {"replace",
			 Effect::Read,
			 true,
			 false,
			 {{"type", "object"},
			  {"required", {"path", "pattern", "replacement"}},
			  {"properties", replace},
			  {"additionalProperties", false}},
			 "a file's text with every match replaced, for an fs write (nothing is written): {text, replacements}"}};
	}
	std::unique_ptr<Service> create(const json& /*configuration*/, ServiceContext context) override {
		return std::make_unique<SearchService>(context.grant.root);
	}
};

} // namespace

std::unique_ptr<Provider> makeSearchProvider() { return std::make_unique<SearchProvider>(); }

} // namespace affresco::services
