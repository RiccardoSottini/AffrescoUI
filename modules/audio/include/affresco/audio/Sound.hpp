#pragma once

/// @file
/// @brief A sound described as text, which the audio module renders: what SVG is to pictures. An agent can't write a
/// song's bytes, but it can write this (`application/vnd.affrescoui.sound+json`, SCREEN_FORMAT.md "Audio"): a tempo,
/// and tracks of notes or step patterns played by built-in instruments.

#include "affresco/audio/Source.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::audio {

/// @brief The built-in instruments: four oscillators and noise with an ADSR envelope, and four drums.
enum class Instrument : std::uint8_t { Sine, Square, Saw, Triangle, Noise, Kick, Snare, Hat, Clap };

/// @brief The bounds of a sound description: a description beyond them is refused.
struct SoundLimits {
	std::size_t bytes = std::size_t{1} << 20; ///< The longest description, as text.
	std::size_t tracks = 16;                  ///< The most tracks.
	std::size_t events = 4096;                ///< The most notes and hits in all.
	std::size_t polyphony = 32;               ///< The most notes sounding at once.
	double seconds = 600;                     ///< The longest it lasts.
};

/// @brief One note or hit, in seconds.
struct SoundEvent {
	double start = 0;        ///< When it starts.
	double gate = 0;         ///< How long it is held (its release follows).
	double frequency = 0;    ///< Hertz (oscillators); drums and noise ignore it.
	float gain = 1;          ///< Its velocity times its track's volume.
	float pan = 0;           ///< Its track's balance, -1 to 1.
	std::uint32_t track = 0; ///< Its track.
};

/// @brief A track's instrument and envelope.
struct SoundTrack {
	Instrument instrument = Instrument::Sine; ///< What plays it.
	float attack = 0.005F;                    ///< Seconds to full level.
	float decay = 0.1F;                       ///< Seconds to the sustain level.
	float sustain = 0.7F;                     ///< The level held, 0 to 1.
	float release = 0.1F;                     ///< Seconds to silence after the gate.
};

/// @brief A sound description, read and checked: its events in time order.
struct SoundScore {
	std::vector<SoundTrack> tracks; ///< The tracks.
	std::vector<SoundEvent> events; ///< Every note and hit, by start.
	double seconds = 0;             ///< How long it lasts (its "length", or until its last event has faded).
};

/// @brief Reads a sound description:
///
///     { "tempo": 120, "length": 8, "tracks": [
///       { "instrument": "triangle", "volume": 0.6, "pan": -0.2, "attack": 0.01, "release": 0.3,
///         "notes": [{ "at": 0, "note": "C4", "length": 1, "velocity": 0.8 }, { "at": 1, "note": 64 }] },
///       { "instrument": "kick", "steps": "x...x...x...x...", "step": 0.25 } ] }
///
/// Times are in beats of the tempo (20 to 300 a minute, 120 by default); "length" (beats) is how long it lasts,
/// by default until the last event has faded. A note is a name ("C4", "F#3", "Bb2") or a MIDI number (0 to 127),
/// "length" 1 beat by default, "velocity" 0 to 1 (1). "steps" is a pattern of one character a step ("x" a hit,
/// "X" an accented one, "." or "-" a rest), each "step" beats long (a quarter by default), from "at" (0).
/// @param text The description.
/// @param limits Its bounds.
/// @param error Receives what is wrong, naming the place ("tracks[1].notes[3].note: ...").
/// @return The score, or std::nullopt.
[[nodiscard]] std::optional<SoundScore> readSound(std::string_view text, std::string& error,
                                                  const SoundLimits& limits = {});

/// @brief Renders frames of a score, from any frame: deterministic (the same frames for the same index), so a voice
/// may seek and loop it. Called from a generator on the audio thread: it doesn't allocate.
/// @param score The score.
/// @param out Interleaved frames in @p format, overwritten.
/// @param first The index of the first frame.
/// @param format The engine's format.
/// @return The frames written: fewer than asked at its end.
std::size_t renderSound(const SoundScore& score, std::span<float> out, std::uint64_t first, const Format& format);

/// @brief A source that plays a sound description, rendered as it plays (a generator: never rendered whole).
/// @param text The description.
/// @param error Receives what is wrong with it.
/// @return The source, or nullptr.
[[nodiscard]] std::shared_ptr<Source> soundSource(std::string_view text, std::string& error);

} // namespace affresco::audio
