#pragma once

/// @file
/// @brief format::UpdateFrame: what an update does around the walk over the panels, before it (the screen, the window,
/// the theme, the locale, the default font, the icon) and after it (watches, the title, the menu bar, the focus ring,
/// onUpdated), each part only when what it read changed.

#include "Reads.hpp"
#include "Tracker.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief The work of an update that isn't the walk over the panels. The window-wide values (the theme, the locale,
/// the icon, the title, the menu bar, the focus ring) are readers of the Tracker: each is evaluated again only when a
/// place it read changed, or on a change of everything.
///
/// Invariants: begin() and end() run once per update, around Reconciler::updateRoot(); the window it keeps is the one
/// the last begin() saw; each part's reads are those of its last evaluation. The theme, the locale and the icon are
/// evaluated as the tracker runs them, before the walk; the title, the menu bar and the focus ring after it, in end().
/// Callers: the reconciler (begin(), end()), the `context` and `keybindings` variables (contextBuilt(), keysBuilt()),
/// Runtime (lastChanges()). Main thread only.
class UpdateFrame {
public:
	/// @brief The frame of an engine's updates, its parts added to the engine's tracker.
	/// @param engine The runtime, its tracker built; outlives this object.
	explicit UpdateFrame(Engine& engine);
	/// @brief Takes its parts out of the tracker.
	~UpdateFrame();
	UpdateFrame(const UpdateFrame&) = delete;
	UpdateFrame& operator=(const UpdateFrame&) = delete;
	UpdateFrame(UpdateFrame&&) = delete;
	UpdateFrame& operator=(UpdateFrame&&) = delete;

	/// @brief Before the walk: keeps the screen an outlet shows, marks the window changed if it did, makes the globals
	/// and built variables stale on a change of everything, and gives the window its default font.
	/// @param changes What changed since the last update; what begin() marks joins them.
	/// @return Whether the window's size, scale, colour scheme or other values changed since the update before (not
	/// on the first).
	bool begin(StateChanges& changes);
	/// @brief After the walk: runs the "watch" handlers whose values changed, shows the title, the menu bar and the
	/// focus ring if what they read changed, marks the `keybindings` and `context` variables changed if the panels
	/// they were built from did, and tells the application (Runtime::onUpdated).
	/// @param changes What changed in the update.
	/// @param full Whether the walk visited everything.
	void end(const StateChanges& changes, bool full);

	/// @brief The places the last update changed.
	/// @return They, or nullopt if it may have changed anything (a full update).
	[[nodiscard]] std::optional<std::span<const std::string>> lastChanges() const noexcept;
	/// @brief The `keybindings` variable was built from the panels as they are now.
	/// @param fingerprint keysFingerprint() of those panels.
	void keysBuilt(std::size_t fingerprint) noexcept { keysBuiltFrom_ = fingerprint; }
	/// @brief The `context` variable was built from the panels as they are now.
	/// @param overlay Whether an overlay was shown.
	void contextBuilt(bool overlay) noexcept { overlayBuiltFrom_ = overlay; }

private:
	/// @brief A window-wide value read outside the panels.
	enum class Part : std::uint8_t { Theme, Locale, Icon, Title, MenuBar, FocusRing };
	/// How many parts there are.
	static constexpr std::size_t partCount = 6;

	/// @brief One part as a reader of the tracker.
	class PartReader final : public Reader {
	public:
		/// @brief A part of a frame.
		/// @param owner The frame.
		/// @param part The part.
		PartReader(UpdateFrame& owner, Part part) noexcept : frame_(owner), part_(part) {}
		/// @brief Evaluates the part (the theme, the locale, the icon), or has end() do it (the others).
		/// @param changes The changes of the update running.
		void update(const StateChanges& changes) override;
		/// @brief Its order: the parts' order, after the variables marks reach.
		/// @return The order.
		[[nodiscard]] std::uint64_t order() const noexcept override { return static_cast<std::uint64_t>(part_) + 1u; }
		/// @brief Its name, for errors.
		/// @return The name.
		[[nodiscard]] std::string name() const override;

		ReaderId id = 0;  ///< Its handle in the tracker.
		bool due = false; ///< Whether end() evaluates it.

	private:
		UpdateFrame& frame_; ///< The frame.
		Part part_;          ///< The part.
	};

	/// @brief Evaluates a part, recording what it reads in the tracker.
	/// @param part The part.
	/// @throws DocumentError If the icon can't be shown (the revision is refused, as an image node's would be).
	void evaluate(Part part);
	/// @brief Marks `keybindings` and `context` changed if the panels they were built from changed since, and asks
	/// for an update then.
	void checkPanels();

	Engine& engine_;                                       ///< The runtime.
	std::array<std::optional<PartReader>, partCount> parts_; ///< The parts, by Part.
	std::string window_;                                   ///< The window's values last seen.
	std::vector<std::string> lastChanges_;                 ///< See lastChanges().
	bool lastAll_ = true;                                  ///< lastChanges() is everything.
	std::size_t keysBuiltFrom_ = 0;                        ///< See keysBuilt().
	bool overlayBuiltFrom_ = false;                        ///< See contextBuilt().
	std::size_t records_ = 0;                              ///< The records there were at the last end().
};

} // namespace affresco::format
