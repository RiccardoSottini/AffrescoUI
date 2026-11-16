/// @file
/// @brief The package commands (VERSION_CONTROL.md, "Sharing a version: packages"): `affrescoui pack` writes a version
/// of an app as one package, `affrescoui inspect` checks one and says what it holds, `affrescoui install` proposes one
/// to the person, and `affrescoui trust` keeps the publishers they trust.

#include "Cli.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/PackageFiles.hpp>
#include <affresco/format/PackageInstall.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <optional>

#ifndef AFFRESCOUI_VERSION
#define AFFRESCOUI_VERSION "0.0.0"
#endif

namespace affresco::cli {

namespace {

/// @brief Opens an app with its history, keeping what the library logs meanwhile at Info and below out of the output.
/// @param app The app's folder or manifest.
/// @param data The data folder.
/// @param history Whether its history is read from the data folder.
/// @param warnings Receives the warnings instead of the output (may be nullptr).
/// @return The screen.
/// @throws DocumentError If it isn't an app.
Screen quietlyOpened(const std::filesystem::path& app, const std::filesystem::path& data, Screen::History history,
                     std::vector<std::string>* warnings = nullptr) {
	setLogSink([warnings](LogLevel level, std::string_view message) {
		if(level == LogLevel::Debug || level == LogLevel::Info) return;
		if(warnings) warnings->emplace_back(message);
		else std::cerr << message << '\n';
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
	if(base.contains("installedRelease") && base["installedRelease"].is_string())
		std::cout << ", you have " << base["installedRelease"].get<std::string>();
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

/// @brief A summary entry's field as text: a string as it is, a missing or null one as "?", anything else as JSON.
/// @param entry The entry.
/// @param name The field.
/// @return It.
std::string field(const json& entry, const char* name) {
	const auto found = entry.find(name);
	if(found == entry.end() || found->is_null()) return "?";
	return found->is_string() ? found->get<std::string>() : found->dump();
}

/// @brief Prints a list of a summary's entries, one line each, or "none".
/// @param title What they are.
/// @param entries The entries.
/// @param line One entry's line.
void printList(std::string_view title, const json& entries, const std::function<std::string(const json&)>& line) {
	std::cout << title << ':' << (entries.empty() ? " none" : "") << '\n';
	for(const json& entry : entries) std::cout << "  " << line(entry) << '\n';
}

/// @brief Prints what installing a package would do: the parts of its summary packageSummary() works out for it.
/// @param summary packageSummary(), with a person's copy.
void printInstallParts(const json& summary) {
	std::vector<std::string> layers;
	for(const auto& [layer, count] : summary["impact"].items())
		if(count.is_number() && count.get<int>() > 0) layers.push_back(layer + " " + std::to_string(count.get<int>()));
	std::cout << "impact: " << listed(layers) << '\n';
	const auto extension = [](const json& entry) {
		return field(entry, "kind") + " " + field(entry, "name") + " (on " + field(entry, "target") + ")";
	};
	printList("extensions added", summary["extensions"]["added"], extension);
	printList("extensions taken away", summary["extensions"]["removed"], extension);
	printList("writes", summary["writes"], [](const json& entry) {
		return field(entry, "step") + " " + field(entry, "collection") + ", in " + field(entry, "in");
	});
	printList("uses", summary["uses"], [](const json& entry) {
		return field(entry, "step") + " " + field(entry, "use") + ", in " + field(entry, "in");
	});
	printList("models", summary["models"], [](const json& entry) {
		return field(entry, "collection") + ": version " + field(entry, "from") + " to " + field(entry, "to") + ", " +
		       std::to_string(entry.value("migrations", json::array()).size()) + " migrations";
	});
	if(summary.contains("capabilities"))
		printList("capabilities", summary["capabilities"], [](const json& entry) {
			return field(entry, "capability") + (entry.value("added", false) ? " (new to you)" : "") + ": " +
			       field(entry, "why");
		});
	if(summary.contains("permissions"))
		printList("permissions", summary["permissions"], [](const json& entry) { return entry.get<std::string>(); });
	if(summary.contains("seed"))
		printList("seed data", summary["seed"], [](const json& entry) {
			return field(entry, "collection") + ": " + field(entry, "file") + ", " + field(entry, "records") +
			       " records";
		});
	printList("problems", summary["problems"], [](const json& entry) { return entry.get<std::string>(); });
	if(summary["conflicts"].is_array())
		printList("not merged (you choose)", summary["conflicts"],
		          [](const json& entry) { return field(entry, "path") + ": " + entry.value("reason", ""); });
}

/// @brief The trusted publishers' file of a data folder.
/// @param arguments The command line (`--data`).
/// @return `<data folder>/publishers.json`.
std::filesystem::path publishersFile(const Arguments& arguments) {
	return std::filesystem::path(arguments.value("--data").value_or(Screen::defaultDataDirectory().string())) /
	       "publishers.json";
}

/// @brief Reads a package's file, saying why it is refused.
/// @param file The file.
/// @param text Receives its text.
/// @return The package, or std::nullopt if it is refused (said on the output).
std::optional<format::Package> readPackageFile(const std::filesystem::path& file, std::string& text) {
	text = readFile(file);
	try {
		return format::readPackage(text);
	} catch(const DocumentError& error) {
		std::cout << "refused: " << error.what() << '\n';
	} catch(const json::parse_error& error) {
		std::cout << "refused: " << file.string() << ": not JSON (" << error.what() << ")\n";
	}
	return std::nullopt;
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
	static constexpr std::array<std::string_view, 2> valued = {"--app", "--data"};
	static constexpr std::array<std::string_view, 1> flags = {"--json"};
	const Arguments arguments(args, valued, flags);
	std::string text;
	const std::optional<format::Package> package = readPackageFile(arguments.file(), text);
	if(!package) return Exit::Problems;
	const json trusted = format::readTrustedPublishers(publishersFile(arguments));
	json summary;
	if(const auto app = arguments.value("--app")) {
		// the person's copy as its history has it: what doesn't merge with it is listed
		const Screen screen = quietlyOpened(
		    *app, arguments.value("--data").value_or(Screen::defaultDataDirectory().string()), Screen::History::OnDisk);
		summary = screen.inspectPackage(text, trusted);
	} else {
		summary = format::packageSummary(*package, format::PackageReceiver{}, trusted);
	}
	if(arguments.flag("--json")) std::cout << summary.dump(1, '\t') << '\n';
	else {
		printSummary(summary);
		printInstallParts(summary);
	}
	return summary["problems"].empty() ? Exit::Success : Exit::Problems;
}

namespace {

/// @brief `install`'s app: the one named, or a new folder the package's files are written into (`--new`).
/// @param arguments The command line.
/// @param package The package.
/// @return The app's folder or manifest, or std::nullopt if its files were refused (said on the output).
/// @throws UsageError If the command line is wrong.
std::optional<std::filesystem::path> installTarget(const Arguments& arguments, const format::Package& package) {
	const auto folder = arguments.value("--new");
	if(arguments.positional().size() != (folder ? 1U : 2U))
		throw UsageError(folder ? "expected a package file" : "expected an app and a package file");
	if(!folder) return std::filesystem::path(arguments.positional()[0]);
	try {
		format::installAppFiles(package, *folder);
	} catch(const DocumentError& error) {
		std::cout << "refused: " << error.what() << '\n';
		return std::nullopt;
	}
	std::cout << "wrote the app as the package's base is into " << *folder << '\n';
	return std::filesystem::path(*folder);
}

} // namespace

Exit installPackage(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 2> valued = {"--data", "--new"};
	const Arguments arguments(args, valued, {});
	if(arguments.positional().empty()) throw UsageError("expected a package file");
	std::string text;
	const std::optional<format::Package> package = readPackageFile(arguments.positional().back(), text);
	if(!package) return Exit::Problems;
	const json trusted = format::readTrustedPublishers(publishersFile(arguments));
	const std::optional<std::filesystem::path> app = installTarget(arguments, *package);
	if(!app) return Exit::Problems;

	std::vector<std::string> warnings;
	Screen screen = quietlyOpened(*app, arguments.value("--data").value_or(Screen::defaultDataDirectory().string()),
	                              Screen::History::OnDisk, &warnings);
	for(const std::string& warning : warnings) std::cerr << warning << '\n';
	if(std::ranges::any_of(warnings, [](const std::string& w) { return w.contains("kept in memory only"); }))
		throw std::runtime_error("another program writes this app's history (is a host showing it?): close it first");
	const json summary = screen.inspectPackage(text, trusted);
	printSummary(summary);
	printInstallParts(summary);
	std::string error;
	const RevisionResult result = screen.proposePackage(text, &error);
	if(result.status != RevisionStatus::Proposed) {
		std::cout << "refused: " << error << '\n';
		return Exit::Problems;
	}
	std::cout << "proposed: " << summary["name"].get<std::string>() << " " << summary["release"].get<std::string>()
	          << " as proposal " << result.proposal << ", which only you can accept: open the app to review it\n";
	return Exit::Success;
}

Exit trustPublisher(std::span<const std::string_view> args) {
	static constexpr std::array<std::string_view, 2> valued = {"--name", "--data"};
	static constexpr std::array<std::string_view, 2> flags = {"--remove", "--list"};
	const Arguments arguments(args, valued, flags);
	const std::filesystem::path path = publishersFile(arguments);
	json publishers = format::readTrustedPublishers(path);
	if(arguments.flag("--list")) {
		if(publishers.empty()) std::cout << "no trusted publishers\n";
		for(const auto& [key, name] : publishers.items()) std::cout << key << "  " << name.get<std::string>() << '\n';
		return Exit::Success;
	}
	if(arguments.positional().size() != 1) throw UsageError("expected a publisher's key (ed25519:...)");
	const std::string key = arguments.positional()[0];
	std::string said;
	if(arguments.flag("--remove")) {
		said = (publishers.erase(key) != 0 ? "no longer trusted: " : "wasn't trusted: ") + key;
	} else {
		const std::string prefix = "ed25519:";
		publishers[key] =
		    arguments.value("--name").value_or(key.substr(key.starts_with(prefix) ? prefix.size() : 0, 16));
		said = "trusted: " + publishers[key].get<std::string>() + " (" + key + ")";
	}
	try {
		const std::string text = publishers.dump(1, '\t') + "\n";
		(void)format::parseTrustedPublishers(text); // never write what wouldn't be read back
		std::filesystem::create_directories(path.parent_path());
		writeFile(path, text);
	} catch(const DocumentError& error) {
		throw UsageError(error.what());
	}
	std::cout << said << '\n';
	return Exit::Success;
}

} // namespace affresco::cli
