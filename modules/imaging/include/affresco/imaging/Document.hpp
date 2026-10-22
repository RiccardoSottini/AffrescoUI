#pragma once

/// @file
/// @brief A layered image: pixel layers, groups and adjustment layers with their masks, the selection, and the
/// history that undoes both the pixels (by tile) and the layers' structure (by snapshot).

#include "affresco/imaging/Adjustment.hpp"
#include "affresco/imaging/Blend.hpp"
#include "affresco/imaging/History.hpp"
#include "affresco/imaging/Image.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace affresco::imaging {

/// @brief What a layer is.
enum class LayerKind : std::uint8_t {
	pixel,      ///< An image of its own.
	group,      ///< Layers composited together, then onto what is below.
	adjustment, ///< Changes the colours of what is below.
};

/// @brief What a layer refuses.
struct LayerLocks {
	bool pixels = false;   ///< Painting and filters.
	bool position = false; ///< Moving.
	bool all = false;      ///< Everything, including its settings and deleting it.

	/// @brief Whether two sets of locks are the same.
	[[nodiscard]] friend bool operator==(const LayerLocks&, const LayerLocks&) = default;
};

/// @brief A layer: its kind, its settings, and the ids of its images.
struct Layer {
	std::uint64_t id = 0;                ///< Its id, never reused in a document.
	LayerKind kind = LayerKind::pixel;   ///< What it is.
	std::string name;                    ///< Its name, at most 255 bytes.
	bool visible = true;                 ///< Whether it shows.
	float opacity = 1.0f;                ///< 0 to 1.
	BlendMode blend = BlendMode::normal; ///< How it combines with what is below.
	LayerLocks locks;                    ///< What it refuses.
	bool clipped = false;                ///< A clipping mask: shows only where the layer below it shows.
	std::uint64_t pixels = 0;            ///< pixel: its image's id.
	std::uint64_t mask = 0;              ///< Its mask's image id (mask8), or 0 for none.
	bool maskEnabled = true;             ///< Whether its mask applies.
	Adjustment adjustment;               ///< adjustment: what it does.
	std::uint64_t parent = 0;            ///< Its group's id, or 0 at the top.
	std::vector<std::uint64_t> children; ///< group: its layers, the bottom one first.

	/// @brief Whether two layers are the same.
	[[nodiscard]] friend bool operator==(const Layer&, const Layer&) = default;
};

/// @brief Everything about a document but its pixels: what a structure step of the history snapshots.
struct Structure {
	std::map<std::uint64_t, Layer> layers;                          ///< The layers, by id.
	std::vector<std::uint64_t> root;                                ///< The top level's layers, the bottom first.
	std::uint64_t active = 0;                                       ///< The layer edited, or 0.
	bool selection = false;                                         ///< Whether a selection is active.
	std::unordered_map<std::uint64_t, std::shared_ptr<Image>> images; ///< Layers' images and masks, by id.
	std::uint64_t nextId = 2;                                       ///< The next id given (1 is the selection's).
};

/// @brief A layered image and its history.
///
/// Pixels change between begin() and commit(), through editTile(), which records each tile's before for undo. Layers
/// change through the structure functions, each one undoable step. The selection is a mask8 image, id 1, whose
/// absent tiles are unselected.
class Document {
public:
	static constexpr std::uint64_t selectionId = 1; ///< The selection's image id.

	/// @brief An empty document of one transparent layer, "Layer 1".
	/// @param width Pixels across.
	/// @param height Pixels down.
	/// @param limits The limits; an image beyond them is refused.
	/// @param format The layers' format (rgba8 or rgba16f).
	/// @throws DocumentError beyond the limits.
	Document(int width, int height, const Limits& limits = {}, PixelFormat format = PixelFormat::rgba8);

