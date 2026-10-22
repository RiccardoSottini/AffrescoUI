/// @file
/// @brief Forge's file buffers (Buffers.hpp).

#include "features/editor/Buffers.hpp"

#include "features/palette/FileIndex.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

namespace forge {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t maxMessage = 120; ///< Bytes of a TODO line todos() keeps.

/// @brief Reads a whole file.
/// @param path The file.
/// @param maxBytes The largest it may be.
/// @param text Gets its bytes.
/// @return An error, or std::nullopt.
std::optional<std::string> readFile(const fs::path& path, std::size_t maxBytes, std::string& text) {
	std::error_code error;
	if(!fs::is_regular_file(path, error)) return "not a file";
	if(fs::file_size(path, error) > maxBytes || error) return "the file is too large to open";
	std::ifstream in(path, std::ios::binary);
	if(!in) return "the file can't be read";
	text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return std::nullopt;
}

/// @brief A line without its surrounding spaces and tabs, cut to maxMessage bytes at a code point.
/// @param line The line.
std::string message(std::string_view line) {
	const std::size_t first = line.find_first_not_of(" \t\r");
	if(first == std::string_view::npos) return {};
	line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
	if(line.size() <= maxMessage) return std::string(line);
	std::size_t end = maxMessage;
	while(end > 0 && (static_cast<unsigned char>(line[end]) & 0xC0) == 0x80) --end; // not inside a code point
	return std::string(line.substr(0, end));
}

/// @brief A line's place in an outline.
/// @param row The line, without its line break.
/// @param markdown Whether the file is Markdown (headings) rather than code (blocks).
/// @return Its depth and label, or std::nullopt if it isn't in the outline.
std::optional<std::pair<int, std::string>> outlineEntry(std::string_view row, bool markdown) {
	if(markdown) {
		const std::size_t hashes = row.find_first_not_of('#');
		if(hashes == 0 || hashes == std::string_view::npos || hashes > 6 || row[hashes] != ' ') return std::nullopt;
		return std::pair{static_cast<int>(hashes) - 1, message(row.substr(hashes))};
	}
	const std::size_t first = row.find_first_not_of(" \t");
	const std::size_t last = row.find_last_not_of(" \t\r");
	if(first == std::string_view::npos || row[last] != '{') return std::nullopt;
	// One tab or up to four spaces is the second level; deeper blocks are left out
	const int depth = first == 0 ? 0 : (row[0] == '\t' ? (first == 1 ? 1 : 2) : (first <= 4 ? 1 : 2));
	const std::string label = message(row.substr(0, last));
	if(depth > 1 || label.empty() || label.starts_with('}') || label.starts_with('/') || label.starts_with('#'))
		return std::nullopt;
	return std::pair{depth, label};
}

} // namespace

Buffers::Buffers(BufferLimits limits) : limits_(limits), model_(std::make_shared<affresco::ListModel>()) {}

Buffers::~Buffers() {
	for(auto& [id, buffer] : buffers_) buffer.text->onChanged.disconnect(buffer.connection);
}

std::optional<std::string> Buffers::open(const fs::path& root, const std::string& id) {
	if(buffers_.contains(id)) return std::nullopt;
	if(buffers_.size() >= limits_.buffers) return "too many files are open: close some first";
	const fs::path path = root.empty() ? fs::path{} : insideFolder(root, id);
	if(path.empty()) return "the file isn't inside the open folder";
	std::string text;
	if(auto error = readFile(path, limits_.fileBytes, text)) return *error + ": " + id;
	Buffer buffer{.text = std::make_shared<affresco::TextModel>(), .path = path, .label = path.filename().string()};
	buffer.text->assign(text);
	markSaved(buffer);
	buffer.connection = buffer.text->onChanged.connect([this, id](const affresco::TextChange&) { changed(id); });
	buffers_.emplace(id, std::move(buffer));
	refreshModel();
	return std::nullopt;
}

std::optional<std::string> Buffers::save(const std::string& id) {
	const auto found = buffers_.find(id);
	if(found == buffers_.end()) return "not open: " + id;
	Buffer& buffer = found->second;
	const std::string text = buffer.text->text();
	if(auto error = writeInFolder(buffer.path, text)) return *error + ": " + id;
	markSaved(buffer);
	refreshModel();
	return std::nullopt;
}

