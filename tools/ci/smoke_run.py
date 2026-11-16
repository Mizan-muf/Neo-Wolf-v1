#!/usr/bin/env python3
"""Headless smoke run of the Neo-Wolf executable (used by CI, works on macOS, Windows, Linux).

Starts the game with SDL's dummy video and audio drivers. The renderer is NOT forced: the game
must fall back to SDL's software renderer on its own (known issue B11, fixed in Phase 3), so
SDL_RENDER_DRIVER is removed from the environment. The game runs from a fresh temporary
directory by default, so it must find its content next to (or above) the executable rather
than in the working directory. It also passes --pref-dir with a fresh temporary folder (unless
the arguments already contain --pref-dir), so a smoke run never touches the player's real log
or progress.json (known issue B15).

Two modes:

  --frames N   (recommended) forwarded to the game, which renders N frames and exits by
               itself. Passes if it exits with code 0 within --timeout seconds.
               At the default 60 FPS frame cap, 300 frames take about 5 seconds.
  otherwise    the game must still be running after --seconds; it is then terminated.

Exit codes: 0 pass, 1 the game failed (exit code, early exit, timeout), 2 bad usage.

Usage:
  python3 tools/ci/smoke_run.py <path-to-neo_wolf> --frames 300
  python3 tools/ci/smoke_run.py <path-to-neo_wolf> [--seconds N] [extra game args]
Options: --timeout S (frames mode, default 60), --cwd DIR (default: a new temporary
directory). Unrecognised arguments are forwarded to the game (e.g. --level 1).
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def run(cmd: list[str], cwd: str, env: dict[str, str], frames_mode: bool, seconds: float, timeout: float) -> int:
    # Output goes to a temporary file, not a pipe: a full pipe buffer would block the game
    # and make a hung process look like a pass.
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8", errors="replace") as log:
        proc = subprocess.Popen(cmd, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT)

        failure = None
        if frames_mode:
            try:
                proc.wait(timeout=timeout)
                if proc.returncode != 0:
                    failure = f"game exited with code {proc.returncode}"
            except subprocess.TimeoutExpired:
                failure = f"game did not exit within {timeout:.0f}s"
        else:
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                if proc.poll() is not None:
                    failure = f"game exited early with code {proc.returncode}"
                    break
                time.sleep(0.1)

        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

        log.seek(0)
        lines = log.read().splitlines()

    print("\n".join(lines[:60]))
    if len(lines) > 60:
        print(f"[smoke] ... ({len(lines) - 60} more lines)")
    if failure:
        print(f"[smoke] FAIL: {failure}", file=sys.stderr)
        return 1
    if frames_mode:
        print("[smoke] PASS: game ran its frames and exited with code 0")
    else:
        print(f"[smoke] PASS: game stayed up for {seconds:.1f}s")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("exe", help="path to the neo_wolf executable")
    parser.add_argument("--seconds", type=float, default=5.0,
                        help="stay-up mode: how long the game must keep running (default 5)")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="--frames mode: how long to wait for the game to exit (default 60)")
    parser.add_argument("--cwd", help="working directory for the game (default: a new temporary directory)")
    args, game_args = parser.parse_known_args()

    exe = Path(args.exe).resolve()
    if not exe.is_file():
        print(f"[smoke] executable not found: {exe}", file=sys.stderr)
        return 2

    frames_mode = any(a == "--frames" or a.startswith("--frames=") for a in game_args)

    env = dict(os.environ)
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    env.setdefault("SDL_AUDIODRIVER", "dummy")
    env.pop("SDL_RENDER_DRIVER", None)

    with tempfile.TemporaryDirectory(prefix="neowolf-smoke-") as temp_cwd, \
            tempfile.TemporaryDirectory(prefix="neowolf-smoke-pref-") as temp_pref:
        if not any(a == "--pref-dir" or a.startswith("--pref-dir=") for a in game_args):
            game_args = [*game_args, "--pref-dir", temp_pref]
        cmd = [str(exe), *game_args]
        cwd = args.cwd or temp_cwd
        mode = "until it exits" if frames_mode else f"for {args.seconds:.1f}s"
        print(f"[smoke] running {cmd} in {cwd} {mode}")
        return run(cmd, cwd, env, frames_mode, args.seconds, args.timeout)


if __name__ == "__main__":
    sys.exit(main())
