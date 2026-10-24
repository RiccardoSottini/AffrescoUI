#pragma once

/// @file
/// @brief detail::Reader: what a voice reads its frames from, one per voice, made by Source::open().

#include "affresco/audio/Source.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace affresco::audio::detail {

/// @brief Why a read gave fewer frames than asked.
enum class ReadStatus : std::uint8_t {
	Ok,       ///< It gave them all; more may follow.
	Starving, ///< A stream has no more yet: more may come.
	End,      ///< The source ended.
	Failed,   ///< The decoder failed (failure() says why).
};

/// @brief The outcome of a read.
struct ReadResult {
	std::size_t frames = 0;              ///< The frames written.
	ReadStatus status = ReadStatus::Ok;  ///< Why fewer than asked, if so.
};

/// @brief Frames of one source, in the engine's format, for one voice.
///
/// read() and seek() run on the audio thread (or under the engine's lock), so they must not wait for long; the
/// constructor and destructor run on other threads (a stream's destructor stops its decoding thread).
class Reader {
public:
	Reader() = default;
	Reader(const Reader&) = delete;
	Reader& operator=(const Reader&) = delete;
	Reader(Reader&&) = delete;
	Reader& operator=(Reader&&) = delete;
	virtual ~Reader() = default;

	/// @brief Reads the next frames.
	/// @param out Room for frames * channels floats.
	/// @param frames How many.
	/// @return What it wrote, and why it is fewer, if so.
	virtual ReadResult read(float* out, std::size_t frames) = 0;
	/// @brief Moves to a frame.
	/// @param frame The frame, from the start.
	/// @return Whether it could.
	virtual bool seek(std::uint64_t frame) {
		(void)frame;
		return false;
	}
	/// @brief Whether seek() works.
	[[nodiscard]] virtual bool seekable() const { return false; }
	/// @brief Its frames in all, if known.
	[[nodiscard]] virtual std::optional<std::uint64_t> length() const { return std::nullopt; }
	/// @brief Why it failed, for people (a string that lives as long as the reader), or nullptr.
	[[nodiscard]] virtual const char* failure() const { return nullptr; }
};

} // namespace affresco::audio::detail
