"""Asset inspection, procedural generation, and format conversion tools for Neo-Wolf."""

import math
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

WAV_HEADER_BYTES = 44  # RIFF, WAVE, a 16-byte 'fmt ' chunk and the 'data' chunk header
WAV_FORMAT_PCM = 1
PCM_FMT_CHUNK_BYTES = 16  # the PCM part of a 'fmt ' chunk (format, channels, rate, byte rate, align, bits)
INT16_MAX = 32767


def ppm_tokens(text: str) -> List[str]:
    """The whitespace-separated tokens of a plain-text PPM, with '#' comments removed."""
    tokens: List[str] = []
    for line in text.splitlines():
        content = line.split("#", 1)[0].strip()
        if content:
            tokens.extend(content.split())
    return tokens


@dataclass
class WavFormat:
    """What the RIFF chunks of a WAV file say (zeros for a chunk that is missing)."""

    fmt_found: bool = False
    data_found: bool = False
    audio_format: int = 0
    num_channels: int = 0
    sample_rate: int = 0
    bits_per_sample: int = 0
    data_size: int = 0


def read_wav_format(data: bytes) -> WavFormat:
    """Walks the chunks after the 12-byte RIFF/WAVE header: 'fmt ' and the 'data' size."""
    info = WavFormat()
    offset = 12
    while offset + 8 <= len(data):
        chunk_id = data[offset : offset + 4]
        chunk_size = struct.unpack("<I", data[offset + 4 : offset + 8])[0]
        chunk_data_offset = offset + 8

        if chunk_id == b"fmt ":
            info.fmt_found = True
            if chunk_size >= PCM_FMT_CHUNK_BYTES:
                info.audio_format, info.num_channels, info.sample_rate, _, _, info.bits_per_sample = struct.unpack(
                    "<HHIIHH", data[chunk_data_offset : chunk_data_offset + PCM_FMT_CHUNK_BYTES]
                )
        elif chunk_id == b"data":
            info.data_found = True
            info.data_size = chunk_size

        offset += 8 + chunk_size
        if chunk_size % 2 != 0:  # chunks are padded to an even size
            offset += 1
    return info


def inspect_ppm(file_path: Path) -> Dict[str, Any]:
    """Extract metadata and statistical summary from a PPM image."""
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"PPM file not found: {file_path}")

    with open(file_path, encoding="latin-1") as f:
        tokens = ppm_tokens(f.read())

    if len(tokens) < 4:
        raise ValueError(f"Invalid PPM header in {file_path}")

    magic = tokens[0]
    width = int(tokens[1])
    height = int(tokens[2])
    maxval = int(tokens[3])

    pixels: List[Tuple[int, int, int]] = []
    token_idx = 4
    num_pixels = width * height

    r_sum, g_sum, b_sum = 0, 0, 0
    magenta_count = 0  # Color key (255, 0, 255) often used for transparency

    for _ in range(num_pixels):
        if token_idx + 2 >= len(tokens):
            break
        r = int(tokens[token_idx])
        g = int(tokens[token_idx + 1])
        b = int(tokens[token_idx + 2])
        token_idx += 3
        pixels.append((r, g, b))
        r_sum += r
        g_sum += g
        b_sum += b
        if r >= 240 and g <= 15 and b >= 240:
            magenta_count += 1

    actual_pixels = len(pixels)
    avg_r = (r_sum / actual_pixels) if actual_pixels > 0 else 0
    avg_g = (g_sum / actual_pixels) if actual_pixels > 0 else 0
    avg_b = (b_sum / actual_pixels) if actual_pixels > 0 else 0

    return {
        "file_name": file_path.name,
        "format": magic,
        "width": width,
        "height": height,
        "maxval": maxval,
        "total_pixels": width * height,
        "parsed_pixels": actual_pixels,
        "is_power_of_two": (width & (width - 1) == 0) and (height & (height - 1) == 0),
        "average_color_rgb": (round(avg_r, 1), round(avg_g, 1), round(avg_b, 1)),
        "color_key_magenta_pixels": magenta_count,
        "color_key_percent": round((magenta_count / actual_pixels) * 100, 1) if actual_pixels > 0 else 0,
        "file_size_bytes": file_path.stat().st_size,
    }


