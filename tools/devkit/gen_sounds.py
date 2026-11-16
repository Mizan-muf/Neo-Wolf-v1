#!/usr/bin/env python3
"""Deterministic generator for Neo-Wolf's sound effects (Roadmap Phase 10, known issue X5).

Writes four 16-bit PCM mono WAV files at 44,100 Hz (the rate AudioSystem::Init asks the
device for) to game/assets/:

    sfx_weapon_fire.wav   gunshot: a noise crack, a filtered noise body, a falling low thump
    sfx_door_open.wav     door: a latch clunk, a filtered-noise slide with a roller rumble,
                          a soft stop thud (the engine plays it for opening and closing)
    sfx_pickup.wav        pickup: a rising three-note triangle-wave chime (E6, A6, C#7)
    sfx_enemy_alert.wav   alert: a two-tone falling square-wave klaxon with a growl (D5 -> A4);
                          the engine also plays it when the player is hit

game/defs/audio.cfg maps the engine's sound slots to these files.

Why they are short: AudioSystem queues effects one after another (SDL_QueueAudio, no mixer),
so a long effect delays the next one. The gunshot is shorter than the weapon's fire interval
(gameplay.json weapon.fireRate, 0.25 s), so holding fire never builds up a backlog; the
others stay at or under 0.30 s. DevKit test test_gen_sounds checks this.

Synthesis uses only the standard library: band-limited square and triangle waves (sums of
odd harmonics below the Nyquist frequency, so no aliasing), white noise from a seeded
random.Random, one-pole low-pass filters, exponential envelopes and pitch sweeps. Each effect
is high-passed at 30 Hz (no DC offset), faded in over 0.3 ms and out over 8 ms (no clicks),
and normalised to its own peak level (PEAK_DBFS, between -3 and -12 dBFS, so below clipping;
the levels balance the effects' loudness: the dense square-wave alert gets the lowest peak).

Deterministic: each sound has its own random.Random seeded from SEED and the sound's name
(zlib.crc32); the arithmetic is plain IEEE double precision and the samples are rounded to
16-bit integers, so every run writes the same bytes (the `wave` module writes a fixed
44-byte header). As with the other generators, the sounds are made from scratch by this
script: no recordings or third-party samples (docs/ASSET_CREDITS.md).

Usage:
    python3 tools/devkit/gen_sounds.py                 # (re)write the files under game/
    python3 tools/devkit/gen_sounds.py --out DIR       # write to DIR instead
    python3 tools/devkit/gen_sounds.py --check         # exit 1 if a committed file differs
"""
from __future__ import annotations

import argparse
import io
import math
import random
import struct
import sys
import wave
import zlib
from pathlib import Path
from typing import Callable, Dict, List, Optional

SEED = 20261116
SAMPLE_RATE = 44100  # AudioSystem::Init's device rate: no resampling at load time
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO_ROOT / "game" / "assets"

Signal = List[float]

# Peak level of each effect after normalisation, in dB relative to full scale.
PEAK_DBFS: Dict[str, float] = {
    "weapon_fire": -3.0,
    "door_open": -6.0,
    "enemy_alert": -12.0,  # a dense square wave: much louder than its peak suggests
    "pickup": -8.0,
}


# --- Building blocks ----------------------------------------------------------------------

def frames(seconds: float) -> int:
    return int(round(seconds * SAMPLE_RATE))


def silence(seconds: float) -> Signal:
    return [0.0] * frames(seconds)


def noise(rng: random.Random, n: int) -> Signal:
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def lowpass(signal: Signal, cutoff: Callable[[float], float]) -> Signal:
    """One-pole low-pass filter; `cutoff(t)` (Hz) may change over time."""
    out: Signal = []
    y = 0.0
    for i, x in enumerate(signal):
        a = 1.0 - math.exp(-2.0 * math.pi * cutoff(i / SAMPLE_RATE) / SAMPLE_RATE)
        y += a * (x - y)
        out.append(y)
    return out


def highpass(signal: Signal, cutoff: float) -> Signal:
    """One-pole high-pass filter (removes DC and rumble below `cutoff` Hz)."""
    rc = 1.0 / (2.0 * math.pi * cutoff)
    alpha = rc / (rc + 1.0 / SAMPLE_RATE)
    out: Signal = []
    prev_x = 0.0
    y = 0.0
    for x in signal:
        y = alpha * (y + x - prev_x)
        prev_x = x
        out.append(y)
    return out


def decay(n: int, tau: float, attack: float = 0.0) -> Signal:
    """Linear attack over `attack` seconds, then exponential decay with time constant `tau`."""
    a = frames(attack)
    env: Signal = []
    for i in range(n):
        t = i / SAMPLE_RATE
        rise = min(1.0, (i + 1) / a) if a > 0 else 1.0
        env.append(rise * math.exp(-max(0.0, t - attack) / tau))
    return env


