/// @file
/// @brief Unit tests for the channel module's sockets: SocketServer and SocketChannel over a real local socket, in a
/// temporary folder.

#include <affresco/channel/Sockets.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#else
#include <sys/stat.h>
#endif

using affresco::Channel;
using affresco::channel::SocketChannel;
using affresco::channel::SocketServer;

namespace {

/// @brief An Executor whose tasks run on the test's thread, inside runUntil().
class QueueExecutor final : public affresco::Executor {
public:
	/// @brief Queues a task. Safe from any thread.
	/// @param task The task.
	void post(std::function<void()> task) override {
		{
			const std::lock_guard lock(mutex_);
			tasks_.push_back(std::move(task));
		}
		ready_.notify_one();
	}

	/// @brief Runs queued tasks until @p done says so, or two seconds pass.
	/// @param done Checked before each task.
	/// @return Whether @p done became true.
	bool runUntil(const std::function<bool()>& done) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while(!done()) {
			std::unique_lock lock(mutex_);
			if(!ready_.wait_until(lock, deadline, [this] { return !tasks_.empty(); })) return done();
			const std::function<void()> task = std::move(tasks_.front());
			tasks_.pop_front();
			lock.unlock();
			task();
		}
		return true;
	}

private:
	std::mutex mutex_;                        ///< Guards tasks_.
	std::condition_variable ready_;           ///< Notified when a task is queued.
	std::deque<std::function<void()>> tasks_; ///< Queued, oldest first.
};

/// @brief A temporary folder with a short name (socket paths are limited to about 100 bytes), removed at the end.
struct TemporaryFolder {
	/// @brief Creates the folder.
	TemporaryFolder() { std::filesystem::create_directories(path); }
	/// @brief Removes it and what is in it.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;

	std::filesystem::path path = std::filesystem::temp_directory_path() /
	                             ("aui-" + std::to_string(std::random_device{}())); ///< The folder.
};

/// @brief A server, the channels it accepted, and what they received.
struct Server {
	/// @brief Starts listening at @p path.
	/// @param executor Delivers connections and messages.
	/// @param path The socket file.
	Server(QueueExecutor& executor, const std::filesystem::path& path)
	    : server(executor, path, [this](std::unique_ptr<Channel> channel) {
		      channel->onMessage.connect([this](const std::string& message) { received.push_back(message); });
		      channels.push_back(std::move(channel));
	      }) {}

	std::vector<std::unique_ptr<Channel>> channels; ///< Accepted, oldest first (destroyed after the server).
	std::vector<std::string> received;              ///< Messages from every channel, oldest first.
	SocketServer server;                            ///< Listens.
};

#if defined(_WIN32)
/// @brief Whether a SID is this process's user.
/// @param sid The SID.
/// @return false also if the user can't be read.
bool isThisUser(PSID sid) {
	HANDLE token = nullptr;
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
	DWORD size = 0;
	GetTokenInformation(token, TokenUser, nullptr, 0, &size); // asks for the size
	std::vector<std::byte> user(size);
	const bool read = GetTokenInformation(token, TokenUser, user.data(), size, &size) != FALSE;
	CloseHandle(token);
	return read && EqualSid(sid, reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid) != FALSE;
}
#endif

