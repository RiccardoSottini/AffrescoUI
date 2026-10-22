/// @file
/// @brief The document: its layers' structure, its pixel steps, and its history.

#include "affresco/imaging/Document.hpp"

#include "affresco/core/Json.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace affresco::imaging {

namespace {

/// @brief The longest layer name kept.
constexpr std::size_t maxNameBytes = 255;

/// @brief A layer list of a structure: a group's children, or the top level's.
/// @param structure The structure.
/// @param parent The group, or 0.
[[nodiscard]] std::vector<std::uint64_t>& listOf(Structure& structure, std::uint64_t parent) {
	if(parent == 0) return structure.root;
	return structure.layers.at(parent).children;
}

/// @brief An estimate of a structure's memory, for the history's budget.
/// @param structure The structure.
[[nodiscard]] std::size_t bytesOf(const Structure& structure) noexcept {
	std::size_t bytes = sizeof(Structure) + structure.root.size() * sizeof(std::uint64_t);
	for(const auto& [id, layer] : structure.layers)
		bytes += sizeof(Layer) + layer.name.size() + layer.children.size() * sizeof(std::uint64_t) +
		         layer.adjustment.curve.size() * sizeof(float) * 2 + layer.adjustment.gradient.stops.size() * 20;
	return bytes;
}

/// @brief Every image a layer and its descendants use.
/// @param structure The structure.
/// @param id The layer.
/// @param images Where they go.
void collectImages(const Structure& structure, std::uint64_t id, std::vector<std::uint64_t>& images) {
	const Layer& layer = structure.layers.at(id);
	if(layer.pixels != 0) images.push_back(layer.pixels);
	if(layer.mask != 0) images.push_back(layer.mask);
	for(const std::uint64_t child : layer.children) collectImages(structure, child, images);
}

/// @brief The deepest nesting below a layer: 0 for a layer, 1 for a group of layers, 2 for a group of groups.
/// @param structure The structure.
/// @param id The layer.
[[nodiscard]] int heightOf(const Structure& structure, std::uint64_t id) {
	const Layer& layer = structure.layers.at(id);
	if(layer.kind != LayerKind::group) return 0;
	int height = 1;
	for(const std::uint64_t child : layer.children) height = std::max(height, 1 + heightOf(structure, child));
	return height;
}

} // namespace

int depthOf(const Structure& structure, std::uint64_t id) noexcept {
	int depth = 0;
	for(auto found = structure.layers.find(id); found != structure.layers.end() && found->second.parent != 0;
	    found = structure.layers.find(found->second.parent))
		++depth;
	return depth;
}

Document::Document(int width, int height, const Limits& limits, PixelFormat format)
    : width_(width), height_(height), format_(format), limits_(limits), history_(limits.undoBytes, limits.undoSteps) {
	if(format == PixelFormat::mask8) throw DocumentError("imaging: a document's layers are rgba8 or rgba16f");
	try {
		selection_ = std::make_shared<Image>(width, height, PixelFormat::mask8, limits);
	} catch(const std::invalid_argument& error) {
		throw DocumentError(error.what());
	}
	Layer layer;
	layer.name = "Layer 1";
	layer.pixels = structure_.nextId++;
	structure_.images.emplace(layer.pixels, std::make_shared<Image>(width, height, format, limits));
	insert(structure_, std::move(layer));
}

const Layer* Document::layer(std::uint64_t id) const noexcept {
	const auto found = structure_.layers.find(id);
	return found != structure_.layers.end() ? &found->second : nullptr;
}

std::vector<std::pair<std::uint64_t, int>> Document::listed() const {
	std::vector<std::pair<std::uint64_t, int>> rows;
	const std::function<void(const std::vector<std::uint64_t>&, int)> visit = [&](const auto& list, int depth) {
		for(auto id = list.rbegin(); id != list.rend(); ++id) {
			rows.emplace_back(*id, depth);
			visit(structure_.layers.at(*id).children, depth + 1);
		}
	};
	visit(structure_.root, 0);
	return rows;
}

Image* Document::image(std::uint64_t id) noexcept {
	if(id == selectionId) return selection_.get();
	const auto found = structure_.images.find(id);
	return found != structure_.images.end() ? found->second.get() : nullptr;
}

const Image* Document::image(std::uint64_t id) const noexcept {
	return const_cast<Document*>(this)->image(id); // NOLINT(cppcoreguidelines-pro-type-const-cast): the same lookup
}

