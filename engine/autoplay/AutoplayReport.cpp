#include "engine/autoplay/AutoplayReport.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/core/CommandLine.h"

namespace {
constexpr double kMaxTimeLimitSeconds = 24.0 * 3600.0;
// Longer text is rejected before it is converted (so it cannot overflow); "007" is level 7.
constexpr std::size_t kMaxLevelDigits = 3;

bool ParseLevel(const std::string& text, int& out) {
    if (text.empty() || text.size() > kMaxLevelDigits) {
        return false;
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    if (value < kMinLevel || value > kMaxLevel) {
        return false;
    }
    out = value;
    return true;
}

bool ParseSeconds(const std::string& text, double& out) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || errno != 0 || !std::isfinite(value) || value <= 0.0 ||
        value > kMaxTimeLimitSeconds) {
        return false;
    }
    out = value;
    return true;
}

double Round3(double value) {
    return std::round(value * 1000.0) / 1000.0;
}

std::string Fixed(double value, int decimals) {
    std::array<char, 48> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.*f", decimals, value);
    return buffer.data();
}

std::string Join(const std::vector<std::string>& items, const char* separator, const char* empty) {
    if (items.empty()) {
        return empty;
    }
    std::string text;
    for (std::size_t i = 0; i < items.size(); ++i) {
        text += (i == 0 ? "" : separator) + items[i];
    }
    return text;
}

std::string Pad(const std::string& text, std::size_t width, bool right) {
    if (text.size() >= width) {
        return text;
    }
    const std::string fill(width - text.size(), ' ');
    return right ? fill + text : text + fill;
}

std::string Fraction(int a, int b) {
    return std::to_string(a) + "/" + std::to_string(b);
}

bool TakesValue(const std::string& option) {
    return option == "--level" || option == "--data" || option == "--pref-dir" || option == "--time-limit" ||
           option == "--json" || option == "--trace";
}

// Stores the value of an option that takes one; returns why it is invalid, or "" when it is fine.
std::string ApplyOptionValue(AutoplayCommandLine& result, const std::string& option, const std::string& value) {
    if (option == "--level") {
        if (result.level != 0) {
            return "--level given twice";
        }
        if (!ParseLevel(value, result.level)) {
            return "--level must be a campaign level number from " + std::to_string(kMinLevel) + " to " +
                   std::to_string(kMaxLevel) + ", got '" + value + "'";
        }
    } else if (option == "--data") {
        result.dataDir = value;
    } else if (option == "--pref-dir") {
        result.prefDir = value;
    } else if (option == "--time-limit") {
        if (!ParseSeconds(value, result.timeLimitSeconds)) {
            return "--time-limit must be a number of seconds above 0 (at most " + Fixed(kMaxTimeLimitSeconds, 0) +
                   "), got '" + value + "'";
        }
    } else if (option == "--json") {
        result.jsonPath = value;
    } else {
        result.tracePath = value;
    }
    if (value.empty()) {
        return option + " needs a non-empty value";
    }
    return "";
}

// The table's first line: campaign, mode, time limit and step.
std::string TableHeading(const AutoplayRunReport& report) {
    std::ostringstream out;
    out << "Neo-Wolf autoplay: \"" << report.campaignTitle << "\", " << report.campaignLevels << " level(s); "
        << (report.wholeCampaign ? "whole campaign (--all)" : "one level") << ", time limit "
        << Fixed(report.timeLimitSeconds, 0) << " s per level, step "
        << (report.fixedStepSeconds > 0.0 ? "1/" + Fixed(1.0 / report.fixedStepSeconds, 0) : std::string("?"))
        << " s\n";
    return out.str();
}

