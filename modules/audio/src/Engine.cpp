/// @file
/// @brief audio::Engine: the miniaudio device (or none), the mix, and the thread that posts the voices' events.

#include "Shared.hpp"

#include "affresco/audio/Engine.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <thread>
#include <utility>

namespace affresco::audio {

namespace detail {

void Shared::mix(float* out, std::size_t frames) {
	const std::uint32_t channels = format.channels;
	std::fill_n(out, frames * channels, 0.0F);
	const std::scoped_lock lock(mutex);
	for(std::size_t done = 0; done < frames;) {
		const std::size_t block = std::min(blockFrames, frames - done);
		for(const auto& voice : voices) voice->render(out + (done * channels), block);
		done += block;
	}
	if(master != 1)
		for(std::size_t i = 0; i < frames * channels; ++i) out[i] *= master;
	levels = measure(out, frames, channels);
}

void Shared::gather() {
	struct Pending {
		std::shared_ptr<const EventHandler> handler; ///< Whom to tell.
		Event event;                                 ///< What.
	};
	std::vector<Pending> events;
	std::vector<std::shared_ptr<VoiceCore>> forgotten; // destroyed after the lock (a stream's reader joins a thread)
	{
		const std::scoped_lock lock(mutex);
		const auto now = std::chrono::steady_clock::now();
		for(const auto& voice : voices) {
			VoiceCore& core = *voice;
			const auto report = [&](EventType type, std::string message = {}) {
				if(core.onEvent)
					events.push_back({core.onEvent,
					                  {type, core.cursor() / format.sampleRate, core.durationSeconds(),
					                   std::move(message)}});
			};
			if(core.startedPending) {
				core.startedPending = false;
				core.lastPosition = now;
				report(EventType::Started);
			}
			if(core.underruns > core.underrunsReported) {
				core.underrunsReported = core.underruns;
				report(EventType::Underrun);
			}
			if(core.failure && !core.failureReported) {
				core.failureReported = true;
				report(EventType::Error, *core.failure);
			}
			if(core.endedPending) {
				core.endedPending = false;
				report(EventType::Ended);
			} else if(core.state == State::Playing && core.positionInterval.count() > 0 &&
			          now - core.lastPosition >= core.positionInterval) {
				core.lastPosition = now;
				if(const double at = core.cursor(); at != core.reportedPosition) {
					core.reportedPosition = at;
					report(EventType::Position);
				}
			}
		}
		for(auto it = voices.begin(); it != voices.end();) {
			if((*it)->released && (*it)->state != State::Playing) {
				forgotten.push_back(std::move(*it));
				it = voices.erase(it);
			} else ++it;
		}
	}
	Executor* target = executor.load();
	if(!target) return;
	for(Pending& pending : events) {
		if(pending.event.type == EventType::Position && queued->load() >= maxQueuedEvents) continue;
		++*queued;
		target->post([alive = alive, queued = queued, pending = std::move(pending)] {
			--*queued;
			if(*alive) (*pending.handler)(pending.event);
		});
	}
}

namespace {

/// @brief A reader that has nothing: what a voice keeps once its engine is gone.
class ClosedReader final : public Reader {
public:
	/// @brief Nothing more.
	/// @return 0 frames, at the end.
	ReadResult read(float* /*out*/, std::size_t /*frames*/) override { return {0, ReadStatus::End}; }
};

/// @brief miniaudio's data callback: mixes the engine's voices into the device's buffer.
/// @param device The device; its user data is the engine's state.
/// @param output Room for frames, in the engine's format.
/// @param input Unused (playback only).
/// @param frames How many.
void mixInto(ma_device* device, void* output, const void* input, ma_uint32 frames) {
	static_cast<void>(input);
	static_cast<Shared*>(device->pUserData)->mix(static_cast<float*>(output), frames);
}

} // namespace

} // namespace detail

/// @brief The output: a miniaudio context and device (System or Null), and the thread that gathers events.
struct Engine::Device {
	Output output = Output::Manual; ///< See Engine::output().
	std::string name = "manual";    ///< See Engine::deviceName().
	std::string error;              ///< See Engine::deviceError().
	std::unique_ptr<ma_context> context; ///< While open.
	std::unique_ptr<ma_device> device;   ///< While open.
	std::thread events;                  ///< Gathers events (System and Null).
	detail::Shared* shared = nullptr;    ///< The engine's state, for the event thread.

