# audio: an output that mixes voices of files, bytes, streams and generators

`audio` plays sound for the app. An `Engine` opens an output (the system's default device, a null device that keeps
time without hardware, or none: the caller pulls the mix) and mixes **voices**, each playing one **source**: a file
decoded as it plays, encoded bytes in memory, a stream of encoded bytes or of PCM frames pushed from any thread, or a
generator that computes samples. A voice has a transport (play, pause, stop, seek, loop), a gain (volume, balance,
fades), a playback rate, and events it posts to the main thread. The engine converts every source's rate and channels
to its own, has a master volume, and measures the mix's levels for meters and visualisers. The screen format's
`audio` section and `audio` step play on it (`format/src/audio/`), and host code may use it directly: a music player,
a game's sound effects, a podcast, a voice assistant's speech, a synthesiser. Target: `affrescoui::audio` (uses `core`
and miniaudio 0.11.25: CoreAudio, WASAPI, PulseAudio or ALSA, and its WAV, FLAC and MP3 decoders). Optional:
`AFFRESCOUI_AUDIO=OFF` leaves it out, and the browser build has none (it has no threads).

## Public headers (`include/affresco/audio/`)

| Header | Offers |
|---|---|
| `Engine.hpp` | `Engine` (`createVoice`, `play`, `setMasterVolume`, `levels`, `stopAll`, `render`), `EngineOptions` (`output`, rate, channels, period, `executor`, `limits`), `Output` (`System`, `Null`, `Manual`) |
| `Voice.hpp` | `Voice` (`play`, `pause`, `stop`, `seek`, `setLoop`, `setVolume`, `fade`, `setPan`, `setRate`; `state`, `position`, `duration`, `levels`, `underruns`, `error`), `VoiceOptions`, `Event` and `EventType` (`Started`, `Ended`, `Underrun`, `Error`, `Position`), `State` |
| `Source.hpp` | `Source` (`file`, `memory`, `generator`, `tone`), `PcmStream` (float or 16-bit frames, `push`, `finish`), `EncodedStream` (bytes, `push`, `finish`), `Generator`, `Format`, `Limits`, `Levels`, `Encoding` and `encodingOf` |

```cpp
affresco::audio::Engine engine({.executor = &app}); // the default device; events on the main thread
std::string error;
auto voice = engine.play(affresco::audio::Source::file("sounds/door.wav"), {.volume = 0.8F}, error);

auto speech = affresco::audio::PcmStream::create({24000, 1}, 48000); // two seconds of room
engine.play(speech, {}, error);
speech->push(frames); // from the thread that receives them; finish() at the end
```

## Invariants

- **Every voice is mixed in the engine's format.** Decoders convert rates and channels as they decode, a PCM stream's
  frames go through miniaudio's data converter, and a generator is asked for the engine's format. A voice's playback
  rate steps through its source by linear interpolation: at rate 1 the source's frames come out exactly.
- **Gains are the documented ones.** A volume from 0 to 4 multiplies; a fade changes it linearly, frame by frame; the
  balance turns the side away from it down (`pan` 1: left 0, right 1; the centre leaves both at 1), on the first two
  channels; the master volume (0 to 4) multiplies the mix. Every sample a source gives is made finite before it is
  mixed (NaN is silence, anything beyond ±16 is clamped), so the mix is finite whatever a file holds.
- **Everything is bounded** (`Limits`, and the streams' capacities): a file at most 1 GiB, bytes in memory 256 MiB, a
  file or bytes playing at most a day (a sound whose length is longer is refused; one whose length isn't known, or a
  generator, ends there), at most 64 voices; a `PcmStream` holds at most 2^23 frames, an `EncodedStream` at most
  256 MiB (a push that doesn't fit is refused whole); an encoded stream decodes into half a second of frames ahead.
  Events: one Position at most every `positionInterval` (20 ms at least) per voice, and past 1,024 events posted and
  not yet run, Position events are dropped (the others come once per change).
- **Decoders read through the module's own bytes.** Every decoder reads its file, memory or stream through a virtual
  file system that refuses a seek past the end (miniaudio's own readers clamp it and report success, which let a WAV
  chunk whose size underflowed hang the parser). miniaudio is compiled with `-fwrapv`, so its arithmetic on
  untrusted samples never overflows into undefined behaviour. Both found by `tests/fuzz/audio.cpp`.
- **A stream plays on one voice.** A second voice on the same `PcmStream` or `EncodedStream` is refused; a stream that
  ended can't play again (its voice's `play()` returns false). Files, bytes in memory and generators play on any
  number of voices, each from its own position.
- **A voice outlives its handle while it plays** (fire and forget), then is forgotten; one that isn't playing is
  forgotten when its last handle goes. After its engine is destroyed a voice does nothing and stays stopped.

## Threads

The device's thread (or the caller of `render()`) mixes: it takes the engine's lock for one block at a time, reads
each playing voice's source (a file is decoded there, a few kilobytes at a time from the OS's cache), and measures the
mix. A second thread, in `System` and `Null`, wakes every `eventInterval` (25 ms) while any voice exists and posts the
voices' events to the `Executor`; `render()` posts them itself in `Manual`. An `EncodedStream` that plays has a thread
of its own that decodes into a ring, waiting for bytes as they arrive. Every public function may be called from any
thread; event handlers run on the `Executor`'s thread (the main thread). A host whose files are on slow storage should
read them itself and push them as an `EncodedStream`, so that the audio thread never waits for a disk.

## Errors

- What can't be opened (a file that is missing, too large or not WAV, FLAC or MP3; bytes over the limit; a sound
  longer than the limit; too many voices; a stream another voice has) makes `createVoice` and `play` return null with
  `error` saying why, naming a file by its name only, never its folder. Nothing throws.
- What goes wrong while playing (a decoding error) stops the voice: `error()` says why and its handler gets `Error`.
- A `System` engine whose device can't be opened plays to `Null` instead (`deviceError()` says why), so a host runs
  the same without audio hardware.

## Extension points

- **Synthesis:** `Source::generator` with a function of the frame index (seekable and loopable) or of nothing.
- **Other codecs** (Opus, Vorbis, AAC): decode them in the application and push the frames into a `PcmStream`.
- **Offline rendering** (an export, a test): an engine with `Output::Manual`, pulled with `render()` as fast as wanted.

## Files

```
include/affresco/audio/   the public API (above)
src/
  Engine.cpp            Engine: the device (miniaudio), the mix, the event thread
  Voice.cpp             the voices: transport, gain, fades, the rate converter, levels; Voice, their handle
  Shared.hpp            the engine's state and a voice's, under one lock
  Sources.cpp           the sources: files, bytes, generators, tones, the two streams and the PCM stream's reader
  Decoders.hpp, .cpp    decoding files, bytes and encoded streams, through a bounded virtual file system
  Buffers.hpp, .cpp     FrameRing (frames to the audio thread, lock-free for it) and ByteQueue (a stream's bytes)
  Reader.hpp            what a voice reads its frames from
  Miniaudio.c           miniaudio's implementation, compiled once
```

Tests: `tests/unit/audio_engine.cpp` (the module on Manual engines and the null device), `audio_format.cpp` (the
format's section and step), `tests/fuzz/audio.cpp` (the decoders on untrusted bytes).
