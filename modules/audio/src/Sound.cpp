/// @file
/// @brief Sound descriptions (Sound.hpp): read and checked from JSON, rendered sample by sample from their events by
/// built-in instruments, as a generator source.

#include "affresco/audio/Sound.hpp"

#include <affresco/core/Json.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <utility>

namespace affresco::audio {

namespace {

constexpr double twoPi = 2 * std::numbers::pi;

/// @brief How long a drum sounds, in seconds.
/// @param instrument The instrument.
/// @return Its length, or 0 for one with an envelope.
double drumSeconds(Instrument instrument) {
	switch(instrument) {
	case Instrument::Kick:
		return 0.5;
	case Instrument::Snare:
		return 0.3;
	case Instrument::Hat:
		return 0.1;
	case Instrument::Clap:
		return 0.3;
	default:
		return 0;
	}
}

/// @brief When an event has faded.
/// @param event The event.
/// @param track Its track.
/// @return Its end, in seconds.
double endOf(const SoundEvent& event, const SoundTrack& track) {
	const double drum = drumSeconds(track.instrument);
	return event.start + (drum > 0 ? drum : event.gate + track.release);
}

/// @brief Noise: the same value for the same event and sample, so rendering is deterministic.
/// @param event The event's index.
/// @param sample The sample since its start.
/// @return A value from -1 to 1.
float noise(std::uint64_t event, std::uint64_t sample) {
	std::uint64_t x = (event << 40) ^ sample ^ 0x9E3779B97F4A7C15ULL;
	x ^= x >> 30;
	x *= 0xBF58476D1CE4E5B9ULL;
	x ^= x >> 27;
	x *= 0x94D049BB133111EBULL;
	x ^= x >> 31;
	return static_cast<float>(static_cast<double>(x >> 11) / static_cast<double>(1ULL << 52) - 1.0);
}

/// @brief An ADSR envelope's level.
/// @param track The envelope.
/// @param t Seconds since the start.
/// @param gate How long it is held.
/// @return The level, 0 to 1.
double envelope(const SoundTrack& track, double t, double gate) {
	const auto held = [&](double at) {
		if(at < track.attack) return at / std::max(1e-6, static_cast<double>(track.attack));
		const double d = at - track.attack;
		if(d < track.decay) return 1.0 - (1.0 - track.sustain) * d / std::max(1e-6, static_cast<double>(track.decay));
		return static_cast<double>(track.sustain);
	};
	if(t < gate) return held(t);
	const double r = t - gate;
	if(r >= track.release) return 0;
	return held(gate) * (1.0 - r / std::max(1e-6, static_cast<double>(track.release)));
}

/// @brief One sample of an event.
/// @param track Its track.
/// @param event The event.
/// @param index Its index, for its noise.
/// @param t Seconds since its start.
/// @param sample Samples since its start.
/// @return The sample, before gain and pan.
double voice(const SoundTrack& track, const SoundEvent& event, std::uint64_t index, double t, std::uint64_t sample) {
	const double f = event.frequency;
	const double phase = f * t - std::floor(f * t);
	switch(track.instrument) {
	case Instrument::Sine:
		return std::sin(twoPi * phase) * envelope(track, t, event.gate);
	case Instrument::Square:
		return (phase < 0.5 ? 0.6 : -0.6) * envelope(track, t, event.gate);
	case Instrument::Saw:
		return (2 * phase - 1) * 0.6 * envelope(track, t, event.gate);
	case Instrument::Triangle:
		return (1 - 4 * std::abs(phase - 0.5)) * envelope(track, t, event.gate);
	case Instrument::Noise:
		return noise(index, sample) * 0.5 * envelope(track, t, event.gate);
	case Instrument::Kick: { // a sine falling from 150 to 50 Hz
		constexpr double k = 20;
		const double cycles = 50 * t + (100 / k) * (1 - std::exp(-k * t));
		return std::sin(twoPi * cycles) * std::exp(-6 * t);
	}
	case Instrument::Snare:
		return 0.6 * noise(index, sample) * std::exp(-20 * t) + 0.4 * std::sin(twoPi * 180 * t) * std::exp(-15 * t);
	case Instrument::Hat: // noise less the sample before: its high part
		return 0.5 * (noise(index, sample) - noise(index, sample == 0 ? 0 : sample - 1)) * std::exp(-60 * t);
	case Instrument::Clap: {
		const double burst = t < 0.03 ? std::exp(-150 * std::fmod(t, 0.01)) : std::exp(-25 * (t - 0.03));
		return 0.6 * noise(index, sample) * burst;
	}
	}
	return 0;
}

/// @brief Reads a number in a range.
/// @param value The object.
/// @param key The key.
/// @param fallback Its value when missing.
/// @param low The least.
/// @param high The most.
/// @param at The place.
/// @return The number.
/// @throws DocumentError If it isn't one in the range.
double number(const json& value, const char* key, double fallback, double low, double high, const std::string& at) {
	if(!value.contains(key)) return fallback;
	const json& n = value[key];
	if(!n.is_number() || !(n.get<double>() >= low && n.get<double>() <= high))
		throw DocumentError(std::format("{}.{}: expected a number from {} to {}", at, key, low, high));
	return n.get<double>();
}

/// @brief A note's frequency.
/// @param note A name ("C4", "F#3", "Bb2") or a MIDI number.
/// @param at The place.
/// @return Hertz.
/// @throws DocumentError If it is neither.
double frequencyOf(const json& note, const std::string& at) {
	double midi = -1;
	if(note.is_number_integer() && note.get<long long>() >= 0 && note.get<long long>() <= 127)
		midi = static_cast<double>(note.get<long long>());
	else if(note.is_string()) {
		const std::string& name = note.get_ref<const std::string&>();
		static constexpr std::array<int, 7> semitones = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
		std::size_t i = 0;
		if(!name.empty() && name[0] >= 'A' && name[0] <= 'G') {
			int pitch = semitones.at(static_cast<std::size_t>(name[0] - 'A'));
			i = 1;
			if(i < name.size() && (name[i] == '#' || name[i] == 'b')) pitch += name[i++] == '#' ? 1 : -1;
			if(i < name.size() && ((name[i] >= '0' && name[i] <= '8') || (name[i] == '-' && i + 1 < name.size()))) {
				const bool negative = name[i] == '-';
				if(negative) ++i;
				if(i + 1 == name.size() && name[i] >= '0' && name[i] <= '9')
					midi = (negative ? -(name[i] - '0') : name[i] - '0') * 12 + 12 + pitch;
			}
		}
	}
	if(midi < 0 || midi > 127) throw DocumentError(at + ": expected a note such as \"C4\", \"F#3\", or 0 to 127");
	return 440 * std::pow(2.0, (midi - 69) / 12);
}

/// @brief Reads an instrument.
/// @param value Its name.
/// @param at The place.
/// @return It.
/// @throws DocumentError If it is none.
Instrument instrumentOf(const json& value, const std::string& at) {
	static constexpr std::array<std::pair<std::string_view, Instrument>, 9> names = {
	    {{"sine", Instrument::Sine},
		 {"square", Instrument::Square},
		 {"saw", Instrument::Saw},
		 {"triangle", Instrument::Triangle},
		 {"noise", Instrument::Noise},
		 {"kick", Instrument::Kick},
		 {"snare", Instrument::Snare},
		 {"hat", Instrument::Hat},
		 {"clap", Instrument::Clap}}};
	if(value.is_string())
		for(const auto& [name, instrument] : names)
			if(name == value.get_ref<const std::string&>()) return instrument;
	throw DocumentError(at + ": expected sine, square, saw, triangle, noise, kick, snare, hat or clap");
}

/// @brief Reads one track, adding its events.
/// @param value The track.
/// @param index Its index.
/// @param beat Seconds a beat.
/// @param limits The bounds.
/// @param score Gets the track and its events.
/// @throws DocumentError If it is malformed or goes beyond the bounds.
void readTrack(const json& value, std::uint32_t index, double beat, const SoundLimits& limits, SoundScore& score) {
	const std::string at = std::format("tracks[{}]", index);
	if(!value.is_object()) throw DocumentError(at + ": expected a track");
	for(const auto& [key, item] : value.items())
		if(key != "instrument" && key != "volume" && key != "pan" && key != "attack" && key != "decay" &&
		   key != "sustain" && key != "release" && key != "notes" && key != "steps" && key != "step" && key != "at" &&
		   key != "note")
			throw DocumentError(at + "." + key + ": unknown key");
	SoundTrack track;
	track.instrument = instrumentOf(value.value("instrument", json()), at + ".instrument");
	track.attack = static_cast<float>(number(value, "attack", track.attack, 0, 5, at));
	track.decay = static_cast<float>(number(value, "decay", track.decay, 0, 5, at));
	track.sustain = static_cast<float>(number(value, "sustain", track.sustain, 0, 1, at));
	track.release = static_cast<float>(number(value, "release", track.release, 0, 5, at));
	const auto volume = static_cast<float>(number(value, "volume", 0.8, 0, 1, at));
	const auto pan = static_cast<float>(number(value, "pan", 0, -1, 1, at));
	score.tracks.push_back(track);
	const auto add = [&](double startBeats, double lengthBeats, double frequency, double velocity) {
		if(score.events.size() >= limits.events)
			throw DocumentError(std::format("{}: more than {} notes and hits in all", at, limits.events));
		score.events.push_back(
		    {startBeats * beat, lengthBeats * beat, frequency, static_cast<float>(velocity) * volume, pan, index});
	};
	if(value.contains("notes")) {
		const json& notes = value["notes"];
		if(!notes.is_array()) throw DocumentError(at + ".notes: expected a list of notes");
		for(std::size_t i = 0; i < notes.size(); ++i) {
			const std::string place = std::format("{}.notes[{}]", at, i);
			if(!notes[i].is_object()) throw DocumentError(place + ": expected {\"at\", \"note\", \"length\"?}");
			add(number(notes[i], "at", 0, 0, 100000, place), number(notes[i], "length", 1, 0.001, 1000, place),
			    notes[i].contains("note") ? frequencyOf(notes[i]["note"], place + ".note") : 440,
			    number(notes[i], "velocity", 1, 0, 1, place));
		}
	}
	if(value.contains("steps")) {
		if(!value["steps"].is_string()) throw DocumentError(at + ".steps: expected a pattern such as \"x...x...\"");
		const std::string& steps = value["steps"].get_ref<const std::string&>();
		const double step = number(value, "step", 0.25, 0.01, 16, at);
		const double from = number(value, "at", 0, 0, 100000, at);
		const double frequency = value.contains("note") ? frequencyOf(value["note"], at + ".note") : 440;
		for(std::size_t i = 0; i < steps.size(); ++i) {
			const char c = steps[i];
			if(c == '.' || c == '-' || c == ' ') continue;
			if(c != 'x' && c != 'X') throw DocumentError(at + ".steps: one of x, X, . or - a step");
			add(from + static_cast<double>(i) * step, step, frequency, c == 'X' ? 1.0 : 0.7);
		}
	}
}

/// @brief Refuses a score with more notes sounding at once than the bounds allow.
/// @param score The score, its events by start.
/// @param limits The bounds.
/// @throws DocumentError If it has.
void expectPolyphony(const SoundScore& score, const SoundLimits& limits) {
	std::vector<std::pair<double, int>> edges;
	edges.reserve(score.events.size() * 2);
	for(const SoundEvent& event : score.events) {
		edges.emplace_back(event.start, 1);
		edges.emplace_back(endOf(event, score.tracks[event.track]), -1);
	}
	std::ranges::sort(edges); // an end before a start at the same time
	int sounding = 0;
	for(const auto& [time, change] : edges)
		if((sounding += change) > static_cast<int>(limits.polyphony))
			throw DocumentError(std::format("more than {} notes sound at once (at {:.2f} s)", limits.polyphony, time));
}

} // namespace

std::optional<SoundScore> readSound(std::string_view text, std::string& error, const SoundLimits& limits) {
	try {
		if(text.size() > limits.bytes) throw DocumentError(std::format("longer than {} bytes", limits.bytes));
		const json value = parseUntrusted(text);
		if(!value.is_object()) throw DocumentError("expected {\"tempo\", \"tracks\": [...]}");
		for(const auto& [key, item] : value.items())
			if(key != "tempo" && key != "length" && key != "tracks" && key != "note" && key != "format")
				throw DocumentError(key + ": unknown key (expected tempo, length, tracks or note)");
		const double beat = 60 / number(value, "tempo", 120, 20, 300, "sound");
		const json& tracks = value.contains("tracks") ? value["tracks"] : json();
		if(!tracks.is_array() || tracks.empty() || tracks.size() > limits.tracks)
			throw DocumentError(std::format("tracks: expected 1 to {} tracks", limits.tracks));
		SoundScore score;
		for(std::size_t i = 0; i < tracks.size(); ++i)
			readTrack(tracks[i], static_cast<std::uint32_t>(i), beat, limits, score);
		std::ranges::stable_sort(score.events, {}, &SoundEvent::start);
		for(const SoundEvent& event : score.events)
			score.seconds = std::max(score.seconds, endOf(event, score.tracks[event.track]));
		if(value.contains("length")) score.seconds = number(value, "length", 0, 0.001, 100000, "sound") * beat;
		if(score.seconds > limits.seconds)
			throw DocumentError(std::format("it lasts {:.0f} s, more than {:.0f}", score.seconds, limits.seconds));
		expectPolyphony(score, limits);
		return score;
	} catch(const std::exception& failure) {
		error = failure.what();
		if(const std::size_t prefix = error.find("AffrescoUI screen: "); prefix == 0) error.erase(0, 19);
		return std::nullopt;
	}
}

std::size_t renderSound(const SoundScore& score, std::span<float> out, std::uint64_t first, const Format& format) {
	const std::size_t channels = std::max<std::size_t>(1, format.channels);
	const double rate = format.sampleRate;
	const auto total = static_cast<std::uint64_t>(score.seconds * rate);
	const std::size_t asked = out.size() / channels;
	const std::size_t frames =
	    first >= total ? 0 : static_cast<std::size_t>(std::min<std::uint64_t>(asked, total - first));
	std::fill(out.begin(), out.end(), 0.0F);
	if(frames == 0) return 0;
	const double from = static_cast<double>(first) / rate, to = static_cast<double>(first + frames) / rate;
	double longest = 0; // the longest any event lasts: those that started that long before the block may still sound
	for(const SoundTrack& track : score.tracks)
		longest = std::max(longest, drumSeconds(track.instrument) > 0 ? drumSeconds(track.instrument) : 0.0);
	for(const SoundEvent& event : score.events) longest = std::max(longest, event.gate + 5.0);
	const auto begin = std::ranges::lower_bound(score.events, from - longest, {}, &SoundEvent::start);
	for(auto it = begin; it != score.events.end() && it->start < to; ++it) {
		const SoundTrack& track = score.tracks[it->track];
		const double end = endOf(*it, track);
		if(end <= from) continue;
		const auto index = static_cast<std::uint64_t>(it - score.events.begin());
		const auto startFrame = static_cast<std::uint64_t>(std::llround(it->start * rate));
		const float left = it->gain * std::min(1.0F, 1.0F - it->pan) * 0.5F;
		const float right = it->gain * std::min(1.0F, 1.0F + it->pan) * 0.5F;
		for(std::size_t i = 0; i < frames; ++i) {
			const std::uint64_t frame = first + i;
			if(frame < startFrame) continue;
			const std::uint64_t sample = frame - startFrame;
			const double t = static_cast<double>(sample) / rate;
			if(it->start + t >= end) break;
			const auto value = static_cast<float>(voice(track, *it, index, t, sample));
			float* at = out.data() + i * channels;
			if(channels == 1) at[0] += value * (left + right) * 0.5F;
			else {
				at[0] += value * left;
				at[1] += value * right;
			}
		}
	}
	for(float& sample : out.first(frames * channels)) sample = std::clamp(sample, -1.0F, 1.0F);
	return frames;
}

std::shared_ptr<Source> soundSource(std::string_view text, std::string& error) {
	auto score = readSound(text, error);
	if(!score) return nullptr;
	const auto shared = std::make_shared<const SoundScore>(std::move(*score));
	return Source::generator([shared](std::span<float> out, std::uint64_t first,
	                                  const Format& format) { return renderSound(*shared, out, first, format); },
	                         shared->seconds, true);
}

} // namespace affresco::audio