	Device() = default;
	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
	Device(Device&&) = delete;
	Device& operator=(Device&&) = delete;
	/// @brief Stops the device, then the event thread.
	~Device() {
		close();
		if(events.joinable()) {
			{
				const std::scoped_lock lock(shared->mutex);
				shared->stopping = true;
			}
			shared->wake.notify_all();
			events.join();
		}
	}

	/// @brief Stops and closes the device, if open.
	void close() noexcept {
		if(device) ma_device_uninit(device.get());
		device.reset();
		if(context) ma_context_uninit(context.get());
		context.reset();
	}

	/// @brief Opens and starts a device.
	/// @param state The engine's state: its format is set to the device's.
	/// @param options The engine's options.
	/// @param null The null backend, else the system's.
	/// @return Why it failed, or empty.
	std::string open(detail::Shared& state, const EngineOptions& options, bool null) {
		context = std::make_unique<ma_context>();
		const ma_backend nullBackend = ma_backend_null;
		ma_result result = ma_context_init(null ? &nullBackend : nullptr, null ? 1 : 0, nullptr, context.get());
		if(result != MA_SUCCESS) {
			context.reset();
			return ma_result_description(result);
		}
		ma_device_config config = ma_device_config_init(ma_device_type_playback);
		config.playback.format = ma_format_f32;
		config.playback.channels = std::clamp<std::uint32_t>(options.channels, 1, 8);
		config.sampleRate = options.sampleRate != 0 ? options.sampleRate : (null ? 48000 : 0);
		config.periodSizeInMilliseconds = std::clamp<std::uint32_t>(options.periodMilliseconds, 1, 100);
		config.dataCallback = detail::mixInto;
		config.pUserData = &state;
		device = std::make_unique<ma_device>();
		result = ma_device_init(context.get(), &config, device.get());
		if(result != MA_SUCCESS) {
			device.reset();
			close();
			return ma_result_description(result);
		}
		state.format = {device->sampleRate, device->playback.channels};
		if(!valid(state.format)) {
			close();
			return "the device's format can't be played";
		}
		name = null ? "null" : device->playback.name;
		result = ma_device_start(device.get());
		if(result != MA_SUCCESS) {
			close();
			return ma_result_description(result);
		}
		output = null ? Output::Null : Output::System;
		return {};
	}

