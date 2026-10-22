#pragma once

/// @file
/// @brief GraphView: a node graph (nodes with ports, links between them) that the pointer or the keyboard edits.

#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

/// @brief A port of a node: where links start (outputs) or end (inputs).
struct GraphPort {
	std::string id;           ///< Its name, unique among the node's inputs or outputs.
	std::string label;        ///< What is written beside it; empty shows the id.
	std::string kind = "any"; ///< What flows through it: links join ports of the same kind, or either "any".
	std::size_t links = 0;    ///< How many links it takes; 0 for any number.

	/// @brief Equal when every member is equal.
	bool operator==(const GraphPort&) const = default;
};

/// @brief A node: a titled box with inputs on its left edge, outputs on its right edge and an optional body below.
struct GraphNode {
	std::string id;                 ///< Its name, unique in the graph.
	std::string title;              ///< The header's text; empty shows the id.
	Vec2 position;                  ///< Its top-left corner, in graph units.
	float width = 160.0f;           ///< Its width, in graph units.
	float bodyHeight = 0.0f;        ///< The height of the body (the panel the graph places for it), in graph units.
	std::vector<GraphPort> inputs;  ///< Its inputs, top to bottom.
	std::vector<GraphPort> outputs; ///< Its outputs, top to bottom.

	/// @brief Equal when every member is equal.
	bool operator==(const GraphNode&) const = default;
};

/// @brief A link from a node's output to a node's input.
struct GraphLink {
	std::string from;   ///< The node it starts at.
	std::string output; ///< That node's output.
	std::string to;     ///< The node it ends at.
	std::string input;  ///< That node's input.

	/// @brief Equal when every member is equal.
	bool operator==(const GraphLink&) const = default;
};

/// @brief The nodes and links of a graph.
struct GraphModel {
	std::vector<GraphNode> nodes; ///< The nodes, drawn in order (the last on top).
	std::vector<GraphLink> links; ///< The links.

	/// @brief The node with an id.
	/// @param id Its id.
	/// @return Its index, or nodes.size() if there is none.
	[[nodiscard]] std::size_t find(std::string_view id) const noexcept;

	/// @brief Equal when every member is equal.
	bool operator==(const GraphModel&) const = default;
};

/// @brief Whether a link may be made, and if not, why.
enum class LinkVerdict {
	Allowed,      ///< It may be made.
	UnknownPort,  ///< A node or port it names doesn't exist.
	KindMismatch, ///< The ports' kinds differ and neither is "any".
	Duplicate,    ///< The same link is already there.
	Full,         ///< One of the ports already has all the links it takes.
	Cycle,        ///< It would close a cycle (a node linked to itself included), and cycles are refused.
};

/// @brief Tells whether two port kinds can be linked: equal, or either is "any".
/// @param output The output's kind.
/// @param input The input's kind.
/// @return true if they can.
[[nodiscard]] bool kindsMatch(std::string_view output, std::string_view input) noexcept;

/// @brief Checks a link against a graph's rules: known ports, matching kinds, no duplicates, room on both ports
/// and, unless cycles are allowed, no cycle.
/// @param model The graph.
/// @param link The link to make.
/// @param cyclesAllowed Whether a link may close a cycle.
/// @param replacing A link of the model the new one replaces (reconnecting), left out of the checks; or nullptr.
/// @return The verdict.
[[nodiscard]] LinkVerdict checkLink(const GraphModel& model, const GraphLink& link, bool cyclesAllowed,
                                    const GraphLink* replacing = nullptr);

/// @brief Where a graph is seen from: a pan and a zoom. Kept apart from the view so it can move onto a shared 2D
/// camera later.
struct GraphCamera {
	static constexpr float minimumZoom = 0.1f; ///< The smallest zoom.
	static constexpr float maximumZoom = 4.0f; ///< The largest zoom.

	Vec2 pan;          ///< Where the graph's origin is, relative to the view's top-left corner, in window units.
	float zoom = 1.0f; ///< Window units per graph unit.

	/// @brief Turns a graph point into a view point.
	/// @param point In graph units.
	/// @return Relative to the view's top-left corner, in window units.
	[[nodiscard]] Vec2 toView(Vec2 point) const noexcept;
	/// @brief Turns a view point into a graph point.
	/// @param point Relative to the view's top-left corner, in window units.
	/// @return In graph units.
	[[nodiscard]] Vec2 toGraph(Vec2 point) const noexcept;
	/// @brief Zooms, keeping the graph point under a view point where it is.
	/// @param point Relative to the view's top-left corner.
	/// @param zoomTo The new zoom; clamped to [minimumZoom, maximumZoom] (a NaN is ignored).
	void zoomAt(Vec2 point, float zoomTo) noexcept;

