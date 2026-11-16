#!/usr/bin/env python3
"""Headless smoke run of the Neo-Wolf executable (used by CI, works on macOS, Windows, Linux).

Starts the game with SDL's dummy video/audio drivers and the software renderer, from the
repository root (so content resolves), waits a few seconds, and checks that the process is
still running (= it initialised and entered its main loop without crashing). It is then
terminated.

Exit codes: 0 pass, 1 the game exited early or never started, 2 bad usage.

Usage: python3 tools/ci/smoke_run.py <path-to-neo_wolf> [--seconds N] [extra args forwarded to the game]
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("exe", help="path to the neo_wolf executable")
    parser.add_argument("--seconds", type=float, default=5.0, help="how long the game must stay up (default 5)")
    # Unrecognised arguments are forwarded to the game (e.g. --level 3 in later phases).
    args, game_args = parser.parse_known_args()
    args.game_args = game_args

    exe = Path(args.exe).resolve()
    if not exe.is_file():
        print(f"[smoke] executable not found: {exe}", file=sys.stderr)
        return 2

    env = dict(os.environ)
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    env.setdefault("SDL_AUDIODRIVER", "dummy")
    # The dummy video driver has no GPU renderer (known issue B11 until Phase 3).
    env.setdefault("SDL_RENDER_DRIVER", "software")

    cmd = [str(exe), *args.game_args]
    print(f"[smoke] running {cmd} in {REPO_ROOT} for {args.seconds:.1f}s")
    # Output goes to a temporary file, not a pipe: a full pipe buffer would block the game
    # and make a hung process look like a pass.
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8", errors="replace") as log:
        proc = subprocess.Popen(cmd, cwd=REPO_ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)

        exited_early = False
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                exited_early = True
                break
            time.sleep(0.1)

        if not exited_early:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

        log.seek(0)
        lines = log.read().splitlines()

    print("\n".join(lines[:40]))
    if len(lines) > 40:
        print(f"[smoke] ... ({len(lines) - 40} more lines)")
    if exited_early:
        print(f"[smoke] FAIL: game exited early with code {proc.returncode}", file=sys.stderr)
        return 1
    print(f"[smoke] PASS: game stayed up for {args.seconds:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
