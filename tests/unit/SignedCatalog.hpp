#pragma once

/// @file
/// @brief Signed feature catalogues for tests: a local catalogue in a temporary folder, its releases signed by a key
/// the test makes, and the reason a call is refused.

#include "FeatureSignatures.hpp"

#include <affresco/core/Json.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace affresco::test {

/// @brief Writes a file.
/// @param file The file.
/// @param text Its bytes.
inline void write(const std::filesystem::path& file, const std::string& text) {
	std::filesystem::create_directories(file.parent_path());
	std::ofstream out(file, std::ios::binary);
	out << text;
	REQUIRE(out);
}

/// @brief A local catalogue in a temporary folder, removed with it.
struct Catalog {
	std::filesystem::path folder; ///< Its folder.
	json index;                   ///< Its index, unsigned.

	/// @brief Makes an empty catalogue.
	Catalog() {
		std::random_device random;
		folder = std::filesystem::temp_directory_path() / ("affrescoui-catalog-" + std::to_string(random()));
		std::filesystem::create_directories(folder);
		index = {{"format", "affrescoui-catalog"},
		         {"version", 2},
		         {"name", "test"},
		         {"publishers", json::object()},
		         {"features", json::object()}};
	}
	~Catalog() {
		std::error_code error;
		std::filesystem::remove_all(folder, error);
	}
	Catalog(const Catalog&) = delete;
	Catalog& operator=(const Catalog&) = delete;

	/// @brief Adds a release: its manifest signed and written, listed in the index with its digest and size.
	/// @param manifest The manifest.
	/// @param key Who signs it.
	/// @return The manifest's file.
	std::filesystem::path add(json manifest, const format::SigningKey& key) {
		const std::string name = manifest["name"], release = manifest["release"];
		const std::string url = name + "/" + release + ".json";
		const std::string bytes = key.sign(std::move(manifest)).dump(1, '\t');
		write(folder / url, bytes);
		index["publishers"][key.publisher()] = {{"name", "Test publisher"}};
		index["features"][name]["title"] = "Word count";
		index["features"][name]["releases"].push_back({{"release", release},
		                                               {"url", url},
		                                               {"digest", affresco::format::digestOf(bytes)},
		                                               {"publisher", key.publisher()},
		                                               {"size", bytes.size()}});
		return folder / url;
	}

	/// @brief Signs the index and writes it.
	/// @param key Who signs it.
	/// @return The index's file.
	std::filesystem::path publish(const format::SigningKey& key) const {
		write(folder / "index.json", key.sign(index).dump(1, '\t'));
		return folder / "index.json";
	}
};

/// @brief The reason something is refused, or "" if it isn't.
/// @param run What to run.
/// @return The reason.
template <typename Run> std::string refusal(Run run) {
	try {
		run();
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return {};
}

} // namespace affresco::test
