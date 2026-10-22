#pragma once

/// @file
/// @brief DapTransport: the byte stream a debug adapter is spoken to over: a granted program's stdin and stdout, or a
/// TCP connection to a port on this machine's loopback address (an adapter already running, attached to).

#include "process/Process.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace affresco::services {

/// @brief A debug adapter's byte stream. read() blocks, so it belongs on a thread of its own; end() may be called from
/// any thread and makes a blocked read return.
class DapTransport {
public:
	DapTransport() = default;
	DapTransport(const DapTransport&) = delete;
	DapTransport& operator=(const DapTransport&) = delete;
	DapTransport(DapTransport&&) = delete;
	DapTransport& operator=(DapTransport&&) = delete;
	/// @brief Ends the stream (end()).
	virtual ~DapTransport() = default;

	/// @brief Starts a granted program, spoken to over its stdin and stdout.
	/// @param launch What to start.
	/// @return The transport.
	/// @throws std::runtime_error If it can't be started.
	[[nodiscard]] static std::unique_ptr<DapTransport> start(const Launch& launch);
	/// @brief Connects to a port of the loopback address (127.0.0.1), never another host.
	/// @param port The port.
	/// @return The transport.
	/// @throws std::runtime_error If nothing listens there, or (on Windows, for now) always.
	[[nodiscard]] static std::unique_ptr<DapTransport> connect(std::uint16_t port);

	/// @brief Writes bytes, all of them.
	/// @param bytes The bytes.
	/// @return false if the adapter closed its end.
	[[nodiscard]] virtual bool write(std::string_view bytes) = 0;
	/// @brief Reads what the adapter wrote.
	/// @param buffer Where the bytes go.
	/// @param size Its size.
	/// @return The count read; 0 once the adapter closed its end (or end() was called); -1 on an error.
	[[nodiscard]] virtual std::ptrdiff_t read(char* buffer, std::size_t size) = 0;
	/// @brief Closes the way to the adapter, so it reads the end of its input.
	virtual void closeInput() noexcept = 0;
	/// @brief Ends the adapter (its program, or the connection) and waits for it. Once only.
	/// @return The program's exit code, or -1 if a signal ended it or it was a connection.
	virtual int end() noexcept = 0;
};

} // namespace affresco::services
