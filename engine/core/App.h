#pragma once

// The game's main loop: per-user folder and log file, engine start-up, frame timing and
// pacing, the --screenshot on exit (docs/ARCHITECTURE.md "Frame loop").

#include "engine/core/CommandLine.h"

class App {
public:
    // Runs the game with already-parsed options. Returns the process exit code:
    // 0 normal exit (including after --frames N), 1 startup failure.
    static int Run(const CommandLineOptions& options);
};
