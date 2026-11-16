#pragma once

// Command line and report output of neo_wolf_autoplay (src/autoplay_main.cpp). Pure functions
// without I/O, so they are unit-tested (native test autoplay_command_line).

#include <string>
#include <string_view>
#include <vector>

#include "engine/autoplay/AutoplayBot.h"

struct AutoplayCommandLine {
    enum class Action { Run, ShowHelp, ShowVersion, Error };
    Action action = Action::Run;
    std::string error;          // Action::Error: why (the caller prints it with the usage, exit 2)
    int level = 0;              // --level N (1-based)
    bool all = false;           // --all
    std::string dataDir;        // --data DIR
    std::string prefDir;        // --pref-dir DIR (empty: a temporary folder, removed afterwards)
    double timeLimitSeconds = AutoplayConfig{}.timeLimitSeconds;  // --time-limit SECONDS
    std::string jsonPath;       // --json FILE
    std::string tracePath;      // --trace FILE: the per-event log (RunAutoplaySession)
    bool verbose = false;       // --verbose: print the engine's log to the console
};

// `args` excludes the program name. Exactly one of --level N and --all is required.
AutoplayCommandLine ParseAutoplayCommandLine(const std::vector<std::string>& args);
std::string AutoplayUsage(std::string_view programName);

// The human-readable table printed on stdout.
std::string FormatAutoplayTable(const AutoplayRunReport& report);
// The machine-readable report (--json). Deterministic: no timestamps, no paths, fixed rounding,
// so two runs of the same build on the same content give identical files.
std::string FormatAutoplayJson(const AutoplayRunReport& report);