std::optional<std::string> Buffers::saveAll() {
	std::optional<std::string> first;
	for(auto& [id, buffer] : buffers_)
		if(buffer.dirty)
			if(auto error = save(id); error && !first) first = std::move(error);
	return first;
}

std::optional<std::string> Buffers::revert(const std::string& id) {
	const auto found = buffers_.find(id);
	if(found == buffers_.end()) return "not open: " + id;
	Buffer& buffer = found->second;
	std::string text;
	if(auto error = readFile(buffer.path, limits_.fileBytes, text)) return *error + ": " + id;
	quiet_ = true;
	buffer.text->assign(text);
	quiet_ = false;
	markSaved(buffer);
	refreshModel();
	return std::nullopt;
}

bool Buffers::close(const std::string& id) {
	const auto found = buffers_.find(id);
	if(found == buffers_.end()) return false;
	found->second.text->onChanged.disconnect(found->second.connection);
	buffers_.erase(found);
	refreshModel();
	return true;
}

std::shared_ptr<affresco::TextModel> Buffers::find(std::string_view id) const {
	const auto found = buffers_.find(id);
	return found == buffers_.end() ? nullptr : found->second.text;
}

bool Buffers::dirty(std::string_view id) const {
	const auto found = buffers_.find(id);
	return found != buffers_.end() && found->second.dirty;
}

std::vector<std::string> Buffers::dirtyIds() const {
	std::vector<std::string> ids;
	for(const auto& [id, buffer] : buffers_)
		if(buffer.dirty) ids.push_back(id);
	return ids;
}

affresco::json Buffers::todos() const {
	affresco::json found = affresco::json::array();
	for(const auto& [id, buffer] : buffers_) {
		if(buffer.text->size() > limits_.keptBytes) continue; // a huge file isn't scanned on every request
		const std::string text = buffer.text->text();
		std::size_t start = 0, line = 1;
		while(start <= text.size() && found.size() < limits_.todos) {
			const std::size_t end = std::min(text.find('\n', start), text.size());
			const std::string_view row(text.data() + start, end - start);
			const bool fixme = row.contains("FIXME");
			if(fixme || row.contains("TODO"))
				found.push_back({{"severity", fixme ? "warning" : "info"},
				                 {"message", message(row)},
				                 {"file", id},
				                 {"label", buffer.label},
				                 {"line", line}});
			start = end + 1;
			++line;
		}
	}
	return found;
}

void Buffers::changed(const std::string& id) {
	if(quiet_) return;
	const auto found = buffers_.find(id);
	if(found == buffers_.end()) return;
	Buffer& buffer = found->second;
	const affresco::TextModel& text = *buffer.text;
	const bool dirty = text.size() != buffer.savedSize || !buffer.saved || !text.equals(*buffer.saved);
	if(dirty == buffer.dirty) return;
	buffer.dirty = dirty;
	refreshModel();
	if(onDirty_) onDirty_(id, dirty);
}

void Buffers::markSaved(Buffer& buffer) const {
	buffer.savedSize = buffer.text->size();
	buffer.saved.reset();
	if(buffer.savedSize <= limits_.keptBytes) buffer.saved = buffer.text->text();
	buffer.dirty = false;
}

void Buffers::refreshModel() const {
	std::vector<affresco::json> rows;
	rows.reserve(buffers_.size());
	for(const auto& [id, buffer] : buffers_)
		rows.push_back({{"id", id},
		                {"label", buffer.label},
		                {"dirty", buffer.dirty},
		                {"lines", buffer.text->lineCount()},
		                {"bytes", buffer.text->size()}});
	model_->reset(std::move(rows));
}

affresco::json Buffers::outline(std::string_view id) const {
	affresco::json found = affresco::json::array();
	const auto it = buffers_.find(id);
	if(it == buffers_.end() || it->second.text->size() > limits_.keptBytes) return found;
	const bool markdown = it->second.label.ends_with(".md");
	const std::string text = it->second.text->text();
	std::size_t start = 0, line = 1;
	while(start <= text.size() && found.size() < limits_.todos) {
		const std::size_t end = std::min(text.find('\n', start), text.size());
		if(auto entry = outlineEntry(std::string_view(text.data() + start, end - start), markdown))
			found.push_back({{"line", line}, {"label", std::move(entry->second)}, {"depth", entry->first}});
		start = end + 1;
		++line;
	}
	return found;
}

} // namespace forge
