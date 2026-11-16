#include "engine/world/EntityPlacementLoader.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "engine/core/JsonDocument.h"
#include "engine/world/GameplayDefinitions.h"

using nlohmann::json;

namespace {
constexpr std::int64_t kMaxMapCoordinate = 100000;

struct Diagnostics {
    std::vector<std::string>& errors;
    std::vector<std::string>& warnings;
};

std::string Trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string ToLower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool ParseType(const std::string& value, PlacementType& outType) {
    static constexpr PlacementType kTypes[] = {
        PlacementType::Player, PlacementType::Enemy, PlacementType::Pickup,
        PlacementType::Door, PlacementType::Trigger, PlacementType::Decoration,
    };
    for (const PlacementType type : kTypes) {
        if (value == PlacementTypeName(type)) {
            outType = type;
            return true;
        }
    }
    return false;
}

// Reads a required or optional number. Returns false only on an error (wrong type).
bool ReadNumber(const json& object, const char* key, const std::string& where, bool required, float& value,
                bool& present, Diagnostics& diag) {
    present = false;
    const auto it = object.find(key);
    if (it == object.end()) {
        if (required) {
            diag.errors.push_back(where + ": field \"" + key + "\" is required (a number)");
            return false;
        }
        return true;
    }
    if (!it->is_number() || !std::isfinite(it->get<double>())) {
        diag.errors.push_back(where + ": field \"" + key + "\" must be a number, got " + DescribeJsonValue(*it));
        return false;
    }
    value = static_cast<float>(it->get<double>());
    present = true;
    return true;
}

// Reads an optional integer in [minValue, maxValue]. Returns false only on an error.
bool ReadInteger(const json& object, const char* key, const std::string& where, std::int64_t minValue,
                 std::int64_t maxValue, int& value, bool& present, Diagnostics& diag) {
    present = false;
    const auto it = object.find(key);
    if (it == object.end()) {
        return true;
    }
    bool ok = it->is_number_integer();
    std::int64_t number = 0;
    if (ok) {
        if (it->is_number_unsigned()) {
            const std::uint64_t unsignedNumber = it->get<std::uint64_t>();
            ok = unsignedNumber <= static_cast<std::uint64_t>(maxValue);
            number = ok ? static_cast<std::int64_t>(unsignedNumber) : 0;
        } else {
            number = it->get<std::int64_t>();
        }
        ok = ok && number >= minValue && number <= maxValue;
    }
    if (!ok) {
        diag.errors.push_back(where + ": field \"" + key + "\" must be an integer from " + std::to_string(minValue) +
                              " to " + std::to_string(maxValue) + ", got " + DescribeJsonValue(*it));
        return false;
    }
    value = static_cast<int>(number);
    present = true;
    return true;
}

// Fills `out` from one entity object. `where` names the file and the entity.
bool ReadPlacement(const json& object, const std::string& where, EntityPlacement& out, Diagnostics& diag) {
    const auto typeIt = object.find("type");
    if (typeIt == object.end()) {
        diag.errors.push_back(where + ": field \"type\" is required "
                                      "(player, enemy, pickup, door, trigger or decoration)");
        return false;
    }
    if (!typeIt->is_string() || !ParseType(typeIt->get<std::string>(), out.type)) {
        diag.errors.push_back(where + ": field \"type\" must be one of player, enemy, pickup, door, trigger, "
                                      "decoration; got " + DescribeJsonValue(*typeIt));
        return false;
    }
    const std::string typeName = PlacementTypeName(out.type);
    const std::string at = where + " (" + typeName + ")";
    out.source = where;

    bool ok = true;
    bool present = false;
    ok = ReadNumber(object, "x", at, true, out.position.x, present, diag) && ok;
    ok = ReadNumber(object, "y", at, true, out.position.y, present, diag) && ok;

    // Fields that apply to some types only. Given on another type, they are ignored (warning).
    const bool isCellEntity = out.type == PlacementType::Door || out.type == PlacementType::Trigger;
    const auto inapplicable = [&](const char* key, const char* appliesTo) {
        if (object.contains(key)) {
            diag.warnings.push_back(at + ": field \"" + key + "\" only applies to " + appliesTo + "; ignored");
        }
    };

    if (isCellEntity) {
        bool hasX = false;
        bool hasY = false;
        ok = ReadInteger(object, "mapX", at, 0, kMaxMapCoordinate, out.mapX, hasX, diag) && ok;
        ok = ReadInteger(object, "mapY", at, 0, kMaxMapCoordinate, out.mapY, hasY, diag) && ok;
        if (ok && (!hasX || !hasY)) {
            diag.errors.push_back(at + ": fields \"mapX\" and \"mapY\" are required (the map cell this " + typeName +
                                  " controls)");
            ok = false;
        }
    } else {
        inapplicable("mapX", "door and trigger");
        inapplicable("mapY", "door and trigger");
    }

    if (out.type == PlacementType::Player) {
        ok = ReadNumber(object, "angle", at, false, out.angleDegrees, present, diag) && ok;
    } else {
        inapplicable("angle", "player");
    }

    if (out.type == PlacementType::Pickup) {
        const auto kindIt = object.find("kind");
        if (kindIt == object.end()) {
            diag.errors.push_back(at + ": field \"kind\" is required (\"health\" or \"ammo\")");
            ok = false;
        } else if (kindIt->is_string() && kindIt->get<std::string>() == "health") {
            out.pickupKind = PickupType::Health;
        } else if (kindIt->is_string() && kindIt->get<std::string>() == "ammo") {
            out.pickupKind = PickupType::Ammo;
        } else {
            diag.errors.push_back(at + ": field \"kind\" must be \"health\" or \"ammo\", got " +
                                  DescribeJsonValue(*kindIt));
            ok = false;
        }
        int value = 0;
        ok = ReadInteger(object, "value", at, 1, kMaxGameplayStat, value, present, diag) && ok;
        if (present) {
            out.pickupValue = value;
        }
    } else {
        inapplicable("kind", "pickup");
        inapplicable("value", "pickup");
    }

    if (out.type == PlacementType::Enemy) {
        const auto variantIt = object.find("variant");
        if (variantIt != object.end()) {
            if (variantIt->is_string() && !variantIt->get<std::string>().empty()) {
                out.variant = variantIt->get<std::string>();
            } else {
                diag.errors.push_back(at + ": field \"variant\" must be a non-empty string, got " +
                                      DescribeJsonValue(*variantIt));
                ok = false;
            }
        }
    } else {
        inapplicable("variant", "enemy");
    }

    static const char* const kKnownFields[] = {"type", "x", "y", "mapX", "mapY", "angle", "kind", "value", "variant"};
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool known = false;
        for (const char* field : kKnownFields) {
            known = known || it.key() == field;
        }
        if (!known) {
            diag.warnings.push_back(at + ": unknown field \"" + it.key() + "\" ignored");
        }
    }
    return ok;
}

