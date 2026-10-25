/// @file
/// @brief Unit tests for the screen module that need no window: its paths (the default socket directory, where
/// "persist" state is kept) and loading a screen from its text.

#include <affresco/Screen.hpp>

#include "StateFile.hpp"                // modules/screen/src
#include "features/FeaturesService.hpp" // modules/screen/src

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <memory>
#include <string>
#include <utility>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using affresco::json;

namespace {

/// @brief Sets an environment variable for one test, and puts the old value back after it.
class ScopedVariable {
public:
	/// @brief Sets it.
	/// @param name The variable.
	/// @param value Its value for the test; empty to unset it.
	ScopedVariable(std::string name, const std::string& value) : name_(std::move(name)) {
		old_ = get();
		set(value);
	}

	ScopedVariable(const ScopedVariable&) = delete;
	ScopedVariable& operator=(const ScopedVariable&) = delete;

	/// @brief Puts the old value back.
	~ScopedVariable() {
		if(old_) set(*old_);
		else set({});
	}

private:
	/// @brief Reads the variable.
	/// @return Its value, if it is set.
	[[nodiscard]] std::optional<std::string> get() const {
#ifdef _WIN32
		char* value = nullptr;
		std::size_t length = 0;
		if(_dupenv_s(&value, &length, name_.c_str()) != 0 || !value) return std::nullopt;
		const std::unique_ptr<char, void (*)(void*)> freer(value, [](void* memory) { std::free(memory); });
#else
		const char* value = std::getenv(name_.c_str());
		if(!value) return std::nullopt;
#endif
		return std::string(value);
	}

	/// @brief Sets or unsets the variable.
	/// @param value The value; empty to unset it.
	void set(const std::string& value) const noexcept {
#ifdef _WIN32
		_putenv_s(name_.c_str(), value.c_str()); // an empty value unsets it
#else
		if(value.empty()) unsetenv(name_.c_str());
		else setenv(name_.c_str(), value.c_str(), 1);
#endif
	}

	std::string name_;               ///< The variable.
	std::optional<std::string> old_; ///< Its value before the test, if it had one.
};

/// @brief A folder of its own for one test, removed after it.
struct TemporaryFolder {
	/// @brief Creates it.
	TemporaryFolder() { std::filesystem::create_directories(path); }
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	/// @brief Removes it and what is in it.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}

	/// The folder: unique to this process, so parallel runs don't meet.
	std::filesystem::path path = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-unit-" + std::to_string(_getpid()));
#else
	                             ("affrescoui-unit-" + std::to_string(getpid()));
#endif
};

/// @brief Writes a file, creating its folder.
/// @param file The file.
/// @param content What it holds.
void write(const std::filesystem::path& file, const std::string& content) {
	std::filesystem::create_directories(file.parent_path());
	std::ofstream(file) << content;
}

/// @brief A screen whose state holds a value nested in @p depth lists.
/// @param depth How many lists.
/// @return The screen's text.
std::string screenNestedIn(std::size_t depth) {
	return R"({"format": "affrescoui-screen", "version": 2, "root": {"type": "panel"}, "state": {"x": )" +
	       std::string(depth, '[') + std::string(depth, ']') + "}}";
}

} // namespace

TEST_CASE("screen: AFFRESCOUI_SOCKET_DIR is the default socket directory when it is set") {
	const ScopedVariable variable("AFFRESCOUI_SOCKET_DIR", "out/s");
	CHECK(affresco::Screen::defaultSocketDirectory() == std::filesystem::path("out/s"));
}

TEST_CASE("screen: without AFFRESCOUI_SOCKET_DIR the default socket directory is the user's own") {
	const ScopedVariable variable("AFFRESCOUI_SOCKET_DIR", "");
#ifdef _WIN32
	const ScopedVariable local("LOCALAPPDATA", "C:\\Users\\someone\\AppData\\Local");
	CHECK(affresco::Screen::defaultSocketDirectory() ==
	      std::filesystem::path("C:\\Users\\someone\\AppData\\Local\\affrescoui\\sockets"));
#else
	CHECK(affresco::Screen::defaultSocketDirectory() ==
	      std::filesystem::path("/tmp/affrescoui-" + std::to_string(getuid())));
#endif
}

