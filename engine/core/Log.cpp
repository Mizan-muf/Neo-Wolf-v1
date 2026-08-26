#include "engine/core/Log.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>

namespace {
std::string g_logFilePath{};
std::mutex g_logMutex{};

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
} // namespace

void SetLogFilePath(std::string_view path) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logFilePath = std::string(path);
}

std::string GetLogFilePath() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    return g_logFilePath;
}

void LogMessage(LogLevel level, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif

    std::lock_guard<std::mutex> lock(g_logMutex);

    std::ostream& stream = (level == LogLevel::Error) ? std::cerr : std::cout;
    stream << "[" << std::put_time(&localTime, "%H:%M:%S") << "] "
           << "[" << LevelToText(level) << "] "
           << message << '\n';

    if (!g_logFilePath.empty()) {
        std::ofstream file(g_logFilePath, std::ios::out | std::ios::app);
        if (file.is_open()) {
            file << "[" << std::put_time(&localTime, "%H:%M:%S") << "] "
                 << "[" << LevelToText(level) << "] "
                 << message << '\n';
        }
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
