"""Terminal colours for the DevKit.

Colours are used only when they will show as colours: never with ``--no-color`` or the
``NO_COLOR`` environment variable (https://no-color.org), never when the output is not a terminal
(a pipe, a file, CTest), and on Windows only when virtual-terminal (ANSI) processing is on or
can be switched on for the console (Windows 10 1511 and later). Older Windows consoles would
print the escape codes literally, so colours stay off there.
"""

import os
import sys
from typing import Optional, TextIO

ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
STD_OUTPUT_HANDLE = -11


def _enable_windows_vt() -> bool:
    """Switches on ANSI processing for this console; False if that is not possible."""
    try:
        import ctypes
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)  # type: ignore[attr-defined]
        handle = kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
        mode = wintypes.DWORD()
        if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            return False
        if mode.value & ENABLE_VIRTUAL_TERMINAL_PROCESSING:
            return True
        return bool(kernel32.SetConsoleMode(handle, mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
    except (AttributeError, OSError, ValueError):
        return False


def supports_color(
    requested: bool = True,
    stream: Optional[TextIO] = None,
    platform: Optional[str] = None,
    environ: Optional[dict] = None,
) -> bool:
    """Whether to print ANSI colours (the arguments other than `requested` are for tests)."""
    if not requested:
        return False
    env = os.environ if environ is None else environ
    if env.get("NO_COLOR"):
        return False
    out = sys.stdout if stream is None else stream
    try:
        if not out.isatty():
            return False
    except (AttributeError, ValueError):
        return False
    if (platform or sys.platform) == "win32":
        return _enable_windows_vt()
    return env.get("TERM") != "dumb"


class Palette:
    """ANSI codes, or empty strings when colours are off."""

    def __init__(self, enabled: bool) -> None:
        code = (lambda value: value) if enabled else (lambda value: "")
        self.reset = code("\033[0m")
        self.bold = code("\033[1m")
        self.green = code("\033[32;1m")
        self.yellow = code("\033[33;1m")
        self.red = code("\033[31;1m")
        self.cyan = code("\033[36m")
