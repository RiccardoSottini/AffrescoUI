#pragma once

/// @file
/// @brief audio::Source and its kinds: a file, encoded bytes in memory, a stream of encoded bytes or of PCM frames
/// pushed from any thread, and a generator called for samples. A source only describes the sound: a Voice opens it to
/// play it (Engine::createVoice()).

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::audio {

namespace detail {
class Reader;
struct FrameRing;
struct ByteQueue;
} // namespace detail

/// @brief A layout of interleaved 32-bit float frames: how many per second, and how many samples in each.
struct Format {
	std::uint32_t sampleRate = 48000; ///< Frames per second, from 8,000 to 384,000.
	std::uint32_t channels = 2;       ///< Samples per frame, from 1 to 8 (1 mono, 2 left and right).
	/// @brief Two formats are equal when both values are.
	/// @param other The other format.
	/// @return Whether they are.
	[[nodiscard]] bool operator==(const Format& other) const noexcept = default;
};

/// @brief Whether a format can be played: a rate from 8,000 to 384,000 and 1 to 8 channels.
/// @param format The format.
/// @return Whether it can.
[[nodiscard]] constexpr bool valid(const Format& format) noexcept {
	return format.sampleRate >= 8000 && format.sampleRate <= 384000 && format.channels >= 1 && format.channels <= 8;
}

/// @brief What an engine accepts (EngineOptions::limits); a voice whose source passes one is refused, or ends there.
struct Limits {
	std::uint64_t fileBytes = std::uint64_t{1} << 30;    ///< The largest file a voice opens (1 GiB).
	std::size_t memoryBytes = std::size_t{256} << 20;    ///< The largest encoded bytes in memory (256 MiB).
	double seconds = 24.0 * 60 * 60;                     ///< The longest a file or bytes in memory plays (a day).
	std::size_t voices = 64;                             ///< The most voices an engine keeps at once.
};

/// @brief How loud a signal was over the last block mixed: the first two channels (a mono signal repeats its one).
struct Levels {
	std::array<float, 2> peak{}; ///< The largest absolute sample, 0 to 1 (more if it clips).
	std::array<float, 2> rms{};  ///< The root mean square of the samples.
};

/// @brief An encoded format, as a hint to the decoder: Unknown tries each it has (WAV, FLAC, MP3).
enum class Encoding : std::uint8_t {
	Unknown, ///< Try each decoder.
	Wav,     ///< RIFF WAVE (PCM, IEEE float, A-law, mu-law, ADPCM).
	Flac,    ///< FLAC.
	Mp3,     ///< MPEG-1/2 layer III.
};

/// @brief Guesses an encoding from a file name or a URL's path: ".wav", ".flac" or ".mp3", in any case.
/// @param name The name.
/// @return The encoding, or Unknown.
[[nodiscard]] Encoding encodingOf(std::string_view name) noexcept;

/// @brief Opens encoded bytes with the decoders, as a voice would, to tell whether they play and how long they last
/// (e.g. a file the person attached, before it is kept). Nothing is played.
/// @param bytes The bytes (WAV, FLAC or MP3), at most Limits::memoryBytes.
/// @param hint Their encoding.
/// @param error Receives why they don't decode.
/// @return Their length in seconds, or std::nullopt if they don't decode; a length the decoder can't tell is 0.
[[nodiscard]] std::optional<double> probeSeconds(std::span<const std::uint8_t> bytes, Encoding hint,
                                                 std::string& error);

/// @brief Fills frames for a generator source (synthesis): interleaved floats in the engine's Format.
///
/// Called on the audio thread, so it must not block, allocate much or throw. @p first is the index of the first frame
/// asked for (since the start, or since a seek): a generator that computes from it is seekable and loops.
/// @return The frames written, from 0 to frames.size() / format.channels; fewer than asked ends the sound.
using Generator = std::function<std::size_t(std::span<float> frames, std::uint64_t first, const Format& format)>;

/// @brief A sound to play: what a Voice opens. Make one with a factory (file(), memory(), generator(), tone()), or a
/// PcmStream or EncodedStream for data that arrives while it plays.
///
/// A file, bytes in memory and a generator may play on several voices at once, each from its own position; a stream
/// plays on one voice only. Sources are immutable and safe to share between threads.
class Source {
public:
	Source() = default;
	Source(const Source&) = delete;
	Source& operator=(const Source&) = delete;
	Source(Source&&) = delete;
	Source& operator=(Source&&) = delete;
	virtual ~Source();

	/// @brief An audio file, decoded while it plays, a little at a time (never read whole). Nothing is read until a
	/// voice opens it: then it must exist, be at most Limits::fileBytes, and decode as WAV, FLAC or MP3.
	/// @param path The file.
	/// @param hint Its encoding, if known (else guessed from its extension).
	/// @return The source.
	[[nodiscard]] static std::shared_ptr<Source> file(std::filesystem::path path, Encoding hint = Encoding::Unknown);
	/// @brief An encoded file's bytes, kept in memory (shared by the voices that play them), at most
	/// Limits::memoryBytes.
	/// @param bytes The bytes.
	/// @param hint Their encoding, if known.
	/// @return The source.
	[[nodiscard]] static std::shared_ptr<Source> memory(std::vector<std::uint8_t> bytes,
	                                                    Encoding hint = Encoding::Unknown);
	/// @brief Samples a function makes as they are played (a synthesiser, a test signal).
	/// @param generate Fills the frames (see Generator).
	/// @param seconds How long it lasts, if known: its duration, and where it ends.
	/// @param seekable Whether it computes frames from their index, so that seek() and loops work.
	/// @return The source, or nullptr without a function.
	[[nodiscard]] static std::shared_ptr<Source> generator(Generator generate, std::optional<double> seconds = {},
	                                                       bool seekable = false);
	/// @brief A sine tone with 5 ms ramps at both ends (an alert, a beep, a test signal); seekable.
	/// @param frequency Hertz, from 1 to 20,000.
	/// @param seconds How long, more than 0 and at most a day.
	/// @param amplitude Its peak, from 0 to 1.
	/// @return The source, or nullptr for values out of range.
	[[nodiscard]] static std::shared_ptr<Source> tone(double frequency, double seconds, float amplitude = 0.5F);

	/// @brief Opens the source for one voice: a reader making frames in the engine's format. Called by the engine.
	/// @param output The engine's format.
	/// @param limits The engine's limits.
	/// @param error Receives why it can't be opened.
	/// @return The reader, or nullptr.
	[[nodiscard]] virtual std::unique_ptr<detail::Reader> open(const Format& output, const Limits& limits,
	                                                           std::string& error) = 0;
};

/// @brief Raw PCM frames pushed as they arrive (a voice assistant's speech, a network call, a decoder of the
/// application's own), from any thread, into a bounded ring; the engine converts their rate and channels. When the
/// ring runs dry before finish(), the voice plays silence and reports an underrun.
class PcmStream final : public Source {
public:
	/// The most frames a ring may hold.
	static constexpr std::size_t maxCapacity = std::size_t{1} << 23;

	/// @brief Makes a stream.
	/// @param format The frames' rate and channels (valid()).
	/// @param capacity The most frames buffered, from 1 to maxCapacity (e.g. 2 seconds' worth).
	/// @return The stream, or nullptr for values out of range.
	[[nodiscard]] static std::shared_ptr<PcmStream> create(Format format, std::size_t capacity);

	/// @brief Use create().
	/// @param format The frames' rate and channels.
	/// @param capacity The most frames buffered.
	PcmStream(Format format, std::size_t capacity);
	PcmStream(const PcmStream&) = delete;
	PcmStream& operator=(const PcmStream&) = delete;
	PcmStream(PcmStream&&) = delete;
	PcmStream& operator=(PcmStream&&) = delete;
	~PcmStream() override;

	/// @brief Adds interleaved float frames (from -1 to 1), as many whole frames as there is room for. Any thread;
	/// never blocks.
	/// @param samples The samples, a multiple of the channels (a partial frame at the end is ignored).
	/// @return The frames taken: fewer than given when the ring is full, 0 after finish().
	std::size_t push(std::span<const float> samples);
	/// @brief Adds interleaved 16-bit frames, as many whole frames as there is room for. Any thread; never blocks.
	/// @param samples The samples, a multiple of the channels.
	/// @return The frames taken.
	std::size_t push(std::span<const std::int16_t> samples);
	/// @brief Says that no more frames come: the voice ends once it has played those buffered.
	void finish() noexcept;
	/// @brief Whether finish() was called.
	[[nodiscard]] bool finished() const noexcept;
	/// @brief The frames buffered and not yet played.
	[[nodiscard]] std::size_t buffered() const noexcept;
	/// @brief The most frames it buffers.
	[[nodiscard]] std::size_t capacity() const noexcept;
	/// @brief The frames' format.
	[[nodiscard]] Format format() const noexcept { return format_; }

	/// @brief Opens it for the one voice that plays it.
	/// @param output The engine's format.
	/// @param limits The engine's limits.
	/// @param error Receives why it can't be opened (another voice has it).
	/// @return The reader, or nullptr.
	[[nodiscard]] std::unique_ptr<detail::Reader> open(const Format& output, const Limits& limits,
	                                                   std::string& error) override;

private:
	Format format_;                           ///< See format().
	std::shared_ptr<detail::FrameRing> ring_; ///< The frames, shared with the reader.
	std::atomic<bool> opened_{false};         ///< A voice has it.
};

/// @brief Encoded bytes pushed as they arrive (a file downloading, a radio stream), from any thread, into a bounded
/// buffer; a thread of the voice that plays it decodes them (WAV, FLAC or MP3). It isn't seekable and has no duration.
class EncodedStream final : public Source {
public:
	/// The most bytes it may buffer.
	static constexpr std::size_t maxCapacity = std::size_t{256} << 20;

	/// @brief Makes a stream.
	/// @param capacity The most bytes buffered and not yet decoded, from 1 KiB to maxCapacity.
	/// @param hint The encoding, if known (the decoder then doesn't try the others).
	/// @return The stream, or nullptr for a capacity out of range.
	[[nodiscard]] static std::shared_ptr<EncodedStream> create(std::size_t capacity,
	                                                           Encoding hint = Encoding::Unknown);

	/// @brief Use create().
	/// @param capacity The most bytes buffered.
	/// @param hint The encoding.
	EncodedStream(std::size_t capacity, Encoding hint);
	EncodedStream(const EncodedStream&) = delete;
	EncodedStream& operator=(const EncodedStream&) = delete;
	EncodedStream(EncodedStream&&) = delete;
	EncodedStream& operator=(EncodedStream&&) = delete;
	~EncodedStream() override;

	/// @brief Adds bytes, all or none. Any thread; never blocks.
	/// @param bytes The bytes.
	/// @return false if they don't fit in the room left (the voice is paused, or decodes slower than they arrive),
	/// or after finish().
	bool push(std::span<const std::uint8_t> bytes);
	/// @brief Adds bytes, all or none (e.g. a network chunk as text).
	/// @param bytes The bytes.
	/// @return As push().
	bool push(std::string_view bytes);
	/// @brief Says that no more bytes come: the voice ends once they are decoded and played.
	void finish() noexcept;
	/// @brief The bytes buffered and not yet decoded.
	[[nodiscard]] std::size_t buffered() const noexcept;
	/// @brief The most bytes it buffers.
	[[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

	/// @brief Opens it for the one voice that plays it, starting its decoding thread.
	/// @param output The engine's format.
	/// @param limits The engine's limits.
	/// @param error Receives why it can't be opened.
	/// @return The reader, or nullptr.
	[[nodiscard]] std::unique_ptr<detail::Reader> open(const Format& output, const Limits& limits,
	                                                   std::string& error) override;

private:
	std::size_t capacity_;                    ///< See capacity().
	Encoding hint_;                           ///< The encoding.
	std::shared_ptr<detail::ByteQueue> bytes_; ///< The bytes, shared with the decoding thread.
	std::atomic<bool> opened_{false};         ///< A voice has it.
};

} // namespace affresco::audio
