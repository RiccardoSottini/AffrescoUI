/// @file
/// @brief The "search" workloads: the services' search engine (affresco/services/Search.hpp) over a generated folder
/// of 2,000 code-like files of 200 lines (about 10 MB): a word found in every file with one thread and with eight, an
/// expression, and a search capped at 100 results, which must stop early. No window.

#include "Bench.hpp"

#include <affresco/services/Search.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

namespace affresco::bench {
namespace {

/// @brief A folder of its own under the system's temporary folder holding the files, removed with the object.
struct TempFolder {
	std::filesystem::path folder =
	    std::filesystem::temp_directory_path() / ("aui-bench-search-" + std::to_string(std::random_device{}()));

	/// @brief Writes the files, ten per subfolder: lines of a few words, the same on every machine.
	/// @param files How many files.
	/// @param lines The lines of each.
	TempFolder(std::size_t files, std::size_t lines) {
		static constexpr const char* words[] = {"int", "value", "return", "if", "(count", "+", "1);", "// note"};
		std::uint32_t random = 2017; // a linear congruential generator, as engine.cpp's
		for(std::size_t file = 0; file < files; ++file) {
			const std::filesystem::path sub = folder / ("module" + std::to_string(file / 10));
			std::filesystem::create_directories(sub);
			std::string text;
			for(std::size_t line = 0; line < lines; ++line) {
				random = random * 1664525u + 1013904223u;
				for(std::uint32_t word = 0, count = 3 + (random >> 8) % 6; word < count; ++word)
					(text += word ? " " : "\t") += words[(random >> (word + 4)) % std::size(words)];
				text += '\n';
			}
			std::ofstream(sub / ("file" + std::to_string(file) + ".cpp"), std::ios::binary) << text;
		}
	}
	/// @brief Removes the folder.
	~TempFolder() {
		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}
	TempFolder(const TempFolder&) = delete;
	TempFolder& operator=(const TempFolder&) = delete;
};

/// @brief A workload searching the folder.
/// @param name Its name.
/// @param folder The folder.
/// @param query The query.
/// @return The workload.
Workload searching(std::string name, std::shared_ptr<const TempFolder> folder, services::SearchQuery query) {
	Workload workload{.name = std::move(name), .unit = "ms", .runs = 5};
	workload.step = [folder = std::move(folder), query = std::move(query)] {
		services::SearchSummary summary;
		std::size_t found = 0;
		const std::optional<std::string> error = services::searchFiles(
		    folder->folder, query, std::stop_token{},
		    [&found](services::SearchFile&& file) { found += file.matches.size(); }, summary);
		if(error) throw std::runtime_error("search: " + *error);
		if(found == 0) throw std::runtime_error("search: nothing found");
	};
	return workload;
}

} // namespace

std::vector<Workload> searchWorkloads(Context&) {
	const auto folder = std::make_shared<const TempFolder>(2000, 200);
	const services::SearchQuery word{.pattern = "value",
	                                 .options = {},
	                                 .include = {},
	                                 .exclude = {},
	                                 .maxResults = services::SearchQuery::mostResults,
	                                 .threads = 1};
	services::SearchQuery threads = word;
	threads.threads = 8;
	services::SearchQuery expression = word;
	expression.pattern = R"(\bvalue\s+\S+)";
	expression.options.regex = true;
	services::SearchQuery capped = word;
	capped.maxResults = 100;
	std::vector<Workload> workloads;
	workloads.push_back(searching("search/word", folder, word));
	workloads.push_back(searching("search/word-8-threads", folder, threads));
	workloads.push_back(searching("search/regex", folder, expression));
	workloads.push_back(searching("search/capped", folder, capped));
	return workloads;
}

} // namespace affresco::bench