	/// @brief Equal when every member is equal.
	bool operator==(const GraphCamera&) const = default;
};

/// @brief Something the user did to a graph. The view doesn't change its model: the owner applies the edit.
struct GraphEvent {
	/// @brief What happened.
	enum class Kind {
		Connect,    ///< A link was made (link); after a Disconnect when an end was moved.
		Disconnect, ///< A link was removed or its end picked up (link, linkIndex).
		LinkDrop,   ///< A link being made was dropped on empty space (node, port, output, at).
		Move,       ///< Nodes were moved (nodes, positions).
		Select,     ///< The selection changed (nodes).
		Delete,     ///< The selected nodes were deleted (nodes).
	};

	Kind kind = Kind::Select;       ///< See Kind.
	GraphLink link;                 ///< Connect and Disconnect: the link.
	std::size_t linkIndex = 0;      ///< Disconnect: its index in the model's links.
	std::vector<std::size_t> nodes; ///< Move, Select, Delete: indices in the model's nodes.
	std::vector<Vec2> positions;    ///< Move: the new position of each, in graph units.
	Vec2 delta;                     ///< Move: how far each moved, in graph units.
	std::string node;               ///< LinkDrop: the node the link started at.
	std::string port;               ///< LinkDrop: its port.
	bool output = true;             ///< LinkDrop: whether that port is an output.
	Vec2 at;                        ///< LinkDrop: where it was dropped, in graph units.
};

/// @brief Draws a node graph and turns the pointer and the keyboard into GraphEvents.
///
/// Nodes are dragged by their header (the selection moves together), with Shift adding to the selection and a drag
/// on empty space drawing a marquee. A drag from a port draws a link; the ports it may end at are highlighted, and
/// dropping it on empty space fires LinkDrop. Pressing a linked input picks up its link to move or detach it. The
/// right or middle button and the wheel pan, + and - zoom; the minimap (bottom right) moves
/// the view where it is clicked. Links are cubic curves coloured by kind. Each child of the view is the body of the
/// node at its index, placed under its ports, and hidden while the node is off screen.
///
/// With the keyboard: Page Up and Page Down select the previous or the next node, the arrows move the selection by
/// 8 units (32 with Shift), [ and ] pick a port of the selected node, Enter starts a link there and Enter on another
/// port ends it, Escape cancels, Delete or Backspace deletes, + and - zoom and Home puts the view back.
class GraphView : public Panel {
public:
	static constexpr std::size_t maximumNodes = 10000; ///< The most nodes setModel() keeps.
	static constexpr std::size_t maximumPorts = 64;    ///< The most inputs, and outputs, a node keeps.
	static constexpr std::size_t maximumLinks = 50000; ///< The most links setModel() keeps.
	static constexpr std::size_t maximumKinds = 64;    ///< The most kinds setKindColor() keeps colours for.

	/// @brief An empty graph.
	GraphView();

	/// @brief Sets the nodes and links, kept within the maximums, without firing events.
	/// @param model The graph.
	void setModel(GraphModel model);
	/// @brief The nodes and links.
	[[nodiscard]] const GraphModel& model() const noexcept { return model_; }

	/// @brief Sets whether a link may close a cycle.
	/// @param allowed true to allow it (the default refuses).
	void setCyclesAllowed(bool allowed);
	/// @brief Whether a link may close a cycle.
	[[nodiscard]] bool cyclesAllowed() const noexcept { return cyclesAllowed_; }

	/// @brief Sets the selected nodes, without firing events.
	/// @param ids Their ids; unknown ones are ignored.
	void setSelection(std::vector<std::string> ids);
	/// @brief The ids of the selected nodes.
	[[nodiscard]] const std::vector<std::string>& selection() const noexcept { return selection_; }

	/// @brief Sets the pan and zoom.
	/// @param camera The camera; the zoom is clamped.
	void setCamera(GraphCamera camera);
	/// @brief The pan and zoom.
	[[nodiscard]] const GraphCamera& camera() const noexcept { return camera_; }

