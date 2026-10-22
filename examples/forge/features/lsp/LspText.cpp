/// @file
/// @brief The language server protocol's text, as Forge's lsp feature reads it (LspProtocol.hpp): file URIs, cut
/// texts, snippets and token styles.

#include "features/lsp/LspProtocol.hpp"

#include <algorithm>
#include <cctype>
#include <map>
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

} // namespace

std::string uriOf(const fs::path& root, std::string_view id) {
	std::string path = (id.empty() ? root : root / fs::path(std::string(id))).lexically_normal().generic_string();
	if(path.size() > 1 && path.back() == '/') path.pop_back();
	if(path.empty() || path.front() != '/') path.insert(path.begin(), '/'); // C:/x becomes /C:/x
	std::string uri = "file://";
	constexpr char digits[] = "0123456789ABCDEF";
	for(const char c : path) {
		const auto byte = static_cast<unsigned char>(c);
		if(std::isalnum(byte) || c == '/' || c == '-' || c == '.' || c == '_' || c == '~') uri += c;
		else {
			uri += '%';
			uri += digits[byte >> 4];
			uri += digits[byte & 0xF];
		}
	}
	return uri;
}

std::optional<std::string> idOfUri(const fs::path& root, std::string_view uri) {
	constexpr std::string_view scheme = "file://";
	if(uri.size() > 4096 || !uri.starts_with(scheme)) return std::nullopt;
	uri.remove_prefix(scheme.size());
	const std::size_t slash = uri.find('/');
	if(slash == std::string_view::npos) return std::nullopt;
	if(const std::string_view host = uri.substr(0, slash); !host.empty() && host != "localhost") return std::nullopt;
	uri.remove_prefix(slash);
	std::string decoded;
	for(std::size_t i = 0; i < uri.size(); ++i) {
		if(uri[i] == '?' || uri[i] == '#') return std::nullopt;
		if(uri[i] != '%') {
			decoded += uri[i];
			continue;
		}
		const int high = i + 2 < uri.size() ? hexValue(uri[i + 1]) : -1;
		const int low = high >= 0 ? hexValue(uri[i + 2]) : -1;
		if(low < 0 || (high == 0 && low == 0)) return std::nullopt;
		decoded += static_cast<char>(high * 16 + low);
		i += 2;
	}
#ifdef _WIN32
	if(decoded.size() > 2 && decoded[0] == '/' && decoded[2] == ':') decoded.erase(0, 1); // /C:/x
#endif
	const fs::path path = fs::path(decoded).lexically_normal();
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
		if(c == '\\' && i + 1 < snippet.size()) {
			out += snippet[++i];
		} else if(c == '}' && open > 0) {
			--open;
		} else if(c == '$' && i + 1 < snippet.size() && snippet[i + 1] == '{') {
			std::size_t j = i + 2;
			while(j < snippet.size() && (std::isalnum(static_cast<unsigned char>(snippet[j])) || snippet[j] == '_'))
				++j;
			if(j < snippet.size() && snippet[j] == ':') {
				++open; // ${1:text}: the text stays
				i = j;
			} else if(j < snippet.size() && snippet[j] == '|') { // ${1|one,two|}: the first choice
				const std::size_t end = snippet.find("|}", j + 1);
				const std::string_view choices =
				    snippet.substr(j + 1, (end == std::string_view::npos ? snippet.size() : end) - j - 1);
				out += choices.substr(0, std::min(choices.find(','), choices.size()));
				i = end == std::string_view::npos ? snippet.size() : end + 1;
			} else {
				i = j < snippet.size() && snippet[j] == '}' ? j : j - 1; // ${1}: nothing
			}
		} else if(c == '$' && i + 1 < snippet.size() &&
		          (std::isalnum(static_cast<unsigned char>(snippet[i + 1])) || snippet[i + 1] == '_')) {
			while(i + 1 < snippet.size() &&
			      (std::isalnum(static_cast<unsigned char>(snippet[i + 1])) || snippet[i + 1] == '_'))
				++i; // $1, $0, $TM_FILENAME: nothing
		} else {
			out += c;
		}
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
