/// @file
/// @brief The decoding readers: files and bytes in memory (on the audio thread), and encoded streams (on a thread of
/// their own), through miniaudio's decoders.

#include "Decoders.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <system_error>
#include <thread>

namespace affresco::audio::detail {

namespace {

/// Seek points an MP3's table has, so that a seek doesn't decode from the start.
constexpr ma_uint32 mp3SeekPoints = 1024;
/// Frames the stream's thread decodes at a time.
constexpr std::size_t decodeFrames = 1024;

/// @brief miniaudio's name for an encoding.
/// @param hint The encoding.
/// @return Its value.
ma_encoding_format encodingFormat(Encoding hint) noexcept {
	switch(hint) {
	case Encoding::Wav: return ma_encoding_format_wav;
	case Encoding::Flac: return ma_encoding_format_flac;
	case Encoding::Mp3: return ma_encoding_format_mp3;
	case Encoding::Unknown: break;
	}
	return ma_encoding_format_unknown;
}

/// @brief A decoder's configuration: float frames in the engine's format, the encoding if known.
/// @param hint The encoding.
/// @param output The engine's format.
/// @return The configuration.
ma_decoder_config configuration(Encoding hint, const Format& output) noexcept {
	ma_decoder_config config = ma_decoder_config_init(ma_format_f32, output.channels, output.sampleRate);
	config.encodingFormat = encodingFormat(hint);
	config.seekPointCount = mp3SeekPoints;
	return config;
}

/// @brief The most frames a sound of the engine plays.
/// @param output The engine's format.
/// @param limits Its limits.
/// @return Frames.
std::uint64_t maxFramesOf(const Format& output, const Limits& limits) noexcept {
	const double frames = std::max(limits.seconds, 0.0) * output.sampleRate;
	return frames >= 1.8e19 ? UINT64_MAX : static_cast<std::uint64_t>(frames);
}

/// @brief Decodes a file or bytes in memory as the voice reads it.
class DecoderReader final : public Reader {
public:
	/// @brief Takes a decoder that is set up.
	/// @param decoder The decoder.
	/// @param bytes The bytes it decodes from memory (kept while it lives), or null for a file.
	DecoderReader(std::unique_ptr<ma_decoder> decoder, std::shared_ptr<const std::vector<std::uint8_t>> bytes)
	    : decoder_(std::move(decoder)), bytes_(std::move(bytes)) {
		ma_uint64 frames = 0;
		if(ma_decoder_get_length_in_pcm_frames(decoder_.get(), &frames) == MA_SUCCESS && frames > 0) length_ = frames;
	}
	DecoderReader(const DecoderReader&) = delete;
	DecoderReader& operator=(const DecoderReader&) = delete;
	DecoderReader(DecoderReader&&) = delete;
	DecoderReader& operator=(DecoderReader&&) = delete;
	/// @brief Closes the decoder.
	~DecoderReader() override { ma_decoder_uninit(decoder_.get()); }

	/// @brief Decodes the next frames.
	/// @param out Room for them.
	/// @param frames How many.
	/// @return What it decoded.
	ReadResult read(float* out, std::size_t frames) override {
		if(failed_) return {0, ReadStatus::Failed};
		ma_uint64 read = 0;
		const ma_result result = ma_decoder_read_pcm_frames(decoder_.get(), out, frames, &read);
		if(result != MA_SUCCESS && result != MA_AT_END && read == 0) {
			failed_ = true;
			return {0, ReadStatus::Failed};
		}
		const auto count = static_cast<std::size_t>(read);
		return {count, count < frames ? ReadStatus::End : ReadStatus::Ok};
	}
	/// @brief Moves the decoder.
	/// @param frame The frame.
	/// @return Whether it could.
	bool seek(std::uint64_t frame) override {
		failed_ = false;
		return ma_decoder_seek_to_pcm_frame(decoder_.get(), frame) == MA_SUCCESS;
	}
	/// @brief Files and bytes in memory are.
	[[nodiscard]] bool seekable() const override { return true; }
	/// @brief The decoder's length, if it knows it.
	[[nodiscard]] std::optional<std::uint64_t> length() const override { return length_; }
	/// @brief Why it failed.
	[[nodiscard]] const char* failure() const override { return failed_ ? "the sound couldn't be decoded" : nullptr; }

