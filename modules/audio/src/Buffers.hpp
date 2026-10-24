#pragma once

/// @file
/// @brief detail::FrameRing (frames between one producer and the audio thread, without a lock on the reading side)
/// and detail::ByteQueue (encoded bytes between producers and a decoding thread, with the seeks decoders make).

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace affresco::audio::detail {

/// @brief A bounded ring of interleaved float frames: writers take turns on a lock, the reader (the audio thread)
/// takes none.
struct FrameRing {
	/// @brief An empty ring.
	/// @param capacity The most frames it holds, at least 1.
	/// @param channels Samples per frame.
	FrameRing(std::size_t capacity, std::uint32_t channels);

	/// @brief Adds frames, as many as fit. Any thread (writers take turns).
	/// @param frames Interleaved samples.
	/// @param count Frames.
	/// @return The frames taken.
	std::size_t write(const float* frames, std::size_t count);
	/// @brief Takes the oldest frames. The one reader only.
	/// @param out Room for count frames.
	/// @param count How many at most.
	/// @return The frames taken.
	std::size_t read(float* out, std::size_t count) noexcept;
	/// @brief Frames waiting.
	[[nodiscard]] std::size_t available() const noexcept;
	/// @brief Room left, in frames.
	[[nodiscard]] std::size_t space() const noexcept;

	const std::size_t capacity;        ///< See the constructor.
	const std::uint32_t channels;      ///< See the constructor.
	std::vector<float> samples;        ///< capacity * channels.
	std::atomic<std::uint64_t> written{0};  ///< Frames written in all.
	std::atomic<std::uint64_t> consumed{0}; ///< Frames read in all.
	std::atomic<bool> finished{false}; ///< No more frames come.
	std::mutex writers;                ///< Writers take turns.
};

/// @brief A bounded queue of encoded bytes read by one decoding thread, which blocks until enough arrive. Until the
/// decoder is set up it keeps every byte, so that its probes may seek back to the start; then it drops what it read.
struct ByteQueue {
	/// @brief An empty queue.
	/// @param capacity The most bytes held.
	explicit ByteQueue(std::size_t capacity) : capacity(capacity) {}

	/// @brief Adds bytes, all or none. Any thread; never blocks.
	/// @param data The bytes.
	/// @return false if they don't fit, or after finish().
	bool push(std::span<const std::uint8_t> data);
	/// @brief No more bytes come.
	void finish() noexcept;
	/// @brief Wakes the reader for good: reads and seeks fail from now on.
	void cancel() noexcept;
	/// @brief Reads bytes, waiting until there are that many, the queue is full, or no more come.
	/// @param out Room for count bytes.
	/// @param count How many.
	/// @return The bytes read: fewer only at the end (or when cancelled, or when count is more than it holds).
	std::size_t read(std::uint8_t* out, std::size_t count);
	/// @brief Moves the reading position, waiting for bytes ahead of it to arrive.
	/// @param offset From the start of the stream (bytes before those kept can't be reached again).
	/// @return Whether it could.
	bool seek(std::uint64_t offset);
	/// @brief The reading position, from the start of the stream.
	[[nodiscard]] std::uint64_t position() const;
	/// @brief The stream's length, if every byte has come and none were dropped; else 0.
	[[nodiscard]] std::uint64_t knownLength() const;
	/// @brief Drops what was read from now on (the decoder is set up).
	void stopKeeping();
	/// @brief Bytes held and not yet read.
	[[nodiscard]] std::size_t buffered() const;

	const std::size_t capacity;          ///< See the constructor.
	mutable std::mutex mutex;            ///< Guards the rest.
	std::condition_variable changed;     ///< Bytes came, or the end, or a cancel.
	std::vector<std::uint8_t> bytes;     ///< The bytes held: from offset `base` of the stream.
	std::uint64_t base = 0;              ///< The stream offset of bytes[0].
	std::uint64_t reading = 0;           ///< The reading position.
	bool keeping = true;                 ///< See stopKeeping().
	bool finished = false;               ///< See finish().
	bool cancelled = false;              ///< See cancel().
};

} // namespace affresco::audio::detail
