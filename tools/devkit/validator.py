"""Comprehensive validator for Neo-Wolf maps, assets, definitions, and configurations."""

import json
import os
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class ValidationReport:
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    info: List[str] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        return len(self.errors) == 0

    def add_error(self, message: str) -> None:
        self.errors.append(message)

    def add_warning(self, message: str) -> None:
        self.warnings.append(message)

    def add_info(self, message: str) -> None:
        self.info.append(message)

    def merge(self, other: "ValidationReport") -> None:
        self.errors.extend(other.errors)
        self.warnings.extend(other.warnings)
        self.info.extend(other.info)


VALID_TILE_CHARS = {
    ".": "Empty",
    "#": "Wall",
    "A": "WallAltA",
    "B": "WallAltB",
    "D": "Door",
    "T": "Trigger",
}

SOLID_TILES = {"#", "A", "B"}
WALKABLE_TILES = {".", "D", "T"}
VALID_ENTITY_TYPES = {"player", "enemy", "pickup", "door", "trigger", "decoration"}


def validate_map(file_path: Path) -> Tuple[ValidationReport, Optional[List[str]]]:
    """Validate a .map grid file.
    
    Checks:
    - File exists and is readable
    - Comments ';' and empty lines ignored
    - Rectangular grid (uniform row widths)
    - Valid tile characters (., #, A, B, D, T)
    - Perimeter is closed with solid walls (#, A, B)
    - Minimum dimensions (>= 3x3)
    - Has at least one walkable tile
    """
    report = ValidationReport()
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"Map file not found: {file_path}")
        return report, None

    try:
        content = file_path.read_text(encoding="utf-8")
    except Exception as exc:
        report.add_error(f"Failed to read map file {file_path}: {exc}")
        return report, None

    raw_lines = content.splitlines()
    rows: List[str] = []
    for idx, line in enumerate(raw_lines):
        line = line.strip()
        if not line or line.startswith(";"):
            continue
        rows.append(line)

    if not rows:
        report.add_error(f"Map file {file_path} is empty or contains only comments.")
        return report, None

    height = len(rows)
    width = len(rows[0])

    if width < 3 or height < 3:
        report.add_error(f"Map {file_path} dimensions ({width}x{height}) must be at least 3x3.")

    # Check row consistency and valid characters
    for y, row in enumerate(rows):
        if len(row) != width:
            report.add_error(
                f"Map {file_path}: Inconsistent row width at row {y} (expected {width}, got {len(row)})"
            )
        for x, char in enumerate(row):
            if char not in VALID_TILE_CHARS:
                report.add_error(
                    f"Map {file_path}: Invalid tile character '{char}' at ({x}, {y})"
                )

    # Perimeter closure check (top, bottom, left, right must be solid walls)
    # Top and bottom rows
    for x in range(min(width, len(rows[0]))):
        if rows[0][x] not in SOLID_TILES:
            report.add_error(
                f"Map {file_path}: North perimeter leak at ({x}, 0) — found '{rows[0][x]}', expected solid wall (#, A, B)"
            )
        if len(rows) > 1 and len(rows[-1]) > x and rows[-1][x] not in SOLID_TILES:
            report.add_error(
                f"Map {file_path}: South perimeter leak at ({x}, {height - 1}) — found '{rows[-1][x]}', expected solid wall"
            )

    # Left and right edges
    for y in range(len(rows)):
        if rows[y] and rows[y][0] not in SOLID_TILES:
            report.add_error(
                f"Map {file_path}: West perimeter leak at (0, {y}) — found '{rows[y][0]}', expected solid wall"
            )
        if len(rows[y]) == width and rows[y][-1] not in SOLID_TILES:
            report.add_error(
                f"Map {file_path}: East perimeter leak at ({width - 1}, {y}) — found '{rows[y][-1]}', expected solid wall"
            )

    # Check walkable tiles count
    walkable_count = sum(row.count(".") + row.count("D") + row.count("T") for row in rows)
    if walkable_count == 0:
        report.add_error(f"Map {file_path} has no walkable interior tiles.")
    else:
        report.add_info(
            f"Map {file_path.name}: {width}x{height} grid ({walkable_count} walkable cells, {width*height - walkable_count} solid cells)"
        )

    return report, rows if report.is_valid else None


