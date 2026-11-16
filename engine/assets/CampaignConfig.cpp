#include "engine/assets/CampaignConfig.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <utility>
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

// "id": a content id, not used by an earlier level. `label` gets the id appended once it is read.
void ReadLevelId(const json& entry, std::string& label, std::set<std::string>& seenIds, CampaignLevel& level,
                 std::vector<std::string>& errors) {
    if (!ReadString(entry, "id", label, level.id, errors)) {
        return;
    }
    label += " (" + level.id + ")";
    if (!IsValidContentId(level.id)) {
        errors.push_back(label + ": id must use only a-z, 0-9, '_' and '-'");
    } else if (!seenIds.insert(level.id).second) {
        errors.push_back(label + ": id \"" + level.id + "\" is used by an earlier level (ids must be unique)");
    }
}

// "theme": a content id whose theme file exists.
void ReadLevelTheme(const json& entry, const std::string& label, const std::filesystem::path& contentRoot,
                    CampaignLevel& level, std::vector<std::string>& errors) {
    if (!ReadString(entry, "theme", label, level.theme, errors)) {
        return;
    }
    if (!IsValidContentId(level.theme)) {
        errors.push_back(label + ": theme must use only a-z, 0-9, '_' and '-', got \"" + level.theme + "\"");
    } else if (!ContentFileExists(contentRoot, ThemeConfigPath(level.theme))) {
        errors.push_back(label + ": theme \"" + level.theme + "\" not found (" + ThemeConfigPath(level.theme) + ")");
    }
}

// One entry of "levels"; `label` names it ("file: levels[2]").
CampaignLevel ReadLevel(const json& entry, std::string label, const std::filesystem::path& contentRoot,
                        std::set<std::string>& seenIds, std::vector<std::string>& errors,
                        std::vector<std::string>& warnings) {
    CampaignLevel level{};
    ReadLevelId(entry, label, seenIds, level, errors);
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
    ReadLevelTheme(entry, label, contentRoot, level, errors);
    WarnUnknownKeys(entry, {"id", "name", "map", "entities", "theme"}, label, warnings);
    return level;
}

// "levels": a non-empty array of level objects. Their ids are added to `seenIds`.
void ReadLevels(const json& document, const std::string& sourceName, const std::filesystem::path& contentRoot,
                std::set<std::string>& seenIds, std::vector<CampaignLevel>& outLevels, std::vector<std::string>& errors,
                std::vector<std::string>& warnings) {
    const auto levelsIt = document.find("levels");
    if (levelsIt == document.end()) {
        errors.push_back(sourceName + ": field \"levels\" is required (an array with at least one level)");
        return;
    }
    if (!levelsIt->is_array()) {
        errors.push_back(sourceName + ": field \"levels\" must be an array, got " + DescribeJsonValue(*levelsIt));
        return;
    }
    if (levelsIt->empty()) {
        errors.push_back(sourceName + ": \"levels\" is empty; the campaign needs at least one level");
        return;
    }
    for (std::size_t i = 0; i < levelsIt->size(); ++i) {
        const json& entry = (*levelsIt)[i];
        const std::string label = sourceName + ": levels[" + std::to_string(i) + "]";
        if (!entry.is_object()) {
            errors.push_back(label + " must be an object, got " + DescribeJsonValue(entry));
            continue;
        }
        outLevels.push_back(ReadLevel(entry, label, contentRoot, seenIds, errors, warnings));
    }
}

// "testLevel" (optional): one level object, outside the campaign; its id must differ from every
// campaign level's (`seenIds`).
void ReadTestLevel(const json& document, const std::string& sourceName, const std::filesystem::path& contentRoot,
                   std::set<std::string>& seenIds, std::optional<CampaignLevel>& outTestLevel,
                   std::vector<std::string>& errors, std::vector<std::string>& warnings) {
    const auto it = document.find("testLevel");
    if (it == document.end()) {
        return;
    }
    const std::string label = sourceName + ": testLevel";
    if (!it->is_object()) {
        errors.push_back(label + " must be an object, got " + DescribeJsonValue(*it));
        return;
    }
    outTestLevel = ReadLevel(*it, label, contentRoot, seenIds, errors, warnings);
}

// "bootIntoTestLevel" (optional, default false): a boolean; true needs a "testLevel".
void ReadBootIntoTestLevel(const json& document, const std::string& sourceName, bool hasTestLevel, bool& out,
                           std::vector<std::string>& errors) {
    const auto it = document.find("bootIntoTestLevel");
    if (it == document.end()) {
        return;
    }
    if (!it->is_boolean()) {
        errors.push_back(sourceName + ": field \"bootIntoTestLevel\" must be true or false, got " +
                         DescribeJsonValue(*it));
        return;
    }
    out = it->get<bool>();
    if (out && !hasTestLevel) {
        errors.push_back(sourceName + ": \"bootIntoTestLevel\" is true, but there is no \"testLevel\" to start in");
    }
}
}  // namespace

bool IsValidContentId(const std::string& id) {
    if (id.empty()) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

bool ParseCampaign(const std::string& jsonText, const std::string& sourceName, const std::filesystem::path& contentRoot,
                   CampaignDefinition& outCampaign, std::vector<std::string>& errors,
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
    WarnUnknownKeys(document, {"title", "levels", "testLevel", "bootIntoTestLevel"}, sourceName, warnings);

    std::set<std::string> seenIds;
    ReadLevels(document, sourceName, contentRoot, seenIds, campaign.levels, errors, warnings);
    ReadTestLevel(document, sourceName, contentRoot, seenIds, campaign.testLevel, errors, warnings);
    ReadBootIntoTestLevel(document, sourceName, document.contains("testLevel"), campaign.bootIntoTestLevel, errors);

    if (errors.size() != errorsBefore) {
        return false;
    }
    outCampaign = std::move(campaign);
    return true;
}

bool LoadCampaign(const std::string& filePath, const std::filesystem::path& contentRoot,
                  CampaignDefinition& outCampaign, std::vector<std::string>& errors,
                  std::vector<std::string>& warnings) {
    std::string text;
    if (!ReadTextFile(filePath, "campaign manifest", text, errors)) {
        return false;
    }
    return ParseCampaign(text, filePath, contentRoot, outCampaign, errors, warnings);
}
