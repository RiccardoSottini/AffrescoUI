#pragma once

/// @file
/// @brief detail::Shared and detail::VoiceCore: an engine's voices and the lock that guards them, and one voice's
/// transport, gain, rate conversion and reporting. Everything here is guarded by Shared::mutex.

#include "Reader.hpp"

#include "affresco/audio/Voice.hpp"
#include "affresco/core/Executor.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace affresco::audio::detail {

/// Frames each voice renders at a time (its scratch buffer); a longer block is mixed in parts.
inline constexpr std::size_t blockFrames = 512;
/// Frames read from a reader at a time.
inline constexpr std::size_t inputFrames = 256;
/// The most events posted and not yet run: past it, Position events are dropped (the others are a few per voice).
inline constexpr std::size_t maxQueuedEvents = 1024;

/// @brief The gains of the first two channels for a balance: the side away from it is turned down, the centre leaves
/// both at 1.
struct PanGains {
	float left = 1;  ///< Channel 0.
	float right = 1; ///< Channel 1.
};

/// @brief The gains of a balance.
/// @param pan From -1 (left) to 1 (right).
/// @return The gains.
[[nodiscard]] constexpr PanGains panGains(float pan) noexcept {
	return {pan > 0 ? 1 - pan : 1, pan < 0 ? 1 + pan : 1};
}

/// @brief Clamps a value to a range; NaN becomes the low end.
/// @param value The value.
/// @param low The least.
/// @param high The most.
/// @return It, clamped.
[[nodiscard]] constexpr float clamped(float value, float low, float high) noexcept {
	return value >= low ? (value <= high ? value : high) : low;
}

/// The largest magnitude a source's sample keeps: a decoder may give anything a file holds (IEEE float WAV can hold
/// infinities and NaN), and the mix must stay finite.
inline constexpr float maxSample = 16;

/// @brief A source's sample made safe to mix: NaN is silence, and anything past maxSample is clamped to it.
/// @param sample The sample.
/// @return It, finite.
[[nodiscard]] float finiteSample(float sample) noexcept;

/// @brief Adds a block's levels: the peak and the root mean square of its first two channels.
/// @param frames Interleaved samples.
/// @param count Frames.
/// @param channels Samples per frame.
/// @return The levels.
[[nodiscard]] Levels measure(const float* frames, std::size_t count, std::uint32_t channels) noexcept;

/// @brief One voice: its reader, transport, gain and rate conversion, and what it has to report.
///
/// The rate converter interpolates linearly between two frames of the source, `last` and `next`, at `t` (0 to 1),
/// stepping t by the rate for each frame out: at rate 1 it gives the source's frames exactly.
struct VoiceCore {
	/// @brief A voice on a reader, stopped at its start.
	/// @param source The source it plays (kept for the voice's life).
	/// @param reader Its reader.
	/// @param format The engine's format.
	/// @param options How it starts.
	/// @param maxFrames The most frames it plays from the start (Limits::seconds).
	VoiceCore(std::shared_ptr<Source> source, std::unique_ptr<Reader> reader, Format format,
	          const VoiceOptions& options, std::uint64_t maxFrames);

	/// @brief Mixes the next frames into a block, if it plays.
	/// @param mix The block: frames * format.channels floats, added to.
	/// @param frames How many, at most blockFrames.
	void render(float* mix, std::size_t frames);
	/// @brief Starts or resumes it (from the start after its end).
	/// @return Whether it could.
	bool start();
	/// @brief Stops it and goes back to the start where it can.
	void halt();
	/// @brief Moves the reader and the converter to a frame.
	/// @param frame The frame.
	/// @return Whether the reader could.
	bool moveTo(std::uint64_t frame);
	/// @brief Where it is, in frames of the source from its start.
	[[nodiscard]] double cursor() const noexcept;
	/// @brief Seconds in all, if known.
	[[nodiscard]] std::optional<double> durationSeconds() const noexcept;

	std::shared_ptr<Source> source;        ///< What it plays.
	std::unique_ptr<Reader> reader;        ///< Its frames.
	Format format;                         ///< The engine's.
	std::optional<std::uint64_t> length;   ///< Frames in all, if known.
	std::uint64_t maxFrames;               ///< See the constructor.

