/// @file
/// @brief detail::VoiceCore (transport, gain, the rate converter, reporting) and audio::Voice, its handle.

#include "Shared.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco::audio {

namespace detail {

float finiteSample(float sample) noexcept {
	if(std::isnan(sample)) return 0;
	return std::clamp(sample, -maxSample, maxSample);
}

Levels measure(const float* frames, std::size_t count, std::uint32_t channels) noexcept {
	Levels levels;
	if(count == 0 || channels == 0) return levels;
	const std::uint32_t measured = std::min<std::uint32_t>(channels, 2);
	std::array<double, 2> squares{};
	for(std::size_t i = 0; i < count; ++i)
		for(std::uint32_t c = 0; c < measured; ++c) {
			const float sample = frames[(i * channels) + c];
			levels.peak[c] = std::max(levels.peak[c], std::fabs(sample));
			squares[c] += static_cast<double>(sample) * sample;
		}
	for(std::uint32_t c = 0; c < measured; ++c)
		levels.rms[c] = static_cast<float>(std::sqrt(squares[c] / static_cast<double>(count)));
	if(measured == 1) {
		levels.peak[1] = levels.peak[0];
		levels.rms[1] = levels.rms[0];
	}
	return levels;
}

VoiceCore::VoiceCore(std::shared_ptr<Source> source_, std::unique_ptr<Reader> reader_, Format format_,
                     const VoiceOptions& options, std::uint64_t maxFrames_)
    : source(std::move(source_)), reader(std::move(reader_)), format(format_), length(reader->length()),
      maxFrames(maxFrames_), volume(clamped(options.volume, 0, 4)), target(volume), restore(volume),
      pan(clamped(options.pan, -1, 1)), rate(clamped(options.rate, 0.25F, 4)), loop(options.loop),
      input(inputFrames * format.channels), last(format.channels), next(format.channels), fetched(format.channels),
      scratch(blockFrames * format.channels),
      onEvent(options.onEvent ? std::make_shared<const EventHandler>(options.onEvent) : nullptr),
      positionInterval(options.positionInterval.count() <= 0
                           ? std::chrono::milliseconds{0}
                           : std::max(options.positionInterval, std::chrono::milliseconds{20})) {
	if(length) length = std::min(*length, maxFrames);
}

ReadStatus VoiceCore::fetch() {
	const std::uint32_t channels = format.channels;
	if(inputPosition >= inputCount) {
		auto read = [this] {
			if(readIndex >= maxFrames) return ReadResult{0, ReadStatus::End};
			const auto room = static_cast<std::size_t>(std::min<std::uint64_t>(inputFrames, maxFrames - readIndex));
			return reader->read(input.data(), room);
		};
		ReadResult result = read();
		// At the end, a loop starts again, if it has played something (an empty source never loops)
		if(result.frames == 0 && result.status == ReadStatus::End && loop && readIndex > 0 && reader->seekable() &&
		   reader->seek(0)) {
			readIndex = 0;
			result = read();
		}
		if(result.frames == 0) return result.status == ReadStatus::Ok ? ReadStatus::End : result.status;
		inputCount = result.frames;
		inputPosition = 0;
	}
	const float* frame = input.data() + (inputPosition * channels);
	for(std::uint32_t c = 0; c < channels; ++c) fetched[c] = finiteSample(frame[c]); // a file may hold NaN
	++inputPosition;
	++readIndex;
	return ReadStatus::Ok;
}

ReadStatus VoiceCore::advance() {
	while(t >= 1.0) {
		if(draining) return ReadStatus::End;
		const ReadStatus status = fetch();
		if(status == ReadStatus::Starving || status == ReadStatus::Failed) return status; // t stays: it resumes
		last.swap(next);
		lastIndex = nextIndex;
		if(status == ReadStatus::Ok) {
			next.swap(fetched);
			nextIndex = readIndex - 1;
		} else { // the end: the last frame is held until t passes it
			next = last;
			draining = true;
		}
		t -= 1.0;
	}
	return ReadStatus::Ok;
}

std::size_t VoiceCore::pull(std::size_t frames) {
	const std::uint32_t channels = format.channels;
	std::size_t made = 0;
	ReadStatus status = ReadStatus::Ok;
	if(!primed) { // the first frame: last and next both hold it, and t = 1 steps to the second
		status = fetch();
		if(status == ReadStatus::Ok) {
			next.swap(fetched);
			nextIndex = readIndex - 1;
			last = next;
			lastIndex = nextIndex;
			t = 1;
			primed = true;
		}
	}
	while(status == ReadStatus::Ok && made < frames) {
		status = advance();
		if(status != ReadStatus::Ok) break;
		const auto fraction = static_cast<float>(t);
		float* out = scratch.data() + (made * channels);
		for(std::uint32_t c = 0; c < channels; ++c) out[c] = last[c] + ((next[c] - last[c]) * fraction);
		++made;
		t += rate;
	}
	if(made > 0) {
		flowed = true;
		starving = false;
	}
	if(status == ReadStatus::Starving) {
		if(flowed && !starving) ++underruns;
		starving = flowed;
	} else if(status != ReadStatus::Ok) finish(status);
	return made;
}

void VoiceCore::finish(ReadStatus status) {
	state = State::Stopped;
	atEnd = true;
	fadeLeft = 0;
	if(status == ReadStatus::Failed) {
		const char* why = reader->failure();
		failure = why ? why : "the sound couldn't be decoded";
	} else endedPending = true;
}

void VoiceCore::render(float* mix, std::size_t frames) {
	if(state != State::Playing) return;
	const std::size_t made = pull(frames);
	const std::uint32_t channels = format.channels;
	const PanGains pans = channels >= 2 ? panGains(pan) : PanGains{};
	bool faded = false;
	for(std::size_t i = 0; i < made; ++i) {
		const float gain = volume;
		if(fadeLeft > 0) {
			volume += step;
			if(--fadeLeft == 0) {
				volume = target;
				faded = stopAfterFade;
			}
		}
		float* frame = scratch.data() + (i * channels);
		for(std::uint32_t c = 0; c < channels; ++c) {
			const float sideGain = c == 0 ? pans.left : c == 1 ? pans.right : 1.0F;
			frame[c] *= gain * sideGain;
			mix[(i * channels) + c] += frame[c];
		}
	}
	levels = measure(scratch.data(), made, channels);
	if(faded) {
		halt();
		volume = target = restore; // a fade out leaves the voice at its volume for the next play
	}
}

bool VoiceCore::start() {
	if(failure) return false;
	if(state == State::Playing) return true;
	if(atEnd && !moveTo(0)) return false; // a stream or a generator that can't go back has ended for good
	state = State::Playing;
	startedPending = true;
	flowed = false;
	starving = false;
	return true;
}

void VoiceCore::halt() {
	state = State::Stopped;
	fadeLeft = 0;
	stopAfterFade = false;
	if(reader->seekable()) static_cast<void>(moveTo(0));
}

bool VoiceCore::moveTo(std::uint64_t frame) {
	if(!reader->seekable()) return false;
	if(length) frame = std::min(frame, *length);
	if(!reader->seek(frame)) return false;
	inputPosition = inputCount = 0;
	primed = draining = atEnd = false;
	readIndex = lastIndex = nextIndex = frame;
	t = 0;
	return true;
}

double VoiceCore::cursor() const noexcept {
	if(atEnd && length) return static_cast<double>(*length);
	double at = static_cast<double>(readIndex);
	if(primed) at = t < 1.0 ? static_cast<double>(lastIndex) + t : static_cast<double>(nextIndex);
	if(length) at = std::min(at, static_cast<double>(*length));
	return at;
}

std::optional<double> VoiceCore::durationSeconds() const noexcept {
	if(!length) return std::nullopt;
	return static_cast<double>(*length) / format.sampleRate;
}

} // namespace detail