	/// @brief Sets the camera a document gives: only when it differs from the one given last, so the user's zoom and
	/// pan stay until the document gives another.
	/// @param camera The camera.
	void setGivenCamera(GraphCamera camera);
	/// @brief Sets the selection a document gives: only when it differs from the one given last, so the user's
	/// selection stays until the document gives another.
	/// @param ids The selected nodes' ids.
	void setGivenSelection(std::vector<std::string> ids);

	/// @brief Sets the colour of the links (and ports) of a kind.
	/// @param kind The kind.
	/// @param color Its colour.
	void setKindColor(std::string kind, Color color);
	/// @brief Sets the look of the nodes.
	/// @param node The nodes' fill.
	/// @param header Their header's fill.
	/// @param text Their text.
	/// @param accent The selection, the highlighted ports and the minimap's view.
	void setColors(Color node, Color header, Color text, Color accent);
	/// @brief Sets the font of the titles and port labels.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief Shows or hides the minimap.
	/// @param shown true to show it (the default).
	void setMinimap(bool shown);

	/// @brief The box of a node, after layout.
	/// @param index Its index.
	/// @return In graph units; empty if there is no such node.
	[[nodiscard]] Rect nodeBox(std::size_t index) const noexcept;
	/// @brief Where a port's link ends, after layout.
	/// @param index The node's index.
	/// @param port The port's index.
	/// @param output Whether it is an output.
	/// @return In graph units.
	[[nodiscard]] Vec2 portPoint(std::size_t index, std::size_t port, bool output) const noexcept;
	/// @brief How many nodes the last layout pass found on screen.
	[[nodiscard]] std::size_t visibleNodes() const noexcept { return visibleCount_; }

	Signal<const GraphEvent&> onEdit; ///< The user edited the graph; the owner changes the model.

protected:
	void arrange() override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	bool scrollContent(Vec2 delta) override;
	void keyEvent(const KeyEvent& event) override;
	/// @brief Repaints: the keyboard's port shows only while the graph has the focus.
	/// @param focused Whether it gained the focus.
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;
	void paintOver(Painter& painter) const override;

private:
	/// @brief A port the pointer or the keyboard picked.
	struct PortRef {
		std::size_t node = 0; ///< The node's index.
		std::size_t port = 0; ///< The port's index.
		bool output = false;  ///< Whether it is an output.
		bool valid = false;   ///< Whether it names a port.
	};
	/// @brief What a left drag is doing.
	enum class Drag { None, Nodes, Marquee, Link, Pan, Minimap };

	/// @brief The font in use.
	[[nodiscard]] const Font* font() const;
	/// @brief A node's position, with the drag in progress.
	/// @param index Its index.
	[[nodiscard]] Vec2 shownPosition(std::size_t index) const noexcept;
	/// @brief Whether a node is selected.
	/// @param index Its index.
	[[nodiscard]] bool isSelected(std::size_t index) const;
	/// @brief The colour of a kind.
	/// @param kind The kind.
	[[nodiscard]] Color kindColor(std::string_view kind) const;
	/// @brief The port under a view point.
	/// @param point Relative to the view's top-left corner.
	[[nodiscard]] PortRef portAt(Vec2 point) const;
	/// @brief The topmost node under a view point.
	/// @param point Relative to the view's top-left corner.
	/// @return Its index, or the number of nodes for none.
	[[nodiscard]] std::size_t nodeAt(Vec2 point) const;
	/// @brief The link a press on a port picks up: the last one ending at an input.
	/// @param port The port.
	/// @return Its index, or the number of links for none.
	[[nodiscard]] std::size_t linkAt(const PortRef& port) const;
	/// @brief The link from the port being dragged to another port, oriented output to input.
	/// @param target The other port.
	[[nodiscard]] GraphLink candidate(const PortRef& target) const;
	/// @brief Whether the link being drawn may end at a port.
	/// @param target The port.
	[[nodiscard]] bool accepts(const PortRef& target) const;
	/// @brief The node with an id, through byId_.
	/// @param id Its id.
	/// @return Its index, or the number of nodes if there is none.
	[[nodiscard]] std::size_t nodeIndex(std::string_view id) const noexcept;
	/// @brief Ends the link being drawn at a port (or on empty space) and fires the events.
	/// @param target The port, or an invalid one for empty space.
	/// @param at Where it was dropped, in graph units.
	void finishLink(const PortRef& target, Vec2 at);
	/// @brief Changes the selection and fires Select.
	/// @param ids The new selection.
	void select(std::vector<std::string> ids);
	/// @brief Fires Move for the selected nodes, moved by an offset.
	/// @param delta In graph units.
	void moveSelection(Vec2 delta);
	/// @brief The minimap's box, relative to the view's top-left corner; empty when hidden.
	[[nodiscard]] Rect minimapBox() const noexcept;
	/// @brief The box holding every node, in graph units.
	[[nodiscard]] Rect graphBounds() const noexcept;
	/// @brief Centres the view on the graph point a minimap point stands for.
	/// @param point Relative to the view's top-left corner.
	void panToMinimap(Vec2 point);
	/// @brief Recomputes selected_ from the selection and the model.
	void refreshSelected();
	/// @brief Tells the accessibility tree what the keyboard is on.
	void describe();
	/// @brief Rebuilds the link meshes, for the links on screen.
	void buildLinks() const;

