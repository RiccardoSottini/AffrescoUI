/// @file
/// @brief format::Engine as the version control's observer: what a new version of the document or of the proposals
/// brings.

#include "Engine.hpp"

namespace affresco::format {

void Engine::documentChanged() {
	data.documentChanged(); // the records follow its models first: records that don't fit refuse the version
	programs.load();        // and its program: one that can't be loaded refuses it too
	builder.compiler().forgetVersion(); // what was compiled belongs to the version before
	document.readDerived();
	document.bumpStateVersion();    // a revision may change the state too
	serviceCalls.load();            // a restore may change the services or the connections
	audio.documentChanged();        // and the sounds, through an include or a feature
	otherWindows.documentChanged(); // and the other windows
	reconciler.reconcile();
}

void Engine::proposalsChanged() {
	document.globalsChanged();
	reconciler.requestReconcile();
}

} // namespace affresco::format
