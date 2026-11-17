/// @file
/// @brief The `packages` service (PackagesService.cpp): an app the person changed exported as one package, reviewed
/// and installed as a new app, every refusal leaving the writable root as it was and naming roots, never paths.

#include "packages/PackagesService.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Package.hpp>
#include <affresco/services/Schema.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <utility>

using affresco::json;
namespace fs = std::filesystem;
namespace detail = affresco::detail;

namespace {

/// @brief Writes a file, making its folder.
/// @param path The file.
/// @param text What it holds.
void write(const fs::path& path, const std::string& text) {
	fs::create_directories(path.parent_path());
	std::ofstream(path, std::ios::binary) << text;
}

/// @brief Reads a whole file.
/// @param path The file.
/// @return Its text, "" if there is none.
std::string read(const fs::path& path) {
	std::ifstream in(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// @brief A fresh folder for a test.
/// @param name Its name.
/// @return It, empty.
fs::path freshFolder(const std::string& name) {
	const fs::path root = fs::temp_directory_path() / name;
	fs::remove_all(root);
	fs::create_directories(root);
	return fs::canonical(root);
}

/// @brief Writes an app "notes" of one label: generated test data only.
/// @param folder Its folder.
void writeNotes(const fs::path& folder) {
	write(folder / "app.json", R"({ "format": "affrescoui-app", "version": 1, "id": "notes", "name": "Notes",
		"release": "1.0.0", "entry": "index.screen.json" })");
	write(folder / "index.screen.json", R"json({ "format": "affrescoui-screen", "version": 2,
		"window": { "title": "Notes" },
		"state": { "title": "Notes" },
		"program": { "app.lisp": { "count": "(defn count [] 1)" } },
		"root": { "type": "label", "id": "title", "text": "{{state.title}}" } })json");
	write(folder / "build.cpp", "// left out of every copy\n");
}

/// @brief The folders of a test: examples (notes), the person's apps (a copy of notes) and the host's data.
struct Folders {
	fs::path top = {};      ///< All of them.
	fs::path examples = {}; ///< The examples' root.
	fs::path apps = {};     ///< The person's apps' root, writable.
	fs::path data = {};     ///< The host's data folder.

	/// @brief The grant the Launcher gets.
	/// @return It.
	[[nodiscard]] affresco::PackagesGrant grant() const {
		affresco::PackagesGrant grant;
		grant.roots = {{"examples", examples}, {"apps", apps}};
		grant.writable = "apps";
		grant.dataDirectory = data;
		return grant;
	}
};

/// @brief Makes the folders: the person's copy of notes changed by one commit, its window closed.
/// @param name The test's folder.
/// @return Them.
Folders changedCopy(const std::string& name) {
	Folders folders;
	folders.top = freshFolder(name);
	folders.examples = folders.top / "examples";
	folders.apps = folders.top / "apps";
	folders.data = folders.top / "data";
	writeNotes(folders.examples / "notes");
	writeNotes(folders.apps / "my-notes");
	affresco::Screen screen =
	    affresco::Screen::fromApp(folders.apps / "my-notes", folders.data, affresco::Screen::History::OnDisk);
	const auto error = screen.versions().commit(
	    {{"author", "user"},
		 {"message", "Count two"},
		 {"patch", {{{"op", "replace"}, {"path", "/program/app.lisp/count"}, {"value", "(defn count [] 2)"}}}}});
	REQUIRE_MESSAGE(!error, error.value_or(""));
	return folders;
}

/// @brief The failure a call of the service ends with.
/// @param work The call.
/// @return Its code and message, or two empty texts if it succeeded.
std::pair<std::string, std::string> failure(const std::function<void()>& work) {
	try {
		work();
	} catch(const detail::PackagesFailure& refused) {
		return {refused.code(), refused.what()};
	}
	return {};
}

/// @brief The arguments of an export of the person's copy.
/// @return Them.
json exportArguments() {
	return {{"root", "apps"}, {"app", "my-notes"}, {"name", "Notes that count two"}, {"description", "Two."}};
}

/// @brief The schema of a method of the service.
/// @param method Its name.
/// @return Its arguments' schema.
json argumentsOf(const std::string& method) {
	const auto provider = affresco::makePackagesProvider({});
	for(const affresco::services::MethodSpec& spec : provider->methods(json::object()))
		if(spec.name == method) return spec.arguments;
	FAIL("no method " << method);
	return {};
}

} // namespace