	State state = State::Stopped; ///< See Voice::state().
	float volume = 1;             ///< The gain now.
	float target = 1;             ///< Where a fade goes.
	float restore = 1;            ///< The volume a fade out leaves the voice at once it stops it.
	float step = 0;               ///< A fade's change per frame.
	std::uint64_t fadeLeft = 0;   ///< Frames until the fade ends.
	bool stopAfterFade = false;   ///< Stop when the fade ends.
	float pan = 0;                ///< See Voice::pan().
	float rate = 1;               ///< See Voice::rate().
	bool loop = false;            ///< See Voice::loop().

	std::vector<float> input;       ///< Frames read from the reader.
	std::size_t inputPosition = 0;  ///< The next frame of input to use.
	std::size_t inputCount = 0;     ///< Frames in input.
	std::vector<float> last;        ///< The frame before the output's position.
	std::vector<float> next;        ///< The frame after it.
	std::vector<float> fetched;     ///< A frame being fetched.
	std::vector<float> scratch;     ///< A block before its gain.
	double t = 0;                   ///< Between last (0) and next (1).
	bool primed = false;            ///< last and next hold frames.
	bool draining = false;          ///< The source ended: next repeats last, then the voice ends.
	std::uint64_t lastIndex = 0;    ///< last's frame index.
	std::uint64_t nextIndex = 0;    ///< next's frame index.
	std::uint64_t readIndex = 0;    ///< The index of the next frame the reader gives.
	ReadStatus pending = ReadStatus::Ok; ///< Why the last read gave nothing.

	bool atEnd = false;           ///< It played to its end.
	bool flowed = false;          ///< It produced a frame since it started: running dry now is an underrun.
	bool starving = false;        ///< A stream has run dry and hasn't recovered.
	bool startedPending = false;  ///< Started is to be reported.
	bool endedPending = false;    ///< Ended is to be reported.
	std::size_t underruns = 0;    ///< See Voice::underruns().
	std::size_t underrunsReported = 0; ///< Underruns reported so far.
	std::optional<std::string> failure; ///< Why it failed.
	bool failureReported = false; ///< Error was reported.
	Levels levels;                ///< See Voice::levels().
	std::shared_ptr<const EventHandler> onEvent; ///< Its handler, or null.
	std::chrono::milliseconds positionInterval;  ///< See VoiceOptions.
	std::chrono::steady_clock::time_point lastPosition{}; ///< When Position was last reported.
	double reportedPosition = -1;  ///< The position last reported.
	bool released = false;         ///< Its last handle is gone.

private:
	/// @brief Fills `fetched` with the next frame of the source, looping at its end if it may.
	/// @return Ok, or why there is none (Starving, End, Failed).
	ReadStatus fetch();
	/// @brief Steps the converter until t is below 1, fetching frames.
	/// @return Ok, or why it can't go on.
	ReadStatus advance();
	/// @brief Makes the next frames of the source, at the rate, into scratch.
	/// @param frames How many.
	/// @return How many it made (fewer when it ends or runs dry).
	std::size_t pull(std::size_t frames);
	/// @brief Notes that the source ended or failed: the voice stops there.
	/// @param status End or Failed.
	void finish(ReadStatus status);
};

/// @brief An engine's state: the voices, the lock, the master gain and the meter, and where events go. Shared with
/// the voices' handles, so it may outlive the engine (closed then).
struct Shared {
	mutable std::mutex mutex;                        ///< Guards everything here but the atomics.
	std::condition_variable wake;                    ///< Wakes the event thread (a voice was made, or closing).
	Format format;                                   ///< The mix's format.
	Limits limits;                                   ///< See Engine::limits().
	std::vector<std::shared_ptr<VoiceCore>> voices;  ///< The voices kept.
	float master = 1;                                ///< See Engine::masterVolume().
	Levels levels;                                   ///< See Engine::levels().
	bool closed = false;                             ///< The engine is gone.
	bool stopping = false;                           ///< The event thread is to end.
	std::atomic<Executor*> executor{nullptr};        ///< Where events go.
	std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true); ///< For posted events.
	std::shared_ptr<std::atomic<std::size_t>> queued = std::make_shared<std::atomic<std::size_t>>(0); ///< Posted.

	/// @brief Mixes every playing voice into a buffer, under the lock, then applies the master gain and measures.
	/// @param out frames * format.channels floats, overwritten.
	/// @param frames How many.
	void mix(float* out, std::size_t frames);
	/// @brief Posts the voices' events and forgets the voices that are released and not playing. Not on the audio
	/// thread.
	void gather();
};

} // namespace affresco::audio::detail
