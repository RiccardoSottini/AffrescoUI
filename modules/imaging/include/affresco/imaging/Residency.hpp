#pragma once

/// @file
/// @brief TileResidency: which tiles are on the GPU, within a budget of bytes, the least recently used evicted first.
/// It holds no GL objects: the canvas keeps a texture per entry and frees the ones evicted.

#include "affresco/imaging/Image.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

namespace affresco::imaging {

/// @brief A tile of one image of a document: the image's id and the tile's key.
struct ResidentKey {
	std::uint64_t image = 0; ///< The image's id.
	TileKey tile;            ///< The tile.

	/// @brief Whether two keys are the same.
	[[nodiscard]] friend constexpr bool operator==(const ResidentKey&, const ResidentKey&) noexcept = default;
};

/// @brief Hashes a ResidentKey.
struct ResidentKeyHash {
	/// @brief The hash.
	/// @param key The key.
	[[nodiscard]] std::size_t operator()(const ResidentKey& key) const noexcept {
		return TileKeyHash{}(key.tile) ^ (key.image * 0x9e3779b97f4a7c15ull);
	}
};

/// @brief A resident tile, as the canvas sees it.
struct Resident {
	unsigned texture = 0;      ///< The canvas's texture holding it.
	std::uint64_t version = 0; ///< The tile's version (Image::version) when it was uploaded.
	std::size_t bytes = 0;     ///< Its GPU memory.
};

/// @brief The tiles on the GPU, at most a budget of bytes; using one makes it the most recent.
class TileResidency {
public:
	/// @brief An empty residency.
	/// @param budget The most bytes resident (Limits::gpuBytes).
	explicit TileResidency(std::size_t budget) noexcept : budget_(budget) {}

	/// @brief A tile if it is resident, made the most recently used.
	/// @param key The tile.
	/// @return Its entry, valid until the next admit() or clear(); or null.
	[[nodiscard]] Resident* find(const ResidentKey& key);
	/// @brief Makes a tile resident, evicting the least recently used until it fits.
	/// @param key The tile, not resident.
	/// @param resident Its texture, version and bytes.
	/// @param evicted Receives the entries evicted, whose textures the caller frees or reuses (cleared first).
	/// @return Whether it was admitted: false only when it alone is over the budget.
	bool admit(const ResidentKey& key, const Resident& resident, std::vector<Resident>& evicted);
	/// @brief Forgets one tile.
	/// @param key The tile.
	/// @return Its entry, whose texture the caller frees or reuses; or nothing if it was not resident.
	std::optional<Resident> forget(const ResidentKey& key);
	/// @brief Forgets every tile.
	/// @param evicted Receives them all.
	void clear(std::vector<Resident>& evicted);

	/// @brief The bytes resident.
	[[nodiscard]] std::size_t bytes() const noexcept { return bytes_; }
	/// @brief The budget.
	[[nodiscard]] std::size_t budget() const noexcept { return budget_; }
	/// @brief The tiles resident.
	[[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
	/// @brief How many tiles were evicted, in all.
	[[nodiscard]] std::uint64_t evictions() const noexcept { return evictions_; }

private:
	/// @brief An entry: the key and the tile.
	using Entry = std::pair<ResidentKey, Resident>;

	std::size_t budget_;                                                                   ///< See budget().
	std::size_t bytes_ = 0;                                                                ///< See bytes().
	std::uint64_t evictions_ = 0;                                                          ///< See evictions().
	std::list<Entry> order_;                                                               ///< The most recent first.
	std::unordered_map<ResidentKey, std::list<Entry>::iterator, ResidentKeyHash> entries_; ///< By key.
};

} // namespace affresco::imaging
