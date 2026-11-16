#include "engine/core/Log.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <system_error>

#include "engine/core/Paths.h"

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

bool StartFreshLogFile(std::string_view path, std::string_view previousPath) {
    const std::filesystem::path logPath = PathFromUtf8(path);
    std::error_code ec;
    if (!previousPath.empty() && std::filesystem::exists(logPath, ec)) {
        const std::filesystem::path prevPath = PathFromUtf8(previousPath);
        std::filesystem::remove(prevPath, ec);  // rename() does not replace on every OS
        std::filesystem::rename(logPath, prevPath, ec);
    }

    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream file(logPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        g_logFilePath.clear();
        return false;
    }
    g_logFilePath = std::string(path);
    return true;
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
        std::ofstream file(PathFromUtf8(g_logFilePath), std::ios::out | std::ios::app);
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
