#pragma once

// Saved campaign progress: progress.json in the per-user folder (docs/GAMEPLAY.md "Saves").
//
//   { "version": 1, "highestUnlocked": 2 }
//
// highestUnlocked is the highest campaign level (1-based) the player may start; level 1 is
// always unlocked. The file is written atomically (a temporary file in the same folder, then
// a rename), so a crash while saving leaves the previous file intact. A missing file means
// "no progress"; an unreadable, malformed or unknown-version file is treated the same way and
// reported as a warning by the caller. SDL-free.

#include <string>

inline constexpr int kProgressSchemaVersion = 1;
inline constexpr int kMaxProgressLevel = 999;

struct ProgressData {
    int highestUnlocked = 1;
};

enum class ProgressLoadResult {
    Loaded,   // outData holds the saved progress
    Missing,  // no file: a first start
    Invalid   // unreadable, not JSON, wrong version or values; `message` says why
};

// The file name inside the per-user folder.
inline constexpr const char* kProgressFileName = "progress.json";

ProgressLoadResult LoadProgressFile(const std::string& filePath, ProgressData& outData, std::string& message);
bool ParseProgressJson(const std::string& text, const std::string& sourceName, ProgressData& outData,
                       std::string& message);
std::string SerializeProgress(const ProgressData& data);
// Atomic write: <path>.tmp, then rename over <path>. Returns false (and sets `error`) on any
// failure; the previous file is then unchanged.
bool SaveProgressFile(const std::string& filePath, const ProgressData& data, std::string& error);
