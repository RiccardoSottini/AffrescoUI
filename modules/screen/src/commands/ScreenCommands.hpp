#pragma once

/// @file
/// @brief detail::ScreenCommands: a screen's commands as tools for its peers (CommandService), over its runtime, its
/// run control and its version control.

#include "affresco/core/Commands.hpp"
#include "affresco/core/ScreenImages.hpp"
#include "affresco/core/VersionControl.hpp"
#include "affresco/format/Runtime.hpp"

#include <string_view>

namespace affresco::detail {

/// @brief What a screen's peers ask about its commands: lists them as a peer may run them, runs one as the peer after
/// the run control's verdict (and, when it waits, after a decision), undoes a run, and passes a reviewer's decisions
/// to the run control.
///
/// Not built yet (f4e fills it in): every request is refused with "not built yet"; pending() gives the run control's
/// lists. Owned by Screen::Impl, after the runtime and the run control, which it reads; each Session holds a pointer
/// to it while the host allows command runs (Screen::allowCommandRuns()). Main thread only.
class ScreenCommands final : public CommandService {
public:
	/// @brief The commands of a screen.
	/// @param runtime Its commands and their runs; outlives this object.
	/// @param control Who may run them, and the runs; outlives this object.
	/// @param versions Its document and its reviewers; outlives this object.
	/// @param images Its pictures, for a run's evidence; outlives this object.
	ScreenCommands(format::Runtime& runtime, RunControl& control, VersionControl& versions, ScreenImages& images);
	/// @brief Stops listening to the run control.
	~ScreenCommands() override;

	ScreenCommands(const ScreenCommands&) = delete;
	ScreenCommands& operator=(const ScreenCommands&) = delete;
	ScreenCommands(ScreenCommands&&) = delete;
	ScreenCommands& operator=(ScreenCommands&&) = delete;

	/// @brief Not built yet (f4e).
	/// @param author The peer.
	/// @param filter Only commands whose name or title holds it.
	/// @param all Also those it may never run.
	/// @return A "rejected", "not built yet".
	[[nodiscard]] json list(std::string_view author, std::string_view filter, bool all) const override;
	/// @brief Not built yet (f4e): answers a "rejected", once.
	/// @param author The runner.
	/// @param message The runCommand message.
	/// @param answer Gives the answer.
	void run(std::string_view author, const json& message, Answer answer) override;
	/// @brief Not built yet (f4e).
	/// @param author The peer.
	/// @param message The undoRun message.
	/// @return A "rejected", "not built yet".
	json undo(std::string_view author, const json& message) override;
	/// @brief Not built yet (f4e).
	/// @param reviewer The peer.
	/// @param message The allowRun, denyRun or revokeRun message.
	/// @return A "rejected", "not built yet".
	json decide(std::string_view reviewer, const json& message) override;
	/// @brief The runs and run grants, from the run control (f4e limits them to what the reader may see).
	/// @param reader The peer.
	/// @return `{"runs", "runGrants"}`.
	[[nodiscard]] json pending(std::string_view reader) const override;

private:
	[[maybe_unused]] format::Runtime& runtime_; ///< The commands and their runs (f4e).
	RunControl& control_;                       ///< Who may run them, and the runs.
	[[maybe_unused]] VersionControl& versions_; ///< The document and its reviewers (f4e).
	[[maybe_unused]] ScreenImages& images_;     ///< The pictures, for a run's evidence (f4e).
	Connection changed_ = 0;                    ///< On control_.onChanged: forwarded as onChanged.
};

} // namespace affresco::detail
