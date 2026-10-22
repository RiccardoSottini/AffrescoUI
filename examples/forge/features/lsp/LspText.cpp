/// @file
/// @brief The language server protocol's text, as Forge's lsp feature reads it (LspProtocol.hpp): file URIs, cut
/// texts, snippets and token styles.

#include "features/lsp/LspProtocol.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <string_view>
#include <system_error>

namespace forge::lsp {

namespace {

namespace fs = std::filesystem;

/// @brief The value of a hexadecimal digit.
/// @param c The character.
/// @return 0 to 15, or -1.
int hexValue(char c) {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/// @brief Whether a file URI keeps a path's character as it is, unescaped.
/// @param c The character.
/// @return true for letters, digits, '/' and RFC 3986's unreserved marks.
bool isUnescaped(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '-' || c == '.' || c == '_' || c == '~';
}

/// @brief The path of a local file URI: `file:///x` or `file://localhost/x`, at most 4096 bytes.
/// @param uri The URI.
/// @return Its path, still escaped, or std::nullopt for any other URI.
std::optional<std::string_view> pathOfUri(std::string_view uri) {
	constexpr std::string_view scheme = "file://";
	if(uri.size() > 4096 || !uri.starts_with(scheme)) return std::nullopt;
	uri.remove_prefix(scheme.size());
	const std::size_t slash = uri.find('/');
	if(slash == std::string_view::npos) return std::nullopt;
	if(const std::string_view host = uri.substr(0, slash); !host.empty() && host != "localhost") return std::nullopt;
	return uri.substr(slash);
}

/// @brief Decodes a URI's path.
/// @param path The path, escaped.
/// @return It, or std::nullopt for a query, a fragment, a broken escape or an escaped NUL (which would cut the name).
std::optional<std::string> decodedPath(std::string_view path) {
	std::string decoded;
	for(std::size_t i = 0; i < path.size(); ++i) {
		if(path[i] == '?' || path[i] == '#') return std::nullopt;
		if(path[i] != '%') {
			decoded += path[i];
			continue;
		}
		const int high = i + 2 < path.size() ? hexValue(path[i + 1]) : -1;
		const int low = high >= 0 ? hexValue(path[i + 2]) : -1;
		if(low < 0 || (high == 0 && low == 0)) return std::nullopt;
		decoded += static_cast<char>(high * 16 + low);
		i += 2;
	}
	return decoded;
}

/// @brief A path's name in a folder, if it is inside it both as written and once links are followed.
/// @param path The path, normal.
/// @param root The folder.
/// @return The name, with forward slashes, or std::nullopt.
std::optional<std::string> relativeInside(const fs::path& path, const fs::path& root) {
	const fs::path base = root.lexically_normal();
	if(base.empty() || !path.is_absolute()) return std::nullopt;
	const fs::path relative = path.lexically_relative(base);
	if(relative.empty() || relative == "." || *relative.begin() == "..") return std::nullopt;
	std::error_code error; // a link inside the folder may lead out of it
	const fs::path real = fs::weakly_canonical(path, error), realBase = fs::weakly_canonical(base, error);
	if(error) return std::nullopt;
	const fs::path inside = real.lexically_relative(realBase);
	if(inside.empty() || *inside.begin() == "..") return std::nullopt;
	return relative.generic_string();
}

/// @brief Whether a character can be in a snippet's tab stop number or variable name.
/// @param c The character.
/// @return true for letters, digits and '_'.
bool isNameChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

/// @brief Where a snippet's name ends.
/// @param snippet The snippet.
/// @param from Where the name starts.
/// @return The index of the first character after it.
std::size_t nameEnd(std::string_view snippet, std::size_t from) {
	while(from < snippet.size() && isNameChar(snippet[from])) ++from;
	return from;
}

/// @brief Reads a snippet's `${...}`, keeping what a plain text shows of it.
/// @param snippet The snippet.
/// @param i Where it starts (its '$').
/// @param out Gets the first choice of `${1|one,two|}`.
/// @param open Counts `${1:text}`, whose text stays and whose closing brace is dropped when it comes.
/// @return The index of the last character read.
std::size_t placeholder(std::string_view snippet, std::size_t i, std::string& out, int& open) {
	const std::size_t j = nameEnd(snippet, i + 2);
	if(j < snippet.size() && snippet[j] == ':') {
		++open;
		return j;
	}
	if(j < snippet.size() && snippet[j] == '|') {
		const std::size_t end = snippet.find("|}", j + 1);
		const std::string_view choices =
		    snippet.substr(j + 1, (end == std::string_view::npos ? snippet.size() : end) - j - 1);
		out += choices.substr(0, std::min(choices.find(','), choices.size()));
		return end == std::string_view::npos ? snippet.size() : end + 1;
	}
	return j < snippet.size() && snippet[j] == '}' ? j : j - 1; // ${1}: nothing
}

} // namespace

std::string uriOf(const fs::path& root, std::string_view id) {
	std::string path = (id.empty() ? root : root / fs::path(std::string(id))).lexically_normal().generic_string();
	if(path.size() > 1 && path.back() == '/') path.pop_back();
	if(path.empty() || path.front() != '/') path.insert(path.begin(), '/'); // C:/x becomes /C:/x
	std::string uri = "file://";
	constexpr std::string_view digits = "0123456789ABCDEF";
	for(const char c : path) {
		const auto byte = static_cast<unsigned char>(c);
		if(isUnescaped(c)) uri += c;
		else {
			uri += '%';
			uri += digits[byte >> 4];
			uri += digits[byte & 0xF];
		}
	}
	return uri;
}

std::optional<std::string> idOfUri(const fs::path& root, std::string_view uri) {
	const auto path = pathOfUri(uri);
	auto decoded = path ? decodedPath(*path) : std::nullopt;
	if(!decoded) return std::nullopt;
#ifdef _WIN32
	if(decoded->size() > 2 && (*decoded)[0] == '/' && (*decoded)[2] == ':') decoded->erase(0, 1); // /C:/x
#endif
	return relativeInside(fs::path(*decoded).lexically_normal(), root);
}

std::string cut(std::string_view text, std::size_t bytes) {
	if(text.size() <= bytes) return std::string(text);
	std::size_t end = bytes;
	while(end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
	return std::string(text.substr(0, end)) + "\u2026";
}

std::string plainSnippet(std::string_view snippet) {
	std::string out;
	int open = 0; // placeholders open, whose closing brace is dropped
	for(std::size_t i = 0; i < snippet.size(); ++i) {
		const char c = snippet[i];
		const bool more = i + 1 < snippet.size();
		if(c == '\\' && more) out += snippet[++i];
		else if(c == '}' && open > 0) --open;
		else if(c == '$' && more && snippet[i + 1] == '{') i = placeholder(snippet, i, out, open);
		else if(c == '$' && more && isNameChar(snippet[i + 1]))
			i = nameEnd(snippet, i + 1) - 1; // $1, $TM_FILENAME: nothing
		else out += c;
	}
	return out;
}

std::string styleOfToken(std::string_view type) {
	static const std::map<std::string_view, std::string_view, std::less<>> styles = {{"class", "type"},
	                                                                                 {"struct", "type"},
	                                                                                 {"enum", "type"},
	                                                                                 {"interface", "type"},
	                                                                                 {"type", "type"},
	                                                                                 {"typeParameter", "type"},
	                                                                                 {"namespace", "type"},
	                                                                                 {"concept", "type"},
	                                                                                 {"keyword", "keyword"},
	                                                                                 {"modifier", "keyword"},
	                                                                                 {"macro", "constant.builtin"},
	                                                                                 {"string", "string"},
	                                                                                 {"regexp", "string"},
	                                                                                 {"number", "number"},
	                                                                                 {"comment", "comment"}};
	const auto found = styles.find(type);
	return std::string(found == styles.end() ? type : found->second);
}

} // namespace forge::lsp
