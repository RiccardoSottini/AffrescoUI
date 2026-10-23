/// @file
/// @brief What a host grants from its command line (Grants.hpp): the git policy's commands and branch names, programs
/// by spec, and a root the person chooses, which the services that use it wait for and then start on.

#include <affresco/services/Grants.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using affresco::json;
using affresco::services::checkGitArguments;
using affresco::services::isBranchName;
using affresco::services::isFolderPath;

namespace {

/// @brief A folder of its own under the system's temporary folder, removed at the end.
struct TemporaryFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "affrescoui-grants-test"; ///< The folder.
	TemporaryFolder() {
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path / "chosen");
	}
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	TemporaryFolder(TemporaryFolder&&) = delete;
	TemporaryFolder& operator=(TemporaryFolder&&) = delete;
};

/// @brief A folder as the chooser gives it: UTF-8.
/// @param path The folder.
/// @return Its text.
std::string utf8(const std::filesystem::path& path) {
	const std::u8string text = path.u8string();
	return {text.begin(), text.end()};
}

} // namespace

TEST_CASE("grants: git runs only the commands a source control view needs") {
	CHECK_FALSE(checkGitArguments({"init"}));
	CHECK_FALSE(checkGitArguments({"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"}));
	CHECK_FALSE(checkGitArguments({"add", "--", "src/a.cpp", "."}));
	CHECK_FALSE(checkGitArguments({"restore", "--staged", "--", "-odd name"})); // after "--", never an option
	CHECK_FALSE(checkGitArguments({"restore", "--", "src/a.cpp"}));
	CHECK_FALSE(checkGitArguments({"commit", "-F", "-"}));
	CHECK_FALSE(checkGitArguments({"switch", "-c", "feature/scm-view"}));
	CHECK_FALSE(checkGitArguments({"switch", "main"}));
	CHECK_FALSE(checkGitArguments({"show", "--no-textconv", "HEAD:src/a.cpp"}));
	CHECK_FALSE(checkGitArguments({"for-each-ref", "--format=%(HEAD)%00%(refname:short)", "refs/heads/"}));
	CHECK_FALSE(checkGitArguments({"log", "--format=%H%x00%an%x00%at%x00%s", "-n", "50"}));
	CHECK_FALSE(checkGitArguments({"log", "--format=%H%x00%an%x00%at%x00%s", "-n", "1000", "--", "src/a.cpp"}));

	CHECK(checkGitArguments({}));
	CHECK(checkGitArguments({"-c", "core.pager=sh", "status"}));
	CHECK(checkGitArguments({"init", "--template=/tmp/hooks"}));
	CHECK(checkGitArguments({"commit", "-m", "message"}));
	CHECK(checkGitArguments({"add", "src/a.cpp"}));
	CHECK(checkGitArguments({"add", "--", "../outside"}));
	CHECK(checkGitArguments({"add", "--", "/etc/passwd"}));
	CHECK(checkGitArguments({"add", "--"}));
	CHECK(checkGitArguments({"switch", "--orphan", "x"}));
	CHECK(checkGitArguments({"switch", "-c", "--upload-pack=x"}));
	CHECK(checkGitArguments({"show", "--no-textconv", "HEAD:../x"}));
	CHECK(checkGitArguments({"show", "HEAD:src/a.cpp"}));
	CHECK(checkGitArguments({"log", "--format=%H%x00%an%x00%at%x00%s", "-n", "0"}));
	CHECK(checkGitArguments({"log", "--format=%H%x00%an%x00%at%x00%s", "-n", "1001"}));
	CHECK(checkGitArguments({"log", "--format=%H%x00%an%x00%at%x00%s", "-n", "5", "--", "../x"}));
	CHECK(checkGitArguments({"log", "--format=%H", "-n", "5"}));
	CHECK(checkGitArguments({"push"}));
}

TEST_CASE("grants: branch names and paths inside a folder") {
	CHECK(isBranchName("main"));
	CHECK(isBranchName("feature/scm-view_2"));
	for(const char* name :
	    {"", "-x", "a..b", "a/", "/a", ".a", "a/.b", "a.lock", "a b", "a~1", "a^", "a:b", "HEAD", "a@{1}", "a//b"})
		CHECK_FALSE(isBranchName(name));
	CHECK(isFolderPath("src/a.cpp"));
	CHECK(isFolderPath("."));
	for(const char* path : {"", "/etc", "../x", "a/../../x", "a\\b", "a\nb"}) CHECK_FALSE(isFolderPath(path));
	CHECK_FALSE(isFolderPath(std::string(4097, 'a')));
}

