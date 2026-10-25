/// @file
/// @brief The project file: the header and manifest are read at once; each image's tiles stay deflated in the file,
/// held in memory, and are inflated by a tile source when first used. Saving deflates each distinct tile once and
/// copies tiles still in the file a document came from as they are.

#include "affresco/imaging/Project.hpp"

#include "ProjectJson.hpp"
#include "Stb.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace affresco::imaging {

namespace {

using project::malformed;
using project::whole;

constexpr std::size_t headerBytes = projectMagic.size() + 8; ///< The magic, the version and the manifest's length.

/// @brief Where bytes are among the blobs.
struct Blob {
	std::size_t offset = 0; ///< From the first blob.
	std::size_t length = 0; ///< Bytes.
};

/// @brief Where each tile of an image is.
using BlobMap = std::unordered_map<TileKey, Blob, TileKeyHash>;

/// @brief Reads a big-endian 32-bit number.
/// @param bytes At least 4 bytes.
[[nodiscard]] std::uint32_t read32(std::span<const std::uint8_t> bytes) noexcept {
	return static_cast<std::uint32_t>(bytes[0]) << 24 | static_cast<std::uint32_t>(bytes[1]) << 16 |
	       static_cast<std::uint32_t>(bytes[2]) << 8 | bytes[3];
}

/// @brief Appends a big-endian 32-bit number.
/// @param bytes The bytes.
/// @param value The number.
void write32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
	for(int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

/// @brief The manifest's name of a pixel format.
/// @param format rgba8 or rgba16f.
[[nodiscard]] const char* formatName(PixelFormat format) noexcept {
	return format == PixelFormat::rgba16f ? "rgba16f" : "rgba8";
}

/// @brief An image's tiles, deflated in a project file kept in memory.
class ProjectTileSource final : public TileSource {
public:
	/// @brief A source.
	/// @param file The file.
	/// @param base Where its blobs start.
	/// @param format The tiles' format.
	/// @param blobs Where each tile is, checked to be inside the file.
	ProjectTileSource(std::shared_ptr<const std::vector<std::uint8_t>> file, std::size_t base, PixelFormat format,
	                  BlobMap blobs)
	    : file_(std::move(file)), base_(base), format_(format), blobs_(std::move(blobs)) {}

	/// @brief Mips are made from the tiles.
	[[nodiscard]] bool providesLevel(int /*level*/) const override { return false; }

	/// @brief Inflates a tile: null when absent or damaged.
	/// @param key Which.
	[[nodiscard]] std::shared_ptr<const Tile> load(const TileKey& key) override {
		const auto found = blobs_.find(key);
		if(key.level != 0 || found == blobs_.end()) return nullptr;
		const std::size_t size = tilePixels * bytesPerPixel(format_);
		auto bytes = stb::inflate(raw(found->second), size);
		if(!bytes || bytes->size() != size) return nullptr;
		return std::make_shared<const Tile>(format_, std::move(*bytes));
	}

	/// @brief A blob's bytes, deflated.
	/// @param blob The blob.
	[[nodiscard]] std::span<const std::uint8_t> raw(const Blob& blob) const noexcept {
		return std::span(*file_).subspan(base_ + blob.offset, blob.length);
	}
	/// @brief Where each tile is.
	[[nodiscard]] const BlobMap& blobs() const noexcept { return blobs_; }

private:
	std::shared_ptr<const std::vector<std::uint8_t>> file_; ///< The file.
	std::size_t base_;                                      ///< Where its blobs start.
	PixelFormat format_;                                    ///< The tiles' format.
	BlobMap blobs_;                                         ///< Where each tile is.
};

/// @brief Gathers the blobs of a file being saved.
class Writer {
public:
	/// @brief Adds bytes as a blob.
	/// @param bytes The bytes.
	/// @return [offset, length].
	[[nodiscard]] json add(std::span<const std::uint8_t> bytes) {
		const json place = json::array({blobs_.size(), bytes.size()});
		blobs_.insert(blobs_.end(), bytes.begin(), bytes.end());
		return place;
	}

	/// @brief Adds an image's tiles that aren't transparent.
	/// @param image The image.
	/// @return [[column, row, offset, length], …], top row first.
	[[nodiscard]] json tiles(const Image& image) {
		const auto* source = dynamic_cast<const ProjectTileSource*>(image.source().get());
		const std::vector<TileKey> keys = image.writtenTiles();
		const std::unordered_set<TileKey, TileKeyHash> written(keys.begin(), keys.end());
		json list = json::array();
		for(int row = 0; row < image.rows(); ++row)
			for(int column = 0; column < image.columns(); ++column) {
				const TileKey key{0, column, row};
				json place;
				if(written.contains(key) || (image.source() && source == nullptr)) place = deflated(image.tile(key));
				else if(source != nullptr) place = copied(*source, key);
				if(!place.is_null()) list.push_back(json::array({column, row, place[0], place[1]}));
			}
		return list;
	}

	/// @brief The blobs.
	[[nodiscard]] const std::vector<std::uint8_t>& blobs() const noexcept { return blobs_; }

private:
	/// @brief A tile deflated, once however many places share it; null when transparent.
	/// @param tile The tile.
	[[nodiscard]] json deflated(const std::shared_ptr<const Tile>& tile) {
		if(!tile || tile->empty()) return nullptr;
		if(const auto found = deflated_.find(tile.get()); found != deflated_.end()) return found->second;
		return deflated_[tile.get()] = add(stb::deflate(tile->bytes()));
	}

	/// @brief A tile still deflated in a loaded file, copied; null when absent.
	/// @param source The file's tiles.
	/// @param key Which.
	[[nodiscard]] json copied(const ProjectTileSource& source, const TileKey& key) {
		const auto found = source.blobs().find(key);
		if(found == source.blobs().end()) return nullptr;
		const std::span<const std::uint8_t> bytes = source.raw(found->second);
		if(const auto again = copied_.find(bytes.data()); again != copied_.end()) return again->second;
		return copied_[bytes.data()] = add(bytes);
	}

	std::vector<std::uint8_t> blobs_;                      ///< See blobs().
	std::unordered_map<const Tile*, json> deflated_;       ///< Tiles deflated, by tile.
	std::unordered_map<const std::uint8_t*, json> copied_; ///< Blobs copied, by where they were.
};

/// @brief A layer and the layers in it, as the manifest lists them.
/// @param document The document.
/// @param id The layer.
/// @param writer Takes the tiles.
[[nodiscard]] json layerToJson(const Document& document, std::uint64_t id, Writer& writer) {
	const Layer& layer = *document.layer(id);
	json value = project::layerSettingsToJson(layer);
	if(layer.kind == LayerKind::pixel) value["tiles"] = writer.tiles(*document.image(layer.pixels));
	if(layer.mask != 0) value["mask"] = writer.tiles(*document.image(layer.mask));
	if(layer.kind == LayerKind::group) {
		json children = json::array();
		for(const std::uint64_t child : layer.children) children.push_back(layerToJson(document, child, writer));
		value["children"] = std::move(children);
	}
	if(id == document.structure().active) value["active"] = true;
	return value;
}

/// @brief Builds a document's layers from a manifest.
class Reader {
public:
	/// @brief A reader.
	/// @param file The file.
	/// @param base Where its blobs start.
	/// @param document The document, for its size, format and limits.
	Reader(std::shared_ptr<const std::vector<std::uint8_t>> file, std::size_t base, const Document& document)
	    : file_(std::move(file)), base_(base), document_(document) {}

	/// @brief A blob from its [offset, length], checked to be inside the file.
	/// @param value The JSON.
	[[nodiscard]] Blob blob(const json& value) const {
		const std::size_t room = file_->size() - base_;
		if(!value.is_array() || value.size() != 2) malformed("has a blob that is not [offset, length]");
		// Both are at most room, a std::size_t, so they fit where std::size_t is 32 bits (wasm32) too
		const Blob found{static_cast<std::size_t>(whole(value[0], room, "blob offset")), 0};
		return {found.offset, static_cast<std::size_t>(whole(value[1], room - found.offset, "blob length"))};
	}

	/// @brief A blob's bytes.
	/// @param blob The blob.
	[[nodiscard]] std::span<const std::uint8_t> bytes(const Blob& blob) const noexcept {
		return std::span(*file_).subspan(base_ + blob.offset, blob.length);
	}

	/// @brief Reads a layer and the layers in it into a structure.
	/// @param value Its JSON.
	/// @param parent Its group, or 0.
	/// @param depth Its depth (0 at the top level).
	/// @param structure Takes the layers and their images.
	/// @return Its id.
	std::uint64_t layer(const json& value, std::uint64_t parent, int depth, Structure& structure) {
		if(++layers_ > document_.limits().layers)
			malformed("has more than " + std::to_string(document_.limits().layers) + " layers");
		Layer layer;
		project::readLayerSettings(value, layer);
		layer.id = structure.nextId++;
		layer.parent = parent;
		if(layer.kind == LayerKind::pixel)
			layer.pixels = image(value.value("tiles", json::array()), document_.format(), structure);
		if(value.contains("mask")) layer.mask = image(value["mask"], PixelFormat::mask8, structure);
		if(value.contains("children") != (layer.kind == LayerKind::group)) malformed("has children outside a group");
		if(layer.kind == LayerKind::group) {
			const json& children = value["children"];
			if(!children.is_array()) malformed("has children that are not a list");
			if(!children.empty() && depth + 1 > document_.limits().groupDepth)
				malformed("nests groups more than " + std::to_string(document_.limits().groupDepth) + " deep");
			for(const json& child : children)
				layer.children.push_back(this->layer(child, layer.id, depth + 1, structure));
		}
		if(value.contains("active") && value["active"] == true) structure.active = layer.id;
		const std::uint64_t id = layer.id;
		structure.layers.emplace(id, std::move(layer));
		return id;
	}

	/// @brief The tiles the manifest listed.
	[[nodiscard]] std::size_t tiles() const noexcept { return tiles_; }

private:
	/// @brief Makes an image whose tiles are in the file.
	/// @param list [[column, row, offset, length], …].
	/// @param format Its format.
	/// @param structure Takes it.
	/// @return Its id.
	std::uint64_t image(const json& list, PixelFormat format, Structure& structure) {
		if(!list.is_array()) malformed("has tiles that are not a list");
		const auto columns = static_cast<std::uint64_t>((document_.width() + tileSize - 1) / tileSize);
		const auto rows = static_cast<std::uint64_t>((document_.height() + tileSize - 1) / tileSize);
		BlobMap blobs;
		for(const json& entry : list) {
			if(!entry.is_array() || entry.size() != 4)
				malformed("has a tile that is not [column, row, offset, length]");
			const TileKey key{0, static_cast<int>(whole(entry[0], columns - 1, "column")),
			                  static_cast<int>(whole(entry[1], rows - 1, "row"))};
			if(!blobs.emplace(key, blob(json::array({entry[2], entry[3]}))).second) malformed("lists a tile twice");
			++tiles_;
		}
		const std::uint64_t id = structure.nextId++;
		structure.images.emplace(
		    id, std::make_shared<Image>(document_.width(), document_.height(), format, document_.limits(),
			                            std::make_shared<ProjectTileSource>(file_, base_, format, std::move(blobs))));
		return id;
	}

	std::shared_ptr<const std::vector<std::uint8_t>> file_; ///< The file.
	std::size_t base_;                                      ///< Where its blobs start.
	const Document& document_;                              ///< The document.
	std::size_t layers_ = 0;                                ///< Layers read.
	std::size_t tiles_ = 0;                                 ///< See tiles().
};

/// @brief Reads the header and parses the manifest.
/// @param file The file.
/// @param limits Its limits.
/// @return The manifest, and where the blobs start.
[[nodiscard]] std::pair<json, std::size_t> manifestOf(const std::vector<std::uint8_t>& file, const Limits& limits) {
	if(file.size() > limits.projectFileBytes)
		malformed("is over " + std::to_string(limits.projectFileBytes) + " bytes");
	if(!isProject(file) || file.size() < headerBytes) malformed("is not a project");
	const std::span<const std::uint8_t> bytes(file);
	if(read32(bytes.subspan(projectMagic.size())) != projectVersion) malformed("is of a newer version");
	const std::size_t length = read32(bytes.subspan(projectMagic.size() + 4));
	if(length > limits.manifestBytes || length > file.size() - headerBytes)
		malformed("has a manifest that is too long or cut short");
	const std::string_view text(reinterpret_cast<const char*>(file.data() + headerBytes), length);
	return {parseUntrusted(text), headerBytes + length};
}

/// @brief Makes the empty document a manifest describes: its size and pixel format.
/// @param manifest The manifest.
/// @param limits Its limits.
/// @return The document.
[[nodiscard]] std::unique_ptr<Document> documentOf(const json& manifest, const Limits& limits) {
	const auto side = static_cast<std::uint64_t>(limits.side);
	const auto format = manifest.value("pixels", std::string("rgba8"));
	if(format != "rgba8" && format != "rgba16f") malformed("has pixels of no known format");
	return std::make_unique<Document>(static_cast<int>(whole(manifest.value("width", json()), side, "width")),
	                                  static_cast<int>(whole(manifest.value("height", json()), side, "height")),
	                                  limits, format == "rgba16f" ? PixelFormat::rgba16f : PixelFormat::rgba8);
}

} // namespace

bool isProject(std::span<const std::uint8_t> file) noexcept {
	return file.size() >= projectMagic.size() && std::equal(projectMagic.begin(), projectMagic.end(), file.begin());
}

std::vector<std::uint8_t> saveProject(const Document& document, const std::optional<ColorProfile>& profile) {
	Writer writer;
	json manifest{
	    {"width", document.width()}, {"height", document.height()}, {"pixels", formatName(document.format())}};
	if(profile) manifest["profile"] = writer.add(profile->bytes());
	json layers = json::array();
	for(const std::uint64_t id : document.structure().root) layers.push_back(layerToJson(document, id, writer));
	manifest["layers"] = std::move(layers);
	const std::string text = manifest.dump();
	std::vector<std::uint8_t> file(projectMagic.begin(), projectMagic.end());
	file.reserve(headerBytes + text.size() + writer.blobs().size());
	write32(file, projectVersion);
	write32(file, static_cast<std::uint32_t>(text.size()));
	file.insert(file.end(), text.begin(), text.end());
	file.insert(file.end(), writer.blobs().begin(), writer.blobs().end());
	return file;
}

// NOLINTNEXTLINE(performance-unnecessary-value-param): its public declaration stays as it is
LoadedProject loadProject(std::shared_ptr<const std::vector<std::uint8_t>> file, const Limits& limits) {
	if(!file) malformed("is missing");
	try {
		const auto [manifest, base] = manifestOf(*file, limits);
		if(!manifest.is_object()) malformed("has a manifest that is not an object");
		LoadedProject loaded;
		loaded.document = documentOf(manifest, limits);
		Reader reader(file, base, *loaded.document);
		if(manifest.contains("profile"))
			loaded.profile = ColorProfile::parse(reader.bytes(reader.blob(manifest["profile"])), limits);
		const json& layers = manifest.value("layers", json());
		if(!layers.is_array() || layers.empty()) malformed("has no layers");
		loaded.document->changeStructure("Open", [&](Structure& structure) {
			structure = Structure{};
			for(const json& layer : layers) structure.root.push_back(reader.layer(layer, 0, 0, structure));
			if(structure.active == 0) structure.active = structure.root.back();
		});
		loaded.document->clearHistory();
		loaded.tiles = reader.tiles();
		return loaded;
	} catch(const json::exception& error) {
		malformed(std::string("has a malformed manifest: ") + error.what());
	} catch(const std::invalid_argument& error) {
		malformed(error.what());
	}
}

} // namespace affresco::imaging