def inspect_wav(file_path: Path) -> Dict[str, Any]:
    """Extract audio metadata from a RIFF WAVE file."""
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"WAV file not found: {file_path}")

    data = file_path.read_bytes()
    if len(data) < WAV_HEADER_BYTES or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"Not a valid RIFF WAVE file: {file_path}")

    wav = read_wav_format(data)
    audio_format, num_channels, sample_rate = wav.audio_format, wav.num_channels, wav.sample_rate
    bits_per_sample, data_size = wav.bits_per_sample, wav.data_size

    bytes_per_sample = (bits_per_sample // 8) * num_channels if bits_per_sample > 0 else 2
    duration_sec = (data_size / (sample_rate * bytes_per_sample)) if (sample_rate * bytes_per_sample) > 0 else 0.0

    return {
        "file_name": file_path.name,
        "format_code": audio_format,
        "format_name": "PCM" if audio_format == WAV_FORMAT_PCM else f"Code_{audio_format}",
        "channels": num_channels,
        "channel_desc": "Mono" if num_channels == 1 else ("Stereo" if num_channels == 2 else f"{num_channels}ch"),
        "sample_rate": sample_rate,
        "bits_per_sample": bits_per_sample,
        "data_size_bytes": data_size,
        "duration_seconds": round(duration_sec, 3),
        "file_size_bytes": file_path.stat().st_size,
    }


def write_p3_ppm(
    file_path: Path,
    width: int,
    height: int,
    pixels: List[Tuple[int, int, int]],
    comment: Optional[str] = None,
) -> None:
    """Write an ASCII P3 PPM image file."""
    file_path = Path(file_path)
    lines: List[str] = ["P3"]
    if comment:
        lines.append(f"# {comment}")
    lines.append(f"{width} {height}")
    lines.append("255")

    current_row: List[str] = []
    for idx, (r, g, b) in enumerate(pixels):
        current_row.append(f"{r} {g} {b}")
        if (idx + 1) % width == 0:
            lines.append(" ".join(current_row))
            current_row = []

    if current_row:
        lines.append(" ".join(current_row))

    file_path.write_text("\n".join(lines) + "\n", encoding="latin-1")


def create_checkerboard_ppm(
    file_path: Path,
    width: int = 64,
    height: int = 64,
    color1: Tuple[int, int, int] = (255, 0, 255),
    color2: Tuple[int, int, int] = (0, 0, 0),
    tile_size: int = 8,
) -> None:
    """Create a procedural checkerboard texture."""
    pixels = []
    for y in range(height):
        for x in range(width):
            cx = x // tile_size
            cy = y // tile_size
            c = color1 if (cx + cy) % 2 == 0 else color2
            pixels.append(c)
    write_p3_ppm(file_path, width, height, pixels, comment="DevKit Generated Checkerboard")


def create_solid_ppm(
    file_path: Path,
    width: int = 64,
    height: int = 64,
    color: Tuple[int, int, int] = (128, 128, 128),
) -> None:
    """Create a procedural solid color texture."""
    pixels = [color for _ in range(width * height)]
    write_p3_ppm(file_path, width, height, pixels, comment="DevKit Generated Solid Color")


def create_brick_ppm(
    file_path: Path,
    width: int = 64,
    height: int = 64,
    brick_color: Tuple[int, int, int] = (180, 50, 40),
    mortar_color: Tuple[int, int, int] = (80, 80, 80),
) -> None:
    """Create a procedural brick wall texture."""
    pixels = []
    brick_h = 8
    brick_w = 16
    for y in range(height):
        row_idx = y // brick_h
        is_mortar_h = (y % brick_h == 0) or (y % brick_h == brick_h - 1)
        shift = (brick_w // 2) if (row_idx % 2 == 1) else 0
        for x in range(width):
            shifted_x = (x + shift) % width
            is_mortar_v = shifted_x % brick_w == 0
            if is_mortar_h or is_mortar_v:
                pixels.append(mortar_color)
            else:
                # Slight noise variation
                shade = (x * 7 + y * 13) % 15 - 7
                r = max(0, min(255, brick_color[0] + shade))
                g = max(0, min(255, brick_color[1] + shade // 2))
                b = max(0, min(255, brick_color[2] + shade // 2))
                pixels.append((r, g, b))
    write_p3_ppm(file_path, width, height, pixels, comment="DevKit Generated Brick Texture")


def generate_beep_wav(
    file_path: Path,
    duration_sec: float = 0.2,
    frequency: float = 440.0,
    sample_rate: int = 22050,
    volume: float = 0.5,
) -> None:
    """Generate a simple mono 16-bit PCM wave sound effect."""
    file_path = Path(file_path)
    total_samples = int(sample_rate * duration_sec)
    max_amplitude = int(INT16_MAX * max(0.0, min(1.0, volume)))

    raw_samples = bytearray()
    for i in range(total_samples):
        t = float(i) / sample_rate
        # Envelope: full level, then a linear fade-out over the last 40% of the tone
        env = min(1.0, (total_samples - i) / (total_samples * 0.4))
        sample_val = int(max_amplitude * env * math.sin(2.0 * math.pi * frequency * t))
        sample_val = max(-INT16_MAX - 1, min(INT16_MAX, sample_val))
        raw_samples.extend(struct.pack("<h", sample_val))

    num_channels = 1
    bits_per_sample = 16
    byte_rate = sample_rate * num_channels * (bits_per_sample // 8)
    block_align = num_channels * (bits_per_sample // 8)
    data_size = len(raw_samples)
    riff_chunk_size = WAV_HEADER_BYTES - 8 + data_size  # everything after 'RIFF' and this size field

    header = bytearray()
    header.extend(b"RIFF")
    header.extend(struct.pack("<I", riff_chunk_size))
    header.extend(b"WAVE")
    # fmt chunk
    header.extend(b"fmt ")
    header.extend(struct.pack("<I", PCM_FMT_CHUNK_BYTES))  # chunk size
    header.extend(struct.pack("<H", WAV_FORMAT_PCM))
    header.extend(struct.pack("<H", num_channels))
    header.extend(struct.pack("<I", sample_rate))
    header.extend(struct.pack("<I", byte_rate))
    header.extend(struct.pack("<H", block_align))
    header.extend(struct.pack("<H", bits_per_sample))
    # data chunk
    header.extend(b"data")
    header.extend(struct.pack("<I", data_size))

    file_path.write_bytes(header + raw_samples)
