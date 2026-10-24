/// @file
/// @brief The decoding readers: files and bytes in memory (on the audio thread), and encoded streams (on a thread of
/// their own), through miniaudio's decoders.
///
/// Every decoder reads through a virtual file system of this module (BoundedVfs), never miniaudio's own: a seek past
/// the end of the bytes fails. miniaudio's memory and file readers clamp such a seek and report success, so a WAV
/// chunk whose size underflows made its parser step through 2^64 bytes, 2 GiB at a time, and hang (found by
/// tests/fuzz/audio.cpp, kept in regressions/audio/).

#include "Decoders.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <system_error>
#include <thread>

namespace affresco::audio::detail {

namespace {

/// Seek points an MP3's table has, so that a seek doesn't decode from the start.
constexpr ma_uint32 mp3SeekPoints = 1024;
/// Frames the stream's thread decodes at a time.
constexpr std::size_t decodeFrames = 1024;

/// @brief Bytes a decoder reads: they say how many there are, if known, and refuse to seek outside them.
class ByteSource {
public:
	ByteSource() = default;
	ByteSource(const ByteSource&) = delete;
	ByteSource& operator=(const ByteSource&) = delete;
	ByteSource(ByteSource&&) = delete;
	ByteSource& operator=(ByteSource&&) = delete;
	virtual ~ByteSource() = default;
	/// @brief Reads the next bytes.
	/// @param out Room for them.
	/// @param count How many.
	/// @return How many it read: fewer only at the end.
	virtual std::size_t read(std::uint8_t* out, std::size_t count) = 0;
	/// @brief Moves to an offset within the bytes.
	/// @param offset From the start, at most size().
	/// @return Whether it could.
	virtual bool seek(std::uint64_t offset) = 0;
	/// @brief The offset of the next byte read.
	[[nodiscard]] virtual std::uint64_t tell() const = 0;
	/// @brief How many bytes there are, or 0 while that isn't known (a stream still arriving).
	[[nodiscard]] virtual std::uint64_t size() const = 0;
};

/// @brief Bytes in memory, shared with their source.
class MemoryBytes final : public ByteSource {
public:
	/// @brief The bytes.
	/// @param bytes Them.
	explicit MemoryBytes(std::shared_ptr<const std::vector<std::uint8_t>> bytes) : bytes_(std::move(bytes)) {}
	/// @brief Copies the next bytes.
	/// @param out Room.
	/// @param count How many.
	/// @return How many.
	std::size_t read(std::uint8_t* out, std::size_t count) override {
		const std::size_t taken = std::min(count, bytes_->size() - at_);
		std::copy_n(bytes_->begin() + static_cast<std::ptrdiff_t>(at_), taken, out);
		at_ += taken;
		return taken;
	}
	/// @brief Moves within the bytes.
	/// @param offset The offset.
	/// @return Whether it is within them.
	bool seek(std::uint64_t offset) override {
		if(offset > bytes_->size()) return false;
		at_ = static_cast<std::size_t>(offset);
		return true;
	}
	/// @brief Where it is.
	[[nodiscard]] std::uint64_t tell() const override { return at_; }
	/// @brief How many bytes.
	[[nodiscard]] std::uint64_t size() const override { return bytes_->size(); }

private:
	std::shared_ptr<const std::vector<std::uint8_t>> bytes_; ///< The bytes.
	std::size_t at_ = 0;                                     ///< See tell().
};

/// @brief A file, read through the C library, its size taken when it opens (a file that grows isn't read further).
class FileBytes final : public ByteSource {
public:
	/// @brief Opens a file.
	/// @param path The file.
	/// @param size Its size.
	FileBytes(const std::filesystem::path& path, std::uint64_t size) : size_(size) {
#ifdef _WIN32
		if(_wfopen_s(&file_, path.c_str(), L"rb") != 0) file_ = nullptr;
#else
		file_ = std::fopen(path.c_str(), "rb"); // NOLINT(cppcoreguidelines-owning-memory): closed by the destructor
#endif
	}
	/// @brief Closes it.
	~FileBytes() override {
		if(file_) std::fclose(file_); // NOLINT(cppcoreguidelines-owning-memory)
	}
	FileBytes(const FileBytes&) = delete;
	FileBytes& operator=(const FileBytes&) = delete;
	FileBytes(FileBytes&&) = delete;
	FileBytes& operator=(FileBytes&&) = delete;
	/// @brief Whether it opened.
	[[nodiscard]] bool opened() const noexcept { return file_ != nullptr; }
	/// @brief Reads the next bytes, up to the size it had.
	/// @param out Room.
	/// @param count How many.
	/// @return How many.
	std::size_t read(std::uint8_t* out, std::size_t count) override {
		count = static_cast<std::size_t>(std::min<std::uint64_t>(count, size_ - std::min(at_, size_)));
		const std::size_t taken = count == 0 ? 0 : std::fread(out, 1, count, file_);
		at_ += taken;
		return taken;
	}
	/// @brief Moves within the file.
	/// @param offset The offset.
	/// @return Whether it is within it.
	bool seek(std::uint64_t offset) override {
		if(offset > size_) return false;
#ifdef _WIN32
		if(_fseeki64(file_, static_cast<long long>(offset), SEEK_SET) != 0) return false;
#else
		if(fseeko(file_, static_cast<off_t>(offset), SEEK_SET) != 0) return false;
#endif
		at_ = offset;
		return true;
	}
	/// @brief Where it is.
	[[nodiscard]] std::uint64_t tell() const override { return at_; }
	/// @brief Its size when it opened.
	[[nodiscard]] std::uint64_t size() const override { return size_; }

private:
	std::FILE* file_ = nullptr; ///< The file.
	std::uint64_t size_;        ///< See size().
	std::uint64_t at_ = 0;      ///< See tell().
};

/// @brief A stream's bytes, which a read waits for.
class QueueBytes final : public ByteSource {
public:
	/// @brief The stream's queue.
	/// @param queue It.
	explicit QueueBytes(std::shared_ptr<ByteQueue> queue) : queue_(std::move(queue)) {}
	/// @brief Waits for the next bytes.
	/// @param out Room.
	/// @param count How many.
	/// @return How many: fewer at the stream's end, or once it is cancelled.
	std::size_t read(std::uint8_t* out, std::size_t count) override { return queue_->read(out, count); }
	/// @brief Moves within the bytes kept, or forward as they arrive.
	/// @param offset The offset.
	/// @return Whether it could.
	bool seek(std::uint64_t offset) override { return queue_->seek(offset); }
	/// @brief Where it is.
	[[nodiscard]] std::uint64_t tell() const override { return queue_->position(); }
	/// @brief The length, once every byte has come.
	[[nodiscard]] std::uint64_t size() const override { return queue_->knownLength(); }

private:
	std::shared_ptr<ByteQueue> queue_; ///< The stream's bytes.
};

/// @brief miniaudio's virtual file system over one ByteSource: its one "file" is the source, whatever the path.
/// The callbacks come first, as miniaudio reads them through the pointer it is given.
struct BoundedVfs {
	ma_vfs_callbacks callbacks{}; ///< What miniaudio calls.
	ByteSource* source = nullptr; ///< The bytes.