def validate_ppm(file_path: Path, expected_dim: Optional[int] = None) -> ValidationReport:
    """Validate a Netpbm PPM image file (P3 ASCII format as expected by engine).
    
    Checks:
    - Magic identifier is P3 (plain text PPM)
    - Valid positive width and height
    - Max color value > 0 and <= 255
    - Exact number of RGB pixel tokens (3 * width * height)
    - Pixel values within [0, maxval]
    """
    report = ValidationReport()
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"PPM file not found: {file_path}")
        return report

    try:
        with open(file_path, "r", encoding="latin-1") as f:
            text = f.read()
    except Exception as exc:
        report.add_error(f"Failed to read PPM file {file_path}: {exc}")
        return report

    # Tokenize ignoring comments starting with #
    tokens: List[str] = []
    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            tokens.extend(line.split())

    if len(tokens) < 4:
        report.add_error(f"PPM {file_path}: Incomplete header. Expected magic, width, height, maxval.")
        return report

    magic = tokens[0]
    if magic != "P3":
        if magic == "P6":
            report.add_error(
                f"PPM {file_path}: Found binary P6 PPM format, but Neo-Wolf engine requires ASCII P3 format."
            )
        else:
            report.add_error(
                f"PPM {file_path}: Unsupported format '{magic}'. Expected 'P3'."
            )
        return report

    try:
        width = int(tokens[1])
        height = int(tokens[2])
        maxval = int(tokens[3])
    except ValueError:
        report.add_error(f"PPM {file_path}: Non-integer values in header: {tokens[1:4]}")
        return report

    if width <= 0 or height <= 0:
        report.add_error(f"PPM {file_path}: Invalid dimensions {width}x{height}")
        return report

    if maxval <= 0 or maxval > 255:
        report.add_error(f"PPM {file_path}: Unsupported max color value {maxval} (must be 1..255)")

    if expected_dim is not None and (width != expected_dim or height != expected_dim):
        report.add_warning(
            f"PPM {file_path}: Dimensions {width}x{height} differ from standard {expected_dim}x{expected_dim}"
        )

    # Check power-of-two dimensions (recommended for raycaster wall textures)
    if (width & (width - 1)) != 0 or (height & (height - 1)) != 0:
        report.add_warning(
            f"PPM {file_path}: Dimensions {width}x{height} are not power-of-two (64x64 recommended)."
        )

    pixel_tokens = tokens[4:]
    expected_token_count = width * height * 3

    if len(pixel_tokens) != expected_token_count:
        report.add_error(
            f"PPM {file_path}: Token count mismatch. Expected {expected_token_count} RGB tokens ({width}x{height}x3), got {len(pixel_tokens)}"
        )
        return report

    # Sample pixel value range
    for i in range(0, min(len(pixel_tokens), 300), 3):
        try:
            r, g, b = int(pixel_tokens[i]), int(pixel_tokens[i + 1]), int(pixel_tokens[i + 2])
            if not (0 <= r <= maxval and 0 <= g <= maxval and 0 <= b <= maxval):
                report.add_error(
                    f"PPM {file_path}: Pixel value out of bounds [0, {maxval}]: ({r},{g},{b})"
                )
                break
        except ValueError:
            report.add_error(f"PPM {file_path}: Non-integer pixel value encountered.")
            break

    report.add_info(f"PPM {file_path.name}: {width}x{height} P3 ASCII format (maxval {maxval}) OK")
    return report


