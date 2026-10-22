/// @file
/// @brief GraphView's input: pressing, dragging and releasing on ports, nodes, the minimap and empty space; the
/// wheel; and the keys, each worked out as a command before any of it happens.

#include "graph/GraphShared.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace affresco {

using graph::bigKeyStep;
using graph::boxOf;
using graph::findSorted;
using graph::flagged;
using graph::KeyPort;
using graph::keyPortOf;
using graph::keyStep;
using graph::keyZoom;
using graph::onShown;
using graph::overlaps;
using graph::portIndex;
using graph::portOf;
using graph::operator+;
using graph::operator-;

namespace {

/// @brief Where a link being drawn starts when a port is pressed, and the link it replaces.
struct LinkStart {
	KeyPort port;          ///< The port it hangs from.
	std::size_t replacing; ///< The link picked up, or the model's link count for a new one.
};

/// @brief Where pressing a port starts a link: a linked input picks its link up, which then hangs from its output
/// (a link whose output isn't in the graph, which the format allows, is picked up as unlinked).
/// @param model The model.
/// @param byId Its nodes' indices sorted by id.
/// @param pressed The pressed port.
/// @param link The link at it, or the model's link count for none.
/// @return The start.
LinkStart linkStart(const GraphModel& model, const std::vector<std::size_t>& byId, const KeyPort& pressed,
                    std::size_t link) {
	if(link >= model.links.size()) return {pressed, model.links.size()};
	const std::size_t from = findSorted(model, byId, model.links[link].from);
	if(from == model.nodes.size()) return {pressed, model.links.size()};
	const std::size_t output = portIndex(model.nodes[from].outputs, model.links[link].output);
	if(output == model.nodes[from].outputs.size()) return {pressed, model.links.size()};
	return {{from, output, true, true}, link};
}

/// @brief The selection after pressing a node: Shift toggles it, else it becomes the selection unless it is in it.
/// @param selection The selection's ids.
/// @param id The node's id.
/// @param shift Whether Shift is down.
/// @param selected Whether the node is selected.
/// @return The ids.
std::vector<std::string> pressedIds(std::vector<std::string> selection, const std::string& id, bool shift,
                                    bool selected) {
	if(!shift) return selected ? selection : std::vector<std::string>{id};
	const auto found = std::ranges::find(selection, id);
	if(found == selection.end()) selection.push_back(id);
	else selection.erase(found);
	return selection;
}

/// @brief The selection a marquee makes: the one it started with, then each node it touches, once.
/// @param model The model, every node where it stands.
/// @param ids The selection it started with.
/// @param a One corner, in graph units.
/// @param b The other.
/// @return The ids.
std::vector<std::string> marqueeIds(const GraphModel& model, std::vector<std::string> ids, Vec2 a, Vec2 b) {
	const Rect area{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
	for(const GraphNode& node : model.nodes)
		if(overlaps(boxOf(node, node.position), area) && std::ranges::find(ids, node.id) == ids.end())
			ids.push_back(node.id);
	return ids;
}

/// @brief What a key does to the graph.
enum class KeyAction { None, Cycle, Move, Camera, StartLink, FinishLink, CancelLink, Deselect, Fire };

/// @brief A key's effect, worked out before any of it happens.
struct KeyCommand {
	KeyAction action = KeyAction::None; ///< What it does.
	std::size_t keyNode = 0;            ///< The node the keyboard is on after it.
	std::size_t keyPort = 0;            ///< The port's place on that node after it.
	Vec2 delta;                         ///< Move: how far the selection goes, in graph units.
	GraphCamera camera;                 ///< Camera: the new camera.
	std::vector<GraphEvent> edits;      ///< Fire: the events, in order.
};

/// @brief What the keys read of a GraphView.
struct KeyState {
	const GraphModel& model;           ///< The model; it has a node.
	const std::vector<bool>& selected; ///< Which nodes are selected.
	std::size_t keyNode;               ///< The node the keyboard is on, in the model.
	std::size_t keyPort;               ///< The port's place on it.
	bool keyLink;                      ///< Whether the keyboard is drawing a link.
	GraphCamera camera;                ///< The camera.
	Vec2 size;                         ///< The view's size.
};

/// @brief The keys that move the keyboard or the selection: Page Up and Down pick the previous or next node, the
/// arrows move the selection (further with Shift), and the brackets step through the node's ports.
/// @param event The key.
/// @param state The view.
/// @param command Filled in when the key is one of them.
/// @return true if it is.
bool stepKey(const KeyEvent& event, const KeyState& state, KeyCommand& command) {
	const std::size_t count = state.model.nodes.size();
	const GraphNode& node = state.model.nodes[state.keyNode];
	const std::size_t ports = node.inputs.size() + node.outputs.size();
	const float step = event.mods.shift() ? bigKeyStep : keyStep;
	switch(event.key) {
	case Key::PageUp:
	case Key::PageDown:
		command.action = KeyAction::Cycle;
		command.keyNode =
		    event.key == Key::PageDown ? (state.keyNode + 1) % count : (state.keyNode + count - 1) % count;
		command.keyPort = 0;
		return true;
	case Key::Left:
	case Key::Right:
		command.action = KeyAction::Move;
		command.delta = {event.key == Key::Left ? -step : step, 0.0f};
		return true;
	case Key::Up:
	case Key::Down:
		command.action = KeyAction::Move;
		command.delta = {0.0f, event.key == Key::Up ? -step : step};
		return true;
	case Key::LeftBracket:
	case Key::RightBracket:
		if(ports == 0) return false;
		command.keyPort =
		    event.key == Key::RightBracket ? (state.keyPort + 1) % ports : (state.keyPort + ports - 1) % ports;
		return true;
	default:
		return false;
	}
}

/// @brief The disconnections Shift+Delete makes at a port: its links, the last first so the indices stay right,
/// gathered before any is fired, since the owner may change the model in between.
/// @param model The model.
/// @param port The port; valid.
/// @return The events.
std::vector<GraphEvent> disconnects(const GraphModel& model, const KeyPort& port) {
	const GraphNode& node = model.nodes[port.node];
	const std::string& id = portOf(node, port).id;
	std::vector<GraphEvent> events;
	for(std::size_t i = model.links.size(); i-- > 0;) {
		const GraphLink& link = model.links[i];
		const bool here =
		    port.output ? link.from == node.id && link.output == id : link.to == node.id && link.input == id;
		if(!here) continue;
		GraphEvent& disconnect = events.emplace_back();
		disconnect.kind = GraphEvent::Kind::Disconnect;
		disconnect.link = link;
		disconnect.linkIndex = i;
	}
	return events;
}

/// @brief The deletion Delete asks for: the selected nodes, if any.
/// @param state The view.
/// @return The event, or none.
std::vector<GraphEvent> deletion(const KeyState& state) {
	GraphEvent erase;
	erase.kind = GraphEvent::Kind::Delete;
	for(std::size_t i = 0; i < state.model.nodes.size(); ++i)
		if(flagged(state.selected, i)) erase.nodes.push_back(i);
	if(erase.nodes.empty()) return {};
	return {std::move(erase)};
}

/// @brief The keys that edit: Enter starts or ends a link at the keyboard's port, Escape drops it or the selection,
/// Shift+Delete unlinks the port and Delete (or Backspace) deletes the selection.
/// @param event The key.
/// @param state The view.
/// @param port The keyboard's port.
/// @param command Filled in when the key is one of them.
/// @return true if it is.
bool editKey(const KeyEvent& event, const KeyState& state, const KeyPort& port, KeyCommand& command) {
	const bool erase = event.key == Key::Delete || event.key == Key::Backspace;
	if(event.key == Key::Enter && port.valid)
		command.action = state.keyLink ? KeyAction::FinishLink : KeyAction::StartLink;
	else if(event.key == Key::Escape) command.action = state.keyLink ? KeyAction::CancelLink : KeyAction::Deselect;
	else if(erase) command.action = KeyAction::Fire;
	else return false;
	// Shift unlinks the port when there is one, and otherwise deletes like the plain key
	if(erase) command.edits = event.mods.shift() && port.valid ? disconnects(state.model, port) : deletion(state);
	return true;
}

/// @brief The keys that move the camera: + and - zoom at the view's centre, Home goes back to the start.
/// @param event The key.
/// @param state The view.
/// @param command Filled in when the key is one of them.
/// @return true if it is.
bool viewKey(const KeyEvent& event, const KeyState& state, KeyCommand& command) {
	if(event.key == Key::Home) {
		command.action = KeyAction::Camera;
		command.camera = {};
		return true;
	}
	if(event.key != Key::Equal && event.key != Key::Minus) return false;
	command.action = KeyAction::Camera;
	command.camera = state.camera;
	command.camera.zoomAt({state.size.x * 0.5f, state.size.y * 0.5f},
	                      event.key == Key::Equal ? state.camera.zoom * keyZoom : state.camera.zoom / keyZoom);
	return true;
}

/// @brief What a key does.
/// @param event The key; pressed or repeated.
/// @param state The view.
/// @param port The keyboard's port.
/// @return The command; KeyAction::None for a key the graph doesn't use.
KeyCommand keyCommand(const KeyEvent& event, const KeyState& state, const KeyPort& port) {
	// A key none of them takes leaves the command as it starts: nothing, the keyboard where it was
	KeyCommand command{.keyNode = state.keyNode, .keyPort = state.keyPort};
	if(!stepKey(event, state, command) && !editKey(event, state, port, command)) viewKey(event, state, command);
	return command;
}

/// @brief The camera that brings a node on screen, centred, when its corner is off it.
/// @param camera The camera.
/// @param box The node's box, in graph units.
/// @param size The view's size.
/// @return The camera, or none if the corner is on screen.
std::optional<GraphCamera> revealing(GraphCamera camera, const Rect& box, Vec2 size) noexcept {
	const Vec2 at = camera.toView({box.left, box.top});
	if(!(at.x < 0.0f || at.y < 0.0f || at.x > size.x || at.y > size.y)) return std::nullopt;
	camera.pan = {size.x * 0.5f - (box.left + box.width() * 0.5f) * camera.zoom,
	              size.y * 0.5f - (box.top + box.height() * 0.5f) * camera.zoom};
	return camera;
}

} // namespace

void GraphView::mouseButtonEvent(const MouseButtonEvent& event) {
	const Vec2 point{event.position.x - frame().left, event.position.y - frame().top};
	if(event.action == Action::Press && event.button == MouseButton::Left) {
		pressed_ = pointer_ = point;
		dragDelta_ = {};
		if(onShown(minimapBox(), point)) {
			drag_ = Drag::Minimap;
			panToMinimap(point);
		} else if(const PortRef port = portAt(point); port.valid) {
			const LinkStart start = linkStart(model_, byId_, {port.node, port.port, port.output, true}, linkAt(port));
			drag_ = Drag::Link;
			replacing_ = start.replacing;
			linkStart_ = {start.port.node, start.port.port, start.port.output, start.port.valid};
			keyNode_ = port.node;
		} else if(const std::size_t node = nodeAt(point); node < model_.nodes.size()) {
			select(pressedIds(selection_, model_.nodes[node].id, event.mods.shift(), isSelected(node)));
			keyNode_ = node;
			keyPort_ = 0;
			drag_ = isSelected(node) ? Drag::Nodes : Drag::None;
		} else {
			drag_ = Drag::Marquee;
			pressSelection_ = event.mods.shift() ? selection_ : std::vector<std::string>{};
		}
		describe();
		invalidatePaint();
	} else if(event.action == Action::Press) {
		drag_ = Drag::Pan;
		pressed_ = point;
		pressCamera_ = camera_;
	} else if(event.action == Action::Release && drag_ != Drag::None) {
		const Drag drag = std::exchange(drag_, Drag::None);
		if(drag == Drag::Nodes) moveSelection(std::exchange(dragDelta_, {}));
		else if(drag == Drag::Link) finishLink(portAt(point), camera_.toGraph(point));
		else if(drag == Drag::Marquee)
			select(marqueeIds(model_, pressSelection_, camera_.toGraph(pressed_), camera_.toGraph(point)));
		linksDirty_ = true;
		invalidate();
	}
	Panel::mouseButtonEvent(event);
}

void GraphView::mouseMoveEvent(const MouseMoveEvent& event) {
	pointer_ = {event.position.x - frame().left, event.position.y - frame().top};
	switch(drag_) {
	case Drag::Nodes:
		dragDelta_ = {(pointer_.x - pressed_.x) / camera_.zoom, (pointer_.y - pressed_.y) / camera_.zoom};
		linksDirty_ = true;
		invalidate();
		break;
	case Drag::Pan: {
		GraphCamera camera = pressCamera_;
		camera.pan = pressCamera_.pan + (pointer_ - pressed_);
		setCamera(camera);
		break;
	}
	case Drag::Minimap:
		panToMinimap(pointer_);
		break;
	case Drag::Link:
	case Drag::Marquee:
		invalidatePaint();
		break;
	case Drag::None:
		break;
	}
	Panel::mouseMoveEvent(event);
}

void GraphView::keyEvent(const KeyEvent& event) {
	const std::size_t count = model_.nodes.size();
	if(event.action != Action::Release && count > 0) {
		keyNode_ = std::min(keyNode_, count - 1);
		const KeyPort keyed = keyPortOf(model_.nodes[keyNode_], keyNode_, keyPort_);
		const PortRef port{keyed.node, keyed.port, keyed.output, keyed.valid};
		const KeyCommand command =
		    keyCommand(event, {model_, selected_, keyNode_, keyPort_, keyLink_, camera_, resolvedSize()}, keyed);
		keyNode_ = command.keyNode;
		keyPort_ = command.keyPort;
		switch(command.action) {
		case KeyAction::Cycle:
			select({model_.nodes[keyNode_].id});
			// The box and the camera are read after select(), since the owner may answer with a new model
			if(const auto camera = revealing(camera_, nodeBox(keyNode_), resolvedSize())) setCamera(*camera);
			break;
		case KeyAction::Move:
			moveSelection(command.delta);
			break;
		case KeyAction::Camera:
			setCamera(command.camera);
			break;
		case KeyAction::StartLink:
			linkStart_ = port;
			replacing_ = model_.links.size();
			keyLink_ = true;
			break;
		case KeyAction::FinishLink:
			keyLink_ = false;
			finishLink(port, portPoint(port.node, port.port, port.output));
			break;
		case KeyAction::CancelLink:
			keyLink_ = false;
			linkStart_ = {};
			break;
		case KeyAction::Deselect:
			select({});
			break;
		case KeyAction::Fire:
			for(const GraphEvent& edit : command.edits) onEdit(edit);
			break;
		case KeyAction::None:
			break;
		}
		describe();
		invalidatePaint();
	}
	Panel::keyEvent(event);
}

} // namespace affresco
