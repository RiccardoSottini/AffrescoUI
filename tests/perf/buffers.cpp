/// @file
/// @brief The "buffers" workloads: Forge's file buffers (examples/forge/features/editor/Buffers.hpp) with a file of
/// 1,000,000 code-like lines (about 40 MB) and one of 10,000: opening it, an edit then its undo with the dirty check
/// (two operations), and saving it. No window.

#include "Bench.hpp"

#include "features/editor/Buffers.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco::bench {
namespace {

/// @brief A folder of its own under the system's temporary folder holding one file, removed with the object.
struct TempFile {
	std::filesystem::path folder = std::filesystem::temp_directory_path() /
	                               ("aui-bench-buffers-" + std::to_string(std::random_device{}()));
	std::string id = "big.cpp"; ///< The file, in the folder.

	/// @brief Writes the file: lines of a few words, the same on every machine.
	/// @param lines How many lines.
	explicit TempFile(std::size_t lines) {
		std::filesystem::create_directories(folder);
		static constexpr const char* words[] = {"int", "value", "return", "if", "(count", "+", "1);", "// note"};
		std::uint32_t random = 2017; // a linear congruential generator, as engine.cpp's
		std::string text;
		for(std::size_t line = 0; line < lines; ++line) {
			random = random * 1664525u + 1013904223u;
			for(std::uint32_t word = 0, count = 3 + (random >> 8) % 6; word < count; ++word)
				(text += word ? " " : "\t") += words[(random >> (word + 4)) % std::size(words)];
			text += '\n';
		}
		std::ofstream(folder / id, std::ios::binary) << text;
	}
	/// @brief Removes the folder.
	~TempFile() {
		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;
};

/// @brief Fails a workload whose call went wrong.
/// @param error What the call returned.
/// @throws std::runtime_error With it.
void require(const std::optional<std::string>& error) {
	if(error) throw std::runtime_error("buffers: " + *error);
}

/// @brief The workloads for a file of some size.
/// @param lines Its lines.
/// @param suffix The names' last part, e.g. "1m".
/// @param workloads Gets them.
void sized(std::size_t lines, const std::string& suffix, std::vector<Workload>& workloads) {
	// Shared by the three workloads, and gone with the last of them
	const auto file = std::make_shared<const TempFile>(lines);
	const auto buffers = std::make_shared<std::unique_ptr<forge::Buffers>>();

	Workload open{.name = "buffers/open-" + suffix, .unit = "ms", .runs = 5};
	open.prepare = [buffers] { *buffers = std::make_unique<forge::Buffers>(); };
	open.step = [buffers, file] { require((*buffers)->open(file->folder, file->id)); };
	workloads.push_back(std::move(open));

	const auto opened = [buffers, file] {
		*buffers = std::make_unique<forge::Buffers>();
		require((*buffers)->open(file->folder, file->id));
	};
	Workload edit{.name = "buffers/edit-" + suffix, .unit = "us", .runs = 9, .operations = 2};
	edit.prepare = opened;
	edit.step = [buffers, file, middle = lines / 2] {
		affresco::TextModel& text = *(*buffers)->find(file->id);
		text.insert({middle, 0}, "x"); // dirty
		text.erase({middle, 0}, {middle, 1}); // clean again, or dirty if the saved text wasn't kept
	};
	workloads.push_back(std::move(edit));

	Workload save{.name = "buffers/save-" + suffix, .unit = "ms", .runs = 5};
	save.prepare = opened;
	save.step = [buffers, file] { require((*buffers)->save(file->id)); };
	workloads.push_back(std::move(save));
}

} // namespace

std::vector<Workload> bufferWorkloads(Context&) {
	std::vector<Workload> workloads;
	sized(10'000, "10k", workloads);
	sized(1'000'000, "1m", workloads);
	return workloads;
}

} // namespace affresco::bench