def validate_wav(file_path: Path) -> ValidationReport:
    """Validate a RIFF WAVE audio file.
    
    Checks:
    - RIFF header format
    - Format code is PCM (1)
    - Channels is 1 (mono) or 2 (stereo)
    - Valid sample rate and bit depth
    - Data chunk present and non-empty
    """
    report = ValidationReport()
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"WAV file not found: {file_path}")
        return report

    try:
        data = file_path.read_bytes()
    except Exception as exc:
        report.add_error(f"Failed to read WAV file {file_path}: {exc}")
        return report

    if len(data) < 44:
        report.add_error(f"WAV {file_path}: File size too small ({len(data)} bytes, min 44 bytes).")
        return report

    riff_header = data[:4]
    wave_header = data[8:12]

    if riff_header != b"RIFF" or wave_header != b"WAVE":
        report.add_error(f"WAV {file_path}: Missing RIFF/WAVE header marker.")
        return report

    # Locate 'fmt ' chunk and 'data' chunk
    offset = 12
    fmt_found = False
    data_found = False
    audio_format = 0
    num_channels = 0
    sample_rate = 0
    bits_per_sample = 0
    data_size = 0

    while offset + 8 <= len(data):
        chunk_id = data[offset : offset + 4]
        chunk_size = struct.unpack("<I", data[offset + 4 : offset + 8])[0]
        chunk_data_offset = offset + 8

        if chunk_id == b"fmt ":
            fmt_found = True
            if chunk_size >= 16:
                audio_format, num_channels, sample_rate, byte_rate, block_align, bits_per_sample = struct.unpack(
                    "<HHIIHH", data[chunk_data_offset : chunk_data_offset + 16]
                )
        elif chunk_id == b"data":
            data_found = True
            data_size = chunk_size

        offset += 8 + chunk_size
        # Account for word alignment padding
        if chunk_size % 2 != 0:
            offset += 1

    if not fmt_found:
        report.add_error(f"WAV {file_path}: Missing 'fmt ' chunk.")
        return report

    if not data_found or data_size <= 0:
        report.add_error(f"WAV {file_path}: Missing or empty 'data' chunk.")
        return report

    if audio_format != 1 and audio_format != 0xFFFE:  # PCM or WAVE_FORMAT_EXTENSIBLE
        report.add_warning(
            f"WAV {file_path}: Audio format code is {audio_format} (non-PCM compression). Standard PCM is 1."
        )

    if num_channels not in (1, 2):
        report.add_error(f"WAV {file_path}: Unsupported channel count {num_channels} (must be 1 or 2)")

    if bits_per_sample not in (8, 16, 24, 32):
        report.add_warning(
            f"WAV {file_path}: Unusual bit depth {bits_per_sample} bits (8 or 16 recommended)."
        )

    duration_sec = 0.0
    bytes_per_sample = (bits_per_sample // 8) * num_channels
    if bytes_per_sample > 0 and sample_rate > 0:
        duration_sec = data_size / (sample_rate * bytes_per_sample)

    report.add_info(
        f"WAV {file_path.name}: {num_channels}ch, {sample_rate}Hz, {bits_per_sample}-bit PCM, duration: {duration_sec:.2f}s"
    )
    return report


def validate_cfg(file_path: Path, required_keys: Optional[Set[str]] = None) -> Tuple[ValidationReport, Dict[str, str]]:
    """Validate a .cfg key=value configuration file."""
    report = ValidationReport()
    values: Dict[str, str] = {}
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"Config file not found: {file_path}")
        return report, values

    try:
        content = file_path.read_text(encoding="utf-8")
    except Exception as exc:
        report.add_error(f"Failed to read config file {file_path}: {exc}")
        return report, values

    for line_num, line in enumerate(content.splitlines(), start=1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        if "=" not in line:
            report.add_error(f"CFG {file_path}:{line_num}: Missing '=' delimiter in '{line}'")
            continue

        key, val = line.split("=", 1)
        key = key.strip()
        val = val.strip()

        if not key:
            report.add_error(f"CFG {file_path}:{line_num}: Empty key name.")
            continue

        values[key] = val

    if required_keys:
        missing = required_keys - set(values.keys())
        if missing:
            for k in sorted(missing):
                report.add_error(f"CFG {file_path}: Missing required key '{k}'")

    report.add_info(f"CFG {file_path.name}: Loaded {len(values)} key-value entries.")
    return report, values


def validate_entity_csv(
    file_path: Path, map_dimensions: Optional[Tuple[int, int]] = None
) -> Tuple[ValidationReport, List[dict]]:
    """Validate entity CSV file (type,x,y[,mapX,mapY])."""
    report = ValidationReport()
    entities: List[dict] = []
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"Entity CSV file not found: {file_path}")
        return report, entities

    try:
        content = file_path.read_text(encoding="utf-8")
    except Exception as exc:
        report.add_error(f"Failed to read entity CSV {file_path}: {exc}")
        return report, entities

    has_player = False
    for line_num, line in enumerate(content.splitlines(), start=1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 3:
            report.add_error(
                f"CSV {file_path}:{line_num}: Expected at least 3 fields (type,x,y), got {len(parts)}"
            )
            continue

        etype = parts[0]
        if etype not in VALID_ENTITY_TYPES:
            report.add_error(
                f"CSV {file_path}:{line_num}: Unknown entity type '{etype}'. Valid types: {VALID_ENTITY_TYPES}"
            )
            continue

        if etype == "player":
            has_player = True

        try:
            x = float(parts[1])
            y = float(parts[2])
        except ValueError:
            report.add_error(
                f"CSV {file_path}:{line_num}: Invalid float coordinates ({parts[1]}, {parts[2]})"
            )
            continue

        map_x, map_y = -1, -1
        if len(parts) >= 5:
            try:
                map_x = int(parts[3])
                map_y = int(parts[4])
            except ValueError:
                report.add_error(
                    f"CSV {file_path}:{line_num}: Invalid integer map cell coordinates ({parts[3]}, {parts[4]})"
                )
                continue

        if map_dimensions:
            mw, mh = map_dimensions
            if not (0 <= x < mw and 0 <= y < mh):
                report.add_error(
                    f"CSV {file_path}:{line_num}: Position ({x}, {y}) is outside map bounds ({mw}x{mh})"
                )
            if map_x != -1 and not (0 <= map_x < mw and 0 <= map_y < mh):
                report.add_error(
                    f"CSV {file_path}:{line_num}: Map cell ({map_x}, {map_y}) is outside map bounds ({mw}x{mh})"
                )

        entity_entry = {"type": etype, "x": x, "y": y}
        if map_x != -1:
            entity_entry["mapX"] = map_x
            entity_entry["mapY"] = map_y
        entities.append(entity_entry)

    if not has_player:
        report.add_warning(
            f"CSV {file_path}: No 'player' entity defined. The engine will fallback to default spawn."
        )

    report.add_info(f"CSV {file_path.name}: Found {len(entities)} entity placements.")
    return report, entities


def validate_entity_json(
    file_path: Path, map_dimensions: Optional[Tuple[int, int]] = None
) -> Tuple[ValidationReport, List[dict]]:
    """Validate entity JSON file (level, entities array)."""
    report = ValidationReport()
    entities: List[dict] = []
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"Entity JSON file not found: {file_path}")
        return report, entities

    try:
        data = json.loads(file_path.read_text(encoding="utf-8"))
    except Exception as exc:
        report.add_error(f"JSON syntax error in {file_path}: {exc}")
        return report, entities

    if not isinstance(data, dict):
        report.add_error(f"JSON {file_path}: Root must be an object.")
        return report, entities

    if "entities" not in data or not isinstance(data["entities"], list):
        report.add_error(f"JSON {file_path}: Missing 'entities' array.")
        return report, entities

    has_player = False
    for idx, item in enumerate(data["entities"]):
        if not isinstance(item, dict):
            report.add_error(f"JSON {file_path}: Entity #{idx} is not an object.")
            continue

        etype = item.get("type")
        if etype not in VALID_ENTITY_TYPES:
            report.add_error(
                f"JSON {file_path}: Entity #{idx} has invalid type '{etype}'."
            )
            continue

        if etype == "player":
            has_player = True

        x = item.get("x")
        y = item.get("y")
        if not isinstance(x, (int, float)) or not isinstance(y, (int, float)):
            report.add_error(f"JSON {file_path}: Entity #{idx} has invalid x/y numbers.")
            continue

        map_x = item.get("mapX")
        map_y = item.get("mapY")
        if map_x is not None and not isinstance(map_x, int):
            report.add_error(f"JSON {file_path}: Entity #{idx} mapX must be integer.")
        if map_y is not None and not isinstance(map_y, int):
            report.add_error(f"JSON {file_path}: Entity #{idx} mapY must be integer.")

        if map_dimensions:
            mw, mh = map_dimensions
            if not (0 <= x < mw and 0 <= y < mh):
                report.add_error(
                    f"JSON {file_path}: Entity #{idx} position ({x}, {y}) is outside map bounds ({mw}x{mh})"
                )
            if isinstance(map_x, int) and isinstance(map_y, int):
                if not (0 <= map_x < mw and 0 <= map_y < mh):
                    report.add_error(
                        f"JSON {file_path}: Entity #{idx} cell ({map_x}, {map_y}) is outside map bounds ({mw}x{mh})"
                    )

        entities.append(item)

    if not has_player:
        report.add_warning(
            f"JSON {file_path}: No 'player' entity defined in 'entities' array."
        )

    report.add_info(f"JSON {file_path.name}: Found {len(entities)} entity placements.")
    return report, entities