	/// @brief The source behind a file handle.
	/// @param file The handle open() gave.
	/// @return The source.
	static ByteSource& of(ma_vfs_file file) { return *static_cast<ByteSource*>(file); }

	/// @brief Sets the callbacks up over a source, which must outlive the decoder.
	/// @param bytes The source.
	explicit BoundedVfs(ByteSource& bytes) : source(&bytes) {
		callbacks.onOpen = [](ma_vfs* vfs, const char*, ma_uint32, ma_vfs_file* file) {
			*file = static_cast<BoundedVfs*>(vfs)->source;
			return MA_SUCCESS;
		};
		callbacks.onOpenW = [](ma_vfs* vfs, const wchar_t*, ma_uint32, ma_vfs_file* file) {
			*file = static_cast<BoundedVfs*>(vfs)->source;
			return MA_SUCCESS;
		};
		callbacks.onClose = [](ma_vfs*, ma_vfs_file) { return MA_SUCCESS; };
		callbacks.onRead = [](ma_vfs*, ma_vfs_file file, void* out, size_t count, size_t* read) {
			const std::size_t taken = of(file).read(static_cast<std::uint8_t*>(out), count);
			if(read) *read = taken;
			return taken == 0 && count > 0 ? MA_AT_END : MA_SUCCESS;
		};
		callbacks.onWrite = [](ma_vfs*, ma_vfs_file, const void*, size_t, size_t*) { return MA_ACCESS_DENIED; };
		callbacks.onSeek = [](ma_vfs*, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin) {
			ByteSource& bytes = of(file);
			std::int64_t from = 0;
			if(origin == ma_seek_origin_current) from = static_cast<std::int64_t>(bytes.tell());
			else if(origin == ma_seek_origin_end) {
				if(bytes.size() == 0) return MA_BAD_SEEK; // a stream's end isn't known yet
				from = static_cast<std::int64_t>(bytes.size());
			}
			if((offset > 0 && from > INT64_MAX - offset) || from + offset < 0) return MA_BAD_SEEK;
			return bytes.seek(static_cast<std::uint64_t>(from + offset)) ? MA_SUCCESS : MA_BAD_SEEK;
		};
		callbacks.onTell = [](ma_vfs*, ma_vfs_file file, ma_int64* cursor) {
			*cursor = static_cast<ma_int64>(of(file).tell());
			return MA_SUCCESS;
		};
		callbacks.onInfo = [](ma_vfs*, ma_vfs_file file, ma_file_info* info) {
			info->sizeInBytes = of(file).size();
			return info->sizeInBytes == 0 ? MA_NOT_IMPLEMENTED : MA_SUCCESS;
		};
	}
	BoundedVfs(const BoundedVfs&) = delete;
	BoundedVfs& operator=(const BoundedVfs&) = delete;
	BoundedVfs(BoundedVfs&&) = delete;
	BoundedVfs& operator=(BoundedVfs&&) = delete;
	~BoundedVfs() = default;

