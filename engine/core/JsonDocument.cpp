#include "engine/core/JsonDocument.h"

#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "engine/core/Paths.h"

bool ReadTextFile(const std::string& filePath, const std::string& what, std::string& outText,
                  std::vector<std::string>& errors) {
    std::ifstream file(PathFromUtf8(filePath), std::ios::binary);
    if (!file.is_open()) {
        errors.push_back("Unable to open " + what + ": " + filePath);
        return false;
    }
    outText.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool ParseJsonText(const std::string& text, const std::string& sourceName, nlohmann::json& outDocument,
                   std::vector<std::string>& errors) {
    // Duplicate-key detection: one set of seen keys per open object. The name of the key that
    // opened an object is kept for the message.
    struct ObjectFrame {
        std::string name;
        std::set<std::string> keys;
    };
    std::vector<ObjectFrame> frames;
    std::string lastKey;
    std::vector<std::string> duplicates;

    const nlohmann::json::parser_callback_t callback = [&](int /*depth*/, nlohmann::json::parse_event_t event,
                                                           nlohmann::json& parsed) {
        switch (event) {
            case nlohmann::json::parse_event_t::object_start:
                frames.push_back(ObjectFrame{lastKey, {}});
                break;
            case nlohmann::json::parse_event_t::object_end:
                if (!frames.empty()) {
                    frames.pop_back();
                }
                break;
            case nlohmann::json::parse_event_t::key:
                lastKey = parsed.get<std::string>();
                if (!frames.empty() && !frames.back().keys.insert(lastKey).second) {
                    const std::string& parent = frames.back().name;
                    duplicates.push_back(
                        "duplicate key \"" + lastKey + "\"" +
                        (parent.empty() ? std::string(" at the top level") : " inside \"" + parent + "\""));
                }
                break;
            default:
                break;
        }
        return true;
    };

    try {
        outDocument = nlohmann::json::parse(text, callback, /*allow_exceptions=*/true, /*ignore_comments=*/false);
    } catch (const nlohmann::json::exception& ex) {
        // ex.what() is e.g. "[json.exception.parse_error.101] parse error at line 3, column 5: ..."
        errors.push_back(sourceName + ": invalid JSON: " + ex.what());
        return false;
    }

    for (const std::string& duplicate : duplicates) {
        errors.push_back(std::string(sourceName)
                             .append(": ")
                             .append(duplicate)
                             .append(" (each key may appear only once per object)"));
    }
    return duplicates.empty();
}

std::string DescribeJsonValue(const nlohmann::json& value) {
    if (value.is_object()) {
        return "an object";
    }
    if (value.is_array()) {
        return "an array";
    }
    if (value.is_null()) {
        return "null";
    }
    // Numbers, strings (quoted) and booleans. A CSV cell is not checked for UTF-8, so invalid
    // bytes are shown as U+FFFD instead of making dump() throw.
    return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}
