/// @file
/// @brief The file steps: saveFile, which writes a value as a JSON file where the person chooses, and openFile, which
/// reads a JSON file the person chooses into the state. Both show the OS's dialog after the event; the person's
/// choice is the grant, and the document learns only the chosen file's own name, never where it is.

#include "FileSteps.hpp"
#include "Engine.hpp"

#include "affresco/core/Json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace affresco::format {

namespace {

constexpr std::size_t maxTitle = 200;     ///< The longest dialog title, in bytes (untrusted input).
constexpr std::size_t maxNameBytes = 120; ///< The longest suggested file name, in bytes.

/// @brief A file's own name, as UTF-8.
/// @param file Its path (UTF-8).
/// @return Its name.
std::string fileNameOf(std::string_view file) {
	const std::u8string name = std::filesystem::path(std::u8string(file.begin(), file.end())).filename().u8string();
	return {name.begin(), name.end()};
}

/// @brief The dialog's title, from the step's "title".
/// @param s The step.
/// @param fallback The title when there is none.
/// @return It.
std::string titleOf(const Running& s, std::string_view fallback) {
	if(!s.step.contains("title")) return std::string(fallback);
	std::string title = string(s.value("title"), s.where + ".title");
	if(title.size() > maxTitle) fail(s.where + ".title", "is longer than 200 bytes");
	return title;
}

/// @brief The extension of a file name, without the dot, in lower case.
/// @param name The name.
/// @return It, or empty.
std::string extensionOf(std::string_view name) {
	const std::size_t dot = name.rfind('.');
	if(dot == std::string_view::npos || dot == 0) return {};
	std::string extension(name.substr(dot + 1));
	std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return std::tolower(c); });
	return extension;
}

/// @brief Writes bytes to a file.
/// @param path The file (UTF-8).
/// @param bytes Them.
/// @return An error, or nothing.
std::optional<std::string> writeBytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
	std::ofstream file(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary);
	file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if(!file) return "the file can't be written";
	return std::nullopt;
}

/// @brief Reads a file of at most maxOpenedBytes.
/// @param path The file (UTF-8).
/// @param bytes Filled in with its text.
/// @return An error, or nothing.
std::optional<std::string> readText(const std::string& path, std::string& bytes) {
	const std::filesystem::path file(std::u8string(path.begin(), path.end()));
	std::error_code error;
	const auto size = std::filesystem::file_size(file, error);
	if(error) return "the file can't be read";
	if(size > maxOpenedBytes) return "the file is larger than 16 MB";
	bytes.resize(static_cast<std::size_t>(size));
	std::ifstream input(file, std::ios::binary);
	input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	if(!input) return "the file can't be read";
	return std::nullopt;
}

/// @brief "saveFile": the value as a JSON file (tab-indented) where the person chooses, named "name" at first.
/// @param s The step.
/// @return true.
bool saveFile(const Running& s) {
	s.steps.checkControl(s.where);
	const json value = s.value("saveFile");
	const std::string name = suggestedFileName(
	    s.step.contains("name") ? string(s.value("name"), s.where + ".name") : std::string(), "Untitled.json");
	const std::string text = value.dump(1, '\t', false, json::error_handler_t::replace) + "\n";
	if(text.size() > maxOpenedBytes) fail(s.where + ".saveFile", "is larger than 16 MB as a file");
	std::string extension = extensionOf(name);
	if(extension.empty()) extension = "json";
	saveAfterEvent(s, {text.begin(), text.end()}, name, {"JSON", {extension}}, {{"bytes", text.size()}});
	return true;
}

/// @brief Reads the chosen JSON file into the state, then runs onDone with result {name, bytes}, or onError.
/// @param engine The runtime.
/// @param path The chosen file.
/// @param into Where its value goes.
/// @param comments Whether comments and trailing commas are read too (plainJson()).
/// @param followers onDone and onError.
void openChosen(Engine& engine, const std::string& path, const json::json_pointer& into, bool comments,
                const Followers& followers) {
	std::string text;
	std::optional<std::string> error = readText(path, text);
	json value;
	if(!error) try {
			value = parseUntrusted(comments ? plainJson(text) : text);
		} catch(const std::exception&) {
			error = "the file isn't JSON, or nests too deeply";
		}
	const std::string name = fileNameOf(path);
	if(error) {
		engine.report("AffrescoUI screen: openFile '" + name + "': " + *error);
		followers.outcome->vars = json{{"error", {{"code", "openFile"}, {"message", *error}}}};
		followers.failed();
		return;
	}
	statePlace(engine.versions.state(), into, "openFile") = std::move(value);
	engine.document.markStateChanged(into);
	engine.reconciler.requestStateUpdate();
	followers.outcome->vars = json{{"result", {{"name", name}, {"bytes", text.size()}}}};
	followers.succeeded();
}

/// @brief An "openFile" step's "extensions": 1 to 16 of them, each 1 to 16 characters without a dot or a slash.
/// @param s The step.
/// @return Its extensions, or "json" if it gives none.
/// @throws DocumentError If they aren't such a list.
std::vector<std::string> extensionsOf(const Running& s) {
	if(!s.step.contains("extensions")) return {"json"};
	std::vector<std::string> extensions;
	const json given = s.value("extensions");
	if(!given.is_array() || given.empty() || given.size() > 16)
		fail(s.where + ".extensions", "must be a list of 1 to 16 extensions");
	for(const json& extension : given) {
		std::string text = string(extension, s.where + ".extensions");
		if(text.empty() || text.size() > 16 || text.find_first_of("./\\") != std::string::npos)
			fail(s.where + ".extensions", "an extension is 1 to 16 characters, without a dot or a slash");
		extensions.push_back(std::move(text));
	}
	return extensions;
}

