#pragma once

// Logging to the console and to the per-user log file (neowolf.log).

#include <string>
#include <string_view>
#include <vector>

enum class LogLevel { Info, Warning, Error };

void LogMessage(LogLevel level, std::string_view message);
void LogInfo(std::string_view message);
void LogWarning(std::string_view message);
void LogError(std::string_view message);
// Logs each message on its own line, indented by two spaces: the details under a summary line
// such as "Invalid gameplay definitions <file>:".
void LogIndented(LogLevel level, const std::vector<std::string>& messages);

// Messages below `minimum` are not printed to the console (stdout/stderr); the log file, if
// any, still receives every message. Default: LogLevel::Info (everything is printed). Used by
// neo_wolf_autoplay to keep its report readable.
void SetConsoleLogLevel(LogLevel minimum);

// Appends every later message to `path` (UTF-8). An empty path turns file output off.
void SetLogFilePath(std::string_view path);
std::string GetLogFilePath();

// Starts a fresh log file for this run: an existing `path` is renamed to `previousPath`
// (replacing it), then `path` is created empty and becomes the log file. Returns false,
// and leaves file output off, if `path` cannot be created.
bool StartFreshLogFile(std::string_view path, std::string_view previousPath);