TEST_CASE("packages service: a changed copy exports as its changes, and installs as a proposal on a new app") {
	const Folders folders = changedCopy("affrescoui-packages-service-changes");
	const affresco::PackagesGrant grant = folders.grant();
	const json exported = detail::exportVersion(grant, exportArguments(), {});
	CHECK(exported["package"]["kind"] == "changes");
	CHECK(exported["package"]["base"]["app"] == "notes");
	CHECK(exported["summary"]["name"] == "Notes that count two");
	CHECK(exported["leftOut"]["commits"].get<int>() >= 1);
	CHECK(exported["leftOut"]["records"] == 0);

	const json reviewed = detail::inspectVersion(grant, {{"package", exported["package"]}});
	CHECK(reviewed["summary"]["problems"].empty());

	const json installed =
	    detail::installVersion(grant, {{"package", exported["package"]}, {"id", "counting"}, {"name", "Counting"}}, {});
	CHECK(installed == json{{"id", "counting"}, {"sub", ""}, {"proposal", true}});
	CHECK(fs::exists(folders.apps / "counting" / "index.screen.json"));
	CHECK(!fs::exists(folders.apps / "counting" / "build.cpp")); // a copy leaves the C++ out
	const json manifest = json::parse(read(folders.apps / "counting" / "app.json"));
	CHECK(manifest["name"] == "Counting");
	CHECK(manifest["id"] == "notes"); // the package's app: its changes apply
	CHECK(json::parse(read(folders.apps / "counting" / "index.screen.json"))["window"]["title"] == "Counting");
	// The change waits for the person: the new app's files are the base's until it is accepted
	const affresco::Screen opened =
	    affresco::Screen::fromApp(folders.apps / "counting", folders.data, affresco::Screen::History::OnDisk);
	CHECK(opened.proposals().size() == 1);

	const std::string all = exported.dump() + reviewed.dump() + installed.dump();
	CHECK(all.find(folders.top.string()) == std::string::npos);
}

TEST_CASE("packages service: a whole app, exported and installed from its own files") {
	const Folders folders = changedCopy("affrescoui-packages-service-whole");
	const affresco::PackagesGrant grant = folders.grant();
	json arguments = exportArguments();
	arguments["whole"] = true;
	const json exported = detail::exportVersion(grant, arguments, {});
	CHECK(exported["package"]["kind"] == "app");
	const json reviewed = detail::inspectVersion(grant, {{"package", exported["package"]}});
	CHECK(reviewed["summary"]["problems"].empty());
	const json installed =
	    detail::installVersion(grant, {{"package", exported["package"]}, {"id", "whole"}, {"name", "Whole"}}, {});
	CHECK(installed["id"] == "whole");
	CHECK(installed["sub"] == "/json");
	CHECK(json::parse(read(folders.apps / "whole" / "json" / "app.json"))["name"] == "Whole");

	// An app no one changed is its own files: installed with no proposal waiting
	const json plain =
	    detail::exportVersion(grant, {{"root", "examples"}, {"app", "notes"}, {"name", "Plain"}, {"whole", true}}, {});
	const json same =
	    detail::installVersion(grant, {{"package", plain["package"]}, {"id", "plain"}, {"name", "Plain"}}, {});
	CHECK(same == json{{"id", "plain"}, {"sub", "/json"}, {"proposal", false}});
	CHECK(json::parse(read(folders.apps / "plain" / "json" / "app.json"))["name"] == "Plain");

	// Files that aren't the base it describes, a definition hidden in them alone, aren't what inspecting it shows
	json hidden = plain["package"];
	for(auto& [path, file] : hidden["base"]["files"].items())
		if(path.ends_with(".screen.json")) {
			json screen = json::parse(file["text"].get<std::string>());
			screen["program"]["app.lisp"]["hidden"] = "(defn hidden [] 1)";
			file["text"] = screen.dump();
		}
	const json shown = detail::inspectVersion(grant, {{"package", hidden}});
	CHECK(shown["summary"]["problems"].dump().contains("aren't the base it describes"));
	const auto refused = failure([&] {
		static_cast<void>(detail::installVersion(grant, {{"package", hidden}, {"id", "hidden"}, {"name", "H"}}, {}));
	});
	CHECK(refused.first == "refused");
	CHECK(refused.second.contains("aren't the base it describes"));
	CHECK(!fs::exists(folders.apps / "hidden"));

	// Without the app it was made on in another root, a copy's changes are the whole app
	fs::remove_all(folders.examples / "notes");
	CHECK(detail::exportVersion(grant, exportArguments(), {})["package"]["kind"] == "app");
}