/// @brief "openFile": a JSON file the person chooses (of the "extensions" given, "json" by default), into a state
/// path; with "comments": true, its comments and trailing commas are read too.
/// @param s The step.
/// @return true.
bool openFile(const Running& s) {
	s.steps.checkControl(s.where);
	const json::json_pointer into = s.target("openFile");
	std::string title = titleOf(s, "Open");
	std::vector<std::string> extensions = extensionsOf(s);
	bool comments = false;
	if(s.step.contains("comments")) {
		const json given = s.value("comments");
		if(!given.is_boolean()) fail(s.where + ".comments", "must be true or false");
		comments = given.get<bool>();
	}
	Engine& engine = s.steps.engine();
	if(!engine.window || dialogRefused(s)) return true; // a check or a preview: nothing to show
	// The dialog is modal: it waits for the event to end, so the steps around it run first
	engine.scheduler.defer([&engine, alive = std::weak_ptr<bool>(engine.alive), title = std::move(title),
	                        extensions = std::move(extensions), into, comments, followers = followersOf(s)] {
		if(!alive.lock() || !engine.window) return;
		const std::vector<std::string> chosen =
		    engine.window->chooseFiles({FileDialogKind::Open, title, {{"JSON", extensions}}, "", "", false});
		if(!alive.lock() || chosen.empty()) return; // cancelled, or the screen went away meanwhile
		openChosen(engine, chosen.front(), into, comments, followers);
	});
	return true;
}

/// @brief Runs a file step.
/// @param s The step.
/// @return std::nullopt if it isn't one, else whether to keep going.
std::optional<bool> runFileStep(const Running& s) {
	if(s.step.contains("saveFile")) return saveFile(s);
	if(s.step.contains("openFile")) return openFile(s);
	return std::nullopt;
}

} // namespace

Followers followersOf(const Running& s) {
	const SharedScope locals = snapshot(s.locals);
	auto outcome = std::make_shared<Scope>(Scope{locals.get(), locals, json::object(), {}, {}});
	auto succeeded =
	    s.steps.later(s.step.value("onDone", json::array()), s.sender, s.event, outcome, s.where + ".onDone");
	auto failed =
	    s.steps.later(s.step.value("onError", json::array()), s.sender, s.event, outcome, s.where + ".onError");
	return {std::move(outcome), std::move(succeeded), std::move(failed)};
}

bool dialogRefused(const Running& s) {
	Engine& engine = s.steps.engine();
	if(engine.fileDialogsAllowed) return false;
	const std::string message = "the host doesn't let this screen show file dialogs (its app's \"files\" capability)";
	engine.reportRefused("AffrescoUI screen: " + s.where + ": " + message);
	const Followers followers = followersOf(s);
	followers.outcome->vars = json{{"error", {{"code", "notAllowed"}, {"message", message}}}};
	followers.failed();
	return true;
}

std::string suggestedFileName(std::string_view text, std::string_view fallback) {
	std::string name;
	for(const char c : text) {
		const auto byte = static_cast<unsigned char>(c);
		if(byte < 0x20 || byte == 0x7f || std::string_view("/\\:*?\"<>|").contains(c)) continue;
		name.push_back(c);
	}
	while(!name.empty() && (name.front() == '.' || name.front() == ' ')) name.erase(name.begin());
	if(name.size() > maxNameBytes) {
		std::size_t end = maxNameBytes;
		while(end > 0 && (static_cast<unsigned char>(name[end]) & 0xC0U) == 0x80U) --end; // not inside a character
		name.resize(end);
	}
	return name.empty() ? std::string(fallback) : name;
}

void saveAfterEvent(const Running& s, std::vector<std::uint8_t> bytes, std::string name, FileFilter filter,
                    json result) {
	std::string title = titleOf(s, "Save");
	Engine& engine = s.steps.engine();
	if(!engine.window || dialogRefused(s)) return; // a check or a preview: nothing to show
	// The dialog is modal: it waits for the event to end, so the steps around it run first
	engine.scheduler.defer([&engine, alive = std::weak_ptr<bool>(engine.alive), title = std::move(title),
	                        bytes = std::move(bytes), name = std::move(name), filter = std::move(filter),
	                        result = std::move(result), followers = followersOf(s)]() mutable {
		if(!alive.lock() || !engine.window) return;
		const std::vector<std::string> chosen =
		    engine.window->chooseFiles({FileDialogKind::Save, title, {filter}, "", name, false});
		if(!alive.lock() || chosen.empty()) return; // cancelled, or the screen went away meanwhile
		if(const std::optional<std::string> error = writeBytes(chosen.front(), bytes)) {
			engine.report("AffrescoUI screen: saving '" + fileNameOf(chosen.front()) + "': " + *error);
			followers.outcome->vars = json{{"error", {{"code", "save"}, {"message", *error}}}};
			followers.failed();
			return;
		}
		result["name"] = fileNameOf(chosen.front());
		followers.outcome->vars = json{{"result", std::move(result)}};
		followers.succeeded();
	});
}

void registerFileSteps(StepRegistry& registry) {
	registry.add({"file",
	              {{"saveFile", {"name", "title", "onDone", "onError"}, true},
	               {"openFile", {"extensions", "comments", "title", "onDone", "onError"}, true}},
	              runFileStep});
}

} // namespace affresco::format
