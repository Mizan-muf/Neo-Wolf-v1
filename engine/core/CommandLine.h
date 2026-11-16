#pragma once

// Command-line parsing for the game executable. Pure (no I/O, no SDL), so it is unit-tested.

#include <string>
#include <string_view>
#include <vector>

#include "engine/core/PreviewScreen.h"

enum class WindowModeOption {
    Default,    // windowed (later phases may remember the last choice)
    Windowed,   // --windowed
    Fullscreen  // --fullscreen (desktop resolution)
};

struct CommandLineOptions {
    std::string dataDir;         // --data <dir> (UTF-8), empty = automatic search
    std::string prefDir;         // --pref-dir <dir>: log and progress.json; empty = NEOWOLF_PREF_DIR or the default
    int level = 0;               // --level <N>: campaign level 1..99 to start in; 0 = main menu
    int frames = 0;              // --frames <N>; 0 = run until the player quits
    std::string screenshotPath;  // --screenshot <file.bmp>: save the last frame on exit (developer tool)
    PreviewScreen preview = PreviewScreen::None;  // --preview <screen> (developer tool)
    WindowModeOption windowMode = WindowModeOption::Default;
};

enum class CommandLineAction {
    Run,
    ShowHelp,
    ShowVersion,
    Error  // `error` says why; the caller prints it with the usage text and exits 2
};

struct CommandLineResult {
    CommandLineAction action = CommandLineAction::Run;
    CommandLineOptions options{};
    std::string error{};
};

// `args` excludes the program name (argv[1..argc-1]), as UTF-8.
CommandLineResult ParseCommandLine(const std::vector<std::string>& args);

std::string CommandLineUsage(std::string_view programName);

// "Neo-Wolf <version>", the version coming from CMake's PROJECT_VERSION.
std::string VersionString();

constexpr int kMinLevel = 1;
constexpr int kMaxLevel = 99;