	/// @brief Refuses a sound longer than the limit, when its length is known.
	/// @param output The engine's format.
	/// @param limits The limits.
	/// @param error Receives why.
	/// @return Whether it may play.
	bool withinLimits(const Format& output, const Limits& limits, std::string& error) const {
		if(!length_ || *length_ <= maxFramesOf(output, limits)) return true;
		error = "it plays for longer than the limit (" + std::to_string(std::lround(limits.seconds)) + " seconds)";
		return false;
	}

private:
	std::unique_ptr<ma_decoder> decoder_;                      ///< The decoder.
	std::shared_ptr<const std::vector<std::uint8_t>> bytes_;   ///< See the constructor.
	std::optional<std::uint64_t> length_;                      ///< See length().
	bool failed_ = false;                                      ///< The last read failed.
};

/// @brief Decodes a stream of encoded bytes on its own thread, into a ring the voice reads.
class StreamReader final : public Reader {
public:
	/// @brief Starts the thread.
	/// @param bytes The stream's bytes.
	/// @param hint The encoding.
	/// @param output The engine's format.
	/// @param limits The limits (its duration).
	StreamReader(std::shared_ptr<ByteQueue> bytes, Encoding hint, const Format& output, const Limits& limits)
	    : bytes_(std::move(bytes)), ring_(std::max<std::size_t>(output.sampleRate / 2, decodeFrames), output.channels),
	      config_(configuration(hint, output)), maxFrames_(maxFramesOf(output, limits)) {
		config_.seekPointCount = 0; // a stream isn't seekable: no table (it would read the whole stream first)
		thread_ = std::thread([this] { decode(); });
	}
	StreamReader(const StreamReader&) = delete;
	StreamReader& operator=(const StreamReader&) = delete;
	StreamReader(StreamReader&&) = delete;
	StreamReader& operator=(StreamReader&&) = delete;
	/// @brief Stops the thread: its reads and seeks fail at once.
	~StreamReader() override {
		stop_ = true;
		bytes_->cancel();
		thread_.join();
	}

	/// @brief Takes decoded frames from the ring.
	/// @param out Room for them.
	/// @param frames How many.
	/// @return What it took; Starving while the stream may still bring more.
	ReadResult read(float* out, std::size_t frames) override {
		const bool done = ring_.finished.load(std::memory_order_acquire);
		const std::size_t count = ring_.read(out, frames);
		if(count == frames) return {count, ReadStatus::Ok};
		if(!done || ring_.available() > 0) return {count, ReadStatus::Starving};
		return {count, failed_.load(std::memory_order_acquire) ? ReadStatus::Failed : ReadStatus::End};
	}
	/// @brief Why it failed.
	[[nodiscard]] const char* failure() const override {
		return failed_.load(std::memory_order_acquire) ? failure_.c_str() : nullptr;
	}

private:
	/// @brief miniaudio's read callback: blocks until the bytes arrive, the stream ends, or the reader closes.
	/// @param decoder The decoder (its user data is the reader).
	/// @param out Room for the bytes.
	/// @param count How many.
	/// @param read Receives how many it read.
	/// @return MA_SUCCESS, or MA_AT_END with none.
	static ma_result onRead(ma_decoder* decoder, void* out, size_t count, size_t* read) {
		auto& self = *static_cast<StreamReader*>(decoder->pUserData);
		const std::size_t taken = self.bytes_->read(static_cast<std::uint8_t*>(out), count);
		if(read) *read = taken;
		return taken == 0 && count > 0 ? MA_AT_END : MA_SUCCESS;
	}
	/// @brief miniaudio's seek callback: within the bytes kept, or forward as they arrive.
	/// @param decoder The decoder.
	/// @param offset The offset.
	/// @param origin From where.
	/// @return MA_SUCCESS, or MA_ERROR where it can't.
	static ma_result onSeek(ma_decoder* decoder, ma_int64 offset, ma_seek_origin origin) {
		auto& self = *static_cast<StreamReader*>(decoder->pUserData);
		ByteQueue& bytes = *self.bytes_;
		std::int64_t from = 0;
		if(origin == ma_seek_origin_current) from = static_cast<std::int64_t>(bytes.position());
		else if(origin == ma_seek_origin_end) {
			const std::uint64_t length = bytes.knownLength();
			if(length == 0) return MA_ERROR; // the end of a stream still arriving isn't known
			from = static_cast<std::int64_t>(length);
		}
		const std::int64_t target = from + offset;
		if(target < 0) return MA_ERROR;
		return bytes.seek(static_cast<std::uint64_t>(target)) ? MA_SUCCESS : MA_ERROR;
	}

	/// @brief Notes a failure: the voice ends with it.
	/// @param why What went wrong.
	void fail(std::string why) {
		failure_ = std::move(why);
		failed_.store(true, std::memory_order_release);
		ring_.finished.store(true, std::memory_order_release);
	}

