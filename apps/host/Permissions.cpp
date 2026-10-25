/// @file
/// @brief What the person lets an app use: the decisions kept per app, the window that asks, and the grants applied.

#include "Permissions.hpp"

#include <affresco/format/Locale.hpp>
#include <affresco/services/Grants.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <utility>

namespace host {

namespace {

using affresco::json;
using affresco::format::AppCapability;

/// The longest grants file read, in bytes: far more than 4 kinds of 64 capabilities need.
constexpr std::size_t maxGrantsBytes = 256 * 1024;

/// @brief The name the prompt screen gives a capability's kind (its "permissions.<kind>" string).
/// @param capability The capability.
/// @return "network", "folderRead", "folderWrite", "program", "programAny" (a shell or an interpreter), "audio",
/// "assistant", "files", "windows" or "channels".
std::string kindName(const AppCapability& capability) {
	switch(capability.kind) {
	case AppCapability::Kind::Network:
		return "network";
	case AppCapability::Kind::Folder:
		return capability.write ? "folderWrite" : "folderRead";
	case AppCapability::Kind::Program:
		return capability.runsAnyCommand() ? "programAny" : "program";
	case AppCapability::Kind::Audio:
		return "audio";
	case AppCapability::Kind::Assistant:
		return "assistant";
	case AppCapability::Kind::Files:
		return "files";
	case AppCapability::Kind::Windows:
		return "windows";
	case AppCapability::Kind::Channels:
		return "channels";
	}
	return "audio";
}

/// @brief What the prompt screen shows: the app's name and each capability asked, checked only if it starts allowed
/// (AppCapability::startsAllowed()): a program, a folder written, an origin, the assistant and the channels take a
/// click.
/// @param app The app.
/// @param asked The capabilities asked.
/// @return `{"name", "capabilities": [{"key", "kind", "name", "why", "allowed"}]}`.
json describe(const affresco::format::AppManifest& app, const std::vector<AppCapability>& asked) {
	json capabilities = json::array();
	for(const AppCapability& capability : asked)
		capabilities.push_back({{"key", capability.key()},
		                        {"kind", kindName(capability)},
		                        {"name", capability.name},
		                        {"why", capability.why},
		                        {"allowed", capability.startsAllowed()}});
	return {{"name", app.name}, {"capabilities", std::move(capabilities)}};
}

} // namespace

std::filesystem::path grantsFile(const std::filesystem::path& dataDirectory, std::string_view id,
                                 const std::filesystem::path& location) {
	return affresco::Screen::appDataDirectory(dataDirectory, id, location) / "grants.json";
}

Decisions loadDecisions(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if(!in) return {};
	std::string text(maxGrantsBytes + 1, '\0');
	in.read(text.data(), static_cast<std::streamsize>(text.size()));
	text.resize(static_cast<std::size_t>(in.gcount()));
	const json read = text.size() > maxGrantsBytes ? json() : json::parse(text, nullptr, false);
	if(!read.is_object() || !read.contains("decisions") || !read["decisions"].is_object()) {
		std::cerr << "affrescoui_launcher: " << file.string() << " is damaged: the app's permissions are asked again\n";
		return {};
	}
	Decisions decisions;
	for(const auto& [key, allowed] : read["decisions"].items())
		if(allowed.is_boolean()) decisions.emplace(key, allowed.get<bool>());
	return decisions;
}

void saveDecisions(const std::filesystem::path& file, const Decisions& decisions) {
	std::filesystem::create_directories(file.parent_path());
	json written = {{"format", "affrescoui-grants"}, {"version", 1}, {"decisions", json::object()}};
	for(const auto& [key, allowed] : decisions) written["decisions"][key] = allowed;
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	out << written.dump(1, '\t') << '\n';
	if(!out) throw std::runtime_error("could not write " + file.string());
}

Decisions askPerson(const affresco::format::AppManifest& app, const std::vector<AppCapability>& asked,
                    const std::filesystem::path& prompt) {
	affresco::Application application;
	std::ifstream in(prompt, std::ios::binary);
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if(!in) throw std::runtime_error("could not open the permissions' screen " + prompt.string());
	affresco::Screen screen = affresco::Screen::fromString(text, prompt.parent_path());
	screen.setLocale(affresco::format::systemLocale());
	Decisions decided;
	screen.onCall("permissions.app",
	              [described = describe(app, asked)](const std::string&) { return described.dump(); });
	screen.onCall("permissions.decide", [&](const std::string& argumentsJson) {
		const json arguments = json::parse(argumentsJson, nullptr, false);
		const json allowed = arguments.is_object() ? arguments.value("allowed", json::array()) : json::array();
		for(const AppCapability& capability : asked)
			decided[capability.key()] =
			    allowed.is_array() && std::ranges::find(allowed, json(capability.key())) != allowed.end();
		application.quit();
		return std::string("null");
	});
	static_cast<void>(screen.createWindow(application));
	application.run();
	return decided;
}

void applyGrants(affresco::Screen& screen, const affresco::format::AppManifest& app, const Allowed& allowed,
                 bool silent) {
	screen.allowFileDialogs(false); // a library screen may, an app only once the person allows it
	screen.allowOtherWindows(false);
	screen.allowChannels(false);
	for(const AppCapability& capability : app.capabilities) {
		if(!allowed.contains(capability.key())) continue;
		std::optional<std::string> refused;
		switch(capability.kind) {
		case AppCapability::Kind::Network:
			refused = screen.network().allow(capability.name);
			break;
		case AppCapability::Kind::Folder:
			screen.services().allowChosenRoot(capability.name, !capability.write);
			break;
		case AppCapability::Kind::Program:
			refused = affresco::services::grantProgram(screen.services(), capability.name);
			break;
		case AppCapability::Kind::Audio:
			if(!silent) static_cast<void>(screen.audio()); // nullptr in a build without audio: its steps then say so
			break;
		case AppCapability::Kind::Assistant: // the host attaches its agent (host.cpp)
			break;
		case AppCapability::Kind::Files:
			screen.allowFileDialogs(true);
			break;
		case AppCapability::Kind::Windows:
			screen.allowOtherWindows(true);
			break;
		case AppCapability::Kind::Channels:
			screen.allowChannels(true);
			break;
		}
		if(refused)
			std::cerr << "affrescoui_launcher: " << capability.key() << " allowed, but not given: " << *refused << '\n';
	}
}

} // namespace host
