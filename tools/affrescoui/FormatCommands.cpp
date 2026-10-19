/// @file
/// @brief `affrescoui fmt` and `affrescoui ids`: changes to a file's text that keep everything else as written.

#include "Cli.hpp"
#include "JsonText.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/format/NodeIds.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>

namespace affresco::cli {

namespace {

/// The flags fmt and ids take.
constexpr std::array<std::string_view, 1> checkFlag = {"--check"};

/// @brief Writes a file's new text, or with --check says whether it would change.
/// @param file The file.
/// @param before Its text.
/// @param after Its new text.
/// @param check Only say whether it would change.
/// @param what What --check reports when it would ("not formatted").
/// @return Success, or Problems if --check finds it would change.
Exit finish(const std::filesystem::path& file, const std::string& before, const std::string& after, bool check,
            std::string_view what) {
	if(before == after) return Exit::Success;
	if(check) {
		std::cout << file.string() << ": " << what << '\n';
		return Exit::Problems;
	}
	writeFile(file, after);
	return Exit::Success;
}

/// @brief Reads a file as written.
/// @param file The file.
/// @param text Its text.
/// @return Its tree.
/// @throws FileProblem If it isn't JSON, naming the file.
TextValue parsed(const std::filesystem::path& file, const std::string& text) {
	try {
		return parseText(text);
	} catch(const std::runtime_error& error) {
		throw FileProblem(file.string() + ": " + error.what());
	}
}

} // namespace

Exit formatScreen(std::span<const std::string_view> args) {
	const Arguments arguments(args, {}, checkFlag);
	const std::filesystem::path file = arguments.file();
	const std::string text = readFile(file);
	return finish(file, text, printText(parsed(file, text)), arguments.flag("--check"), "not formatted (run fmt)");
}

// The ids come from the format's own namer, on the document as the library reads it. Each is written into the text
// right after its node's '{', laid out like the node's first member, so that nothing else in the file changes
Exit nameScreenNodes(std::span<const std::string_view> args) {
	const Arguments arguments(args, {}, checkFlag);
	const std::filesystem::path file = arguments.file();
	std::string text = readFile(file);
	const std::string before = text;
	TextValue tree = parsed(file, text);
	json document = parseUntrusted(text);

	const auto named = format::NodeNamer::nameAll(document);
	std::vector<std::pair<std::size_t, std::string>> insertions; // where, what
	for(const auto& [pointer, id] : named) {
		const TextValue* node = valueAt(tree, pointer);
		if(!node || node->kind != TextValue::Kind::Object)
			throw std::runtime_error(file.string() + ": no node at " + pointer);
		const std::string member = "\"id\": " + json(id).dump();
		insertions.emplace_back(node->start + 1, node->members.empty() ? " " + member + " "
		                                         : node->lead.empty()  ? member + ", "
		                                                               : node->lead + member + ",");
	}
	std::ranges::sort(insertions, std::greater<>{}); // from the end, so the offsets before stay right
	for(const auto& [offset, member] : insertions) text.insert(offset, member);

	if(!arguments.flag("--check"))
		for(const auto& [pointer, id] : named) std::cout << pointer << " #" << id << '\n';
	return finish(file, before, text, arguments.flag("--check"),
	              std::to_string(named.size()) + " nodes have no id (run ids)");
}

} // namespace affresco::cli
