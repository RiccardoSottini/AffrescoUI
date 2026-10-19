/// @file
/// @brief format::Engine: the runtime's composition root.

#include "Engine.hpp"

#include "affresco/core/Log.hpp"

namespace affresco::format {

Engine::Engine(Runtime& runtime, VersionControl& vcs, std::filesystem::path baseDirectory)
    : owner(runtime), versions(vcs), unhandled([](const std::string& action, Panel& sender) {
	      log(LogLevel::Warning,
		      "AffrescoUI screen: no handler for action '" + action + "' (from '" + sender.id() + "')");
      }),
      document(*this, std::move(baseDirectory)), builder(*this), scheduler(*this), properties(*this), reconciler(*this),
      events(*this), runner(*this) {
	registerBuiltInTypes(types);
}

void Engine::documentChanged() {
	document.readDerived();
	document.bumpStateVersion(); // a revision may change the state too
	reconciler.reconcile();
}

void Engine::proposalsChanged() {
	document.globalsChanged();
	reconciler.requestReconcile();
}

void Engine::report(const std::string& message) {
	log(LogLevel::Error, message);
	owner.onError(message);
}

double Engine::now() const noexcept { return window ? window->clock().seconds() : SteadyClock().seconds(); }

} // namespace affresco::format
