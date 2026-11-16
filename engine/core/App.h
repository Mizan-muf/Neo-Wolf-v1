#pragma once

#include "engine/core/CommandLine.h"

class App {
public:
    // Runs the game with already-parsed options. Returns the process exit code:
    // 0 normal exit (including after --frames N), 1 startup failure.
    int Run(const CommandLineOptions& options);
};
