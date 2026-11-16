/// @file
/// @brief format::Engine: the runtime's composition root.

#include "Engine.hpp"
#include "Contributions.hpp"
#include "ImagePublishing.hpp"
#include "Settings.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace affresco::format {

Engine::Engine(Runtime& runtime, VersionControl& vcs, std::filesystem::path baseDirectory)
    : owner(runtime), versions(vcs), unhandled([](const std::string& action, Panel& sender) {
	      log(LogLevel::Warning,
		      "AffrescoUI screen: no handler for action '" + action + "' (from '" + sender.id() + "')");
      }),
      document(*this, std::move(baseDirectory)), builder(*this), scheduler(*this), properties(*this), reconciler(*this),
      events(*this), runner(*this), programs(*this), models(*this), connections(*this), serviceCalls(*this),
      asks(*this), calls(*this) {
	registerBuiltInTypes(types);
	attachImageDocuments(*this);
	addContributionVariables(*this);
	addSettingsVariables(*this);
	document.addVariable("services", [this] { // {name: {state, error, restarts, folder}} (RFC 0002)
		return serviceCalls.services() ? serviceCalls.services()->status() : json::object();
	});
	document.addVariable("connections", [this] { return connections.status(); }); // {name: {online, pending, ...}}
	document.addVariable("auth", [this] { // {name: {state, account, expires, ...}}: never a token
		return identity ? identity->status() : json::object();
	});
	connections.onStatus([this] {
		document.globalsChanged(); // the "connections" variable
		reconciler.requestReconcile();
	});
	document.addVariable("requests", [this] { return asks.list(); }); // the person's requests ("ask") and replies
	document.addVariable("peers", [this] {                            // [{author, role}]: who is connected
		const json coordination = versions.coordination();
		return coordination.contains("peers") ? coordination["peers"] : json::array();
	});
	document.addVariable("windows", [this] { return otherWindows.status(); }); // {main: {fullscreen}, name: {open, ..}}
	document.addVariable("runs", [this] { // [run]: the runs peers asked for, waiting or done, oldest first
		json runs = json::array();
		for(const RunRecord& run : runControl ? runControl->runs() : std::vector<RunRecord>())
			runs.push_back(toJson(run));
		return runs;
	});
	document.addVariable("runGrants", [this] { // [grant]: what "always" allowed, oldest first
		json grants = json::array();
		for(const Grant& grant : runControl ? runControl->grants() : std::vector<Grant>())
			grants.push_back(toJson(grant));
		return grants;
	});
	peersChanged = versions.onReviewChanged.connect([this] { proposalsChanged(); });
}

Engine::~Engine() {
	versions.onReviewChanged.disconnect(peersChanged);
	detachImageDocuments(*this);
	terminals->forget();
}

void Engine::report(const std::string& message) {
	log(LogLevel::Error, message);
	owner.onError(message);
}

void Engine::reportRefused(const std::string& message) {
	static constexpr std::array<std::string_view, 4> chosenByTheHost{
	    "the host allows", "the host gives this screen no network", "isn't a host this app may connect to",
	    "the host doesn't let this screen"};
	if(std::ranges::any_of(chosenByTheHost, [&](std::string_view part) { return message.contains(part); }))
		log(LogLevel::Info, message);
	else report(message);
}

double Engine::now() const noexcept { return window ? window->clock().seconds() : SteadyClock().seconds(); }

} // namespace affresco::format