std::optional<std::string> Document::paintable(std::uint64_t target) const {
	if(target == selectionId) return std::nullopt;
	for(const auto& [id, layer] : structure_.layers) {
		if(layer.pixels != target && layer.mask != target) continue;
		if(layer.locks.all) return "The layer \"" + layer.name + "\" is locked";
		if(layer.locks.pixels && layer.pixels == target) return "The layer's pixels are locked";
		if(!layer.visible) return "The layer is hidden";
		return std::nullopt;
	}
	return "Choose a pixel layer or a mask";
}

void Document::begin(std::string name) {
	if(editing_) commit();
	editing_.emplace(Editing{std::move(name), {}});
}

Tile& Document::editTile(std::uint64_t target, int column, int row) {
	if(!editing_) throw std::logic_error("imaging: a tile edited outside a step");
	Image* edited = image(target);
	if(edited == nullptr) throw std::out_of_range("imaging: no image " + std::to_string(target));
	editing_->recorder.touch(target, *edited, column, row);
	++revision_;
	return edited->writableTile(column, row);
}

void Document::replaceTile(std::uint64_t target, int column, int row, std::shared_ptr<const Tile> tile) {
	if(!editing_) throw std::logic_error("imaging: a tile replaced outside a step");
	Image* edited = image(target);
	if(edited == nullptr) throw std::out_of_range("imaging: no image " + std::to_string(target));
	editing_->recorder.touch(target, *edited, column, row);
	++revision_;
	edited->setTile(column, row, std::move(tile));
}

void Document::commit(std::optional<bool> selection) {
	if(!editing_) return;
	HistoryStep step;
	step.name = std::move(editing_->name);
	step.tiles = editing_->recorder.finish([this](std::uint64_t id) { return image(id); });
	editing_.reset();
	if(selection && *selection != structure_.selection) {
		step.structureBefore = std::make_shared<const Structure>(structure_);
		structure_.selection = *selection;
		step.structureAfter = std::make_shared<const Structure>(structure_);
		step.structureBytes = 2 * bytesOf(structure_);
	}
	if(step.tiles.empty() && !step.structureAfter) return;
	++revision_;
	history_.push(std::move(step));
}

void Document::cancel() {
	if(!editing_) return;
	HistoryStep step;
	step.tiles = editing_->recorder.finish([this](std::uint64_t id) { return image(id); });
	editing_.reset();
	applyTiles(step, false, [this](std::uint64_t id) { return image(id); });
	++revision_;
}

void Document::changeStructure(const std::string& name, const std::function<void(Structure&)>& change, bool coalesce) {
	if(editing_) commit();
	auto before = std::make_shared<const Structure>(structure_);
	Structure changed = structure_;
	change(changed); // may throw: nothing changed yet
	if(changed.layers.size() > limits_.layers)
		throw DocumentError("A document has at most " + std::to_string(limits_.layers) + " layers");
	auto after = std::make_shared<const Structure>(std::move(changed));
	structure_ = *after;
	++revision_;
	if(coalesce && history_.amendLast(name, after)) return;
	HistoryStep step;
	step.name = name;
	step.structureBytes = 2 * bytesOf(structure_);
	step.structureAfter = std::move(after);
	step.structureBefore = std::move(before);
	history_.push(std::move(step));
}

Layer& Document::find(Structure& structure, std::uint64_t id) {
	const auto found = structure.layers.find(id);
	if(found == structure.layers.end()) throw DocumentError("No layer " + std::to_string(id));
	return found->second;
}

std::uint64_t Document::insert(Structure& structure, Layer layer) const {
	layer.id = structure.nextId++;
	if(layer.name.size() > maxNameBytes) layer.name.resize(maxNameBytes);
	std::uint64_t parent = 0;
	std::size_t index = structure.root.size();
	if(const auto active = structure.layers.find(structure.active); active != structure.layers.end()) {
		parent = active->second.parent;
		const auto& list = listOf(structure, parent);
		index = static_cast<std::size_t>(std::find(list.begin(), list.end(), active->second.id) - list.begin()) + 1;
	}
	// Too deep (a group holds groups to limits_.groupDepth levels): above the group instead
	const int height = layer.kind == LayerKind::group ? 1 : 0;
	while(parent != 0 && depthOf(structure, parent) + 1 + height > limits_.groupDepth) {
		const std::uint64_t group = parent;
		parent = structure.layers.at(group).parent;
		const auto& list = listOf(structure, parent);
		index = static_cast<std::size_t>(std::find(list.begin(), list.end(), group) - list.begin()) + 1;
	}
	layer.parent = parent;
	const std::uint64_t id = layer.id;
	auto& list = listOf(structure, parent);
	list.insert(list.begin() + static_cast<std::ptrdiff_t>(std::min(index, list.size())), id);
	structure.layers.emplace(id, std::move(layer));
	structure.active = id;
	return id;
}