def validate_project(project_root: Path) -> ValidationReport:
    """Run a complete validation pass on all maps, assets, defs, and config files in project."""
    project_root = Path(project_root).resolve()
    report = ValidationReport()

    report.add_info(f"Starting complete project validation for {project_root}")

    maps_dir = project_root / "game" / "maps"
    assets_dir = project_root / "game" / "assets"
    defs_dir = project_root / "game" / "defs"

    # 1. Validate Map files
    map_dimensions: Dict[str, Tuple[int, int]] = {}
    if not maps_dir.exists():
        report.add_error(f"Missing maps directory: {maps_dir}")
    else:
        map_files = list(maps_dir.glob("*.map"))
        if not map_files:
            report.add_error(f"No .map files found in {maps_dir}")
        for map_file in map_files:
            map_rep, rows = validate_map(map_file)
            report.merge(map_rep)
            if rows:
                map_dimensions[map_file.stem] = (len(rows[0]), len(rows))

    # 2. Validate PPM textures & sprites
    if not assets_dir.exists():
        report.add_error(f"Missing assets directory: {assets_dir}")
    else:
        ppm_files = list(assets_dir.glob("*.ppm"))
        if not ppm_files:
            report.add_error(f"No .ppm files found in {assets_dir}")
        for ppm_file in ppm_files:
            ppm_rep = validate_ppm(ppm_file, expected_dim=None)
            report.merge(ppm_rep)

        # Validate WAV audio files
        wav_files = list(assets_dir.glob("*.wav"))
        if not wav_files:
            report.add_error(f"No .wav files found in {assets_dir}")
        for wav_file in wav_files:
            wav_rep = validate_wav(wav_file)
            report.merge(wav_rep)

    # 3. Validate Defs & Configs
    if not defs_dir.exists():
        report.add_error(f"Missing defs directory: {defs_dir}")
    else:
        # Wall textures cfg
        wall_cfg = defs_dir / "wall_textures.cfg"
        if wall_cfg.exists():
            cfg_rep, wall_vals = validate_cfg(
                wall_cfg, {"wall", "wall_alt_a", "wall_alt_b", "door", "floor", "ceiling"}
            )
            report.merge(cfg_rep)
            for key, rel_path in wall_vals.items():
                target = project_root / rel_path
                if not target.exists():
                    report.add_error(
                        f"wall_textures.cfg references non-existent file for '{key}': {rel_path}"
                    )
        else:
            report.add_error(f"Missing wall_textures.cfg in {defs_dir}")

        # Sprites cfg
        sprites_cfg = defs_dir / "sprites.cfg"
        if sprites_cfg.exists():
            cfg_rep, spr_vals = validate_cfg(
                sprites_cfg, {"enemy", "pickup", "decoration"}
            )
            report.merge(cfg_rep)
            for key, rel_path in spr_vals.items():
                target = project_root / rel_path
                if not target.exists():
                    report.add_error(
                        f"sprites.cfg references non-existent file for '{key}': {rel_path}"
                    )
        else:
            report.add_error(f"Missing sprites.cfg in {defs_dir}")

        # Audio cfg
        audio_cfg = defs_dir / "audio.cfg"
        if audio_cfg.exists():
            cfg_rep, audio_vals = validate_cfg(
                audio_cfg, {"door_open", "pickup", "enemy_alert"}
            )
            report.merge(cfg_rep)
            for key, rel_path in audio_vals.items():
                if key == "master_volume":
                    try:
                        vol = float(rel_path)
                        if not (0.0 <= vol <= 1.0):
                            report.add_warning(
                                f"audio.cfg master_volume ({vol}) is outside typical range [0.0, 1.0]"
                            )
                    except ValueError:
                        report.add_error(f"audio.cfg invalid master_volume: {rel_path}")
                    continue
                target = project_root / rel_path
                if not target.exists():
                    report.add_error(
                        f"audio.cfg references non-existent file for '{key}': {rel_path}"
                    )
        else:
            report.add_error(f"Missing audio.cfg in {defs_dir}")

        # Entities JSON & CSVs
        for csv_file in defs_dir.glob("*.entities.csv"):
            level_name = csv_file.name.replace(".entities.csv", "")
            dims = map_dimensions.get(level_name)
            csv_rep, _ = validate_entity_csv(csv_file, dims)
            report.merge(csv_rep)

        for json_file in defs_dir.glob("*.entities.json"):
            level_name = json_file.name.replace(".entities.json", "")
            dims = map_dimensions.get(level_name)
            json_rep, _ = validate_entity_json(json_file, dims)
            report.merge(json_rep)

        # Main entities.json registry
        main_ent_json = defs_dir / "entities.json"
        if main_ent_json.exists():
            try:
                data = json.loads(main_ent_json.read_text(encoding="utf-8"))
                levels = data.get("levels", [])
                for lvl in levels:
                    lvl_map = project_root / lvl.get("map", "")
                    lvl_ent = project_root / lvl.get("entities", "")
                    if not lvl_map.exists():
                        report.add_error(
                            f"entities.json level '{lvl.get('id')}' references missing map: {lvl.get('map')}"
                        )
                    if not lvl_ent.exists():
                        report.add_error(
                            f"entities.json level '{lvl.get('id')}' references missing entities: {lvl.get('entities')}"
                        )
            except Exception as exc:
                report.add_error(f"Failed to parse entities.json: {exc}")

    return report
