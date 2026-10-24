/// @file
/// @brief The "attach" step: files the person gives the app (chosen in the OS's file dialog, or dropped on the window)
/// become attachments, which agents may then name in a revision's assets (VERSION_CONTROL.md, "Assets"). A control
/// step: only the person's own input attaches, never a revision.

#include "Engine.hpp"

#include "affresco/core/Assets.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace affresco::format {

namespace {

constexpr std::size_t maxTitle = 200; ///< The longest dialog title, in bytes.
constexpr std::size_t maxAccept = 16; ///< The most patterns in "accept".
constexpr std::size_t maxFiles = 16;  ///< The most files one step attaches.

/// @brief What an "attach" step asks for.
struct Request {
	std::vector<std::string> accept; ///< Media types or "audio/*"; empty takes every asset type.
	bool multiple = false;           ///< Several files at once.
	std::string title;               ///< The dialog's title.
	std::vector<std::string> from;   ///< Dropped files to attach instead of a dialog.
};

/// @brief Tells whether a type is one a step accepts.
/// @param accept The patterns.
/// @param type The type.
/// @return true if any matches (or there are none).
bool accepts(const std::vector<std::string>& accept, std::string_view type) {
	return accept.empty() || std::ranges::any_of(accept, [&](const std::string& pattern) {
		       return pattern == type || pattern == "*/*" ||
			          (pattern.ends_with("/*") &&
			           type.starts_with(std::string_view(pattern).substr(0, pattern.size() - 1)));
	       });
}

/// @brief Reads an "attach" step's options.
/// @param s The step.
/// @return What it asks for.
Request requestOf(const Running& s) {
	const json options = s.value("attach");
	if(!options.is_object()) fail(s.where + ".attach", R"(expected {"accept"?, "multiple"?, "title"?, "from"?})");
	Request request;
	for(const auto& [key, value] : options.items()) {
		const std::string at = s.where + ".attach." + key;
		if(key == "accept") {
			if(!value.is_array() || value.size() > maxAccept)
				fail(at, "expected a list of media types, e.g. \"audio/*\"");
			for(const json& type : value) request.accept.push_back(string(type, at));
		} else if(key == "multiple") request.multiple = expect::boolean(value, at);
		else if(key == "title") {
			request.title = string(value, at);
			if(request.title.size() > maxTitle) fail(at, "is longer than 200 bytes");
		} else if(key == "from") {
			if(!value.is_array() || value.size() > maxFiles)
				fail(at, "expected the dropped files, \"{{event.files}}\"");
			for(const json& file : value) request.from.push_back(string(file, at));
		} else fail(at, "unknown key (expected accept, multiple, title or from)");
	}
	return request;
}

/// @brief The dialog's filter: the extensions of the types accepted.
/// @param accept The patterns.
/// @return One filter, "Files".
std::vector<FileFilter> filtersOf(const std::vector<std::string>& accept) {
	FileFilter filter{"Files", {}};
	for(const AssetType& type : assetTypes())
		if(accepts(accept, type.type))
			for(const std::string_view extension : type.extensions)
				if(!extension.empty()) filter.extensions.emplace_back(extension);
	return {filter};
}

/// @brief Reads a file the person gave, up to maxAssetBytes.
/// @param path Its path (UTF-8).
/// @return Its bytes.
/// @throws DocumentError If it isn't a regular file, is larger, or can't be read.
std::string readGiven(const std::string& path) {
	const std::filesystem::path file{std::u8string(path.begin(), path.end())};
	const std::string name = std::filesystem::path(file).filename().string();
	std::error_code error;
	if(!std::filesystem::is_regular_file(file, error)) throw DocumentError("'" + name + "' isn't a file");
	const auto size = std::filesystem::file_size(file, error);
	if(error || size > maxAssetBytes)
		throw DocumentError("'" + name + "' is larger than " + std::to_string(maxAssetBytes >> 20) + " MiB");
	std::string bytes(static_cast<std::size_t>(size), '\0');
	std::ifstream in(file, std::ios::binary);
	in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	if(!in) throw DocumentError("'" + name + "' can't be read");
	return bytes;
}

/// @brief Attaches files, writes them "into" the state, and runs "onDone" (with result, the attachments) or
/// "onError" (with error {code, message}) when none could be attached.
/// @param engine The runtime.
/// @param files The paths.
/// @param accept The types accepted.
/// @param into Where the attachments go in the state, or none.
/// @param outcome The variables onDone and onError see.
/// @param succeeded Runs onDone.
/// @param failed Runs onError.
void attachFiles(Engine& engine, const std::vector<std::string>& files, const std::vector<std::string>& accept,
                 const std::optional<json::json_pointer>& into, const std::shared_ptr<Scope>& outcome,
                 const std::function<void()>& succeeded, const std::function<void()>& failed) {
	json attached = json::array();
	std::string problems;
	for(const std::string& path : files) {
		const std::string name = std::filesystem::path(std::u8string(path.begin(), path.end())).filename().string();
		const std::string_view type = assetTypeOf(name);
		std::string problem;
		if(type.empty() || !accepts(accept, type)) problem = "'" + name + "' isn't a kind of file this accepts";
		else try {
				Attached result = engine.versions.attach(name, type, readGiven(path), json::object());
				if(result.error.empty()) attached.push_back(toJson(result.attachment));
				else problem = "'" + name + "': " + result.error;
			} catch(const DocumentError& error) {
				problem = error.what();
			}
		if(!problem.empty()) problems += (problems.empty() ? "" : "; ") + problem;
	}
	if(!problems.empty()) engine.report("AffrescoUI screen: attach: " + problems);
	if(attached.empty()) {
		outcome->vars = json{{"error", {{"code", "attach"}, {"message", problems}}}};
		failed();
		return;
	}
	if(into) {
		statePlace(engine.versions.state(), *into, "attach.into") = attached;
		engine.document.markStateChanged(*into);
		engine.reconciler.requestStateUpdate();
	}
	outcome->vars = json{{"result", attached}};
	if(!problems.empty()) outcome->vars["error"] = {{"code", "attach"}, {"message", problems}};
	succeeded();
}

/// @brief "attach": the person's files become attachments, from the file dialog after the event, or at once from the
/// files they just dropped ("from": only paths of the last drop on the window are taken).
/// @param s The step.
/// @return true.
bool attach(const Running& s) {
	s.steps.checkControl(s.where); // the person's input only: a revision can't add one, a preview can't run one
	Engine& engine = s.steps.engine();
	Request request = requestOf(s);
	std::optional<json::json_pointer> into;
	if(s.step.contains("into")) into = s.target("into");

	const SharedScope locals = snapshot(s.locals);
	const auto outcome = std::make_shared<Scope>(Scope{locals.get(), locals, json::object(), {}, {}});
	auto succeeded =
	    s.steps.later(s.step.value("onDone", json::array()), s.sender, s.event, outcome, s.where + ".onDone");
	auto failed =
	    s.steps.later(s.step.value("onError", json::array()), s.sender, s.event, outcome, s.where + ".onError");

	if(s.step["attach"].is_object() && s.step["attach"].contains("from")) {
		for(const std::string& file : request.from)
			if(std::ranges::find(engine.droppedFiles, file) == engine.droppedFiles.end())
				fail(s.where + ".attach.from", "only files the person dropped on the window may be attached");
		if(!request.multiple && request.from.size() > 1) request.from.resize(1);
		attachFiles(engine, request.from, request.accept, into, outcome, succeeded, failed);
		return true;
	}
	if(!engine.window) return true; // a check: nothing to show
	// The dialog is modal: it waits for the event to end, so the steps around it run first
	engine.scheduler.defer([&engine, alive = std::weak_ptr<bool>(engine.alive), request = std::move(request), into,
	                        outcome, succeeded = std::move(succeeded), failed = std::move(failed)] {
		if(!alive.lock() || !engine.window) return;
		std::vector<std::string> chosen =
		    engine.window->chooseFiles({FileDialogKind::Open, request.title.empty() ? "Attach" : request.title,
			                            filtersOf(request.accept), "", "", request.multiple});
		if(!alive.lock() || chosen.empty()) return; // cancelled, or the screen went away meanwhile
		if(chosen.size() > maxFiles) chosen.resize(maxFiles);
		attachFiles(engine, chosen, request.accept, into, outcome, succeeded, failed);
	});
	return true;
}

/// @brief Runs an attach step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runAttachStep(const Running& s) {
	if(!s.step.contains("attach")) return std::nullopt;
	return attach(s);
}

} // namespace

void registerAttachSteps(StepRegistry& registry) {
	registry.add({"attach", {{"attach", {"into", "onDone", "onError"}, true}}, runAttachStep});
}

} // namespace affresco::format