def oscillator(n: int, freq: Callable[[float], float], harmonic: Callable[[int], float]) -> Signal:
    """Additive oscillator: sum of harmonic k * f with amplitude harmonic(k) (0 skips it),
    kept below 0.45 * SAMPLE_RATE. `freq(t)` may sweep; the phase is integrated per sample."""
    out: Signal = []
    phase = 0.0
    nyquist = 0.45 * SAMPLE_RATE
    for i in range(n):
        f = freq(i / SAMPLE_RATE)
        value = 0.0
        k = 1
        while k * f < nyquist:
            amp = harmonic(k)
            if amp:
                value += amp * math.sin(k * phase)
            k += 1
        out.append(value)
        phase += 2.0 * math.pi * f / SAMPLE_RATE
    return out


def square_harmonic(k: int) -> float:
    return 4.0 / (math.pi * k) if k % 2 else 0.0


def triangle_harmonic(k: int) -> float:
    if k % 2 == 0:
        return 0.0
    return (8.0 / (math.pi * math.pi)) * (1.0 if (k // 2) % 2 == 0 else -1.0) / (k * k)


def sine_harmonic(k: int) -> float:
    return 1.0 if k == 1 else 0.0


def mul(a: Signal, b: Signal) -> Signal:
    return [x * y for x, y in zip(a, b)]


def mix(n: int, *parts: tuple) -> Signal:
    """mix(n, (gain, signal, start_seconds), ...) -> one signal of n samples."""
    out = [0.0] * n
    for gain, signal, start in parts:
        offset = frames(start)
        for i, x in enumerate(signal):
            if 0 <= offset + i < n:
                out[offset + i] += gain * x
    return out


def exp_sweep(f0: float, f1: float, seconds: float) -> Callable[[float], float]:
    """Exponential pitch sweep from f0 to f1 over `seconds`, then held at f1."""
    return lambda t: f0 * (f1 / f0) ** min(1.0, t / seconds)


# --- The effects --------------------------------------------------------------------------

def weapon_fire(rng: random.Random) -> Signal:
    """0.20 s gunshot. A bright crack (high-passed noise, tau 12 ms), a body of noise whose
    low-pass falls from 4 kHz to 500 Hz (tau 45 ms), a sine thump sweeping 160 -> 45 Hz
    (tau 50 ms) and a dark room tail (600 Hz low-pass noise, tau 90 ms)."""
    n = frames(0.20)
    white = noise(rng, n)
    crack = mul(highpass(white, 2000.0), decay(n, 0.012))
    body = mul(lowpass(noise(rng, n), exp_sweep(4000.0, 500.0, 0.08)), decay(n, 0.045))
    thump = mul(oscillator(n, exp_sweep(160.0, 45.0, 0.06), sine_harmonic), decay(n, 0.05, 0.001))
    tail = mul(lowpass(noise(rng, n), lambda t: 600.0), decay(n, 0.09, 0.004))
    return mix(n, (1.2, crack, 0.0), (2.0, body, 0.0), (0.6, thump, 0.0), (0.9, tail, 0.0))


def door_open(rng: random.Random) -> Signal:
    """0.30 s heavy door. A latch clunk at the start (110 Hz with 2nd and 3rd partials,
    tau 25 ms, plus a 4 ms click), a slide of noise through a two-pole low-pass that opens
    from 300 Hz to 2.2 kHz and closes again, under a rising-and-falling envelope
    (0.02-0.27 s) with a 55 Hz triangle roller rumble, and a soft stop thud (95 -> 70 Hz)
    at 0.25 s."""
    n = frames(0.30)
    clunk_n = frames(0.12)
    clunk = mul(oscillator(clunk_n, lambda t: 110.0, lambda k: {1: 1.0, 2: 0.5, 3: 0.25}.get(k, 0.0)),
                decay(clunk_n, 0.025, 0.002))
    click_n = frames(0.004)
    click = mul(highpass(noise(rng, click_n), 1500.0), decay(click_n, 0.0015))

    slide_s = 0.25
    slide_n = frames(slide_s)

    def bump(t: float) -> float:  # 0 -> 1 -> 0 over the slide
        return math.sin(math.pi * min(1.0, t / slide_s)) ** 2

    def cutoff(t: float) -> float:
        return 300.0 + 1900.0 * bump(t)

    slide = lowpass(lowpass(noise(rng, slide_n), cutoff), cutoff)  # two poles: a darker whoosh
    rumble = oscillator(slide_n, lambda t: 55.0 + 6.0 * math.sin(2.0 * math.pi * 9.0 * t), triangle_harmonic)
    envelope = [bump(i / SAMPLE_RATE) for i in range(slide_n)]
    slide_mix = [(3.5 * s + 0.35 * r) * e for s, r, e in zip(slide, rumble, envelope)]

    thud_n = frames(0.05)
    thud = mul(oscillator(thud_n, exp_sweep(95.0, 70.0, 0.04), sine_harmonic), decay(thud_n, 0.018, 0.002))
    return mix(n, (1.6, clunk, 0.0), (0.8, click, 0.0), (1.0, slide_mix, 0.02), (1.0, thud, 0.25))


def pickup(rng: random.Random) -> Signal:
    """0.20 s chime: an A-major arpeggio of triangle tones, E6 (1318.5 Hz), A6 (1760 Hz) and
    C#7 (2217.5 Hz), 45 ms apart, each with a 2 ms attack, tau 60 ms and a quiet octave
    partial for sparkle. Rising, bright and pure: unlike the alert, it has no buzz."""
    del rng  # no noise in this one
    n = frames(0.20)
    parts = []
    for index, freq in enumerate((1318.5, 1760.0, 2217.5)):
        start = 0.045 * index
        length = n - frames(start)
        tone = oscillator(length, lambda t, f=freq: f, triangle_harmonic)
        octave = oscillator(length, lambda t, f=freq: 2.0 * f, sine_harmonic)
        env = decay(length, 0.06, 0.002)
        parts.append((1.0, [(a + 0.2 * b) * e for a, b, e in zip(tone, octave, env)], start))
    return mix(n, *parts)


def enemy_alert(rng: random.Random) -> Signal:
    """0.28 s alarm bark: two square-wave tones falling a fourth, D5 (587 Hz, 0-0.11 s, scooping
    up from 90 %) then A4 (440 Hz, 0.12-0.27 s, drooping to 85 % at the end), with a 35 Hz
    amplitude growl (depth 30 %), a 3.5 kHz low-pass to take the edge off, and a breath of
    low-passed noise."""
    n = frames(0.28)

    def tone(freq: Callable[[float], float], seconds: float) -> Signal:
        m = frames(seconds)
        wave_ = lowpass(oscillator(m, freq, square_harmonic), lambda t: 3500.0)
        env = []
        for i in range(m):
            t = i / SAMPLE_RATE
            attack = min(1.0, t / 0.005)
            release = min(1.0, (seconds - t) / 0.02)
            growl = 1.0 - 0.3 * (0.5 + 0.5 * math.sin(2.0 * math.pi * 35.0 * t))
            env.append(max(0.0, attack * release * growl))
        return mul(wave_, env)

    high = tone(lambda t: 587.33 * (0.9 + 0.1 * min(1.0, t / 0.015)), 0.11)
    low = tone(lambda t: 440.0 * (1.0 - 0.15 * max(0.0, (t - 0.09) / 0.06)), 0.15)
    breath = mul(lowpass(noise(rng, n), lambda t: 1200.0), decay(n, 0.12, 0.01))
    return mix(n, (1.0, high, 0.0), (1.0, low, 0.12), (0.35, breath, 0.0))


SOUNDS: Dict[str, Callable[[random.Random], Signal]] = {
    "weapon_fire": weapon_fire,
    "door_open": door_open,
    "pickup": pickup,
    "enemy_alert": enemy_alert,
}


# --- Output -------------------------------------------------------------------------------

def finish(signal: Signal, peak_dbfs: float) -> List[int]:
    """High-pass at 30 Hz, fade in (0.3 ms) and out (8 ms), normalise the peak to
    `peak_dbfs` and round to 16-bit samples."""
    signal = highpass(signal, 30.0)
    n = len(signal)
    fade_in, fade_out = frames(0.0003), frames(0.008)
    for i in range(min(fade_in, n)):
        signal[i] *= i / fade_in
    for i in range(min(fade_out, n)):
        signal[n - 1 - i] *= i / fade_out
    peak = max(abs(x) for x in signal)
    scale = (10.0 ** (peak_dbfs / 20.0)) * 32767.0 / peak
    return [max(-32767, min(32767, int(round(x * scale)))) for x in signal]


def wav_bytes(samples: List[int]) -> bytes:
    buffer = io.BytesIO()
    with wave.open(buffer, "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(SAMPLE_RATE)
        out.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    return buffer.getvalue()


def generate() -> Dict[str, bytes]:
    """Returns {"sfx_<name>.wav": WAV bytes} for every sound."""
    files: Dict[str, bytes] = {}
    for name, synth in SOUNDS.items():
        rng = random.Random(SEED ^ zlib.crc32(f"sound/{name}".encode("utf-8")))
        files[f"sfx_{name}.wav"] = wav_bytes(finish(synth(rng), PEAK_DBFS[name]))
    return files


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="output folder (default: game/assets)")
    parser.add_argument("--check", action="store_true", help="compare with the files on disk instead of writing")
    args = parser.parse_args(argv if argv is not None else sys.argv[1:])

    files = generate()
    if args.check:
        stale = [rel for rel, data in files.items()
                 if not (args.out / rel).is_file() or (args.out / rel).read_bytes() != data]
        if stale:
            print("Out of date (run tools/devkit/gen_sounds.py): " + ", ".join(stale))
            return 1
        print(f"All {len(files)} generated sounds are up to date.")
        return 0

    args.out.mkdir(parents=True, exist_ok=True)
    for rel, data in files.items():
        (args.out / rel).write_bytes(data)
    print(f"Wrote {len(files)} sounds to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