	GraphModel model_;                                       ///< See model().
	bool cyclesAllowed_ = false;                             ///< See cyclesAllowed().
	std::vector<std::string> selection_;                     ///< See selection().
	GraphCamera camera_;                                     ///< See camera().
	std::vector<std::pair<std::string, Color>> kindColors_;  ///< See setKindColor().
	Color nodeColor_ = Color::hex(0x2b2e36);                 ///< See setColors().
	Color headerColor_ = Color::hex(0x3d424e);               ///< See setColors().
	Color textColor_ = Color::hex(0xeceef2);                 ///< See setColors().
	Color accent_ = colors::accent;                          ///< See setColors().
	std::shared_ptr<const Font> font_;                       ///< See setFont().
	bool minimap_ = true;                                    ///< See setMinimap().
	std::optional<GraphCamera> givenCamera_;                 ///< See setGivenCamera().
	std::optional<std::vector<std::string>> givenSelection_; ///< See setGivenSelection().

	std::vector<float> heights_;    ///< Each node's height, in graph units, from the last layout pass.
	std::vector<bool> visible_;     ///< Whether each node was on screen in the last layout pass.
	std::vector<bool> selected_;    ///< Whether each node is selected, from selection_.
	std::vector<std::size_t> byId_; ///< The nodes' indices sorted by id, for nodeIndex().
	std::size_t visibleCount_ = 0;  ///< See visibleNodes().

	Drag drag_ = Drag::None;                  ///< What the left button is doing.
	Vec2 pressed_;                            ///< Where the drag started, relative to the view.
	Vec2 pointer_;                            ///< Where the pointer is, relative to the view.
	Vec2 dragDelta_;                          ///< How far the dragged nodes moved, in graph units.
	GraphCamera pressCamera_;                 ///< The camera when a pan started.
	PortRef linkStart_;                       ///< The port the link being drawn starts at.
	std::size_t replacing_ = 0;               ///< The link whose end was picked up, or the number of links for none.
	std::vector<std::string> pressSelection_; ///< The selection when a marquee with Shift started.

	std::size_t keyNode_ = 0; ///< The node the keyboard is on.
	std::size_t keyPort_ = 0; ///< The port of that node the keyboard is on: inputs, then outputs.
	bool keyLink_ = false;    ///< Whether Enter started a link.

	/// @brief The meshes of the links of one kind.
	struct LinkMesh {
		Color color;                   ///< The kind's colour.
		std::vector<MeshVertex> quads; ///< The quads, relative to the view.
	};
	/// @brief What cycleNodes_ was found for: the start of the link being drawn and the link it replaces.
	struct CycleKey {
		std::size_t node = 0;      ///< The node it starts at.
		bool output = false;       ///< Whether it starts at an output.
		std::size_t replacing = 0; ///< The link it replaces, or the number of links.

		/// @brief Equal when every member is equal.
		bool operator==(const CycleKey&) const = default;
	};
	mutable std::optional<CycleKey> cycleKey_; ///< What cycleNodes_ holds, or none (the model changed).
	mutable std::vector<bool> cycleNodes_;     ///< The nodes a link from cycleKey_'s start may not end at.

	mutable std::vector<LinkMesh> linkMeshes_; ///< The link meshes, rebuilt when linksDirty_.
	mutable bool linksDirty_ = true;           ///< Whether the links moved since they were built.
};

} // namespace affresco