// A level's cells in the table; a level that was not played shows "-" for every result.
std::vector<std::string> LevelRow(const AutoplayLevelReport& level) {
    std::vector<std::string> row = {std::to_string(level.levelNumber), level.id, AutoplayResultName(level.result)};
    std::vector<std::string> results = {Fixed(level.gameSeconds, 2),
                                        std::to_string(level.frames),
                                        std::to_string(level.damageTaken),
                                        std::to_string(level.healthEnd),
                                        std::to_string(level.ammoEnd),
                                        Fraction(level.kills, level.totalEnemies),
                                        Fraction(level.pickups, level.totalPickups),
                                        Join(level.keysUsed, ",", "-"),
                                        std::to_string(level.shotsFired)};
    if (level.result == AutoplayResult::NotPlayed) {
        std::fill(results.begin(), results.end(), "-");
    }
    row.insert(row.end(), results.begin(), results.end());
    return row;
}

// Rows of cells as text columns two spaces apart, each as wide as its widest cell; no trailing spaces.
std::string FormatColumns(const std::vector<std::vector<std::string>>& rows, const std::vector<bool>& rightAligned) {
    std::vector<std::size_t> widths(rightAligned.size(), 0);
    for (const auto& row : rows) {
        for (std::size_t c = 0; c < row.size(); ++c) {
            widths[c] = std::max(widths[c], row[c].size());
        }
    }
    std::string text;
    for (const auto& row : rows) {
        std::string line;
        for (std::size_t c = 0; c < row.size(); ++c) {
            line += (c == 0 ? "" : "  ") + Pad(row[c], widths[c], rightAligned[c]);
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        text += line + "\n";
    }
    return text;
}

// Below the table: why levels failed, the carry-over check and the overall result.
std::string TableSummary(const AutoplayRunReport& report) {
    std::ostringstream out;
    for (const AutoplayLevelReport& level : report.levels) {
        if (!level.reason.empty()) {
            out << "  level " << level.levelNumber << " (" << level.id << "): " << level.reason << "\n";
        }
    }
    if (report.carryOverChecked) {
        if (report.carryOverProblems.empty()) {
            out << "Carry-over: OK (each level started with the previous level's health and ammo; progress saved)\n";
        }
        for (const std::string& problem : report.carryOverProblems) {
            out << "Carry-over: FAILED: " << problem << "\n";
        }
    }
    const auto completed =
        std::count_if(report.levels.begin(), report.levels.end(),
                      [](const AutoplayLevelReport& level) { return level.result == AutoplayResult::Complete; });
    out << "Result: " << (report.Success() ? "PASS" : "FAIL") << " (" << completed << " of " << report.levels.size()
        << " level(s) completed" << (report.wholeCampaign && report.campaignCompleted ? ", campaign complete" : "")
        << ")\n";
    return out.str();
}
}  // namespace

AutoplayCommandLine ParseAutoplayCommandLine(const std::vector<std::string>& args) {
    AutoplayCommandLine result{};
    const auto fail = [&result](const std::string& message) {
        result.action = AutoplayCommandLine::Action::Error;
        result.error = message;
        return result;
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--help" || arg == "-h") {
            result.action = AutoplayCommandLine::Action::ShowHelp;
            return result;
        }
        if (arg == "--version") {
            result.action = AutoplayCommandLine::Action::ShowVersion;
            return result;
        }
        if (arg == "--all") {
            result.all = true;
        } else if (arg == "--verbose") {
            result.verbose = true;
        } else if (TakesValue(arg)) {
            if (i + 1 >= args.size()) {
                return fail(arg + " needs a value");
            }
            const std::string error = ApplyOptionValue(result, arg, args[++i]);
            if (!error.empty()) {
                return fail(error);
            }
        } else {
            return fail("unknown option '" + arg + "'");
        }
    }
    if (result.all == (result.level != 0)) {
        return fail(result.all ? "use either --level N or --all, not both" : "one of --level N or --all is required");
    }
    return result;
}