	/// @brief Sets a decoder up on the source.
	/// @param config The decoder's configuration.
	/// @param decoder The decoder.
	/// @return miniaudio's result.
	ma_result init(const ma_decoder_config& config, ma_decoder* decoder) {
		static_assert(std::is_standard_layout_v<BoundedVfs>, "miniaudio reads the callbacks through the pointer");
		return ma_decoder_init_vfs(this, "sound", &config, decoder);
	}
};

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
/// @param seekPoints An MP3's seek points (0 for none).
/// @return The configuration.
ma_decoder_config configuration(Encoding hint, const Format& output, ma_uint32 seekPoints) noexcept {
	ma_decoder_config config = ma_decoder_config_init(ma_format_f32, output.channels, output.sampleRate);
	config.encodingFormat = encodingFormat(hint);
	config.seekPointCount = seekPoints;
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
	/// @brief A reader of bytes; open() sets the decoder up.
	/// @param bytes The bytes.
	explicit DecoderReader(std::unique_ptr<ByteSource> bytes) : bytes_(std::move(bytes)), vfs_(*bytes_) {}
	DecoderReader(const DecoderReader&) = delete;
	DecoderReader& operator=(const DecoderReader&) = delete;
	DecoderReader(DecoderReader&&) = delete;
	DecoderReader& operator=(DecoderReader&&) = delete;
	/// @brief Closes the decoder.
	~DecoderReader() override {
		if(open_) ma_decoder_uninit(&decoder_);
	}

	/// @brief Sets the decoder up and reads its length.
	/// @param config The decoder's configuration.
	/// @return Whether it decodes.
	bool open(const ma_decoder_config& config) {
		open_ = vfs_.init(config, &decoder_) == MA_SUCCESS;
		if(!open_) return false;
		ma_uint64 frames = 0;
		if(ma_decoder_get_length_in_pcm_frames(&decoder_, &frames) == MA_SUCCESS && frames > 0) length_ = frames;
		return true;
	}

	/// @brief Decodes the next frames.
	/// @param out Room for them.
	/// @param frames How many.
	/// @return What it decoded.
	ReadResult read(float* out, std::size_t frames) override {
		if(failed_) return {0, ReadStatus::Failed};
		ma_uint64 read = 0;
		const ma_result result = ma_decoder_read_pcm_frames(&decoder_, out, frames, &read);
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
		return ma_decoder_seek_to_pcm_frame(&decoder_, frame) == MA_SUCCESS;
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
	std::unique_ptr<ByteSource> bytes_;   ///< What it decodes.
	BoundedVfs vfs_;                      ///< The decoder reads bytes_ through it.
	ma_decoder decoder_{};                ///< The decoder, once open_.
	bool open_ = false;                   ///< The decoder is set up.
	std::optional<std::uint64_t> length_; ///< See length().
	bool failed_ = false;                 ///< The last read failed.
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
	    : queue_(std::move(bytes)), bytes_(queue_), vfs_(bytes_),
	      ring_(std::max<std::size_t>(output.sampleRate / 2, decodeFrames), output.channels),
	      config_(configuration(hint, output, 0)), maxFrames_(maxFramesOf(output, limits)) {
		thread_ = std::thread([this] { decode(); }); // no seek table: it would read the whole stream first
	}
	StreamReader(const StreamReader&) = delete;
	StreamReader& operator=(const StreamReader&) = delete;
	StreamReader(StreamReader&&) = delete;
	StreamReader& operator=(StreamReader&&) = delete;
	/// @brief Stops the thread: its reads and seeks fail at once.
	~StreamReader() override {
		stop_ = true;
		queue_->cancel();
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
		if(vfs_.init(config_, &decoder) != MA_SUCCESS) {
			if(!stop_) fail("the stream couldn't be decoded (WAV, FLAC or MP3)");
			else ring_.finished.store(true, std::memory_order_release);
			return;
		}
		queue_->stopKeeping();
		std::vector<float> frames(decodeFrames * ring_.channels);
		std::uint64_t total = 0;
		while(!stop_) {
			if(ring_.space() < decodeFrames) { // the voice is paused, or plays slower than this decodes
				std::this_thread::sleep_for(std::chrono::milliseconds{5});
				continue;
			}
			const auto room = static_cast<ma_uint64>(std::min<std::uint64_t>(decodeFrames, maxFrames_ - total));
			ma_uint64 read = 0;
			const ma_result result =
			    room == 0 ? MA_AT_END : ma_decoder_read_pcm_frames(&decoder, frames.data(), room, &read);
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

	std::shared_ptr<ByteQueue> queue_; ///< The stream's bytes.
	QueueBytes bytes_;                 ///< Read through vfs_.
	BoundedVfs vfs_;                   ///< What the decoder reads.
	FrameRing ring_;                   ///< Decoded frames, half a second.
	ma_decoder_config config_;         ///< The decoder's configuration.
	std::uint64_t maxFrames_;          ///< The most frames decoded.
	std::string failure_;              ///< Why it failed (set before failed_).
	std::atomic<bool> failed_{false};  ///< It failed.
	std::atomic<bool> stop_{false};    ///< The reader closes.
	std::thread thread_;               ///< Decodes.
};

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
	auto bytes = std::make_unique<FileBytes>(path, size);
	if(!bytes->opened()) {
		error = "'" + name + "' isn't a file that can be read";
		return nullptr;
	}
	if(hint == Encoding::Unknown) hint = encodingOf(name);
	auto reader = std::make_unique<DecoderReader>(std::move(bytes));
	if(!reader->open(configuration(hint, output, mp3SeekPoints))) {
		error = "'" + name + "' couldn't be decoded (WAV, FLAC or MP3)";
		return nullptr;
	}
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
	auto reader = std::make_unique<DecoderReader>(std::make_unique<MemoryBytes>(std::move(bytes)));
	if(!reader->open(configuration(hint, output, mp3SeekPoints))) {
		error = "the bytes couldn't be decoded (WAV, FLAC or MP3)";
		return nullptr;
	}
	if(!reader->withinLimits(output, limits, error)) return nullptr;
	return reader;
}

std::unique_ptr<Reader> openEncodedStream(std::shared_ptr<ByteQueue> bytes, Encoding hint, const Format& output,
                                          const Limits& limits) {
	return std::make_unique<StreamReader>(std::move(bytes), hint, output, limits);
}

} // namespace affresco::audio::detail
