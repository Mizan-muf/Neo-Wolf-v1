"""The sound generator is deterministic, writes what the engine expects, and the committed
sounds match it."""

import io
import json
import math
import struct
import unittest
import wave
from pathlib import Path

from tools.devkit import gen_sounds

REPO_ROOT = Path(__file__).resolve().parents[3]


def decode(data: bytes):
    with wave.open(io.BytesIO(data), "rb") as w:
        params = (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getcomptype())
        n = w.getnframes()
        samples = struct.unpack(f"<{n}h", w.readframes(n))
    return params, samples


def band_power(samples, freq: float) -> float:
    """Goertzel: the signal's power at `freq`, relative to its total energy."""
    coeff = 2.0 * math.cos(2.0 * math.pi * freq / gen_sounds.SAMPLE_RATE)
    s1 = s2 = 0.0
    for x in samples:
        s1, s2 = x + coeff * s1 - s2, s1
    power = s1 * s1 + s2 * s2 - coeff * s1 * s2
    return power / (len(samples) * sum(x * x for x in samples))


def energy_centre(samples) -> float:
    """The time (seconds) at which half of the sound's energy has been played."""
    total = sum(x * x for x in samples)
    running = 0.0
    for i, x in enumerate(samples):
        running += x * x
        if running >= total / 2:
            return i / gen_sounds.SAMPLE_RATE
    return len(samples) / gen_sounds.SAMPLE_RATE


class TestGenSounds(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.files = gen_sounds.generate()
        cls.decoded = {name: decode(data) for name, data in cls.files.items()}

    def test_deterministic_and_matches_audio_cfg(self) -> None:
        self.assertEqual(self.files, gen_sounds.generate())
        referenced = set()
        for line in (REPO_ROOT / "game" / "defs" / "audio.cfg").read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#") and line.split("=", 1)[0].strip() != "master_volume":
                referenced.add(Path(line.split("=", 1)[1].strip()).name)
        self.assertEqual(set(self.files), referenced)  # every slot has a generated sound, nothing extra
        self.assertEqual(len(set(self.files.values())), len(self.files))  # all different

    def test_format_and_levels(self) -> None:
        """16-bit PCM mono at the device rate; peaks normalised below clipping; no clicks or DC."""
        for name, (params, samples) in self.decoded.items():
            self.assertEqual(params, (1, 2, 44100, "NONE"), name)
            self.assertEqual(self.files[name][:4] + self.files[name][8:16], b"RIFFWAVEfmt ", name)
            peak = max(abs(x) for x in samples)
            peak_dbfs = 20 * math.log10(peak / 32768)
            self.assertLessEqual(peak_dbfs, -2.9, name)  # headroom: never clips, even after mixing in SDL
            self.assertGreaterEqual(peak_dbfs, -12.1, name)  # but clearly audible
            key = name[len("sfx_"):-len(".wav")]
            self.assertAlmostEqual(peak_dbfs, gen_sounds.PEAK_DBFS[key], delta=0.05, msg=name)
            self.assertEqual((samples[0], samples[-1]), (0, 0), name)  # faded in and out: no clicks
            self.assertLess(abs(sum(samples) / len(samples)), 50, name)  # no DC offset

    def test_durations_suit_the_queue(self) -> None:
        """AudioSystem queues effects back to back: the gunshot must end before the next shot
        can be fired, and no effect may hold up the queue for long."""
        gameplay = json.loads((REPO_ROOT / "game" / "defs" / "gameplay.json").read_text(encoding="utf-8"))
        fire_rate = gameplay["weapon"]["fireRate"]
        seconds = {name: len(samples) / 44100 for name, (_, samples) in self.decoded.items()}
        self.assertLess(seconds["sfx_weapon_fire.wav"], fire_rate)
        for name, value in seconds.items():
            self.assertGreaterEqual(value, 0.1, name)
            self.assertLessEqual(value, 0.30, name)

    def test_effects_are_distinct(self) -> None:
        """Each effect has its own character: the gunshot's energy is all at the start, the door's
        in its middle; the pickup chime is high (E6 and up), the alert low (D5, A4)."""
        samples = {name[len("sfx_"):-len(".wav")]: s for name, (_, s) in self.decoded.items()}
        self.assertLess(energy_centre(samples["weapon_fire"]), 0.03)
        self.assertGreater(energy_centre(samples["door_open"]), 0.08)
        chime, alert = samples["pickup"], samples["enemy_alert"]
        self.assertGreater(band_power(chime, 1318.5), 20 * band_power(chime, 587.33))
        self.assertGreater(band_power(alert, 587.33), 5 * band_power(alert, 1318.5))
        self.assertGreater(band_power(alert, 440.0), 5 * band_power(alert, 1318.5))

    def test_committed_sounds_are_up_to_date(self) -> None:
        self.assertEqual(gen_sounds.main(["--check"]), 0)


if __name__ == "__main__":
    unittest.main()
