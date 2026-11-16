#include "engine/core/Log.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "engine/core/Paths.h"

namespace {
// The log's shared state. A function-local static is initialised on first use, so logging
// works even from other static initialisers.
struct LogState {
    std::mutex mutex;
    std::string filePath;  // empty: no log file
    LogLevel consoleMinimum = LogLevel::Info;
};

LogState& State() {
    static LogState state;
    return state;
}

int Severity(LogLevel level) {
    switch (level) {
        case LogLevel::Warning:
            return 1;
        case LogLevel::Error:
            return 2;
        case LogLevel::Info:
        default:
            return 0;
    }
}

const char* LevelToText(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "LOG";
    }
}

// "[HH:MM:SS] [LEVEL] message", local time.
std::string FormatLogLine(LogLevel level, std::string_view message) {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream line;
    line << "[" << std::put_time(&localTime, "%H:%M:%S") << "] "
         << "[" << LevelToText(level) << "] " << message << '\n';
    return line.str();
}
}  // namespace

void SetConsoleLogLevel(LogLevel minimum) {
    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    state.consoleMinimum = minimum;
}

void SetLogFilePath(std::string_view path) {
    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    state.filePath = std::string(path);
}

bool StartFreshLogFile(std::string_view path, std::string_view previousPath) {
    const std::filesystem::path logPath = PathFromUtf8(path);
    std::error_code ec;
    if (!previousPath.empty() && std::filesystem::exists(logPath, ec)) {
        const std::filesystem::path prevPath = PathFromUtf8(previousPath);
        std::filesystem::remove(prevPath, ec);  // rename() does not replace on every OS
        std::filesystem::rename(logPath, prevPath, ec);
    }

    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    const std::ofstream file(logPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        state.filePath.clear();
        return false;
    }
    state.filePath = std::string(path);
    return true;
}

std::string GetLogFilePath() {
    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    return state.filePath;
}

void LogMessage(LogLevel level, std::string_view message) {
    const std::string line = FormatLogLine(level, message);

    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    if (Severity(level) >= Severity(state.consoleMinimum)) {
        std::ostream& stream = (level == LogLevel::Error) ? std::cerr : std::cout;
        stream << line;
    }
    if (!state.filePath.empty()) {
        std::ofstream file(PathFromUtf8(state.filePath), std::ios::out | std::ios::app);
        if (file.is_open()) {
            file << line;
        }
    }
}

void LogIndented(LogLevel level, const std::vector<std::string>& messages) {
    for (const std::string& message : messages) {
        LogMessage(level, "  " + message);
    }
}

void LogInfo(std::string_view message) {
    LogMessage(LogLevel::Info, message);
}

void LogWarning(std::string_view message) {
    LogMessage(LogLevel::Warning, message);
}

void LogError(std::string_view message) {
    LogMessage(LogLevel::Error, message);
}
