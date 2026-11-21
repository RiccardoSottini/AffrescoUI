/// @file
/// @brief The file steps: saveFile, which writes a value as a JSON file where the person chooses, and openFile, which
/// reads a JSON file the person chooses into the state (or, "as": "image", a PNG or JPEG picture as a data URI). Both
/// show the OS's dialog after the event; the person's choice is the grant, and the document learns only the chosen
/// file's own name, never where it is.

#include "FileSteps.hpp"
#include "Engine.hpp"

#include "affresco/core/Assets.hpp"
#include "affresco/core/Json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace affresco::format {

namespace {

constexpr std::size_t maxTitle = 200;     ///< The longest dialog title, in bytes (untrusted input).
constexpr std::size_t maxNameBytes = 120; ///< The longest suggested file name, in bytes.
constexpr std::size_t maxImageBytes = std::size_t{8} << 20; ///< The largest picture openFile reads ("as": "image").

/// @brief A big-endian number of two or four bytes.
/// @param bytes The bytes.
/// @param at Where it starts (checked by the caller).
/// @param count 2 or 4.
/// @return It.
std::uint32_t bigEndian(std::string_view bytes, std::size_t at, std::size_t count) {
	std::uint32_t value = 0;
	for(std::size_t i = 0; i < count; ++i) value = (value << 8U) | static_cast<std::uint8_t>(bytes[at + i]);
	return value;
}

/// @brief A PNG's or a JPEG's size in pixels, from its header (a PNG's IHDR, a JPEG's first start-of-frame marker).
/// @param bytes The file's bytes, already known to start as a PNG or a JPEG does.
/// @param png Whether it is a PNG.
/// @return {width, height}, or {0, 0} when the header doesn't say.
std::pair<std::uint32_t, std::uint32_t> pictureSize(std::string_view bytes, bool png) {
	if(png) return bytes.size() >= 24 ? std::pair{bigEndian(bytes, 16, 4), bigEndian(bytes, 20, 4)} : std::pair{0U, 0U};
	std::size_t at = 2;
	while(at + 9 < bytes.size() && static_cast<std::uint8_t>(bytes[at]) == 0xFF) {
		const auto marker = static_cast<std::uint8_t>(bytes[at + 1]);
		if(marker == 0xFF) { // a fill byte
			++at;
			continue;
		}
		const bool frame = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
		if(frame) return {bigEndian(bytes, at + 7, 2), bigEndian(bytes, at + 5, 2)};
		at += 2 + bigEndian(bytes, at + 2, 2);
	}
	return {0, 0};
}

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

/// @brief The path of one file of a numbered series beside the chosen one: "Deck.png" becomes "Deck-3.png".
/// @param chosen The chosen file (UTF-8).
/// @param number The file's number, from 1.
/// @return Its path (UTF-8), in the chosen file's folder.
std::string numberedPath(const std::string& chosen, std::size_t number) {
	std::filesystem::path path(std::u8string(chosen.begin(), chosen.end()));
	const std::u8string stem = path.stem().u8string();
	const std::u8string extension = path.extension().u8string();
	const std::string suffix = "-" + std::to_string(number);
	path.replace_filename(stem + std::u8string(suffix.begin(), suffix.end()) + extension);
	const std::u8string text = path.u8string();
	return {text.begin(), text.end()};
}

/// @brief Reads a file of at most @p limit bytes.
/// @param path The file (UTF-8).
/// @param bytes Filled in with its text.
/// @param limit The most it may hold (maxOpenedBytes or maxImageBytes).
/// @return An error, or nothing.
std::optional<std::string> readText(const std::string& path, std::string& bytes, std::size_t limit = maxOpenedBytes) {
	const std::filesystem::path file(std::u8string(path.begin(), path.end()));
	std::error_code error;
	const auto size = std::filesystem::file_size(file, error);
	if(error) return "the file can't be read";
	if(size > limit) return "the file is larger than " + std::to_string(limit >> 20U) + " MB";
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

/// @brief Reads a chosen picture as a data URI ("data:image/png;base64,..."), which an image node's "source" shows and
/// a saved document carries.
/// @param path The chosen file.
/// @param value Filled in with the data URI.
/// @param result Filled in with {bytes, width, height}.
/// @return An error, or nothing.
std::optional<std::string> readPicture(const std::string& path, json& value, json& result) {
	std::string bytes;
	if(std::optional<std::string> error = readText(path, bytes, maxImageBytes)) return error;
	const bool png = !assetBytesProblem("image/png", bytes);
	if(!png && assetBytesProblem("image/jpeg", bytes)) return "the file isn't a PNG or JPEG picture";
	const auto [width, height] = pictureSize(bytes, png);
	value = std::string(png ? "data:image/png;base64," : "data:image/jpeg;base64,") + encodeBase64(bytes);
	result = json{{"bytes", bytes.size()}, {"width", width}, {"height", height}};
	return std::nullopt;
}

/// @brief Reads the chosen file into the state, then runs onDone with result {name, bytes} (a picture's also with
/// {width, height}), or onError.
/// @param engine The runtime.
/// @param path The chosen file.
/// @param into Where its value goes.
/// @param comments Whether comments and trailing commas are read too (plainJson()).
/// @param image Whether it is read as a picture (readPicture()) rather than as JSON.
/// @param followers onDone and onError.
void openChosen(Engine& engine, const std::string& path, const json::json_pointer& into, bool comments, bool image,
                const Followers& followers) {
	std::string text;
	json value;
	json result;
	std::optional<std::string> error;
	if(image) error = readPicture(path, value, result);
	else {
		error = readText(path, text);
		result = json{{"bytes", text.size()}};
	}
	if(!error && !image) try {
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
	result["name"] = name;
	followers.outcome->vars = json{{"result", std::move(result)}};
	followers.succeeded();
}

/// @brief An "openFile" step's "extensions": 1 to 16 of them, each 1 to 16 characters without a dot or a slash.
/// @param s The step.
/// @param image Whether it opens a picture: then "png", "jpg" and "jpeg" if it gives none.
/// @return Its extensions, or "json" if it gives none.
/// @throws DocumentError If they aren't such a list.
std::vector<std::string> extensionsOf(const Running& s, bool image) {
	if(!s.step.contains("extensions"))
		return image ? std::vector<std::string>{"png", "jpg", "jpeg"} : std::vector<std::string>{"json"};
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
/// path; with "comments": true, its comments and trailing commas are read too; with "as": "image", a PNG or JPEG
/// picture of at most 8 MB, as a data URI.
/// @param s The step.
/// @return true.
bool openFile(const Running& s) {
	s.steps.checkControl(s.where);
	const json::json_pointer into = s.target("openFile");
	std::string title = titleOf(s, "Open");
	bool image = false;
	if(s.step.contains("as")) {
		const std::string as = string(s.value("as"), s.where + ".as");
		if(as != "json" && as != "image") fail(s.where + ".as", R"(must be "json" or "image")");
		image = as == "image";
	}
	std::vector<std::string> extensions = extensionsOf(s, image);
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
	                        extensions = std::move(extensions), into, comments, image, followers = followersOf(s)] {
		if(!alive.lock() || !engine.window) return;
		const std::vector<std::string> chosen = engine.window->chooseFiles(
		    {FileDialogKind::Open, title, {{image ? "Pictures" : "JSON", extensions}}, "", "", false});
		if(!alive.lock() || chosen.empty()) return; // cancelled, or the screen went away meanwhile
		openChosen(engine, chosen.front(), into, comments, image, followers);
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
	std::vector<std::vector<std::uint8_t>> files;
	files.push_back(std::move(bytes));
	saveSeriesAfterEvent(s, std::move(files), std::move(name), std::move(filter), std::move(result));
}

void saveSeriesAfterEvent(const Running& s, std::vector<std::vector<std::uint8_t>> files, std::string name,
                          FileFilter filter, json result) {
	std::string title = titleOf(s, "Save");
	Engine& engine = s.steps.engine();
	if(!engine.window || dialogRefused(s)) return; // a check or a preview: nothing to show
	// The dialog is modal: it waits for the event to end, so the steps around it run first
	engine.scheduler.defer([&engine, alive = std::weak_ptr<bool>(engine.alive), title = std::move(title),
	                        files = std::move(files), name = std::move(name), filter = std::move(filter),
	                        result = std::move(result), followers = followersOf(s)]() mutable {
		if(!alive.lock() || !engine.window) return;
		const std::vector<std::string> chosen =
		    engine.window->chooseFiles({FileDialogKind::Save, title, {filter}, "", name, false});
		if(!alive.lock() || chosen.empty()) return; // cancelled, or the screen went away meanwhile
		json names = json::array();
		for(std::size_t i = 0; i < files.size(); ++i) {
			const std::string path = files.size() == 1 ? chosen.front() : numberedPath(chosen.front(), i + 1);
			if(const std::optional<std::string> error = writeBytes(path, files[i])) {
				engine.report("AffrescoUI screen: saving '" + fileNameOf(path) + "': " + *error);
				followers.outcome->vars = json{{"error", {{"code", "save"}, {"message", *error}}}};
				followers.failed();
				return;
			}
			names.push_back(fileNameOf(path));
		}
		result["name"] = names.front();
		if(files.size() > 1) result["files"] = std::move(names);
		followers.outcome->vars = json{{"result", std::move(result)}};
		followers.succeeded();
	});
}

void registerFileSteps(StepRegistry& registry) {
	registry.add({"file",
	              {{"saveFile", {"name", "title", "onDone", "onError"}, true},
	               {"openFile", {"extensions", "comments", "as", "title", "onDone", "onError"}, true}},
	              runFileStep});
}

} // namespace affresco::format
