/// @file
/// @brief The folder step: chooseFolder, which asks the person for the folder a root names, in the OS's folder dialog.
/// The host lets them choose it (Services::choosable); their choice is the grant. The services that use the root start
/// again on it; the document learns only the folder's own name, never where it is.

#include "Engine.hpp"

#include "affresco/core/Services.hpp"
#include "affresco/render/Window.hpp"

namespace affresco::format {

namespace {

constexpr std::size_t maxTitle = 200; ///< The longest dialog title, in bytes (untrusted input).

/// @brief A folder's own name, as UTF-8.
/// @param folder The folder (UTF-8).
/// @return Its name.
std::string folderName(std::string_view folder) {
	const std::filesystem::path path{std::u8string(folder.begin(), folder.end())};
	const std::u8string name = (path.has_filename() ? path : path.parent_path()).filename().u8string();
	return {name.begin(), name.end()};
}

/// @brief "chooseFolder": shows the folder dialog after the event, then points the root at the chosen folder and runs
/// "onDone" with result {name}; "onError" with error {code, message} if it can't. Nothing runs if the person cancels.
/// @param s The step.
/// @return true.
bool chooseFolder(const Running& s) {
	const std::string root = string(s.value("chooseFolder"), s.where + ".chooseFolder");
	s.steps.checkControl(s.where);
	Engine& engine = s.steps.engine();
	services::Services* services = engine.serviceCalls.services();
	if(!services || !services->choosable(root))
		fail(s.where + ".chooseFolder", "the host doesn't let the person choose the folder '" + root + "'");
	std::string title = s.step.contains("title") ? string(s.value("title"), s.where + ".title") : std::string();
	if(title.size() > maxTitle) fail(s.where + ".title", "is longer than 200 bytes");
	if(!engine.window) return true; // a check or a preview: nothing to show

	const SharedScope locals = snapshot(s.locals);
	const auto outcome = std::make_shared<const Scope>(Scope{locals.get(), locals, json::object(), {}, {}});
	auto succeeded = s.steps.later(s.step.value("onDone", json()), s.sender, s.event, outcome, s.where + ".onDone");
	auto failed = s.steps.later(s.step.value("onError", json()), s.sender, s.event, outcome, s.where + ".onError");
	// The dialog is modal: it waits for the event to end, so the steps around it run first
	engine.scheduler.defer([&engine, alive = std::weak_ptr<bool>(engine.alive), root, title = std::move(title),
	                        succeeded = std::move(succeeded), failed = std::move(failed), outcome] {
		if(!alive.lock() || !engine.window) return;
		const std::vector<std::string> chosen = engine.window->chooseFiles({FileDialogKind::Folder, title});
		if(!alive.lock() || chosen.empty()) return; // cancelled, or the screen went away meanwhile
		services::Services* current = engine.serviceCalls.services(); // the host's, now that the dialog closed
		const std::optional<std::string> error =
		    current ? current->chooseRoot(root, chosen.front()) : std::optional<std::string>("no services");
		if(error) {
			engine.report("AffrescoUI screen: chooseFolder '" + root + "': " + *error);
			outcome->vars = json{{"error", {{"code", "chooseFolder"}, {"message", *error}}}};
			failed();
			return;
		}
		engine.document.globalsChanged(); // the "services" variable
		outcome->vars = json{{"result", {{"name", folderName(chosen.front())}}}};
		succeeded();
	});
	return true;
}

/// @brief Runs a folder step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runFolderStep(const Running& s) {
	if(!s.step.contains("chooseFolder")) return std::nullopt;
	return chooseFolder(s);
}

} // namespace

void registerFolderSteps(StepRegistry& registry) {
	registry.add({"folder", {{"chooseFolder", {"title", "onDone", "onError"}, true}}, runFolderStep});
}

} // namespace affresco::format