// CSV: one cell converted to the JSON value the shared reader expects.
enum class CsvColumnType { Text, Number, Integer };

struct CsvColumn {
    const char* name;
    CsvColumnType type;
};

constexpr CsvColumn kCsvColumns[] = {
    {"type", CsvColumnType::Text},     {"x", CsvColumnType::Number},     {"y", CsvColumnType::Number},
    {"mapX", CsvColumnType::Integer},  {"mapY", CsvColumnType::Integer}, {"kind", CsvColumnType::Text},
    {"value", CsvColumnType::Integer}, {"angle", CsvColumnType::Number}, {"variant", CsvColumnType::Text},
};
constexpr std::size_t kCsvColumnCount = sizeof(kCsvColumns) / sizeof(kCsvColumns[0]);

bool ConvertCsvCell(const std::string& cell, const CsvColumn& column, const std::string& where, json& out,
                    Diagnostics& diag) {
    if (column.type == CsvColumnType::Text) {
        out = cell;
        return true;
    }
    const char* begin = cell.c_str();
    char* end = nullptr;
    errno = 0;
    if (column.type == CsvColumnType::Number) {
        const double number = std::strtod(begin, &end);
        if (end != begin && *end == '\0' && errno == 0 && std::isfinite(number)) {
            out = number;
            return true;
        }
        diag.errors.push_back(where + ": column \"" + column.name + "\" must be a number, got '" + cell + "'");
        return false;
    }
    const long long number = std::strtoll(begin, &end, 10);
    if (end != begin && *end == '\0' && errno == 0) {
        out = static_cast<std::int64_t>(number);
        return true;
    }
    diag.errors.push_back(where + ": column \"" + column.name + "\" must be an integer, got '" + cell + "'");
    return false;
}

bool HasExtension(const std::string& path, const std::string& extension) {
    return path.size() >= extension.size() &&
           ToLower(path.substr(path.size() - extension.size())) == extension;
}
} // namespace

const char* PlacementTypeName(PlacementType type) {
    switch (type) {
    case PlacementType::Player:
        return "player";
    case PlacementType::Enemy:
        return "enemy";
    case PlacementType::Pickup:
        return "pickup";
    case PlacementType::Door:
        return "door";
    case PlacementType::Trigger:
        return "trigger";
    case PlacementType::Decoration:
        return "decoration";
    }
    return "unknown";
}

