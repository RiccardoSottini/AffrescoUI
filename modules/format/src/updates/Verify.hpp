#pragma once

/// @file
/// @brief format::Verify: the check that a partial update gives what a full one gives (verify mode).

#include <functional>
#include <string>

namespace affresco {
class Panel;
} // namespace affresco

namespace affresco::format {

struct Engine;

/// @brief Verify mode: after every partial update, the panels are written down, a full pass runs over the same panels
/// with handlers suspended, they are written down again and compared. A difference is reported once per update, for
/// the first one: "update check: <place>.<property>: a partial update and a full update differ", with the two values
/// only in the log.
///
/// The full pass leaves no trace: no "onShow", "onHide" or "watch" handler, no transition started, no state written,
/// and the update's statistics put back. On only through the host (Runtime::setVerifyUpdates(), or
/// AFFRESCOUI_VERIFY_UPDATES=1 when the engine is made): never from a document, a revision, a channel or MCP.
/// Callers: the reconciler (Reconciler::update()). Main thread only.
class Verify {
public:
	/// @brief The check of an engine's updates, on if AFFRESCOUI_VERIFY_UPDATES is "1".
	/// @param engine The runtime; outlives this object.
	explicit Verify(Engine& engine) noexcept;

	/// @brief Turns verify mode on or off.
	/// @param on true to check every partial update.
	void setOn(bool on) noexcept { on_ = on; }
	/// @brief Whether verify mode is on.
	/// @return true if it is.
	[[nodiscard]] bool on() const noexcept { return on_; }
	/// @brief Checks the partial update that just ran: writes the panels down, runs a full pass, writes them down
	/// again and reports the first difference.
	/// @param root The window's root.
	/// @param fullPass Brings every panel in line with the document and the state, whatever changed.
	void check(Panel& root, const std::function<void()>& fullPass);
	/// @brief Notes a panel the reconciler creates: during the check's full pass, a difference.
	/// @param place The panel's place.
	void created(const std::string& place) {
		if(checking_ && firstCreated_.empty()) firstCreated_ = place;
	}

private:
	Engine& engine_;           ///< The runtime.
	bool on_ = false;          ///< Whether it checks.
	bool checking_ = false;    ///< Whether the check's full pass runs.
	std::string firstCreated_; ///< The place of the first panel that pass created.
};

/// Test-only hooks: nothing in a document, a revision, a channel or MCP reaches them.
namespace testing {
/// @brief Makes the panel with an id read nothing when its values are applied, so a partial update misses it (a read
/// dropped, as a bug would): verify mode then reports it.
/// @param id The panel's id; empty to drop nothing.
void dropReadsOf(std::string id);
/// @brief The id dropReadsOf() set.
/// @return It; empty if none.
[[nodiscard]] const std::string& droppedReads() noexcept;
} // namespace testing

} // namespace affresco::format
