/// @file
/// @brief Edits applied to the open folder (LspEdits.hpp).

#include "features/lsp/LspEdits.hpp"

#include "features/lsp/LspCall.hpp"
#include "features/lsp/LspProtocol.hpp"
#include "features/palette/FileIndex.hpp"

#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace forge::lsp {

namespace {

using affresco::json;
namespace fs = std::filesystem;

constexpr std::size_t largestFile = std::size_t{8} << 20; ///< A file forge.lsp.apply edits on disk, at most 8 MiB.

/// @brief Groups edits by the file they change.
/// @param edits The edits.
/// @param root The open folder.
/// @return The edits of each file, by id.
/// @throws std::runtime_error If an edit names a file outside the folder.
std::map<std::string, json> byFile(const json& edits, const fs::path& root) {
	std::map<std::string, json> files;
	for(const json& edit : edits) {
		const std::string id = textArgument(edit, "id", 4096);
		// the id comes from the document: it must name a file inside the folder
		if(id.empty() || idOfUri(root, uriOf(root, id)) != id)
			throw std::runtime_error("an edit names a file outside the open folder");
		files[id].push_back(edit);
	}
	return files;
}

/// @brief A file's text as it is now: its open buffer's, else the file's.
/// @param id The file.
/// @param root The open folder.
/// @param buffers The open files' texts.
/// @return The text.
/// @throws std::runtime_error If the file can't be read, or is larger than largestFile.
std::string currentText(const std::string& id, const fs::path& root, const Buffers& buffers) {
	if(const auto model = buffers.find(id)) return model->text();
	std::error_code error;
	if(fs::file_size(root / id, error) > largestFile || error)
		throw std::runtime_error(id + " can't be read, or is too large");
	std::ifstream in(root / id, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// @brief Replaces a buffer's text as one undo step, changing only the lines between the first and last that differ.
/// @param model The buffer.
/// @param text The new text.
void replace(affresco::TextModel& model, std::string_view text) {
	const std::string old = model.text();
	std::size_t prefix = 0;
	while(prefix < old.size() && prefix < text.size() && old[prefix] == text[prefix]) ++prefix;
	std::size_t suffix = 0;
	while(suffix < old.size() - prefix && suffix < text.size() - prefix &&
	      old[old.size() - 1 - suffix] == text[text.size() - 1 - suffix])
		++suffix;
	while(prefix > 0 && (static_cast<unsigned char>(old[prefix]) & 0xC0) == 0x80) --prefix;
	while(suffix > 0 && (static_cast<unsigned char>(old[old.size() - suffix]) & 0xC0) == 0x80) --suffix;
	const affresco::TextPosition from = model.positionOf(prefix), to = model.positionOf(old.size() - suffix);
	model.beginStep(false, from, from);
	model.erase(from, to);
	model.insert(from, text.substr(prefix, text.size() - prefix - suffix));
	model.endStep();
}

} // namespace

json applyToFolder(const json& with, const fs::path& root, const Buffers& buffers) {
	const json& edits = with.is_object() && with.contains("edits") ? with["edits"] : json();
	if(!edits.is_array() || edits.size() > Limits{}.edits) throw std::runtime_error("expected { \"edits\": [...] }");
	if(root.empty()) throw std::runtime_error("no folder is open");
	std::map<std::string, std::string> texts; // checked first, so a bad edit changes nothing
	for(auto& [id, list] : byFile(edits, root)) {
		std::string text = currentText(id, root, buffers);
		if(auto problem = applyEdits(text, list)) throw std::runtime_error(id + ": " + *problem);
		texts.emplace(id, std::move(text));
	}
	for(auto& [id, text] : texts) {
		if(const auto model = buffers.find(id)) {
			replace(*model, text);
			continue;
		}
		// A fresh name, created exclusively: a link planted at a fixed name would send the text elsewhere
		if(auto failed = writeInFolder(root / id, text)) throw std::runtime_error(id + ": " + *failed);
	}
	return {{"files", texts.size()}, {"edits", edits.size()}};
}

} // namespace forge::lsp
