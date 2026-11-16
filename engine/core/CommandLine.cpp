#include "engine/core/CommandLine.h"

#include <cstdio>
#include <utility>

#ifndef NEOWOLF_VERSION
#define NEOWOLF_VERSION "unknown"
#endif

namespace {
// Strict decimal parse: digits only, no sign, no trailing text, no overflow.
bool ParsePositiveInt(const std::string& text, int& out) {
    if (text.empty() || text.size() > 9) {
        return false;
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

std::string PaddedLevelName(int level) {
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "level%02d", level);
    return buffer;
}

CommandLineResult Fail(std::string message) {
    CommandLineResult result{};
    result.action = CommandLineAction::Error;
    result.error = std::move(message);
    return result;
}
} // namespace

CommandLineResult ParseCommandLine(const std::vector<std::string>& args) {
    CommandLineResult result{};
    bool sawWindowed = false;
    bool sawFullscreen = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string arg = args[i];
        std::string inlineValue;
        bool hasInlineValue = false;
        if (arg.rfind("--", 0) == 0) {
            const std::size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                inlineValue = arg.substr(eq + 1);
                arg = arg.substr(0, eq);
                hasInlineValue = true;
            }
        }

        // Fetches the value of an option that takes one (--opt value or --opt=value).
        const auto takeValue = [&](std::string& value) -> bool {
            if (hasInlineValue) {
                value = inlineValue;
                return true;
            }
            if (i + 1 >= args.size()) {
                return false;
            }
            value = args[++i];
            return true;
        };
        const auto rejectInlineValue = [&]() { return hasInlineValue; };

        if (arg == "--help" || arg == "-h" || arg == "-?" || arg == "/?") {
            if (rejectInlineValue()) {
                return Fail(arg + " does not take a value");
            }
            result.action = CommandLineAction::ShowHelp;
            return result;
        }
        if (arg == "--version") {
            if (rejectInlineValue()) {
                return Fail(arg + " does not take a value");
            }
            result.action = CommandLineAction::ShowVersion;
            return result;
        }
        if (arg == "--data") {
            std::string value;
            if (!takeValue(value) || value.empty()) {
                return Fail("--data needs a directory");
            }
            result.options.dataDir = value;
            continue;
        }
        if (arg == "--level") {
            std::string value;
            int level = 0;
            if (!takeValue(value) || !ParsePositiveInt(value, level) || level < kMinLevel || level > kMaxLevel) {
                return Fail("--level needs a number from " + std::to_string(kMinLevel) + " to " +
                            std::to_string(kMaxLevel));
            }
            result.options.level = level;
            continue;
        }
        if (arg == "--frames") {
            std::string value;
            int frames = 0;
            if (!takeValue(value) || !ParsePositiveInt(value, frames) || frames < 1) {
                return Fail("--frames needs a positive number of frames");
            }
            result.options.frames = frames;
            continue;
        }
        if (arg == "--windowed" || arg == "--fullscreen") {
            if (rejectInlineValue()) {
                return Fail(arg + " does not take a value");
            }
            if (arg == "--windowed") {
                sawWindowed = true;
                result.options.windowMode = WindowModeOption::Windowed;
            } else {
                sawFullscreen = true;
                result.options.windowMode = WindowModeOption::Fullscreen;
            }
            if (sawWindowed && sawFullscreen) {
                return Fail("--windowed and --fullscreen cannot be combined");
            }
            continue;
        }
        // macOS before 10.9 passed -psn_<serial> to apps launched from Finder. Ignore it.
        if (arg.rfind("-psn_", 0) == 0) {
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            return Fail("unknown option: " + args[i]);
        }
        return Fail("unexpected argument: " + args[i]);
    }
    return result;
}

std::string CommandLineUsage(std::string_view programName) {
    std::string usage = "Usage: ";
    usage += programName.empty() ? std::string_view("neo_wolf") : programName;
    usage +=
        " [options]\n"
        "\n"
        "Options:\n"
        "  --help            show this help and exit\n"
        "  --version         print the version and exit\n"
        "  --data <dir>      use the content in <dir> (the folder that contains game/);\n"
        "                    overrides the NEOWOLF_DATA_DIR environment variable\n"
        "  --level <N>       start on level N (1-99): game/maps/levelNN.map and\n"
        "                    game/defs/levelNN.entities.json\n"
        "  --frames <N>      run N frames, then exit with code 0 (automated smoke tests)\n"
        "  --windowed        start in a window (default)\n"
        "  --fullscreen      start in desktop fullscreen\n"
        "\n"
        "Without --data or NEOWOLF_DATA_DIR the game looks for game/ next to the executable\n"
        "(and up to 3 folders above it), in a macOS app's Resources folder, then in the\n"
        "current folder and its parent.\n"
        "\n"
        "Exit codes: 0 normal exit, 1 startup failed (for example content not found),\n"
        "2 bad command line.\n";
    return usage;
}

std::string VersionString() {
    return std::string("Neo-Wolf ") + NEOWOLF_VERSION;
}

std::string LevelMapPath(int level) {
    return "game/maps/" + PaddedLevelName(level) + ".map";
}

std::string LevelEntitiesPath(int level) {
    return "game/defs/" + PaddedLevelName(level) + ".entities.json";
}
