#pragma once

/// @file
/// @brief audio::Voice: one source playing on an engine, with its transport (play, pause, stop, seek, loop), its gain
/// (volume, pan, fades), its playback rate, and the events it posts to the main thread.

#include "affresco/audio/Source.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace affresco::audio {

namespace detail {
struct VoiceCore;
struct Shared;
} // namespace detail

/// @brief Where a voice is.
enum class State : std::uint8_t {
	Stopped, ///< Not playing: at its start, after stop(), or at its end.
	Playing, ///< Mixed into the output.
	Paused,  ///< Holding its position.
};

/// @brief What a voice tells its handler.
enum class EventType : std::uint8_t {
	Started,  ///< play() started or resumed it.
	Ended,    ///< It reached its end (not when stop() stopped it).
	Underrun, ///< A stream ran dry before its end: silence until more arrives.
	Error,    ///< It couldn't go on (a decoding error); it stopped.
	Position, ///< Its position moved while playing (every VoiceOptions::positionInterval at most).
};

/// @brief An event of a voice, on the main thread.
struct Event {
	EventType type = EventType::Position; ///< What happened.
	double position = 0;                  ///< Seconds from the start, when it happened.
	std::optional<double> duration;       ///< Seconds in all, if known.
	std::string message;                  ///< For Error: what went wrong, for people.
};

/// @brief Receives a voice's events, on the thread of the engine's Executor (the main thread).
using EventHandler = std::function<void(const Event& event)>;

/// @brief How a voice starts.
struct VoiceOptions {
	float volume = 1;   ///< The gain, from 0 to 4 (1 leaves the source as it is).
	float pan = 0;      ///< From -1 (left) to 1 (right); the balance of the first two channels.
	float rate = 1;     ///< The playback rate, from 0.25 to 4: speed and pitch together (2 is an octave up).
	bool loop = false;  ///< Start again at the end (for a seekable source).
	EventHandler onEvent; ///< Its events, or empty for none.
	/// How often a Position event may come while it plays: at least 20 ms; 0 for none.
	std::chrono::milliseconds positionInterval{250};
};

/// @brief One source playing on an Engine (Engine::createVoice()). Every function may be called from any thread.
///
/// A voice keeps playing when its last handle goes, until it ends ("fire and forget" for sound effects); a voice
/// that isn't playing then is forgotten. After its Engine is destroyed, it does nothing and stays stopped.
class Voice {
public:
	/// @brief Made by the engine.
	/// @param shared The engine's state.
	/// @param core The voice's state in it.
	Voice(std::shared_ptr<detail::Shared> shared, std::shared_ptr<detail::VoiceCore> core);
	Voice(const Voice&) = delete;
	Voice& operator=(const Voice&) = delete;
	Voice(Voice&&) = delete;
	Voice& operator=(Voice&&) = delete;
	/// @brief Lets the engine forget the voice once it isn't playing.
	~Voice();

	/// @brief Plays it: from where it is, or from the start after its end.
	/// @return false if it can't (a stream that ended, an engine that is gone, a voice that failed).
	bool play();
	/// @brief Holds it where it is.
	void pause();
	/// @brief Stops it and goes back to the start (a stream can't go back: it stays where it is).
	void stop();
	/// @brief Moves it to a time.
	/// @param seconds From the start, clamped to the duration.
	/// @return false if the source isn't seekable, or the decoder couldn't.
	bool seek(double seconds);
	/// @brief Sets whether it starts again at its end.
	/// @param loop Whether.
	void setLoop(bool loop);
	/// @brief Sets the volume now (it stops a fade).
	/// @param volume From 0 to 4.
	void setVolume(float volume);
	/// @brief Changes the volume gradually.
	/// @param volume The volume to reach, from 0 to 4.
	/// @param seconds How long it takes; 0 at once.
	/// @param stopAtEnd Stop (as stop()) when it gets there: a fade out.
	void fade(float volume, double seconds, bool stopAtEnd = false);
	/// @brief Sets the balance.
	/// @param pan From -1 to 1.
	void setPan(float pan);
	/// @brief Sets the playback rate.
	/// @param rate From 0.25 to 4.
	void setRate(float rate);

	/// @brief Where it is.
	[[nodiscard]] State state() const;
	/// @brief Seconds from the start.
	[[nodiscard]] double position() const;
	/// @brief Seconds in all: unknown for a live stream or a generator without one.
	[[nodiscard]] std::optional<double> duration() const;
	/// @brief Whether seek() works.
	[[nodiscard]] bool seekable() const;
	/// @brief Whether it starts again at its end.
	[[nodiscard]] bool loop() const;
	/// @brief The volume (during a fade, where it is now).
	[[nodiscard]] float volume() const;
	/// @brief The balance.
	[[nodiscard]] float pan() const;
	/// @brief The playback rate.
	[[nodiscard]] float rate() const;
	/// @brief Its levels over the last block mixed, after its gain.
	[[nodiscard]] Levels levels() const;
	/// @brief How many times a stream has run dry.
	[[nodiscard]] std::size_t underruns() const;
	/// @brief Why it stopped, if it failed.
	[[nodiscard]] std::optional<std::string> error() const;

private:
	std::shared_ptr<detail::Shared> shared_;  ///< The engine's state (its lock).
	std::shared_ptr<detail::VoiceCore> core_; ///< The voice's state.
};

} // namespace affresco::audio