	/// @brief Starts the thread that gathers events while any voice exists.
	/// @param state The engine's state.
	/// @param interval How often.
	void startEvents(detail::Shared& state, std::chrono::milliseconds interval) {
		shared = &state;
		events = std::thread([&state, interval] {
			std::unique_lock lock(state.mutex);
			while(!state.stopping) {
				if(state.voices.empty()) state.wake.wait(lock, [&] { return state.stopping || !state.voices.empty(); });
				else state.wake.wait_for(lock, interval, [&] { return state.stopping; });
				if(state.stopping) break;
				lock.unlock();
				state.gather();
				lock.lock();
			}
		});
	}
};

Engine::Engine(EngineOptions options) : shared_(std::make_shared<detail::Shared>()), device_(std::make_unique<Device>()) {
	detail::Shared& shared = *shared_;
	shared.limits = options.limits;
	shared.limits.voices = std::max<std::size_t>(shared.limits.voices, 1);
	shared.executor = options.executor;
	shared.format = {options.sampleRate != 0 ? std::clamp<std::uint32_t>(options.sampleRate, 8000, 384000) : 48000,
	                 std::clamp<std::uint32_t>(options.channels, 1, 8)};
	if(options.output == Output::Manual) return;
	if(options.output == Output::System) device_->error = device_->open(shared, options, false);
	if(device_->output != Output::System) { // no device, or a null one asked for
		const std::string error = device_->open(shared, options, true);
		if(!error.empty()) {
			device_->name = "none";
			if(device_->error.empty()) device_->error = error;
		}
	}
	device_->startEvents(shared, std::clamp(options.eventInterval, std::chrono::milliseconds{5},
	                                        std::chrono::milliseconds{100}));
}

Engine::~Engine() {
	*shared_->alive = false; // events already posted do nothing
	device_.reset();         // no more callbacks, no more gathering
	std::vector<std::unique_ptr<detail::Reader>> readers; // closed outside the lock (a stream's joins its thread)
	{
		const std::scoped_lock lock(shared_->mutex);
		shared_->closed = true;
		for(const auto& voice : shared_->voices) {
			voice->state = State::Stopped;
			readers.push_back(std::exchange(voice->reader, std::make_unique<detail::ClosedReader>()));
		}
		shared_->voices.clear();
	}
}

Output Engine::output() const noexcept { return device_->output; }

Format Engine::format() const noexcept { return shared_->format; }

const std::string& Engine::deviceName() const noexcept { return device_->name; }

const std::string& Engine::deviceError() const noexcept { return device_->error; }

const Limits& Engine::limits() const noexcept { return shared_->limits; }

void Engine::setExecutor(Executor* executor) noexcept { shared_->executor = executor; }

std::shared_ptr<Voice> Engine::createVoice(const std::shared_ptr<Source>& source, VoiceOptions options,
                                           std::string& error) {
	detail::Shared& shared = *shared_;
	if(!source) {
		error = "no source";
		return nullptr;
	}
	const auto full = [&shared, &error] {
		if(shared.voices.size() < shared.limits.voices) return false;
		error = "too many voices (an engine keeps at most " + std::to_string(shared.limits.voices) + ")";
		return true;
	};
	{
		const std::scoped_lock lock(shared.mutex);
		if(full()) return nullptr;
	}
	std::unique_ptr<detail::Reader> reader = source->open(shared.format, shared.limits, error);
	if(!reader) return nullptr;
	const double most = std::max(shared.limits.seconds, 0.0) * shared.format.sampleRate;
	const std::uint64_t maxFrames = most >= 1.8e19 ? UINT64_MAX : static_cast<std::uint64_t>(most);
	auto core = std::make_shared<detail::VoiceCore>(source, std::move(reader), shared.format, options, maxFrames);
	{
		const std::scoped_lock lock(shared.mutex);
		if(full()) return nullptr;
		shared.voices.push_back(core);
	}
	shared.wake.notify_all();
	return std::make_shared<Voice>(shared_, std::move(core));
}

std::shared_ptr<Voice> Engine::play(const std::shared_ptr<Source>& source, VoiceOptions options, std::string& error) {
	std::shared_ptr<Voice> voice = createVoice(source, std::move(options), error);
	if(voice) voice->play();
	return voice;
}

void Engine::setMasterVolume(float volume) {
	const std::scoped_lock lock(shared_->mutex);
	shared_->master = detail::clamped(volume, 0, 4);
}

float Engine::masterVolume() const {
	const std::scoped_lock lock(shared_->mutex);
	return shared_->master;
}

Levels Engine::levels() const {
	const std::scoped_lock lock(shared_->mutex);
	return shared_->levels;
}

std::size_t Engine::voiceCount() const {
	const std::scoped_lock lock(shared_->mutex);
	return shared_->voices.size();
}

void Engine::stopAll() {
	const std::scoped_lock lock(shared_->mutex);
	for(const auto& voice : shared_->voices) voice->halt();
}

std::size_t Engine::render(std::span<float> frames) {
	if(device_->output != Output::Manual || device_->name != "manual") return 0;
	const std::size_t count = frames.size() / shared_->format.channels;
	shared_->mix(frames.data(), count);
	shared_->gather();
	return count;
}

} // namespace affresco::audio