/// @brief Whether only this user may open a socket file.
/// @param path The file.
/// @return On POSIX, mode 0600; on Windows, an access list that inherits nothing from the folder and has one entry,
/// which allows this user.
bool onlyForThisUser(const std::filesystem::path& path) {
#if defined(_WIN32)
	const HANDLE file = CreateFileW(path.c_str(), READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                                nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
	                                nullptr);
	if(file == INVALID_HANDLE_VALUE) return false;
	PACL access = nullptr;
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	const DWORD error = GetSecurityInfo(file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &access,
	                                    nullptr, &descriptor);
	CloseHandle(file);
	if(error != ERROR_SUCCESS) return false;
	SECURITY_DESCRIPTOR_CONTROL control = 0;
	DWORD revision = 0;
	const bool protectedList = GetSecurityDescriptorControl(descriptor, &control, &revision) &&
	                           (control & SE_DACL_PROTECTED) != 0;
	ACCESS_ALLOWED_ACE* entry = nullptr;
	const bool oneEntry = access != nullptr && access->AceCount == 1 &&
	                      GetAce(access, 0, reinterpret_cast<void**>(&entry)) != FALSE &&
	                      entry->Header.AceType == ACCESS_ALLOWED_ACE_TYPE;
	const bool forThisUser = oneEntry && isThisUser(&entry->SidStart); // the SID starts at SidStart
	LocalFree(descriptor);
	return protectedList && forThisUser;
#else
	struct stat info{};
	return stat(path.c_str(), &info) == 0 && (info.st_mode & 0777) == 0600;
#endif
}

} // namespace

TEST_CASE("channel: a SocketChannel and a SocketServer carry lines both ways") {
	QueueExecutor executor;
	const TemporaryFolder folder;
	Server server(executor, folder.path / "s");

	const std::unique_ptr<SocketChannel> client = SocketChannel::connect(executor, folder.path / "s");
	std::vector<std::string> received;
	client->onMessage.connect([&](const std::string& message) { received.push_back(message); });
	REQUIRE(executor.runUntil([&] { return server.channels.size() == 1; }));

	const std::string large(100'000, 'x'); // many reads and writes, one line
	client->send("hello");
	client->send(large);
	REQUIRE(executor.runUntil([&] { return server.received.size() == 2; }));
	CHECK(server.received[0] == "hello");
	CHECK(server.received[1] == large);

	server.channels[0]->send("first");
	server.channels[0]->send("second");
	REQUIRE(executor.runUntil([&] { return received.size() == 2; }));
	CHECK(received == std::vector<std::string>{"first", "second"});
	CHECK(client->isOpen());
}

TEST_CASE("channel: a SocketChannel whose peer goes away is closed") {
	QueueExecutor executor;
	const TemporaryFolder folder;
	Server server(executor, folder.path / "s");

	const std::unique_ptr<SocketChannel> client = SocketChannel::connect(executor, folder.path / "s");
	bool closed = false;
	client->onClosed.connect([&] { closed = true; });
	REQUIRE(executor.runUntil([&] { return server.channels.size() == 1; }));

	server.channels.clear();
	REQUIRE(executor.runUntil([&] { return closed; }));
	CHECK_FALSE(client->isOpen());
	client->send("nobody hears this"); // must neither block nor end the process
}

TEST_CASE("channel: a SocketServer's socket is only for this user, and is removed when it stops") {
	QueueExecutor executor;
	const TemporaryFolder folder;
	const std::filesystem::path path = folder.path / "s";
	{
		const SocketServer server(executor, path, [](std::unique_ptr<Channel>) {});
		CHECK(onlyForThisUser(path));
		CHECK_THROWS_WITH(SocketServer(executor, path, [](std::unique_ptr<Channel>) {}),
		                  doctest::Contains("another app is already listening"));
	}
	CHECK_FALSE(std::filesystem::exists(path));
	CHECK_THROWS_WITH((void)SocketChannel::connect(executor, path), doctest::Contains("nothing is listening"));
}

TEST_CASE("channel: a SocketServer replaces a file nobody listens on") {
	QueueExecutor executor;
	const TemporaryFolder folder;
	const std::filesystem::path path = folder.path / "s";
	std::ofstream(path) << "left by a server that crashed";

	Server server(executor, path);
	const std::unique_ptr<SocketChannel> client = SocketChannel::connect(executor, path);
	client->send("hello");
	REQUIRE(executor.runUntil([&] { return server.received.size() == 1; }));
	CHECK(server.received[0] == "hello");
}