	/// @brief The thread: sets the decoder up, then decodes into the ring until the end or a stop.
	void decode() {
		ma_decoder decoder{};
		if(ma_decoder_init(onRead, onSeek, this, &config_, &decoder) != MA_SUCCESS) {
			if(!stop_) fail("the stream couldn't be decoded (WAV, FLAC or MP3)");
			return;
		}
		bytes_->stopKeeping();
		std::vector<float> frames(decodeFrames * ring_.channels);
		std::uint64_t total = 0;
		while(!stop_) {
			if(ring_.space() < decodeFrames) { // the voice is paused, or plays slower than this decodes
				std::this_thread::sleep_for(std::chrono::milliseconds{5});
				continue;
			}
			const auto room = static_cast<ma_uint64>(std::min<std::uint64_t>(decodeFrames, maxFrames_ - total));
			ma_uint64 read = 0;
			const ma_result result = room == 0 ? MA_AT_END : ma_decoder_read_pcm_frames(&decoder, frames.data(), room, &read);
			if(read > 0) {
				ring_.write(frames.data(), static_cast<std::size_t>(read));
				total += read;
			}
			if(result != MA_SUCCESS || read < room || total >= maxFrames_) {
				if(result != MA_SUCCESS && result != MA_AT_END && !stop_) fail("the stream couldn't be decoded");
				break;
			}
		}
		ma_decoder_uninit(&decoder);
		ring_.finished.store(true, std::memory_order_release);
	}

	std::shared_ptr<ByteQueue> bytes_; ///< The stream's bytes.
	FrameRing ring_;                   ///< Decoded frames, half a second.
	ma_decoder_config config_;         ///< The decoder's configuration.
	std::uint64_t maxFrames_;          ///< The most frames decoded.
	std::string failure_;              ///< Why it failed (set before failed_).
	std::atomic<bool> failed_{false};  ///< It failed.
	std::atomic<bool> stop_{false};    ///< The reader closes.
	std::thread thread_;               ///< Decodes.
};

/// @brief Sets a file's decoder up, with the path as the platform spells it.
/// @param path The file.
/// @param config The configuration.
/// @param decoder The decoder.
/// @return miniaudio's result.
ma_result initFile(const std::filesystem::path& path, const ma_decoder_config& config, ma_decoder* decoder) {
#ifdef _WIN32
	return ma_decoder_init_file_w(path.c_str(), &config, decoder);
#else
	return ma_decoder_init_file(path.c_str(), &config, decoder);
#endif
}

} // namespace

std::unique_ptr<Reader> openFile(const std::filesystem::path& path, Encoding hint, const Format& output,
                                 const Limits& limits, std::string& error) {
	const std::string name = path.filename().string();
	std::error_code failed;
	const std::uintmax_t size = std::filesystem::file_size(path, failed);
	if(failed || !std::filesystem::is_regular_file(path, failed)) {
		error = "'" + name + "' isn't a file that can be read";
		return nullptr;
	}
	if(size > limits.fileBytes) {
		error = "'" + name + "' is larger than the limit (" + std::to_string(limits.fileBytes) + " bytes)";
		return nullptr;
	}
	if(hint == Encoding::Unknown) hint = encodingOf(name);
	auto decoder = std::make_unique<ma_decoder>();
	const ma_decoder_config config = configuration(hint, output);
	if(initFile(path, config, decoder.get()) != MA_SUCCESS) {
		error = "'" + name + "' couldn't be decoded (WAV, FLAC or MP3)";
		return nullptr;
	}
	auto reader = std::make_unique<DecoderReader>(std::move(decoder), nullptr);
	if(!reader->withinLimits(output, limits, error)) {
		error = "'" + name + "': " + error;
		return nullptr;
	}
	return reader;
}

std::unique_ptr<Reader> openMemory(std::shared_ptr<const std::vector<std::uint8_t>> bytes, Encoding hint,
                                   const Format& output, const Limits& limits, std::string& error) {
	if(!bytes || bytes->empty()) {
		error = "no bytes to decode";
		return nullptr;
	}
	if(bytes->size() > limits.memoryBytes) {
		error = "the bytes are more than the limit (" + std::to_string(limits.memoryBytes) + ")";
		return nullptr;
	}
	auto decoder = std::make_unique<ma_decoder>();
	const ma_decoder_config config = configuration(hint, output);
	if(ma_decoder_init_memory(bytes->data(), bytes->size(), &config, decoder.get()) != MA_SUCCESS) {
		error = "the bytes couldn't be decoded (WAV, FLAC or MP3)";
		return nullptr;
	}
	auto reader = std::make_unique<DecoderReader>(std::move(decoder), std::move(bytes));
	if(!reader->withinLimits(output, limits, error)) return nullptr;
	return reader;
}

std::unique_ptr<Reader> openEncodedStream(std::shared_ptr<ByteQueue> bytes, Encoding hint, const Format& output,
                                          const Limits& limits) {
	return std::make_unique<StreamReader>(std::move(bytes), hint, output, limits);
}

} // namespace affresco::audio::detail