Voice::Voice(std::shared_ptr<detail::Shared> shared, std::shared_ptr<detail::VoiceCore> core)
    : shared_(std::move(shared)), core_(std::move(core)) {}

Voice::~Voice() {
	const std::scoped_lock lock(shared_->mutex);
	core_->released = true;
}

bool Voice::play() {
	const std::scoped_lock lock(shared_->mutex);
	return !shared_->closed && core_->start();
}

void Voice::pause() {
	const std::scoped_lock lock(shared_->mutex);
	if(core_->state == State::Playing) core_->state = State::Paused;
}

void Voice::stop() {
	const std::scoped_lock lock(shared_->mutex);
	core_->halt();
}

bool Voice::seek(double seconds) {
	const std::scoped_lock lock(shared_->mutex);
	if(!(seconds > 0)) seconds = 0; // and NaN
	const double frames = std::min(seconds * core_->format.sampleRate, static_cast<double>(core_->maxFrames));
	return core_->moveTo(static_cast<std::uint64_t>(std::llround(frames)));
}

void Voice::setLoop(bool loop) {
	const std::scoped_lock lock(shared_->mutex);
	core_->loop = loop;
}

void Voice::setVolume(float volume) { fade(volume, 0); }

void Voice::fade(float volume, double seconds, bool stopAtEnd) {
	const std::scoped_lock lock(shared_->mutex);
	detail::VoiceCore& core = *core_;
	if(stopAtEnd && !core.stopAfterFade) core.restore = core.volume;
	core.target = detail::clamped(volume, 0, 4);
	const double frames = seconds > 0 ? std::min(seconds, 3600.0) * core.format.sampleRate : 0;
	core.fadeLeft = static_cast<std::uint64_t>(frames);
	core.stopAfterFade = stopAtEnd;
	if(core.fadeLeft == 0) {
		core.volume = core.target;
		if(stopAtEnd) {
			core.halt();
			core.volume = core.target = core.restore;
		}
		return;
	}
	core.step = (core.target - core.volume) / static_cast<float>(core.fadeLeft);
}

void Voice::setPan(float pan) {
	const std::scoped_lock lock(shared_->mutex);
	core_->pan = detail::clamped(pan, -1, 1);
}

void Voice::setRate(float rate) {
	const std::scoped_lock lock(shared_->mutex);
	core_->rate = detail::clamped(rate, 0.25F, 4);
}

State Voice::state() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->state;
}

double Voice::position() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->cursor() / core_->format.sampleRate;
}

std::optional<double> Voice::duration() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->durationSeconds();
}

bool Voice::seekable() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->reader->seekable();
}

bool Voice::loop() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->loop;
}

float Voice::volume() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->volume;
}

float Voice::pan() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->pan;
}

float Voice::rate() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->rate;
}

Levels Voice::levels() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->state == State::Playing ? core_->levels : Levels{};
}

std::size_t Voice::underruns() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->underruns;
}

std::optional<std::string> Voice::error() const {
	const std::scoped_lock lock(shared_->mutex);
	return core_->failure;
}

} // namespace affresco::audio