TEST_CASE("screen: persisted state is read from the platform's folder for application data") {
	const TemporaryFolder folder;
#ifdef _WIN32
	const ScopedVariable data("APPDATA", folder.path.string());
	const std::filesystem::path stateFile = folder.path / "affrescoui" / "counter.state.json";
#else
	const ScopedVariable home("HOME", folder.path.string());
	const std::filesystem::path stateFile = folder.path / ".affrescoui" / "counter.state.json";
#endif
	write(folder.path / "screens" / "counter.screen.json", R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "count": 0, "other": 0 },
		"persist": ["state.count"],
		"root": { "type": "panel", "id": "main" }
	})json");
	write(stateFile, R"json({ "/count": 7, "/other": 9 })json");

	const affresco::Screen screen = affresco::Screen::fromFile(folder.path / "screens" / "counter.screen.json");
	CHECK(json::parse(screen.stateJson()) == json{{"count", 7}, {"other", 0}}); // only what "persist" names
}

TEST_CASE("screen: an index screen keeps its data under its folder's name, so two apps never share it") {
	const TemporaryFolder folder;
#ifdef _WIN32
	const ScopedVariable data("APPDATA", folder.path.string());
	const std::filesystem::path dataFolder = folder.path / "affrescoui";
#else
	const ScopedVariable home("HOME", folder.path.string());
	const std::filesystem::path dataFolder = folder.path / ".affrescoui";
#endif
	for(const char* app : {"alpha", "beta"})
		write(folder.path / app / "index.screen.json", R"json({
			"format": "affrescoui-screen", "version": 2,
			"state": { "count": 0 },
			"persist": ["state.count"],
			"root": { "type": "panel", "id": "main" }
		})json");
	write(dataFolder / "alpha.state.json", R"json({ "/count": 1 })json");
	write(dataFolder / "beta.state.json", R"json({ "/count": 2 })json");

	const affresco::Screen alpha = affresco::Screen::fromFile(folder.path / "alpha" / "index.screen.json");
	const affresco::Screen beta = affresco::Screen::fromFile(folder.path / "beta" / "index.screen.json");
	CHECK(json::parse(alpha.stateJson())["count"] == 1);
	CHECK(json::parse(beta.stateJson())["count"] == 2);
	CHECK(std::filesystem::exists(dataFolder / "alpha.history"));
	CHECK(std::filesystem::exists(dataFolder / "beta.history"));
	CHECK_FALSE(std::filesystem::exists(dataFolder / "index.history"));
}

// A screen file is untrusted input: before, one nested 200,000 deep overflowed the stack (AddressSanitizer: SEGV)
TEST_CASE("screen: a screen nested deeper than the limit is refused, not parsed") {
	CHECK_NOTHROW((void)affresco::Screen::fromString(screenNestedIn(100)));
	CHECK_THROWS_WITH_AS((void)affresco::Screen::fromString(screenNestedIn(200000)),
	                     doctest::Contains("nested more than 256 levels deep"), affresco::ScreenError);
}

// Before, a submission queued by the features service checked only that the guard had expired, and other tasks still
// queued keep it from expiring: after the Screen ended, the submission reached its repository, already destroyed
TEST_CASE("screen: a task queued by a screen that has ended does nothing, while other queued tasks hold its guard") {
	auto alive = std::make_shared<bool>(true);
	const std::weak_ptr<bool> guard = alive;
	CHECK(affresco::detail::screenAlive(guard));

	const std::shared_ptr<bool> heldByAnotherTask = alive;
	*alive = false; // the Screen's destructor
	alive.reset();
	CHECK_FALSE(guard.expired());
	CHECK_FALSE(affresco::detail::screenAlive(guard));
	CHECK_FALSE(affresco::detail::screenAlive(std::weak_ptr<bool>()));
}

#ifndef _WIN32
// Before, a new state file was created as the process's umask allows, most often readable by every user
TEST_CASE("screen: a new state file is readable by its owner only, and one already there keeps its permissions") {
	const TemporaryFolder folder;
	namespace fs = std::filesystem;
	const fs::path created = folder.path / "new.state.json";
	REQUIRE(affresco::detail::writeStateFile(created, "{}\n"));
	CHECK(fs::status(created).permissions() == (fs::perms::owner_read | fs::perms::owner_write));

	const fs::path existing = folder.path / "old.state.json";
	write(existing, "{ \"/count\": 1 }");
	fs::permissions(existing, fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read);
	REQUIRE(affresco::detail::writeStateFile(existing, "{}\n"));
	CHECK(fs::status(existing).permissions() ==
	      (fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read));
	std::ifstream in(existing);
	CHECK(std::string(std::istreambuf_iterator<char>(in), {}) == "{}\n");

	// A link planted in the state file's place is never written through
	const fs::path canary = folder.path / "canary.txt";
	write(canary, "canary");
	const fs::path linked = folder.path / "linked.state.json";
	fs::create_symlink(canary, linked);
	CHECK_FALSE(affresco::detail::writeStateFile(linked, "{}\n"));
	std::ifstream kept(canary);
	CHECK(std::string(std::istreambuf_iterator<char>(kept), {}) == "canary");
}
#endif
