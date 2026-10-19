#pragma once

/// @file
/// @brief Where the examples meet: the host listens there, the Studio and the MCP server (mcp/affrescoui_mcp.py)
/// connect there. The default folder is affresco::Screen::defaultSocketDirectory(); these check and create it.

#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <aclapi.h>

#include <cstddef>
#include <memory>
#include <vector>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32

/// @brief Closes a Windows handle when it goes out of scope.
struct HandleCloser {
	/// @brief Closes it.
	/// @param handle The handle.
	void operator()(HANDLE handle) const noexcept { CloseHandle(handle); }
};

/// @brief Frees memory Windows allocated with LocalAlloc when it goes out of scope.
struct LocalMemoryFreer {
	/// @brief Frees it.
	/// @param memory The memory.
	void operator()(void* memory) const noexcept { LocalFree(memory); }
};

/// @brief Reads one kind of information about this process's access token.
/// @param kind TokenUser (who the process runs as) or TokenOwner (who it makes the owner of new files).
/// @return The TOKEN_USER or TOKEN_OWNER it holds; empty if it can't be read.
inline std::vector<std::byte> processToken(TOKEN_INFORMATION_CLASS kind) {
	HANDLE token = nullptr;
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
	const std::unique_ptr<void, HandleCloser> closer(token);
	DWORD size = 0;
	GetTokenInformation(token, kind, nullptr, 0, &size); // fails, giving the size it needs
	std::vector<std::byte> buffer(size);
	if(size == 0 || !GetTokenInformation(token, kind, buffer.data(), size, &size)) return {};
	return buffer;
}

/// @brief Whether this user owns a file: its owner is the user this process runs as, or the owner the process gives
/// new files (the Administrators group when it runs elevated, so a folder an elevated run created passes there too).
/// @param file The file or directory.
/// @return true if so; false if it belongs to someone else or its owner can't be read.
inline bool ownedByThisUser(const std::filesystem::path& file) {
	PSID owner = nullptr;
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	if(GetNamedSecurityInfoW(file.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr,
	                         nullptr, &descriptor) != ERROR_SUCCESS)
		return false;
	const std::unique_ptr<void, LocalMemoryFreer> freer(descriptor); // owner points into it
	const std::vector<std::byte> user = processToken(TokenUser);
	const std::vector<std::byte> newFileOwner = processToken(TokenOwner);
	return (!user.empty() && EqualSid(owner, reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid)) ||
	       (!newFileOwner.empty() && EqualSid(owner, reinterpret_cast<const TOKEN_OWNER*>(newFileOwner.data())->Owner));
}

/// @brief Lets only this user use a directory and what is created in it, like chmod 700: it drops the access the
/// directory inherits from its parents, so no other user can connect to the sockets in it.
/// @param dir The directory.
/// @throws std::runtime_error If its access can't be set (a file system without access lists, such as FAT).
inline void restrictToThisUser(const std::filesystem::path& dir) {
	const std::vector<std::byte> user = processToken(TokenUser);
	EXPLICIT_ACCESS_W access{};
	access.grfAccessPermissions = GENERIC_ALL;
	access.grfAccessMode = SET_ACCESS;
	access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	access.Trustee.TrusteeType = TRUSTEE_IS_USER;
	PACL list = nullptr;
	if(!user.empty()) {
		access.Trustee.ptstrName = static_cast<LPWSTR>(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid);
		if(SetEntriesInAclW(1, &access, nullptr, &list) != ERROR_SUCCESS) list = nullptr;
	}
	const std::unique_ptr<void, LocalMemoryFreer> freer(list);
	if(!list || SetNamedSecurityInfoW(const_cast<LPWSTR>(dir.c_str()), SE_FILE_OBJECT,
	                                  DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr,
	                                  list, nullptr) != ERROR_SUCCESS)
		throw std::runtime_error("could not make " + dir.string() + " private to this user: pass --socket DIR");
}

#endif

/// @brief Refuses a socket directory another user could control: one they made first (a path in /tmp is easy to guess)
/// or one --socket or AFFRESCOUI_SOCKET_DIR points to in a shared place.
/// @param dir The directory; nothing is checked if it doesn't exist yet.
/// @throws std::runtime_error If it is a symbolic link (or a junction), not a directory, or not owned by this user.
inline void checkSocketDir(const std::filesystem::path& dir) {
#ifdef _WIN32
	std::error_code error;
	const std::filesystem::file_status status = std::filesystem::symlink_status(dir, error);
	if(error || !std::filesystem::exists(status)) return;
	const bool safe = std::filesystem::is_directory(status) && ownedByThisUser(dir); // a link or junction is neither
#else
	struct stat info{};
	if(lstat(dir.c_str(), &info) != 0) return;
	const bool safe = S_ISDIR(info.st_mode) && info.st_uid == getuid();
#endif
	if(!safe)
		throw std::runtime_error(dir.string() +
		                         " is not a directory owned by this user: remove it or pass --socket DIR");
}

/// @brief Creates the socket directory, accessible only to this user.
/// @param dir The directory.
/// @throws std::runtime_error If it exists but another user could control it, or (on Windows) can't be made private.
inline void createSocketDir(const std::filesystem::path& dir) {
	std::filesystem::create_directories(dir);
	checkSocketDir(dir);
#ifdef _WIN32
	restrictToThisUser(dir);
#else
	chmod(dir.c_str(), S_IRWXU);
#endif
}