bool ParseEntityPlacementsJson(
    const std::string& text,
    const std::string& sourceName,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) {
    outPlacements.clear();
    const std::size_t errorsBefore = errors.size();
    Diagnostics diag{errors, warnings};

    json document;
    if (!ParseJsonText(text, sourceName, document, errors)) {
        return false;
    }
    if (!document.is_object()) {
        errors.push_back(sourceName + ": the top level must be an object with an \"entities\" array, got " +
                         DescribeJsonValue(document));
        return false;
    }

    const auto levelIt = document.find("level");
    if (levelIt != document.end() && !levelIt->is_string()) {
        warnings.push_back(sourceName + ": field \"level\" should be a string (the level id), got " +
                           DescribeJsonValue(*levelIt));
    }
    for (auto it = document.begin(); it != document.end(); ++it) {
        if (it.key() != "level" && it.key() != "entities") {
            warnings.push_back(sourceName + ": unknown top-level field \"" + it.key() + "\" ignored");
        }
    }

    const auto entitiesIt = document.find("entities");
    if (entitiesIt == document.end() || !entitiesIt->is_array()) {
        errors.push_back(sourceName + ": field \"entities\" is required and must be an array" +
                         (entitiesIt == document.end() ? std::string() : ", got " + DescribeJsonValue(*entitiesIt)));
        return false;
    }

    std::size_t index = 0;
    for (const json& entry : *entitiesIt) {
        const std::string where = sourceName + ": entities[" + std::to_string(index) + "]";
        ++index;
        if (!entry.is_object()) {
            errors.push_back(where + ": each entity must be an object, got " + DescribeJsonValue(entry));
            continue;
        }
        EntityPlacement placement{};
        if (ReadPlacement(entry, where, placement, diag)) {
            outPlacements.push_back(placement);
        }
    }
    return errors.size() == errorsBefore;
}

bool ParseEntityPlacementsCsv(
    const std::string& text,
    const std::string& sourceName,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) {
    outPlacements.clear();
    const std::size_t errorsBefore = errors.size();
    Diagnostics diag{errors, warnings};

    std::size_t lineStart = 0;
    int lineNumber = 0;
    int entityIndex = 0;
    while (lineStart <= text.size()) {
        std::size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = text.size();
        }
        std::string line = text.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;
        ++lineNumber;

        if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            line.erase(0, 3);  // UTF-8 byte order mark
        }
        const std::string trimmed = Trim(line);  // also drops the \r of a CRLF file
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const std::string where = sourceName + ":" + std::to_string(lineNumber) + " (entity " +
                                  std::to_string(entityIndex) + ")";
        ++entityIndex;

        std::vector<std::string> cells;
        std::size_t cellStart = 0;
        for (;;) {
            const std::size_t comma = trimmed.find(',', cellStart);
            cells.push_back(Trim(trimmed.substr(cellStart, comma == std::string::npos ? std::string::npos : comma - cellStart)));
            if (comma == std::string::npos) {
                break;
            }
            cellStart = comma + 1;
        }

        if (cells.size() < 3) {
            errors.push_back(where + ": expected at least 3 columns (type,x,y), got " + std::to_string(cells.size()));
            continue;
        }
        if (cells.size() > kCsvColumnCount) {
            warnings.push_back(where + ": " + std::to_string(cells.size() - kCsvColumnCount) +
                               " extra column(s) after \"variant\" ignored");
        }

        json object = json::object();
        bool ok = true;
        for (std::size_t i = 0; i < cells.size() && i < kCsvColumnCount; ++i) {
            if (cells[i].empty()) {
                continue;  // an empty column means "not given"
            }
            json value;
            if (ConvertCsvCell(cells[i], kCsvColumns[i], where, value, diag)) {
                object[kCsvColumns[i].name] = value;
            } else {
                ok = false;
            }
        }
        EntityPlacement placement{};
        if (ok && ReadPlacement(object, where, placement, diag)) {
            outPlacements.push_back(placement);
        }
    }
    return errors.size() == errorsBefore;
}

bool LoadEntityPlacementsFromFile(
    const std::string& filePath,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) {
    outPlacements.clear();
    const bool isJson = HasExtension(filePath, ".json");
    const bool isCsv = HasExtension(filePath, ".csv");
    if (!isJson && !isCsv) {
        errors.push_back(filePath + ": unsupported entity file type (expected .json, or legacy .csv)");
        return false;
    }

    std::string text;
    if (!ReadTextFile(filePath, "entity placement file", text, errors)) {
        return false;
    }
    return isJson ? ParseEntityPlacementsJson(text, filePath, outPlacements, errors, warnings)
                  : ParseEntityPlacementsCsv(text, filePath, outPlacements, errors, warnings);
}
