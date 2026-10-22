/// @file
/// @brief Fuzzes a history read back from disk (Repository::openHistory, VERSION_CONTROL.md "History on disk"): the
/// folder may have been damaged by a crash, a full disk or anyone who can write to it.
///
/// A Kanban history is written once to a template folder. Each input is copied over one of its files: refs.json when
/// the first byte is even, otherwise the first object file. Opening the damaged copy must load it or throw
/// DocumentError (what() naming the file); any other exception, or a crash, is a bug.

#include "Fuzz.hpp"

#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

/// @brief The rules every repository here uses.
/// @return The default rules.
const affresco::format::Rules& rules() {
	static const affresco::format::Rules defaults;
	return defaults;
}

/// @brief The folder this process fuzzes in.
/// @param name What it holds.
/// @return The folder under the system's temporary folder, named after the process.
fs::path folder(const std::string& name) {
	return fs::temp_directory_path() / ("affrescoui-fuzz-" + name + "-" + std::to_string(getpid()));
}

/// @brief The document every history here holds: the Kanban template without its include, which a screen's loader
/// resolves before a history ever sees the document.
/// @return The document.
const affresco::json& document() {
	static const affresco::json kanban = [] {
		affresco::json copy = affresco::fuzz::kanbanTemplate();
		copy.erase("include");
		return copy;
	}();
	return kanban;
}

/// @brief Writes the Kanban history to the template folder, once.
/// @return The template folder.
const fs::path& templateHistory() {
	static const fs::path directory = [] {
		const fs::path path = folder("history-template");
		fs::remove_all(path);
		affresco::vcs::Repository repository(rules(), document());
		static_cast<void>(repository.openHistory(path));
		return path;
	}();
	return directory;
}

/// @brief The first object file of a history.
/// @param directory The history's folder.
/// @return The file, or an empty path if there is none.
fs::path firstObject(const fs::path& directory) {
	for(const fs::directory_entry& entry : fs::recursive_directory_iterator(directory / "objects"))
		if(entry.is_regular_file()) return entry.path();
	return {};
}

} // namespace

/// @brief Silences the log and writes the template history before the first input.
/// @return 0.
extern "C" int LLVMFuzzerInitialize(int*, char***) {
	affresco::fuzz::silenceLog();
	static_cast<void>(templateHistory());
	return 0;
}

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const fs::path work = folder("history");
	fs::remove_all(work);
	fs::copy(templateHistory(), work, fs::copy_options::recursive);
	const fs::path damaged = data[0] % 2 == 0 ? work / "refs.json" : firstObject(work);
	if(damaged.empty()) affresco::fuzz::broken("the template history has an object");
	{
		std::ofstream file(damaged, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(data + 1), static_cast<std::streamsize>(size - 1));
	}
	try {
		affresco::vcs::Repository repository(rules(), document());
		static_cast<void>(repository.openHistory(work));
	} catch(const affresco::DocumentError& error) {
		if(std::string(error.what()).empty()) affresco::fuzz::broken("a damaged history names what is wrong");
	}
	return 0;
}
