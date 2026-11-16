#include "engine/core/Progress.h"

#include <cstdint>
#include <fstream>
#include <system_error>
#include <vector>

#include "engine/core/JsonDocument.h"
#include "engine/core/Paths.h"

using nlohmann::json;

namespace {
// An integer field in [min, max]; false (with `message`) otherwise.
bool ReadBoundedInt(const json& document, const char* key, std::int64_t min, std::int64_t max,
                    const std::string& sourceName, std::int64_t& out, std::string& message) {
    const auto it = document.find(key);
    if (it == document.end()) {
        message = sourceName + ": field \"" + key + "\" is missing";
        return false;
    }
    if (!it->is_number_integer()) {
        message = sourceName + ": field \"" + key + "\" must be an integer, got " + DescribeJsonValue(*it);
        return false;
    }
    if (it->is_number_unsigned() && it->get<std::uint64_t>() > static_cast<std::uint64_t>(max)) {
        message = sourceName + ": field \"" + key + "\" is out of range, got " + DescribeJsonValue(*it);
        return false;
    }
    const std::int64_t value = it->get<std::int64_t>();
    if (value < min || value > max) {
        message = sourceName + ": field \"" + key + "\" must be from " + std::to_string(min) + " to " +
                  std::to_string(max) + ", got " + DescribeJsonValue(*it);
        return false;
    }
    out = value;
    return true;
}
} // namespace

bool ParseProgressJson(const std::string& text, const std::string& sourceName, ProgressData& outData,
                       std::string& message) {
    std::vector<std::string> errors;
    json document;
    if (!ParseJsonText(text, sourceName, document, errors)) {
        message = errors.empty() ? sourceName + ": invalid JSON" : errors.front();
        return false;
    }
    if (!document.is_object()) {
        message = sourceName + ": the top level must be an object, got " + DescribeJsonValue(document);
        return false;
    }
    std::int64_t version = 0;
    if (!ReadBoundedInt(document, "version", 1, kProgressSchemaVersion, sourceName, version, message)) {
        message += " (this game reads version " + std::to_string(kProgressSchemaVersion) + ")";
        return false;
    }
    std::int64_t highest = 0;
    if (!ReadBoundedInt(document, "highestUnlocked", 1, kMaxProgressLevel, sourceName, highest, message)) {
        return false;
    }
    outData.highestUnlocked = static_cast<int>(highest);
    return true;
}

ProgressLoadResult LoadProgressFile(const std::string& filePath, ProgressData& outData, std::string& message) {
    std::error_code ec;
    const std::filesystem::path path = PathFromUtf8(filePath);
    if (!std::filesystem::exists(path, ec)) {
        message = "no saved progress (" + filePath + ")";
        return ProgressLoadResult::Missing;
    }
    std::vector<std::string> errors;
    std::string text;
    if (!ReadTextFile(filePath, "progress file", text, errors)) {
        message = errors.empty() ? "cannot read " + filePath : errors.front();
        return ProgressLoadResult::Invalid;
    }
    ProgressData parsed{};
    if (!ParseProgressJson(text, filePath, parsed, message)) {
        return ProgressLoadResult::Invalid;
    }
    outData = parsed;
    message = "progress loaded from " + filePath + ": highest unlocked level " + std::to_string(parsed.highestUnlocked);
    return ProgressLoadResult::Loaded;
}

std::string SerializeProgress(const ProgressData& data) {
    json document = json::object();
    document["version"] = kProgressSchemaVersion;
    document["highestUnlocked"] = data.highestUnlocked;
    return document.dump(2) + "\n";
}

bool SaveProgressFile(const std::string& filePath, const ProgressData& data, std::string& error) {
    const std::filesystem::path path = PathFromUtf8(filePath);
    std::filesystem::path tempPath = path;
    tempPath += ".tmp";

    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            error = "cannot create " + PathToUtf8(tempPath);
            return false;
        }
        file << SerializeProgress(data);
        file.flush();
        if (!file.good()) {
            error = "cannot write " + PathToUtf8(tempPath);
            file.close();
            std::error_code ignored;
            std::filesystem::remove(tempPath, ignored);
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, path, ec);
    if (ec) {
        // Some C++ runtimes refuse to rename over an existing file on Windows: remove the old
        // file and try once more. (A crash between the two steps loses only the old save; the
        // new one is complete in the .tmp file.)
        std::error_code removeError;
        std::filesystem::remove(path, removeError);
        ec.clear();
        std::filesystem::rename(tempPath, path, ec);
    }
    if (ec) {
        error = "cannot replace " + filePath + ": " + ec.message();
        std::error_code ignored;
        std::filesystem::remove(tempPath, ignored);
        return false;
    }
    return true;
}
