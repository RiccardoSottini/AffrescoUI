#pragma once

/// @file
/// @brief detail::Inspector: a debug overlay on a screen's window, for the person reviewing it.

#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/format/Runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace affresco {
class Label;
class Panel;
class Window;
} // namespace affresco

namespace affresco::detail {

/// @brief Shows, above everything, the panel under the pointer: its node, bounds, bindings as written and the
/// commit that last changed it (VersionControl::blame), with the window's frame statistics, the runtime's last
/// update and a trace of the latest events.
///
/// Off by default. It shows no secret state: bindings as written, never their values (Runtime::inspect), and the
/// trace names keys such as Enter or Escape but never a typed character, nor what a "send" step sends.
/// The overlay isn't hit: clicks reach what is under it. Main thread only.
class Inspector {
public:
	/// @brief The latest events the trace keeps.
	static constexpr std::size_t traceLength = 64;

	/// @brief Inspects a screen; off until setOn(true).
	/// @param versions Its document; must outlive this object.
	/// @param runtime Its panels and window; must outlive this object.
	Inspector(VersionControl& versions, format::Runtime& runtime) : versions_(versions), runtime_(runtime) {}
	/// @brief Turns the overlay off.
	~Inspector();
	Inspector(const Inspector&) = delete;
	Inspector& operator=(const Inspector&) = delete;

	/// @brief Turns the overlay on or off.
	/// @param on true to show it.
	/// @return An error (no window yet), or std::nullopt.
	std::optional<std::string> setOn(bool on);
	/// @brief Whether the overlay is on.
	[[nodiscard]] bool on() const noexcept { return window_ != nullptr; }
	/// @brief Shows times and counts, or leaves them out so that a picture of the overlay is the same every time
	/// (the rendering check).
	/// @param show true (the default) to show them.
	void setShowStatistics(bool show) { showStatistics_ = show; }
	/// @brief Shows or hides the overlay while it stays on, e.g. while a picture is drawn for a peer.
	/// @param visible false to hide it.
	void setVisible(bool visible) const;
	/// @brief Brings the overlay up to date with the panel under the pointer.
	void refresh();
	/// @brief The latest events, oldest first: at most traceLength lines.
	[[nodiscard]] const std::deque<std::string>& trace() const noexcept { return trace_; }

private:
	/// @brief Builds the overlay in a window and starts listening to it.
	/// @param window The screen's window.
	void start(Window& window);
	/// @brief Listens to the window, the runtime and the document, for the trace.
	void connect();
	/// @brief Adds a line to the trace, forgetting the oldest beyond traceLength.
	/// @param line The line.
	void record(std::string line);
	/// @brief Re-reads the statistics every half second, while on.
	void scheduleRefresh();
	/// @brief The lines the overlay shows for a panel.
	/// @param panel The panel under the pointer, or nullptr.
	/// @return The lines.
	[[nodiscard]] std::vector<std::string> describe(const Panel* panel);
	/// @brief The lines that say who last changed a node.
	/// @param node "#id" or the node's place.
	/// @return One line.
	[[nodiscard]] std::string blameLine(const std::string& node) const;
	/// @brief The lines of the window's and the runtime's statistics.
	/// @return The lines, or one that says they are left out.
	[[nodiscard]] std::vector<std::string> statisticsLines() const;
	/// @brief The node a panel was built from, for the trace.
	/// @param panel A panel, or nullptr.
	/// @return "#id", the node's place, or "nothing".
	[[nodiscard]] std::string nodeOf(const Panel* panel);
	/// @brief Puts the highlight on a panel and the information next to it.
	/// @param panel The panel, or nullptr.
	/// @param text The information.
	void place(const Panel* panel, const std::string& text);

	VersionControl& versions_;             ///< The document, for blame and the trace.
	format::Runtime& runtime_;             ///< The panels, the window and the update statistics.
	Window* window_ = nullptr;             ///< While on: the window the overlay is in.
	Panel* highlight_ = nullptr;           ///< While on: the box around the panel under the pointer.
	Label* info_ = nullptr;                ///< While on: what the overlay says.
	std::vector<Connection> windowSlots_;  ///< onMouseMove, onMouseButton, onScroll, onKey.
	std::vector<Connection> runtimeSlots_; ///< onSend, onError.
	std::vector<Connection> versionSlots_; ///< onCommit, onDecision.
	std::uint64_t timer_ = 0;              ///< The refresh timer, while on.
	bool showStatistics_ = true;           ///< See setShowStatistics().
	std::deque<std::string> trace_;        ///< See trace().
};

} // namespace affresco::detail
