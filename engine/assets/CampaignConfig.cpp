#include "engine/assets/CampaignConfig.h"

#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "engine/assets/ContentConfig.h"
#include "engine/core/JsonDocument.h"
#include "engine/core/Paths.h"

using nlohmann::json;

namespace {
bool ContentFileExists(const std::filesystem::path& contentRoot, const std::string& relativePath) {
    std::error_code ec;
    return std::filesystem::is_regular_file(PathFromUtf8(JoinContentPath(contentRoot, relativePath)), ec);
}

// Reads one required, non-empty string field; records an error and returns false otherwise.
bool ReadString(const json& object, const char* key, const std::string& label, std::string& out,
                std::vector<std::string>& errors) {
    const auto it = object.find(key);
    if (it == object.end()) {
        errors.push_back(label + ": field \"" + key + "\" is required");
        return false;
    }
    if (!it->is_string() || it->get<std::string>().empty()) {
        errors.push_back(label + ": field \"" + key + "\" must be a non-empty string, got " + DescribeJsonValue(*it));
        return false;
    }
    out = it->get<std::string>();
    return true;
}

void WarnUnknownKeys(const json& object, const std::set<std::string>& known, const std::string& label,
                     std::vector<std::string>& warnings) {
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (known.count(it.key()) == 0) {
            warnings.push_back(label + ": unknown key \"" + it.key() + "\" ignored");
        }
    }
}

void WarnLongName(const std::string& name, const std::string& label, std::vector<std::string>& warnings) {
    if (name.size() > kMaxCampaignNameLength) {
        warnings.push_back(label + " is longer than " + std::to_string(kMaxCampaignNameLength) +
                           " characters and is shortened on screen");
    }
}
} // namespace

bool IsValidContentId(const std::string& id) {
    if (id.empty()) {
        return false;
    }
    for (const char c : id) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool ParseCampaign(
    const std::string& jsonText,
    const std::string& sourceName,
    const std::filesystem::path& contentRoot,
    CampaignDefinition& outCampaign,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) {
    const std::size_t errorsBefore = errors.size();

    json document;
    if (!ParseJsonText(jsonText, sourceName, document, errors)) {
        return false;
    }
    if (!document.is_object()) {
        errors.push_back(sourceName + ": the top level must be an object, got " + DescribeJsonValue(document));
        return false;
    }

    CampaignDefinition campaign{};
    if (ReadString(document, "title", sourceName, campaign.title, errors)) {
        WarnLongName(campaign.title, sourceName + ": title", warnings);
    }
    WarnUnknownKeys(document, {"title", "levels"}, sourceName, warnings);

    const auto levelsIt = document.find("levels");
    if (levelsIt == document.end()) {
        errors.push_back(sourceName + ": field \"levels\" is required (an array with at least one level)");
    } else if (!levelsIt->is_array()) {
        errors.push_back(sourceName + ": field \"levels\" must be an array, got " + DescribeJsonValue(*levelsIt));
    } else if (levelsIt->empty()) {
        errors.push_back(sourceName + ": \"levels\" is empty; the campaign needs at least one level");
    } else {
        std::set<std::string> seenIds;
        for (std::size_t i = 0; i < levelsIt->size(); ++i) {
            const json& entry = (*levelsIt)[i];
            std::string label = sourceName + ": levels[" + std::to_string(i) + "]";
            if (!entry.is_object()) {
                errors.push_back(label + " must be an object, got " + DescribeJsonValue(entry));
                continue;
            }
            CampaignLevel level{};
            if (ReadString(entry, "id", label, level.id, errors)) {
                label += " (" + level.id + ")";
                if (!IsValidContentId(level.id)) {
                    errors.push_back(label + ": id must use only a-z, 0-9, '_' and '-'");
                } else if (!seenIds.insert(level.id).second) {
                    errors.push_back(label + ": id \"" + level.id + "\" is used by an earlier level (ids must be unique)");
                }
            }
            if (ReadString(entry, "name", label, level.name, errors)) {
                WarnLongName(level.name, label + ": name", warnings);
            }
            if (ReadString(entry, "map", label, level.map, errors) && !ContentFileExists(contentRoot, level.map)) {
                errors.push_back(label + ": map file not found: " + level.map);
            }
            if (ReadString(entry, "entities", label, level.entities, errors) &&
                !ContentFileExists(contentRoot, level.entities)) {
                errors.push_back(label + ": entities file not found: " + level.entities);
            }
            if (ReadString(entry, "theme", label, level.theme, errors)) {
                if (!IsValidContentId(level.theme)) {
                    errors.push_back(label + ": theme must use only a-z, 0-9, '_' and '-', got \"" + level.theme + "\"");
                } else if (!ContentFileExists(contentRoot, ThemeConfigPath(level.theme))) {
                    errors.push_back(label + ": theme \"" + level.theme + "\" not found (" +
                                     ThemeConfigPath(level.theme) + ")");
                }
            }
            WarnUnknownKeys(entry, {"id", "name", "map", "entities", "theme"}, label, warnings);
            campaign.levels.push_back(level);
        }
    }

    if (errors.size() != errorsBefore) {
        return false;
    }
    outCampaign = campaign;
    return true;
}

bool LoadCampaign(
    const std::string& filePath,
    const std::filesystem::path& contentRoot,
    CampaignDefinition& outCampaign,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) {
    std::string text;
    if (!ReadTextFile(filePath, "campaign manifest", text, errors)) {
        return false;
    }
    return ParseCampaign(text, filePath, contentRoot, outCampaign, errors, warnings);
}
