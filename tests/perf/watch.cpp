/// @file
/// @brief The "watch" workloads: a folder of 100,000 files listed and watched as a DirectoryModel, and one change
/// in it seen by the model.

#include "Bench.hpp"

#include <affresco/services/DirectoryModel.hpp>

#include <chrono>
#include <fstream>
#include <stdexcept>

namespace affresco::bench {
namespace {

constexpr int folderCount = 100;     ///< Folders in the tree.
constexpr int filesPerFolder = 1000; ///< Files in each, so 100,000 in all.

/// @brief The tree of 100,000 files, under the system's temporary folder, removed at the end.
struct ManyFiles {
	std::filesystem::path root; ///< The tree's folder (canonical).
	PumpedExecutor executor;    ///< The models' main thread.

	/// @brief Writes the tree.
	ManyFiles() {
		const std::filesystem::path made = std::filesystem::temp_directory_path() / "affrescoui-bench-watch";
		std::filesystem::remove_all(made);
		for(int folder = 0; folder < folderCount; ++folder) {
			const std::filesystem::path path = made / ("folder" + std::to_string(folder));
			std::filesystem::create_directories(path);
			for(int file = 0; file < filesPerFolder; ++file)
				std::ofstream(path / ("file" + std::to_string(file) + ".txt"));
		}
		root = std::filesystem::canonical(made);
	}
	~ManyFiles() {
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
	}
	ManyFiles(const ManyFiles&) = delete;
	ManyFiles& operator=(const ManyFiles&) = delete;
	ManyFiles(ManyFiles&&) = delete;
	ManyFiles& operator=(ManyFiles&&) = delete;
};

} // namespace

std::vector<Workload> watchWorkloads(Context& /*context*/) {
	const auto files = std::make_shared<ManyFiles>();
	std::vector<Workload> workloads;
	workloads.push_back({.name = "watch/100000", .unit = "ms", .runs = 5, .step = [files] {
		                     const auto model = services::DirectoryModel::open(files->executor, files->root);
		                     files->executor.pumpUntil([&model] { return model->ready(); }, "watch: the model");
		                     if(model->size() != std::size_t{folderCount} * (filesPerFolder + 1) || !model->watching())
			                     throw std::runtime_error("watch: the listing is incomplete");
	                     }});
	const auto model = services::DirectoryModel::open(files->executor, files->root,
	                                                  {.ignore = {}, .debounce = std::chrono::milliseconds(20)});
	files->executor.pumpUntil([model] { return model->ready(); }, "watch: the model");
	const auto changed = std::make_shared<bool>(false);
	(void)model->onChange.connect(
	    [changed](const ModelChange& change) { *changed = change.kind == ModelChange::Kind::Changed; });
	const auto size = std::make_shared<int>(0);
	workloads.push_back({.name = "watch/100000/change", .unit = "ms", .runs = 9, .step = [files, model, changed, size] {
		                     *changed = false;
		                     std::ofstream(files->root / "folder7" / "file7.txt")
		                         << std::string(static_cast<std::size_t>(++*size), 'x');
		                     files->executor.pumpUntil([changed] { return *changed; }, "watch: the model");
	                     }});
	return workloads;
}

} // namespace affresco::bench
