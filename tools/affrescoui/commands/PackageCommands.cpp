/// @file
/// @brief The package commands (VERSION_CONTROL.md, "Sharing a version: packages"): `affrescoui pack` writes a version
/// of an app as one package, and `affrescoui inspect` checks one and says what it holds.

#include "Cli.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/PackageFiles.hpp>

#include <array>
#include <cstdlib>
#include <iostream>

#ifndef AFFRESCOUI_VERSION
#define AFFRESCOUI_VERSION "0.0.0"
#endif

namespace affresco::cli {

namespace {

/// @brief Opens an app with its history, keeping what the library logs meanwhile at Info and below out of the output.
/// @param app The app's folder or manifest.
/// @param data The data folder.
/// @param history Whether its history is read from the data folder.
/// @return The screen.
/// @throws DocumentError If it isn't an app.
Screen quietlyOpened(const std::filesystem::path& app, const std::filesystem::path& data, Screen::History history) {
	setLogSink([](LogLevel level, std::string_view message) {
		if(level != LogLevel::Debug && level != LogLevel::Info) std::cerr << message << '\n';
	});
	try {
		Screen screen = Screen::fromApp(app, data, history);
		setLogSink({});
		return screen;
	} catch(...) {
		setLogSink({});
		throw;
	}
}

/// @brief A list of names, or "none".
/// @param names The names.
/// @return "a, b" or "none".
std::string listed(const std::vector<std::string>& names) {
	if(names.empty()) return "none";
	std::string text;
	for(const std::string& name : names) text += (text.empty() ? "" : ", ") + name;
	return text;
}

/// @brief Prints a package's summary, one line per row.
/// @param summary packageSummary().
void printSummary(const json& summary) {
	const json& base = summary["base"];
	std::cout << "kind: " << summary["kind"].get<std::string>() << '\n'
	          << "name: " << summary["name"].get<std::string>() << ' ' << summary["release"].get<std::string>() << '\n';
	if(!summary["author"].get<std::string>().empty())
		std::cout << "author: " << summary["author"].get<std::string>() << " (as written: not a proof)\n";
	std::cout << "engine: AffrescoUI " << summary["engine"].get<std::string>() << '\n'
	          << "base: " << base["app"].get<std::string>() << " (" << base["name"].get<std::string>()
	          << (base["release"].get<std::string>().empty() ? "" : " " + base["release"].get<std::string>()) << ")";
	if(base["exact"].is_boolean())
		std::cout << (base["exact"].get<bool>() ? ", exactly the app you have"
		                                        : ", not the app you have (it was made from another release of it)");
	std::cout << '\n' << "bytes: " << summary["bytes"].get<std::size_t>() << '\n';
	std::cout << "files: " << summary["files"].get<std::size_t>() << '\n';
	std::vector<std::string> assets;
	for(const json& asset : summary["assets"])
		assets.push_back(asset["name"].get<std::string>() + " (" + asset["type"].get<std::string>() + ", " +
		                 std::to_string(asset["size"].get<std::size_t>()) + " bytes)");
	std::cout << "assets: " << listed(assets) << '\n';
	std::vector<std::string> keys;
	for(const auto& [key, unused] : summary["state"].items()) keys.push_back(key);
	std::cout << "state: " << listed(keys) << '\n';
	const json& definitions = summary["definitions"];
	std::cout << "definitions: " << definitions["added"].size() << " added, " << definitions["changed"].size()
	          << " changed, " << definitions["removed"].size() << " removed\n";
	std::cout << "signature: " << summary["signature"].get<std::string>() << '\n';
}

/// @brief The texts a package must never hold: the person's home folder, the data folder and the app's folder.
/// @param data The data folder.
/// @param app The app's folder or manifest.
/// @return Them, absolute.
std::vector<std::string> privateTexts(const std::filesystem::path& data, const std::filesystem::path& app) {
	std::vector<std::string> texts;
	const auto add = [&texts](const std::filesystem::path& path) {
		std::error_code error;
		const std::filesystem::path absolute = std::filesystem::absolute(path, error);
		if(!error) texts.push_back(absolute.lexically_normal().string());
		const std::filesystem::path real = std::filesystem::weakly_canonical(path, error);
		if(!error) texts.push_back(real.string());
	};
	for(const char* name : {"HOME", "USERPROFILE"})
		if(const char* home = std::getenv(name); home != nullptr && *home != '\0')
			add(home); // NOLINT(concurrency-mt-unsafe)
	add(data);
	add(std::filesystem::is_directory(app) ? app : app.parent_path());
	return texts;
}

} // namespace

Exit packVersion(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 7> valued = {"-o",       "--name", "--release", "--description",
	                                                           "--author", "--data", "--license"};
	static constexpr std::array<std::string_view, 1> flags = {"--whole"};
	const Arguments arguments(args, valued, flags);
	const std::filesystem::path app = arguments.file();
	const std::filesystem::path data = arguments.value("--data").value_or(Screen::defaultDataDirectory().string());

	format::PackageOptions options;
	options.kind = arguments.flag("--whole") ? format::PackageKind::App : format::PackageKind::Changes;
	options.name = arguments.value("--name").value_or("");
	options.release = arguments.value("--release").value_or("1.0.0");
	options.description = arguments.value("--description").value_or("");
	options.author = arguments.value("--author").value_or("");
	options.license = arguments.value("--license").value_or("");
	options.engine = AFFRESCOUI_VERSION;

	Screen screen = quietlyOpened(app, data, Screen::History::OnDisk);
	format::Package package;
	try {
		package = screen.exportPackage(options);
	} catch(const DocumentError& error) {
		throw FileProblem(error.what());
	}
	if(const auto problems = format::scrubProblems(package, privateTexts(data, app)); !problems.empty()) {
		for(const std::string& problem : problems) std::cout << "refused: " << problem << '\n';
		std::cout << "nothing written: take these out of the app, or out of its history, and pack it again\n";
		return Exit::Problems;
	}

	const std::string text = format::packageJson(package).dump(1, '\t') + "\n";
	const std::filesystem::path out = arguments.value("-o").value_or(package.base.app + ".affresco.json");
	writeFile(out, text);

	std::size_t records = 0;
	const data::Store& store = screen.data();
	for(const auto& [collection, unused] : store.models()) records += store.size(collection);
	std::cout << "wrote " << out.string() << " (" << text.size() << " bytes)\n";
	printSummary(format::packageSummary(package, json(), json::object()));
	std::cout << "left out: " << records << " records, the saved state, the history (" << screen.log().size()
	          << (screen.log().size() == 1 ? " commit)\n" : " commits)\n");
	return Exit::Success;
}

Exit inspectPackage(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 1> valued = {"--app"};
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	const Arguments arguments(args, valued, flags);
	const std::filesystem::path file = arguments.file();
	const std::string text = readFile(file);
	format::Package package;
	try {
		package = format::readPackage(text);
	} catch(const DocumentError& error) {
		std::cout << "refused: " << error.what() << '\n';
		return Exit::Problems;
	} catch(const json::parse_error& error) {
		std::cout << "refused: " << file.string() << ": not JSON (" << error.what() << ")\n";
		return Exit::Problems;
	}
	json installed;
	if(const auto app = arguments.value("--app")) {
		// the app as its files are: no data folder, so neither its history nor its state is read
		Screen screen = quietlyOpened(*app, {}, Screen::History::InMemory);
		installed = screen.versions().sourceDocument();
	}
	const json summary = format::packageSummary(package, installed, json::object());
	if(arguments.flag("--json")) std::cout << summary.dump(1, '\t') << '\n';
	else printSummary(summary);
	return Exit::Success;
}

} // namespace affresco::cli