TEST_CASE("packages service: an existing id, a path or a stopped install leaves the writable root as it was") {
	const Folders folders = changedCopy("affrescoui-packages-service-refused");
	const affresco::PackagesGrant grant = folders.grant();
	const json package = detail::exportVersion(grant, exportArguments(), {})["package"];
	write(folders.apps / "taken" / "note.txt", "mine");

	const auto exists = failure([&] {
		static_cast<void>(detail::installVersion(grant, {{"package", package}, {"id", "taken"}, {"name", "T"}}, {}));
	});
	CHECK(exists.first == "exists");
	CHECK(read(folders.apps / "taken" / "note.txt") == "mine");
	CHECK(!fs::exists(folders.apps / "taken" / "app.json"));

	for(const char* path : {"../evil", "/tmp/evil", "a/b", "Evil", "-x", "shared", "launcher"}) {
		CAPTURE(path);
		const auto refused = failure([&] {
			static_cast<void>(detail::installVersion(grant, {{"package", package}, {"id", path}, {"name", "E"}}, {}));
		});
		CHECK(refused.first == "invalid");
		CHECK(!fs::exists(folders.apps / path));
	}
	CHECK(failure([&] {
		      json arguments = exportArguments();
		      arguments["app"] = "../examples/notes";
		      static_cast<void>(detail::exportVersion(grant, arguments, {}));
	      }).first == "invalid");
	CHECK(failure([&] {
		      json arguments = exportArguments();
		      arguments["root"] = folders.apps.string();
		      static_cast<void>(detail::exportVersion(grant, arguments, {}));
	      }).first == "invalid");
	// The schema takes no other argument: no path rides along
	json withPath = exportArguments();
	withPath["path"] = folders.apps.string();
	CHECK(affresco::services::checkSchema(argumentsOf("export"), withPath, "with").has_value());

	std::stop_source stop;
	stop.request_stop();
	const auto stopped = failure([&] {
		static_cast<void>(
		    detail::installVersion(grant, {{"package", package}, {"id", "stopped"}, {"name", "S"}}, stop.get_token()));
	});
	CHECK(stopped.first == "failed");
	CHECK(!fs::exists(folders.apps / "stopped"));

	// A package whose app no root has can't be installed as its changes: nothing is made
	json orphan = package;
	orphan["base"]["app"] = "nowhere";
	const auto missing = failure([&] {
		static_cast<void>(detail::installVersion(grant, {{"package", orphan}, {"id", "orphan"}, {"name", "O"}}, {}));
	});
	CHECK(!missing.first.empty());
	CHECK(!fs::exists(folders.apps / "orphan"));

	for(const auto& [code, message] : {exists, stopped, missing}) {
		CAPTURE(message);
		CHECK(message.find(folders.top.string()) == std::string::npos);
	}
}

TEST_CASE("packages service: a package larger than its limit is refused before it is read") {
	const Folders folders = changedCopy("affrescoui-packages-service-large");
	const json large = {{"format", "affrescoui-package"},
	                    {"padding", std::string(affresco::format::maxPackageBytes + 1, 'x')}};
	const auto inspected =
	    failure([&] { static_cast<void>(detail::inspectVersion(folders.grant(), {{"package", large}})); });
	CHECK(inspected.first == "tooLarge");
	const auto installed = failure([&] {
		static_cast<void>(
		    detail::installVersion(folders.grant(), {{"package", large}, {"id", "large"}, {"name", "L"}}, {}));
	});
	CHECK(installed.first == "tooLarge");
	CHECK(!fs::exists(folders.apps / "large"));
	CHECK(failure([&] {
		      static_cast<void>(detail::inspectVersion(folders.grant(), {{"package", {{"a", 1}}}}));
	      }).first == "invalid");
}

TEST_CASE("packages service: an app that isn't one, and a grant with nowhere to install") {
	const Folders folders = changedCopy("affrescoui-packages-service-none");
	write(folders.apps / "blank" / "index.screen.json", R"({ "format": "affrescoui-screen", "version": 2 })");
	json arguments = exportArguments();
	arguments["app"] = "blank";
	const auto blank = failure([&] { static_cast<void>(detail::exportVersion(folders.grant(), arguments, {})); });
	CHECK(blank.first == "notFound");
	CHECK(blank.second == "apps:blank isn't an app: it has no app.json");

	affresco::PackagesGrant readOnly = folders.grant();
	readOnly.writable.clear();
	const json package = detail::exportVersion(readOnly, exportArguments(), {})["package"];
	CHECK(failure([&] {
		      static_cast<void>(
		          detail::installVersion(readOnly, {{"package", package}, {"id", "x"}, {"name", "X"}}, {}));
	      }).first == "refused");
}
