#pragma once

/// @file
/// @brief The readers that decode: a file or bytes in memory, decoded on the audio thread a little at a time, and an
/// encoded stream, decoded on a thread of its own into a ring (miniaudio's WAV, FLAC and MP3 decoders, which also
/// convert to the engine's rate and channels).

#include "Buffers.hpp"
#include "Reader.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace affresco::audio::detail {

/// @brief Opens a file to decode.
/// @param path The file.
/// @param hint Its encoding.
/// @param output The engine's format.
/// @param limits The engine's limits (its size, its duration).
/// @param error Receives why it can't.
/// @return The reader, or nullptr.
[[nodiscard]] std::unique_ptr<Reader> openFile(const std::filesystem::path& path, Encoding hint, const Format& output,
                                               const Limits& limits, std::string& error);

/// @brief Opens bytes in memory to decode.
/// @param bytes The bytes, shared with the source.
/// @param hint Their encoding.
/// @param output The engine's format.
/// @param limits The engine's limits.
/// @param error Receives why it can't.
/// @return The reader, or nullptr.
[[nodiscard]] std::unique_ptr<Reader> openMemory(std::shared_ptr<const std::vector<std::uint8_t>> bytes,
                                                 Encoding hint, const Format& output, const Limits& limits,
                                                 std::string& error);

/// @brief Starts decoding a stream of encoded bytes on a thread of its own.
/// @param bytes The queue the stream pushes into.
/// @param hint The encoding.
/// @param output The engine's format.
/// @param limits The engine's limits (its duration).
/// @return The reader; a stream that doesn't decode fails when it is read.
[[nodiscard]] std::unique_ptr<Reader> openEncodedStream(std::shared_ptr<ByteQueue> bytes, Encoding hint,
                                                        const Format& output, const Limits& limits);

} // namespace affresco::audio::detail
