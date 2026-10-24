/// @file
/// @brief The sources: files, bytes in memory, generators and tones, and the two streams, with their readers.

#include "Buffers.hpp"
#include "Decoders.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numbers>
#include <utility>

namespace affresco::audio {

namespace {

/// Frames a PCM stream's reader takes from its ring at a time, before converting them.
constexpr std::size_t stageFrames = 256;

/// @brief A file, opened by each voice that plays it.
class FileSource final : public Source {
public:
	/// @brief The file.
	/// @param path Where it is.
	/// @param hint Its encoding.
	FileSource(std::filesystem::path path, Encoding hint) : path_(std::move(path)), hint_(hint) {}
	/// @brief Opens a decoder on it.
	/// @param output The engine's format.
	/// @param limits The engine's limits.
	/// @param error Receives why it can't.
	/// @return The reader, or nullptr.
	std::unique_ptr<detail::Reader> open(const Format& output, const Limits& limits, std::string& error) override {
		return detail::openFile(path_, hint_, output, limits, error);
	}

private:
	std::filesystem::path path_; ///< The file.
	Encoding hint_;              ///< Its encoding.
};

/// @brief Encoded bytes in memory, shared by the voices that play them.
class MemorySource final : public Source {
public:
	/// @brief The bytes.
	/// @param bytes Them.
	/// @param hint Their encoding.
	MemorySource(std::vector<std::uint8_t> bytes, Encoding hint)
	    : bytes_(std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes))), hint_(hint) {}
	/// @brief Opens a decoder on them.
	/// @param output The engine's format.
	/// @param limits The engine's limits.
	/// @param error Receives why it can't.
	/// @return The reader, or nullptr.
	std::unique_ptr<detail::Reader> open(const Format& output, const Limits& limits, std::string& error) override {
		return detail::openMemory(bytes_, hint_, output, limits, error);
	}

private:
	std::shared_ptr<const std::vector<std::uint8_t>> bytes_; ///< The bytes.
	Encoding hint_;                                          ///< Their encoding.
};

/// @brief Calls a generator for each block.
class GeneratorReader final : public detail::Reader {
public:
	/// @brief A reader of a generator.
	/// @param generate The function.
	/// @param output The engine's format.
	/// @param length Frames in all, if known.
	/// @param seekable Whether frames come from their index.
	GeneratorReader(Generator generate, const Format& output, std::optional<std::uint64_t> length, bool seekable)
	    : generate_(std::move(generate)), format_(output), length_(length), seekable_(seekable) {}

	/// @brief Asks the generator for frames.
	/// @param out Room for them.
	/// @param frames How many.
	/// @return What it made; fewer than asked is its end.
	detail::ReadResult read(float* out, std::size_t frames) override {
		if(length_) frames = static_cast<std::size_t>(std::min<std::uint64_t>(frames, *length_ - std::min(*length_, at_)));
		if(frames == 0) return {0, detail::ReadStatus::End};
		std::size_t made = 0;
		try {
			made = std::min(frames, generate_(std::span<float>(out, frames * format_.channels), at_, format_));
		} catch(...) {
			failed_ = true;
			return {0, detail::ReadStatus::Failed};
		}
		at_ += made;
		return {made, made < frames ? detail::ReadStatus::End : detail::ReadStatus::Ok};
	}
	/// @brief Goes to a frame, for a generator that computes from it.
	/// @param frame The frame.
	/// @return Whether it can.
	bool seek(std::uint64_t frame) override {
		if(!seekable_) return false;
		at_ = frame;
		return true;
	}
	/// @brief See the constructor.
	[[nodiscard]] bool seekable() const override { return seekable_; }
	/// @brief See the constructor.
	[[nodiscard]] std::optional<std::uint64_t> length() const override { return length_; }
	/// @brief Why it failed.
	[[nodiscard]] const char* failure() const override { return failed_ ? "the generator failed" : nullptr; }

private:
	Generator generate_;                  ///< The function.
	Format format_;                       ///< The engine's format.
	std::optional<std::uint64_t> length_; ///< See length().
	bool seekable_;                       ///< See seekable().
	std::uint64_t at_ = 0;                ///< The next frame's index.
	bool failed_ = false;                 ///< It threw.
};