TEST_CASE("grants: the git grant runs on the declaration's own repository, never one above it") {
	const affresco::services::ProcessShaper shape = affresco::services::gitShaper();
	affresco::services::ProcessRun run{{"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"},
	                                   std::filesystem::path("/work/project")};
	REQUIRE_FALSE(shape(run));
	REQUIRE(run.arguments.size() == 7);
	CHECK(run.arguments[0] == "--git-dir=/work/project/.git");
	CHECK(run.arguments[1] == "--work-tree=/work/project");
	CHECK(run.arguments[2] == "status");

	affresco::services::ProcessRun init{{"init"}, std::filesystem::path("/work/project")};
	REQUIRE_FALSE(shape(init));
	CHECK(init.arguments == std::vector<std::string>{"init"});

	affresco::services::ProcessRun nowhere{{"status", "--porcelain=v2", "-z", "--branch", "--untracked-files=all"}, {}};
	CHECK(shape(nowhere)); // no root: refused, rather than whatever repository the host runs in
	affresco::services::ProcessRun pushed{{"push"}, std::filesystem::path("/work/project")};
	CHECK(shape(pushed));
}

TEST_CASE("grants: programs by spec") {
	affresco::services::ServiceRegistry registry;
	CHECK_FALSE(affresco::services::grantProgram(registry, "shell"));
	CHECK_FALSE(affresco::services::grantProgram(registry, "listing=/bin/ls -a -l"));
	CHECK(affresco::services::loginShell().is_absolute());
	CHECK(affresco::services::findProgram("sh").is_absolute());
	CHECK(affresco::services::findProgram("no-such-program-affrescoui").empty());
	CHECK(affresco::services::findProgram("../sh").empty()); // a name only, never a path

	CHECK(affresco::services::grantProgram(registry, ""));
	CHECK(affresco::services::grantProgram(registry, "bad name"));
	CHECK(affresco::services::grantProgram(registry, "x=relative/tool"));
	CHECK(affresco::services::grantProgram(registry, "x="));
	CHECK(affresco::services::grantProgram(registry, "no-such-program-affrescoui"));
	std::string many = "x=/bin/ls";
	for(int i = 0; i < 40; ++i) many += " a";
	CHECK(affresco::services::grantProgram(registry, many));
	CHECK(affresco::services::grantProgram(registry, "x=/bin/ls " + std::string(5000, 'a')));
}

TEST_CASE("grants: a root the person chooses: its services wait for it, then start on the folder chosen") {
	const TemporaryFolder folder;
	affresco::services::ServiceRegistry registry;
	CHECK_FALSE(registry.choosable("workspace"));
	CHECK(registry.chooseRoot("workspace", utf8(folder.path / "chosen"))); // not allowed: refused

	registry.allowChosenRoot("workspace");
	CHECK(registry.choosable("workspace"));
	const std::vector<std::string> refused = registry.load({{"files", {{"provider", "fs"}, {"root", "workspace"}}},
	                                                        {"other", {{"provider", "fs"}, {"root", "elsewhere"}}}});
	CHECK(refused.size() == 1); // "other" names a root no one grants; "files" only waits for a folder
	CHECK(registry.status()["files"]["state"] == "refused");
	CHECK(registry.status()["files"]["folder"].is_null());

	CHECK(registry.chooseRoot("workspace", utf8(folder.path / "missing")));
	CHECK(registry.chooseRoot("elsewhere", utf8(folder.path / "chosen")));
	CHECK_FALSE(registry.chooseRoot("workspace", utf8(folder.path / "chosen")));
	const json status = registry.status();
	CHECK(status["files"]["state"] != "refused");
	CHECK(status["files"]["folder"] == "chosen");
	CHECK(status["other"]["state"] == "refused");

	// Choosing again moves the services to the new folder
	std::filesystem::create_directories(folder.path / "second");
	CHECK_FALSE(registry.chooseRoot("workspace", utf8(folder.path / "second")));
	CHECK(registry.status()["files"]["folder"] == "second");
}