	/// @brief Pixels across.
	[[nodiscard]] int width() const noexcept { return width_; }
	/// @brief Pixels down.
	[[nodiscard]] int height() const noexcept { return height_; }
	/// @brief The layers' format.
	[[nodiscard]] PixelFormat format() const noexcept { return format_; }
	/// @brief The limits.
	[[nodiscard]] const Limits& limits() const noexcept { return limits_; }
	/// @brief Everything but the pixels.
	[[nodiscard]] const Structure& structure() const noexcept { return structure_; }
	/// @brief A layer, or null.
	/// @param id Its id.
	[[nodiscard]] const Layer* layer(std::uint64_t id) const noexcept;
	/// @brief The active layer, or null.
	[[nodiscard]] const Layer* activeLayer() const noexcept { return layer(structure_.active); }
	/// @brief The layers top first, as the Layers panel lists them, with their depth (0 at the top level).
	[[nodiscard]] std::vector<std::pair<std::uint64_t, int>> listed() const;
	/// @brief An image (a layer's, a mask or the selection), or null.
	/// @param id Its id.
	[[nodiscard]] Image* image(std::uint64_t id) noexcept;
	/// @copydoc image(std::uint64_t)
	[[nodiscard]] const Image* image(std::uint64_t id) const noexcept;
	/// @brief The selection.
	[[nodiscard]] const Image& selection() const noexcept { return *selection_; }
	/// @brief Whether there is a selection (otherwise edits apply everywhere).
	[[nodiscard]] bool hasSelection() const noexcept { return structure_.selection; }
	/// @brief Bumped by every change, so views know to redraw.
	[[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

	/// @brief Why the active layer can't be painted, or nothing when it can.
	/// @param target The image to paint (a layer's pixels or its mask, or the selection).
	[[nodiscard]] std::optional<std::string> paintable(std::uint64_t target) const;
	/// @brief Starts a pixel step.
	/// @param name Its name in the History panel.
	void begin(std::string name);
	/// @brief Whether a pixel step is open.
	[[nodiscard]] bool editing() const noexcept { return editing_.has_value(); }
	/// @brief A tile to change within the open step, its before recorded; an absent mask tile starts filled white.
	/// @param target The image.
	/// @param column The column.
	/// @param row The row.
	/// @throws std::logic_error outside a step, std::out_of_range outside the image.
	[[nodiscard]] Tile& editTile(std::uint64_t target, int column, int row);
	/// @brief Replaces a tile within the open step (null for transparent), its before recorded.
	/// @param target The image.
	/// @param column The column.
	/// @param row The row.
	/// @param tile The tile.
	void replaceTile(std::uint64_t target, int column, int row, std::shared_ptr<const Tile> tile);
	/// @brief Ends the open step, kept in the history when it changed anything.
	/// @param selection Whether there is a selection afterwards (unchanged when absent).
	void commit(std::optional<bool> selection = std::nullopt);
	/// @brief Ends the open step, putting back every tile it changed.
	void cancel();

	/// @brief Changes the structure as one undoable step.
	/// @param name Its name in the History panel.
	/// @param change The change, which may throw DocumentError to refuse (nothing is then changed).
	/// @param coalesce Whether a step of the same name just before (a slider being dragged) takes this one in.
	void changeStructure(const std::string& name, const std::function<void(Structure&)>& change, bool coalesce = false);
	/// @brief Adds a transparent pixel layer above the active one.
	/// @param name Its name.
	/// @return Its id.
	std::uint64_t addPixelLayer(const std::string& name);
	/// @brief Adds an empty group above the active layer.
	/// @param name Its name.
	/// @return Its id.
	std::uint64_t addGroup(const std::string& name);
	/// @brief Adds an adjustment layer above the active one.
	/// @param name Its name.
	/// @param adjustment What it does.
	/// @return Its id.
	std::uint64_t addAdjustment(const std::string& name, const Adjustment& adjustment);
	/// @brief Adds a layer whose pixels are an image's (tiles shared until changed), above the active one.
	/// @param name Its name.
	/// @param pixels The image, of the document's size and format.
	/// @return Its id.
	std::uint64_t addImageLayer(const std::string& name, std::shared_ptr<Image> pixels);
	/// @brief Deletes a layer (a group with its layers).
	/// @param id The layer.
	void deleteLayer(std::uint64_t id);
	/// @brief Duplicates a layer (not a group) above it, its tiles shared until changed.
	/// @param id The layer.
	/// @return The copy's id.
	std::uint64_t duplicateLayer(std::uint64_t id);
	/// @brief Moves a layer into a group (0 for the top level) at a place in its list, the bottom 0.
	/// @param id The layer.
	/// @param parent The group.
	/// @param index The place, clamped to the list.
	void moveLayer(std::uint64_t id, std::uint64_t parent, std::size_t index);
	/// @brief Changes a layer's settings (name, visibility, opacity, blend, locks, clipping, adjustment) as one step.
	/// @param id The layer.
	/// @param name The step's name.
	/// @param change The change.
	/// @param coalesce See changeStructure().
	void changeLayer(std::uint64_t id, const std::string& name, const std::function<void(Layer&)>& change,
	                 bool coalesce = false);
	/// @brief Gives a layer a mask: from the selection when there is one, otherwise revealing everything.
	/// @param id The layer.
	void addMask(std::uint64_t id);
	/// @brief Takes a layer's mask away.
	/// @param id The layer.
	void deleteMask(std::uint64_t id);
	/// @brief Makes a layer the active one (not a step).
	/// @param id The layer.
	void setActive(std::uint64_t id);

	/// @brief The history.
	[[nodiscard]] const History& history() const noexcept { return history_; }
	/// @brief Undoes a step.
	/// @return Whether there was one.
	bool undo();
	/// @brief Redoes a step.
	/// @return Whether there was one.
	bool redo();
	/// @brief Undoes or redoes until a place in the history (0 before the first step).
	/// @param position The place.
	void goTo(std::size_t position);

private:
	/// @brief The open pixel step.
	struct Editing {
		std::string name;      ///< Its name.
		TileRecorder recorder; ///< Its tiles' befores.
	};

	/// @brief The layer, or a DocumentError.
	/// @param structure The structure.
	/// @param id The id.
	[[nodiscard]] static Layer& find(Structure& structure, std::uint64_t id);
	/// @brief Puts a new layer into a structure above its active layer and makes it active.
	/// @param structure The structure.
	/// @param layer The layer, its id given here.
	/// @return Its id.
	std::uint64_t insert(Structure& structure, Layer layer) const;
	/// @brief Puts back a step's structure and tiles.
	/// @param step The step.
	/// @param after Whether to the step's after (redo) or before (undo).
	void apply(const HistoryStep& step, bool after);

	int width_;                         ///< See width().
	int height_;                        ///< See height().
	PixelFormat format_;                ///< See format().
	Limits limits_;                     ///< See limits().
	Structure structure_;               ///< See structure().
	std::shared_ptr<Image> selection_;  ///< See selection().
	History history_;                   ///< See history().
	std::optional<Editing> editing_;    ///< The open pixel step.
	std::uint64_t revision_ = 0;        ///< See revision().
};

/// @brief The depth of a layer: 0 at the top level, 1 in a group, 2 in a group in a group.
/// @param structure The structure.
/// @param id The layer.
[[nodiscard]] int depthOf(const Structure& structure, std::uint64_t id) noexcept;

} // namespace affresco::imaging