std::string AutoplayUsage(std::string_view programName) {
    std::ostringstream out;
    out << "Usage: " << programName << " (--level N | --all) [options]\n"
        << "\n"
        << "Plays Neo-Wolf headless with a bot (real engine, simulated keyboard input, fixed 1/60 s\n"
        << "steps) and reports per level: result, time, damage, kills, pickups, keys and shots.\n"
        << "\n"
        << "  --level N            play campaign level N (1-based) once, starting with the initial\n"
        << "                       health and ammo\n"
        << "  --all                play the whole campaign from New Game, carrying health and ammo\n"
        << "                       over like a player, and check the carry-over\n"
        << "  --data DIR           content root (the folder that contains game/); default: the\n"
        << "                       game's automatic search\n"
        << "  --pref-dir DIR       folder for progress.json; default: a temporary folder that is\n"
        << "                       removed afterwards\n"
        << "  --time-limit SECONDS game time allowed per level (default " << Fixed(AutoplayConfig{}.timeLimitSeconds, 0)
        << ")\n"
        << "  --json FILE          also write a machine-readable report\n"
        << "  --trace FILE         also write an event log, one line per event (level start/end,\n"
        << "                       enemy states, hits taken and dealt, pickups, keys, doors,\n"
        << "                       triggers, the exit); deterministic, for balancing\n"
        << "  --verbose            print the engine's log (default: warnings and errors only)\n"
        << "  --help, --version\n"
        << "\n"
        << "Exit codes: 0 every requested level was completed, 1 otherwise (or the game could not\n"
        << "start, or the JSON report or trace could not be written), 2 usage error.\n";
    return out.str();
}

std::string FormatAutoplayTable(const AutoplayRunReport& report) {
    const std::string text = TableHeading(report);
    if (!report.error.empty()) {
        return text + "ERROR: " + report.error + "\n";
    }
    std::vector<std::vector<std::string>> rows = {
        {"#", "Level", "Result", "Time s", "Frames", "Damage", "HP", "Ammo", "Kills", "Pickups", "Keys", "Shots"}};
    for (const AutoplayLevelReport& level : report.levels) {
        rows.push_back(LevelRow(level));
    }
    const std::vector<bool> rightAligned = {true, false, false, true, true, true, true, true, true, true, false, true};
    return text + FormatColumns(rows, rightAligned) + TableSummary(report);
}

std::string FormatAutoplayJson(const AutoplayRunReport& report) {
    using nlohmann::ordered_json;
    ordered_json document;
    document["tool"] = "neo_wolf_autoplay";
    document["version"] = VersionString();
    document["mode"] = report.wholeCampaign ? "all" : "level";
    document["fixedStepSeconds"] = std::round(report.fixedStepSeconds * 1e6) / 1e6;
    document["timeLimitSeconds"] = Round3(report.timeLimitSeconds);
    document["campaign"] = ordered_json{{"title", report.campaignTitle}, {"levels", report.campaignLevels}};
    document["success"] = report.Success();
    document["error"] = report.error;
    ordered_json levels = ordered_json::array();
    for (const AutoplayLevelReport& level : report.levels) {
        ordered_json entry;
        entry["level"] = level.levelNumber;
        entry["id"] = level.id;
        entry["name"] = level.name;
        entry["result"] = AutoplayResultName(level.result);
        entry["reason"] = level.reason;
        entry["gameSeconds"] = Round3(level.gameSeconds);
        entry["frames"] = level.frames;
        entry["damageTaken"] = level.damageTaken;
        entry["healthStart"] = level.healthStart;
        entry["ammoStart"] = level.ammoStart;
        entry["healthEnd"] = level.healthEnd;
        entry["ammoEnd"] = level.ammoEnd;
        entry["kills"] = level.kills;
        entry["totalEnemies"] = level.totalEnemies;
        entry["pickups"] = level.pickups;
        entry["totalPickups"] = level.totalPickups;
        entry["keysCollected"] = level.keysCollected;
        entry["keysUsed"] = level.keysUsed;
        entry["shotsFired"] = level.shotsFired;
        entry["doorsOpened"] = level.doorsOpened;
        entry["replans"] = level.replans;
        levels.push_back(entry);
    }
    document["levels"] = levels;
    document["carryOver"] = ordered_json{{"checked", report.carryOverChecked},
                                         {"ok", report.carryOverProblems.empty()},
                                         {"problems", report.carryOverProblems}};
    document["campaignCompleted"] = report.campaignCompleted;
    return document.dump(2) + "\n";
}