/// @brief A generator, opened by each voice that plays it.
class GeneratorSource final : public Source {
public:
	/// @brief The generator.
	/// @param generate The function.
	/// @param seconds How long, if known.
	/// @param seekable Whether frames come from their index.
	GeneratorSource(Generator generate, std::optional<double> seconds, bool seekable)
	    : generate_(std::move(generate)), seconds_(seconds), seekable_(seekable) {}
	/// @brief A reader of it.
	/// @param output The engine's format.
	/// @param limits The engine's limits (unused: a voice ends at Limits::seconds).
	/// @param error Unused.
	/// @return The reader.
	std::unique_ptr<detail::Reader> open(const Format& output, const Limits& limits, std::string& error) override {
		static_cast<void>(limits), static_cast<void>(error);
		std::optional<std::uint64_t> length;
		if(seconds_) length = static_cast<std::uint64_t>(std::llround(std::clamp(*seconds_, 0.0, 1e7) * output.sampleRate));
		return std::make_unique<GeneratorReader>(generate_, output, length, seekable_);
	}

private:
	Generator generate_;            ///< The function.
	std::optional<double> seconds_; ///< How long.
	bool seekable_;                 ///< See the constructor.
};

/// @brief Reads a PCM stream's ring, converting its rate and channels to the engine's.
class PcmReader final : public detail::Reader {
public:
	/// @brief A reader of a ring.
	/// @param ring The ring.
	/// @param input The stream's format.
	/// @param output The engine's format.
	PcmReader(std::shared_ptr<detail::FrameRing> ring, const Format& input, const Format& output)
	    : ring_(std::move(ring)), input_(input), output_(output), converting_(!(input == output)),
	      stage_(stageFrames * input.channels) {
		if(!converting_) return;
		const ma_data_converter_config config = ma_data_converter_config_init(
		    ma_format_f32, ma_format_f32, input.channels, output.channels, input.sampleRate, output.sampleRate);
		ready_ = ma_data_converter_init(&config, nullptr, &converter_) == MA_SUCCESS;
	}
	PcmReader(const PcmReader&) = delete;
	PcmReader& operator=(const PcmReader&) = delete;
	PcmReader(PcmReader&&) = delete;
	PcmReader& operator=(PcmReader&&) = delete;
	/// @brief Frees the converter.
	~PcmReader() override {
		if(converting_ && ready_) ma_data_converter_uninit(&converter_, nullptr);
	}

	/// @brief Takes frames from the ring, converted.
	/// @param out Room for them.
	/// @param frames How many.
	/// @return What it took; Starving while more may come.
	detail::ReadResult read(float* out, std::size_t frames) override {
		const bool done = ring_->finished.load(std::memory_order_acquire);
		std::size_t made = 0;
		if(!converting_) made = ring_->read(out, frames);
		else if(ready_) {
			while(made < frames) {
				if(staged_ == 0) {
					staged_ = ring_->read(stage_.data(), stageFrames);
					stagePosition_ = 0;
					if(staged_ == 0) break;
				}
				ma_uint64 in = staged_;
				ma_uint64 produced = frames - made;
				ma_data_converter_process_pcm_frames(&converter_, stage_.data() + (stagePosition_ * input_.channels), &in,
				                                     out + (made * output_.channels), &produced);
				stagePosition_ += static_cast<std::size_t>(in);
				staged_ -= static_cast<std::size_t>(in);
				made += static_cast<std::size_t>(produced);
				if(in == 0 && produced == 0) break;
			}
		} else return {0, detail::ReadStatus::Failed};
		if(made == frames) return {made, detail::ReadStatus::Ok};
		const bool empty = staged_ == 0 && ring_->available() == 0;
		return {made, done && empty ? detail::ReadStatus::End : detail::ReadStatus::Starving};
	}
	/// @brief Why it failed.
	[[nodiscard]] const char* failure() const override {
		return converting_ && !ready_ ? "the stream's format couldn't be converted" : nullptr;
	}

private:
	std::shared_ptr<detail::FrameRing> ring_; ///< The stream's frames.
	Format input_;                            ///< Their format.
	Format output_;                           ///< The engine's.
	bool converting_;                         ///< The formats differ.
	bool ready_ = false;                      ///< The converter is set up.
	ma_data_converter converter_{};           ///< Converts the rate and channels.
	std::vector<float> stage_;                ///< Frames taken from the ring, not yet converted.
	std::size_t stagePosition_ = 0;           ///< The next of them.
	std::size_t staged_ = 0;                  ///< How many are left.
};

} // namespace

Source::~Source() = default;

Encoding encodingOf(std::string_view name) noexcept {
	const auto query = name.find_first_of("?#");
	if(query != std::string_view::npos) name = name.substr(0, query);
	const auto dot = name.rfind('.');
	if(dot == std::string_view::npos) return Encoding::Unknown;
	std::string extension;
	for(const char c : name.substr(dot + 1)) extension += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if(extension == "wav" || extension == "wave") return Encoding::Wav;
	if(extension == "flac") return Encoding::Flac;
	if(extension == "mp3") return Encoding::Mp3;
	return Encoding::Unknown;
}

std::shared_ptr<Source> Source::file(std::filesystem::path path, Encoding hint) {
	return std::make_shared<FileSource>(std::move(path), hint);
}

