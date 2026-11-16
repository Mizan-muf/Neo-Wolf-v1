#!/usr/bin/env python3
"""Direct entry point for Neo-Wolf DevKit CLI."""

import sys
from pathlib import Path

# Add project root to sys.path
root_dir = Path(__file__).resolve().parents[2]
if str(root_dir) not in sys.path:
    sys.path.insert(0, str(root_dir))

from tools.devkit.cli import main  # noqa: E402  needs the sys.path entry above (run as a script)

if __name__ == "__main__":
    sys.exit(main())
