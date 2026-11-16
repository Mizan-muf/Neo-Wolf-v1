#include "engine/autoplay/AutoplayReport.h"

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

bool ParseLevel(const std::string& text, int& out) {
    if (text.empty() || text.size() > 3) {
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
    char buffer[48] = {};
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return buffer;
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
} // namespace

AutoplayCommandLine ParseAutoplayCommandLine(const std::vector<std::string>& args) {
    AutoplayCommandLine result{};
    const auto fail = [&result](const std::string& message) {
        result.action = AutoplayCommandLine::Action::Error;
        result.error = message;
        return result;
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        const bool hasValue = i + 1 < args.size();
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
        } else if (arg == "--level" || arg == "--data" || arg == "--pref-dir" || arg == "--time-limit" ||
                   arg == "--json") {
            if (!hasValue) {
                return fail(arg + " needs a value");
            }
            const std::string& value = args[++i];
            if (arg == "--level") {
                if (result.level != 0) {
                    return fail("--level given twice");
                }
                if (!ParseLevel(value, result.level)) {
                    return fail("--level must be a campaign level number from " + std::to_string(kMinLevel) +
                                " to " + std::to_string(kMaxLevel) + ", got '" + value + "'");
                }
            } else if (arg == "--data") {
                result.dataDir = value;
            } else if (arg == "--pref-dir") {
                result.prefDir = value;
            } else if (arg == "--time-limit") {
                if (!ParseSeconds(value, result.timeLimitSeconds)) {
                    return fail("--time-limit must be a number of seconds above 0 (at most 86400), got '" + value + "'");
                }
            } else {
                result.jsonPath = value;
            }
            if (value.empty()) {
                return fail(arg + " needs a non-empty value");
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
        << "  --time-limit SECONDS game time allowed per level (default "
        << Fixed(AutoplayConfig{}.timeLimitSeconds, 0) << ")\n"
        << "  --json FILE          also write a machine-readable report\n"
        << "  --verbose            print the engine's log (default: warnings and errors only)\n"
        << "  --help, --version\n"
        << "\n"
        << "Exit codes: 0 every requested level was completed, 1 otherwise (or the game could not\n"
        << "start), 2 usage error.\n";
    return out.str();
}

std::string FormatAutoplayTable(const AutoplayRunReport& report) {
    std::ostringstream out;
    out << "Neo-Wolf autoplay: \"" << report.campaignTitle << "\", " << report.campaignLevels << " level(s); "
        << (report.wholeCampaign ? "whole campaign (--all)" : "one level") << ", time limit "
        << Fixed(report.timeLimitSeconds, 0) << " s per level, step "
        << (report.fixedStepSeconds > 0.0 ? "1/" + Fixed(1.0 / report.fixedStepSeconds, 0) : std::string("?"))
        << " s\n";
    if (!report.error.empty()) {
        out << "ERROR: " << report.error << "\n";
        return out.str();
    }

    const std::vector<std::string> header = {"#",  "Level", "Result", "Time s", "Frames", "Damage", "HP",
                                             "Ammo", "Kills", "Pickups", "Keys", "Shots"};
    const std::vector<bool> right = {true, false, false, true, true, true, true, true, true, true, false, true};
    std::vector<std::vector<std::string>> rows;
    for (const AutoplayLevelReport& level : report.levels) {
        const bool played = level.result != AutoplayResult::NotPlayed;
        rows.push_back({std::to_string(level.levelNumber), level.id, AutoplayResultName(level.result),
                        played ? Fixed(level.gameSeconds, 2) : "-", played ? std::to_string(level.frames) : "-",
                        played ? std::to_string(level.damageTaken) : "-",
                        played ? std::to_string(level.healthEnd) : "-", played ? std::to_string(level.ammoEnd) : "-",
                        played ? Fraction(level.kills, level.totalEnemies) : "-",
                        played ? Fraction(level.pickups, level.totalPickups) : "-",
                        played ? Join(level.keysUsed, ",", "-") : "-", played ? std::to_string(level.shotsFired) : "-"});
    }
    std::vector<std::size_t> widths(header.size());
    for (std::size_t c = 0; c < header.size(); ++c) {
        widths[c] = header[c].size();
        for (const auto& row : rows) {
            widths[c] = std::max(widths[c], row[c].size());
        }
    }
    const auto printRow = [&](const std::vector<std::string>& cells) {
        std::string line;
        for (std::size_t c = 0; c < cells.size(); ++c) {
            line += (c == 0 ? "" : "  ") + Pad(cells[c], widths[c], right[c]);
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        out << line << "\n";
    };
    printRow(header);
    for (const auto& row : rows) {
        printRow(row);
    }
    for (const AutoplayLevelReport& level : report.levels) {
        if (!level.reason.empty()) {
            out << "  level " << level.levelNumber << " (" << level.id << "): " << level.reason << "\n";
        }
    }
    if (report.carryOverChecked) {
        if (report.carryOverProblems.empty()) {
            out << "Carry-over: OK (each level started with the previous level's health and ammo; progress saved)\n";
        } else {
            for (const std::string& problem : report.carryOverProblems) {
                out << "Carry-over: FAILED: " << problem << "\n";
            }
        }
    }
    int completed = 0;
    for (const AutoplayLevelReport& level : report.levels) {
        completed += level.result == AutoplayResult::Complete ? 1 : 0;
    }
    out << "Result: " << (report.Success() ? "PASS" : "FAIL") << " (" << completed << " of " << report.levels.size()
        << " level(s) completed" << (report.wholeCampaign && report.campaignCompleted ? ", campaign complete" : "")
        << ")\n";
    return out.str();
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