std::uint64_t Document::addPixelLayer(const std::string& name) {
	return addImageLayer(name, std::make_shared<Image>(width_, height_, format_, limits_));
}

std::uint64_t Document::addImageLayer(const std::string& name, std::shared_ptr<Image> pixels) {
	if(!pixels || pixels->width() != width_ || pixels->height() != height_ || pixels->format() != format_)
		throw DocumentError("A layer's image is the document's size and format");
	std::uint64_t id = 0;
	changeStructure("New Layer", [&](Structure& structure) {
		Layer layer;
		layer.name = name;
		layer.pixels = structure.nextId++;
		structure.images.emplace(layer.pixels, pixels);
		id = insert(structure, std::move(layer));
	});
	return id;
}

std::uint64_t Document::addGroup(const std::string& name) {
	std::uint64_t id = 0;
	changeStructure("New Group", [&](Structure& structure) {
		Layer layer;
		layer.kind = LayerKind::group;
		layer.name = name;
		id = insert(structure, std::move(layer));
	});
	return id;
}

std::uint64_t Document::addAdjustment(const std::string& name, const Adjustment& adjustment) {
	std::uint64_t id = 0;
	changeStructure("New Adjustment Layer", [&](Structure& structure) {
		Layer layer;
		layer.kind = LayerKind::adjustment;
		layer.name = name;
		layer.adjustment = adjustment;
		layer.adjustment.normalize();
		id = insert(structure, std::move(layer));
	});
	return id;
}

void Document::deleteLayer(std::uint64_t id) {
	changeStructure("Delete Layer", [&](Structure& structure) {
		const Layer& layer = find(structure, id);
		if(layer.locks.all) throw DocumentError("The layer \"" + layer.name + "\" is locked");
		std::vector<std::uint64_t> images;
		collectImages(structure, id, images);
		for(const std::uint64_t image : images) structure.images.erase(image);
		const std::function<void(std::uint64_t)> erase = [&](std::uint64_t at) {
			for(const std::uint64_t child : structure.layers.at(at).children) erase(child);
			structure.layers.erase(at);
		};
		auto& list = listOf(structure, layer.parent);
		const auto place = std::find(list.begin(), list.end(), id);
		const std::uint64_t parent = layer.parent;
		const std::uint64_t below =
		    place != list.begin() ? *(place - 1) : (place + 1 != list.end() ? *(place + 1) : parent);
		list.erase(place);
		erase(id);
		if(structure.active == id || !structure.layers.contains(structure.active)) structure.active = below;
	});
}

std::uint64_t Document::duplicateLayer(std::uint64_t id) {
	std::uint64_t copy = 0;
	changeStructure("Duplicate Layer", [&](Structure& structure) {
		Layer layer = find(structure, id);
		if(layer.kind == LayerKind::group) throw DocumentError("Duplicate the group's layers one by one");
		const auto share = [&](std::uint64_t from) -> std::uint64_t {
			if(from == 0) return 0;
			const Image& original = *structure.images.at(from);
			auto image = std::make_shared<Image>(original.width(), original.height(), original.format(), limits_,
			                                     original.source());
			for(const TileKey& key : original.writtenTiles()) image->setTile(key.column, key.row, original.tile(key));
			const std::uint64_t to = structure.nextId++;
			structure.images.emplace(to, std::move(image));
			return to;
		};
		layer.pixels = share(layer.pixels);
		layer.mask = share(layer.mask);
		layer.name = layer.name.substr(0, maxNameBytes - 5) + " copy";
		structure.active = id;
		copy = insert(structure, std::move(layer));
	});
	return copy;
}

