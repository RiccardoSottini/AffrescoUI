#pragma once

/// @file
/// @brief audio::Engine: an output device that mixes voices, with a master volume and a level meter. It plays to the
/// system's default device, to a null device (no hardware: the clock runs, nothing is heard), or to the caller, who
/// pulls the mix (render(): tests, offline rendering, an export).

#include "affresco/audio/Source.hpp"
#include "affresco/audio/Voice.hpp"

#include "affresco/core/Executor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace affresco::audio {

/// @brief Where an engine's mix goes.
enum class Output : std::uint8_t {
	System, ///< The system's default output device (CoreAudio, WASAPI, PulseAudio or ALSA); Null if none opens.
	Null,   ///< No hardware: a thread pulls the mix in real time and throws it away (CI, headless hosts).
	Manual, ///< No thread: the caller pulls the mix with render(), as fast as it likes.
};

/// @brief How an engine is made.
struct EngineOptions {
	Output output = Output::System; ///< Where the mix goes.
	/// Frames per second of the mix: 0 for the device's own (48,000 for Null and Manual).
	std::uint32_t sampleRate = 0;
	std::uint32_t channels = 2;            ///< Samples per frame of the mix, from 1 to 8.
	std::uint32_t periodMilliseconds = 10; ///< How much each device callback mixes (latency), from 1 to 100.
	/// Where voices' events are posted (it must outlive the engine), or nullptr: then no event is delivered until
	/// setExecutor().
	Executor* executor = nullptr;
	Limits limits{}; ///< What the engine accepts.
	/// How often a thread gathers the voices' events while any voice exists (System and Null): from 5 to 100 ms.
	std::chrono::milliseconds eventInterval{25};
};

/// @brief An output that mixes voices.
///
/// Threads: the device (or the caller of render()) mixes on its own thread; a second thread gathers the voices'
/// events and posts them to the Executor (in Manual, render() posts them). Every public function may be called from
/// any thread: one lock guards the voices, held by the mixer for one block at a time. Destroying the engine stops the
/// device and its threads; events already posted then do nothing.
class Engine {
public:
	/// @brief Opens the output. A System engine whose device fails to open plays to Null instead (deviceError() says
	/// why), so that a host runs the same without audio hardware.
	/// @param options How.
	explicit Engine(EngineOptions options = {});
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	Engine(Engine&&) = delete;
	Engine& operator=(Engine&&) = delete;
	/// @brief Stops the device and the threads; every voice stops.
	~Engine();

	/// @brief Where the mix goes: System, or Null if the device didn't open.
	[[nodiscard]] Output output() const noexcept;
	/// @brief The mix's format (the device's, if it chose).
	[[nodiscard]] Format format() const noexcept;
	/// @brief The device's name, or "null" or "manual".
	[[nodiscard]] const std::string& deviceName() const noexcept;
	/// @brief Why the System device didn't open, or empty.
	[[nodiscard]] const std::string& deviceError() const noexcept;
	/// @brief What the engine accepts.
	[[nodiscard]] const Limits& limits() const noexcept;
	/// @brief Sets where events are posted from now on.
	/// @param executor The executor (outlives the engine), or nullptr for none.
	void setExecutor(Executor* executor) noexcept;

	/// @brief Opens a source on a new voice, stopped at its start.
	/// @param source The source.
	/// @param options Its volume, pan, rate, loop and event handler.
	/// @param error Receives why it can't (the source fails to open, too many voices).
	/// @return The voice, or nullptr.
	[[nodiscard]] std::shared_ptr<Voice> createVoice(const std::shared_ptr<Source>& source, VoiceOptions options,
	                                                 std::string& error);
	/// @brief Opens a source on a new voice and plays it.
	/// @param source The source.
	/// @param options Its volume, pan, rate, loop and event handler.
	/// @param error Receives why it can't.
	/// @return The voice, or nullptr.
	std::shared_ptr<Voice> play(const std::shared_ptr<Source>& source, VoiceOptions options, std::string& error);

	/// @brief Sets the gain of the whole mix.
	/// @param volume From 0 to 4.
	void setMasterVolume(float volume);
	/// @brief The gain of the whole mix.
	[[nodiscard]] float masterVolume() const;
	/// @brief The mix's levels over the last block (after the master volume): for meters and visualisers.
	[[nodiscard]] Levels levels() const;
	/// @brief The voices the engine keeps (playing, paused, or stopped with a handle).
	[[nodiscard]] std::size_t voiceCount() const;
	/// @brief Stops every voice.
	void stopAll();

	/// @brief Manual only: mixes the next frames into a buffer, then posts the events they caused.
	/// @param frames Interleaved floats in format(), a multiple of its channels.
	/// @return The frames written (0 unless Manual).
	std::size_t render(std::span<float> frames);

private:
	struct Device;                        ///< The miniaudio device and the event thread.
	std::shared_ptr<detail::Shared> shared_; ///< The voices and the lock, shared with the voices' handles.
	std::unique_ptr<Device> device_;      ///< The output.
};

} // namespace affresco::audio
