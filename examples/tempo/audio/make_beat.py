#!/usr/bin/env python3
"""Writes beatbox.wav, the loop Tempo plays: a beatbox of four bars at 120 beats a minute, synthesised here from
nothing but sines and seeded noise (a kick, a snare, closed and open hi-hats, a clap and a bass blip). No sample comes
from anywhere else: the sound is this project's own work, under the repository's Apache-2.0 licence.

    python3 examples/tempo/audio/make_beat.py          # writes examples/tempo/audio/beatbox.wav

16-bit PCM, mono, 22,050 Hz, 8 seconds. Every voice's tail that runs past the end is wrapped round to the start, so
that the file loops without a gap; 3 ms fades at both ends keep a single play from clicking. The mix is normalised to
-1 dBFS, so nothing clips. Only the standard library is used (wave, struct, math, random with a fixed seed), so the
same Python writes the same bytes.
"""

import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
BPM = 120
BARS = 4
STEPS = 16  # sixteenth notes in a bar
STEP = 60 / BPM / 4  # seconds in a sixteenth
LENGTH = int(round(BARS * STEPS * STEP * RATE))
OUT = Path(__file__).resolve().parent / "beatbox.wav"


def kick(noise):
    """A kick drum: a sine falling from 150 to 45 Hz, with a short click."""
    out, phase = [], 0.0
    for i in range(int(0.35 * RATE)):
        t = i / RATE
        phase += 2 * math.pi * (45 + 105 * math.exp(-t * 30)) / RATE
        click = (noise.random() * 2 - 1) * math.exp(-t * 900) * 0.3
        out.append(math.sin(phase) * math.exp(-t * 9) + click)
    return out


def snare(noise):
    """A snare: a band of noise and a 190 Hz body, both decaying."""
    out, last = [], 0.0
    for i in range(int(0.2 * RATE)):
        t = i / RATE
        white = noise.random() * 2 - 1
        bright = white - last * 0.6  # a gentle high-pass
        last = white
        out.append(bright * math.exp(-t * 22) * 0.55 + math.sin(2 * math.pi * 190 * t) * math.exp(-t * 35) * 0.4)
    return out


def hat(noise, seconds, decay):
    """A hi-hat: noise with its lows taken out (each sample less the one before), decaying fast."""
    out, last = [], 0.0
    for i in range(int(seconds * RATE)):
        white = noise.random() * 2 - 1
        out.append((white - last) * 0.5 * math.exp(-i / RATE * decay))
        last = white
    return out


def clap(noise):
    """A clap: three short bursts of noise 11 ms apart, then a tail."""
    out = []
    for i in range(int(0.18 * RATE)):
        t = i / RATE
        burst = max(math.exp(-((t - k * 0.011) * 400) ** 2) for k in range(3)) if t < 0.04 else 0.0
        out.append((noise.random() * 2 - 1) * (burst * 0.6 + math.exp(-t * 18) * 0.25))
    return out


def bass(frequency):
    """A bass blip: a sine and its octave, with a soft attack and a quick decay."""
    out = []
    for i in range(int(0.22 * RATE)):
        t = i / RATE
        envelope = min(t / 0.004, 1.0) * math.exp(-t * 12)
        out.append((math.sin(2 * math.pi * frequency * t) + 0.3 * math.sin(4 * math.pi * frequency * t)) * envelope)
    return out


def pattern(noise):
    """Where each sound starts, in sixteenths from the start of the loop, and how loud."""
    hits = []
    for bar in range(BARS):
        at = bar * STEPS
        for step in (0, 8) + ((10,) if bar % 2 == 1 else (6,)):
            hits.append((at + step, kick(noise), 0.9))
        for step in (4, 12):
            hits.append((at + step, snare(noise), 0.55))
        for step in range(0, STEPS, 2):
            open_hat = bar == BARS - 1 and step == 14
            hits.append((at + step, hat(noise, 0.25 if open_hat else 0.05, 9 if open_hat else 80),
                         0.3 if step % 4 else 0.22))
        if bar % 2 == 1:
            hits.append((at + 12, clap(noise), 0.4))
        for step, note in zip((0, 3, 7, 11), (55.0, 55.0, 65.41, 49.0) if bar < BARS - 1 else (55.0, 65.41, 73.42, 82.41)):
            hits.append((at + step, bass(note), 0.45))
    return hits


def mix():
    """The loop's samples, from -1 to 1."""
    noise = random.Random(2026)  # a fixed seed: the same noise every time
    samples = [0.0] * LENGTH
    for step, sound, gain in pattern(noise):
        start = int(round(step * STEP * RATE))
        for i, value in enumerate(sound):
            samples[(start + i) % LENGTH] += value * gain  # a tail past the end plays at the start: it loops
    peak = max(abs(value) for value in samples)
    scale = 10 ** (-1 / 20) / peak  # -1 dBFS
    fade = int(0.003 * RATE)
    for i in range(fade):
        samples[i] *= i / fade
        samples[LENGTH - 1 - i] *= i / fade
    return [value * scale for value in samples]


def main():
    """Writes the file."""
    frames = b"".join(struct.pack("<h", max(-32767, min(32767, int(round(value * 32767))))) for value in mix())
    with wave.open(str(OUT), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes(frames)
    print(f"{OUT.name}: {LENGTH / RATE:.2f} s, {OUT.stat().st_size} bytes")


if __name__ == "__main__":
    main()