void Document::moveLayer(std::uint64_t id, std::uint64_t parent, std::size_t index) {
	changeStructure("Move Layer", [&](Structure& structure) {
		Layer& layer = find(structure, id);
		if(parent != 0 && find(structure, parent).kind != LayerKind::group) throw DocumentError("Move into a group");
		for(std::uint64_t at = parent; at != 0; at = structure.layers.at(at).parent)
			if(at == id) throw DocumentError("A group can't go into itself");
		if(parent != 0 && depthOf(structure, parent) + 1 + heightOf(structure, id) > limits_.groupDepth)
			throw DocumentError("Groups nest at most " + std::to_string(limits_.groupDepth) + " deep");
		auto& from = listOf(structure, layer.parent);
		const auto place = std::find(from.begin(), from.end(), id);
		if(layer.parent == parent && static_cast<std::size_t>(place - from.begin()) < index) --index;
		from.erase(place);
		auto& to = listOf(structure, parent);
		to.insert(to.begin() + static_cast<std::ptrdiff_t>(std::min(index, to.size())), id);
		layer.parent = parent;
	});
}

void Document::changeLayer(std::uint64_t id, const std::string& name, const std::function<void(Layer&)>& change,
                           bool coalesce) {
	changeStructure(
	    name,
	    [&](Structure& structure) {
		    Layer& layer = find(structure, id);
		    const Layer before = layer;
		    change(layer);
		    // A locked layer still shows or hides and can be unlocked, nothing else
		    if(before.locks.all && layer.locks.all) {
			    const bool visible = layer.visible;
			    layer = before;
			    layer.visible = visible;
		    }
		    layer.id = before.id, layer.kind = before.kind, layer.pixels = before.pixels, layer.mask = before.mask;
		    layer.parent = before.parent, layer.children = before.children;
		    if(layer.name.size() > maxNameBytes) layer.name.resize(maxNameBytes);
		    layer.opacity = clamped(layer.opacity);
		    layer.adjustment.normalize();
	    },
	    coalesce);
}

void Document::addMask(std::uint64_t id) {
	changeStructure("Add Layer Mask", [&](Structure& structure) {
		Layer& layer = find(structure, id);
		if(layer.mask != 0) throw DocumentError("The layer has a mask");
		if(layer.locks.all) throw DocumentError("The layer \"" + layer.name + "\" is locked");
		auto mask = std::make_shared<Image>(width_, height_, PixelFormat::mask8, limits_);
		if(structure.selection) {
			for(const TileKey& key : selection_->writtenTiles())
				mask->setTile(key.column, key.row, selection_->tile(key));
		} else {
			// Revealing everything: one white tile shared by every place, copied where painted
			const std::shared_ptr<const Tile> white = Tile::filled(PixelFormat::mask8, {1.0f, 1.0f, 1.0f, 1.0f});
			for(int row = 0; row < mask->rows(); ++row)
				for(int column = 0; column < mask->columns(); ++column) mask->setTile(column, row, white);
		}
		layer.mask = structure.nextId++;
		layer.maskEnabled = true;
		structure.images.emplace(layer.mask, std::move(mask));
	});
}

void Document::deleteMask(std::uint64_t id) {
	changeStructure("Delete Layer Mask", [&](Structure& structure) {
		Layer& layer = find(structure, id);
		if(layer.mask == 0) throw DocumentError("The layer has no mask");
		structure.images.erase(layer.mask);
		layer.mask = 0;
	});
}

void Document::setActive(std::uint64_t id) {
	if(!structure_.layers.contains(id)) throw DocumentError("No layer " + std::to_string(id));
	structure_.active = id;
	++revision_;
}

void Document::apply(const HistoryStep& step, bool after) {
	const auto& structure = after ? step.structureAfter : step.structureBefore;
	if(!after) applyTiles(step, false, [this](std::uint64_t id) { return image(id); });
	if(structure) {
		const std::uint64_t active = structure_.active;
		structure_ = *static_cast<const Structure*>(structure.get());
		if(structure_.layers.contains(active)) structure_.active = active; // undo doesn't move the user
	}
	if(after) applyTiles(step, true, [this](std::uint64_t id) { return image(id); });
	++revision_;
}

bool Document::undo() {
	if(editing_) commit();
	const HistoryStep* step = history_.undo();
	if(step == nullptr) return false;
	apply(*step, false);
	return true;
}

bool Document::redo() {
	if(editing_) commit();
	const HistoryStep* step = history_.redo();
	if(step == nullptr) return false;
	apply(*step, true);
	return true;
}

void Document::goTo(std::size_t position) {
	while(history_.position() > position && undo()) {
	}
	while(history_.position() < position && redo()) {
	}
}

} // namespace affresco::imaging