std::shared_ptr<Source> Source::memory(std::vector<std::uint8_t> bytes, Encoding hint) {
	return std::make_shared<MemorySource>(std::move(bytes), hint);
}

std::shared_ptr<Source> Source::generator(Generator generate, std::optional<double> seconds, bool seekable) {
	if(!generate || (seconds && !(*seconds >= 0))) return nullptr;
	return std::make_shared<GeneratorSource>(std::move(generate), seconds, seekable);
}

std::shared_ptr<Source> Source::tone(double frequency, double seconds, float amplitude) {
	if(!(frequency >= 1 && frequency <= 20000) || !(seconds > 0 && seconds <= 86400) ||
	   !(amplitude >= 0 && amplitude <= 1))
		return nullptr;
	Generator sine = [frequency, seconds, amplitude](std::span<float> out, std::uint64_t first, const Format& format) {
		constexpr double ramp = 0.005; // seconds at each end, so that it starts and stops without a click
		const std::size_t frames = out.size() / format.channels;
		for(std::size_t i = 0; i < frames; ++i) {
			const double time = static_cast<double>(first + i) / format.sampleRate;
			const double envelope = std::clamp(std::min(time, seconds - time) / ramp, 0.0, 1.0);
			const auto sample =
			    static_cast<float>(amplitude * envelope * std::sin(2 * std::numbers::pi * frequency * time));
			std::fill_n(out.begin() + static_cast<std::ptrdiff_t>(i * format.channels), format.channels, sample);
		}
		return frames;
	};
	return generator(std::move(sine), seconds, true);
}

PcmStream::PcmStream(Format format, std::size_t capacity)
    : format_(format), ring_(std::make_shared<detail::FrameRing>(capacity, format.channels)) {}

PcmStream::~PcmStream() = default;

std::shared_ptr<PcmStream> PcmStream::create(Format format, std::size_t capacity) {
	if(!valid(format) || capacity == 0 || capacity > maxCapacity) return nullptr;
	return std::make_shared<PcmStream>(format, capacity);
}

std::size_t PcmStream::push(std::span<const float> samples) {
	if(ring_->finished.load(std::memory_order_acquire)) return 0;
	return ring_->write(samples.data(), samples.size() / format_.channels);
}

std::size_t PcmStream::push(std::span<const std::int16_t> samples) {
	std::array<float, 4096> converted{};
	const std::size_t perChunk = converted.size() / format_.channels * format_.channels;
	std::size_t taken = 0;
	for(std::size_t start = 0; start + format_.channels <= samples.size(); start += perChunk) {
		const std::size_t count = std::min(perChunk, samples.size() - start) / format_.channels * format_.channels;
		for(std::size_t i = 0; i < count; ++i) converted[i] = static_cast<float>(samples[start + i]) / 32768.0F;
		const std::size_t frames = push(std::span<const float>(converted.data(), count));
		taken += frames;
		if(frames < count / format_.channels) break; // the ring is full
	}
	return taken;
}

void PcmStream::finish() noexcept { ring_->finished.store(true, std::memory_order_release); }

bool PcmStream::finished() const noexcept { return ring_->finished.load(std::memory_order_acquire); }

std::size_t PcmStream::buffered() const noexcept { return ring_->available(); }

std::size_t PcmStream::capacity() const noexcept { return ring_->capacity; }

std::unique_ptr<detail::Reader> PcmStream::open(const Format& output, const Limits& limits, std::string& error) {
	static_cast<void>(limits);
	if(opened_.exchange(true)) {
		error = "a stream plays on one voice only";
		return nullptr;
	}
	return std::make_unique<PcmReader>(ring_, format_, output);
}

EncodedStream::EncodedStream(std::size_t capacity, Encoding hint)
    : capacity_(capacity), hint_(hint), bytes_(std::make_shared<detail::ByteQueue>(capacity)) {}

EncodedStream::~EncodedStream() = default;

std::shared_ptr<EncodedStream> EncodedStream::create(std::size_t capacity, Encoding hint) {
	if(capacity < 1024 || capacity > maxCapacity) return nullptr;
	return std::make_shared<EncodedStream>(capacity, hint);
}

bool EncodedStream::push(std::span<const std::uint8_t> bytes) { return bytes_->push(bytes); }

bool EncodedStream::push(std::string_view bytes) {
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the same bytes, read as unsigned
	return bytes_->push({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
}

void EncodedStream::finish() noexcept { bytes_->finish(); }

std::size_t EncodedStream::buffered() const noexcept { return bytes_->buffered(); }

std::unique_ptr<detail::Reader> EncodedStream::open(const Format& output, const Limits& limits, std::string& error) {
	if(opened_.exchange(true)) {
		error = "a stream plays on one voice only";
		return nullptr;
	}
	return detail::openEncodedStream(bytes_, hint_, output, limits);
}

} // namespace affresco::audio
