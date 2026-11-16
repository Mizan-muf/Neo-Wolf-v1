#include "engine/core/CommandLine.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef NEOWOLF_VERSION
#define NEOWOLF_VERSION "unknown"
#endif

namespace {
// More digits than this could overflow an int.
constexpr std::size_t kMaxNumberDigits = 9;

// Strict decimal parse: digits only, no sign, no trailing text, no overflow.
bool ParsePositiveInt(const std::string& text, int& out) {
    if (text.empty() || text.size() > kMaxNumberDigits) {
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

std::string PreviewScreenList() {
    std::string list;
    for (const PreviewScreen screen : kPreviewScreens) {
        list += (list.empty() ? "" : ", ") + std::string(PreviewScreenName(screen));
    }
    return list;
}

CommandLineResult Fail(std::string message) {
    CommandLineResult result{};
    result.action = CommandLineAction::Error;
    result.error = std::move(message);
    return result;
}

// One argument; "--opt=value" is split into the option and its inline value.
struct Argument {
    std::string name;
    std::string inlineValue;
    bool hasInlineValue = false;
};

Argument SplitArgument(const std::string& text) {
    Argument argument{text, {}, false};
    if (text.rfind("--", 0) == 0) {
        const std::size_t eq = text.find('=');
        if (eq != std::string::npos) {
            argument.name = text.substr(0, eq);
            argument.inlineValue = text.substr(eq + 1);
            argument.hasInlineValue = true;
        }
    }
    return argument;
}

// The value of an option that takes one: --opt=value, or the next argument (then `index`
// moves past it). False if there is none.
bool TakeValue(const Argument& argument, const std::vector<std::string>& args, std::size_t& index, std::string& value) {
    if (argument.hasInlineValue) {
        value = argument.inlineValue;
        return true;
    }
    if (index + 1 >= args.size()) {
        return false;
    }
    value = args[++index];
    return true;
}

bool SetNonEmpty(const std::string& value, std::string& target) {
    if (value.empty()) {
        return false;
    }
    target = value;
    return true;
}

// An option that takes a value. `apply` stores a valid value in the options and returns
// false for an invalid one; `needs` completes the error message "<name> needs ...".
struct ValueOption {
    std::string_view name;
    bool (*apply)(const std::string& value, CommandLineOptions& options);
    std::string (*needs)();
};

constexpr std::array<ValueOption, 6> kValueOptions = {{
    {"--data",
     [](const std::string& value, CommandLineOptions& options) { return SetNonEmpty(value, options.dataDir); },
     [] { return std::string("a directory"); }},
    {"--level",
     [](const std::string& value, CommandLineOptions& options) {
         int level = 0;
         if (!ParsePositiveInt(value, level) || level < kMinLevel || level > kMaxLevel) {
             return false;
         }
         options.level = level;
         return true;
     },
     [] { return "a number from " + std::to_string(kMinLevel) + " to " + std::to_string(kMaxLevel); }},
    {"--pref-dir",
     [](const std::string& value, CommandLineOptions& options) { return SetNonEmpty(value, options.prefDir); },
     [] { return std::string("a directory"); }},
    {"--screenshot",
     [](const std::string& value, CommandLineOptions& options) { return SetNonEmpty(value, options.screenshotPath); },
     [] { return std::string("a file name (.bmp)"); }},
    {"--preview",
     [](const std::string& value, CommandLineOptions& options) { return ParsePreviewScreen(value, options.preview); },
     [] { return "one of: " + PreviewScreenList(); }},
    {"--frames",
     [](const std::string& value, CommandLineOptions& options) {
         int frames = 0;
         if (!ParsePositiveInt(value, frames) || frames < 1) {
             return false;
         }
         options.frames = frames;
         return true;
     },
     [] { return std::string("a positive number of frames"); }},
}};

const ValueOption* FindValueOption(std::string_view name) {
    for (const ValueOption& option : kValueOptions) {
        if (option.name == name) {
            return &option;
        }
    }
    return nullptr;
}

bool IsHelpOption(const std::string& name) {
    return name == "--help" || name == "-h" || name == "-?" || name == "/?";
}

bool IsFlag(const std::string& name) {
    return IsHelpOption(name) || name == "--version" || name == "--windowed" || name == "--fullscreen";
}

// The options without a value. Returns false when parsing ends here (help, version or an
// error; `result` says which).
bool ParseFlag(const Argument& argument, CommandLineResult& result) {
    const std::string& name = argument.name;
    if (argument.hasInlineValue) {
        result = Fail(name + " does not take a value");
        return false;
    }
    if (IsHelpOption(name)) {
        result.action = CommandLineAction::ShowHelp;
        return false;
    }
    if (name == "--version") {
        result.action = CommandLineAction::ShowVersion;
        return false;
    }
    // --windowed and --fullscreen may each be repeated, but not combined.
    const WindowModeOption requested = name == "--windowed" ? WindowModeOption::Windowed : WindowModeOption::Fullscreen;
    WindowModeOption& mode = result.options.windowMode;
    if (mode != WindowModeOption::Default && mode != requested) {
        result = Fail("--windowed and --fullscreen cannot be combined");
        return false;
    }
    mode = requested;
    return true;
}

// Parses args[index] and, for an option with a separate value, the value after it (`index`
// then moves past it). Returns false when parsing ends here (help, version or an error;
// `result` says which).
bool ParseArgument(const std::vector<std::string>& args, std::size_t& index, CommandLineResult& result) {
    const Argument argument = SplitArgument(args[index]);
    const std::string& name = argument.name;
    if (IsFlag(name)) {
        return ParseFlag(argument, result);
    }
    if (const ValueOption* option = FindValueOption(name)) {
        std::string value;
        if (!TakeValue(argument, args, index, value) || !option->apply(value, result.options)) {
            result = Fail(name + " needs " + option->needs());
            return false;
        }
        return true;
    }
    // macOS before 10.9 passed -psn_<serial> to apps launched from Finder. Ignore it.
    if (name.rfind("-psn_", 0) == 0) {
        return true;
    }
    const bool looksLikeOption = !name.empty() && name[0] == '-';
    result = Fail((looksLikeOption ? "unknown option: " : "unexpected argument: ") + args[index]);
    return false;
}
}  // namespace

CommandLineResult ParseCommandLine(const std::vector<std::string>& args) {
    CommandLineResult result{};
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!ParseArgument(args, i, result)) {
            break;
        }
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
        "  --help              show this help and exit\n"
        "  --version           print the version and exit\n"
        "  --data <dir>        use the content in <dir> (the folder that contains game/);\n"
        "                      overrides the NEOWOLF_DATA_DIR environment variable\n"
        "  --pref-dir <dir>    keep the log and progress.json in <dir> instead of the\n"
        "                      per-user folder; overrides NEOWOLF_PREF_DIR (tests, CI)\n"
        "  --level <N>         start campaign level N (1-99, see game/defs/campaign.json)\n"
        "                      directly, skipping the menu and the unlocked-level check\n"
        "                      (development and tests)\n"
        "  --frames <N>        run N frames, then exit with code 0 (automated smoke tests)\n"
        "  --windowed          start in a window (default)\n"
        "  --fullscreen        start in desktop fullscreen\n"
        "\n"
        "Developer tools:\n"
        "  --preview <screen>  show one screen with fixed sample data, one of:\n"
        "                      " +
        PreviewScreenList() +
        "\n"
        "  --screenshot <file> when the game exits, save the last 320x240 frame as a BMP\n"
        "                      (use with --frames N)\n"
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
