/// @file
/// @brief vcs::Repository's undo of an earlier commit that keeps the later changes (VERSION_CONTROL.md, "History"):
/// its plan, worked out before anything changes, and the revision that does it.

#include "affresco/vcs/Repository.hpp"

namespace affresco::vcs {

UndoPlan Repository::undoPlan(const json& revision, std::string_view author, const UndoOptions& options) const {
	UndoPlan plan;
	plan.of = resolve(revision); // an unknown commit throws, as resolve() does
	plan.head = mainHead().id;
	if(!options.empty()) {
		plan.blocked = "this build can't keep later changes yet";
		return plan;
	}
	try {
		static_cast<void>(revertRevision(revision, author));
		plan.outcome = "clean";
	} catch(const DocumentError& error) {
		plan.blocked = error.what();
	}
	return plan;
}

json Repository::undoRevision(const json& revision, std::string_view author, const UndoOptions& options) const {
	return VersionControl::undoRevision(revision, author, options);
}

} // namespace affresco::vcs
