#include "engine/world/EntityPlacementLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/JsonDocument.h"
#include "engine/world/GameplayDefinitions.h"

using nlohmann::json;

namespace {
constexpr std::int64_t kMaxMapCoordinate = 100000;
constexpr const char* kUtf8ByteOrderMark = "\xEF\xBB\xBF";
constexpr std::size_t kUtf8ByteOrderMarkLength = 3;
constexpr std::size_t kCsvRequiredColumns = 3;  // type,x,y

// The caller's message lists for one parse: errors fail the load, warnings do not.
class Diagnostics {
public:
    Diagnostics(std::vector<std::string>& errors, std::vector<std::string>& warnings)
        : errors_(&errors), warnings_(&warnings) {}

    void Error(std::string message) { errors_->push_back(std::move(message)); }
    void Warning(std::string message) { warnings_->push_back(std::move(message)); }

private:
    std::vector<std::string>* errors_;  // borrowed
    std::vector<std::string>* warnings_;
};

std::string Trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
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
    static constexpr std::array<PlacementType, 6> kTypes = {
        PlacementType::Player, PlacementType::Enemy,   PlacementType::Pickup,
        PlacementType::Door,   PlacementType::Trigger, PlacementType::Decoration,
    };
    for (const PlacementType type : kTypes) {
        if (value == PlacementTypeName(type)) {
            outType = type;
            return true;
        }
    }
    return false;
}

bool ParsePickupKind(const std::string& value, PickupType& outKind) {
    if (value == "health") {
        outKind = PickupType::Health;
    } else if (value == "ammo") {
        outKind = PickupType::Ammo;
    } else if (value == "key") {
        outKind = PickupType::Key;
    } else {
        return false;
    }
    return true;
}

bool ParseEnemyVariant(const std::string& value, EnemyVariant& outVariant) {
    for (const EnemyVariant variant : kEnemyVariants) {
        if (value == EnemyVariantName(variant)) {
            outVariant = variant;
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
            diag.Error(where + ": field \"" + key + "\" is required (a number)");
            return false;
        }
        return true;
    }
    if (!it->is_number() || !std::isfinite(it->get<double>())) {
        diag.Error(where + ": field \"" + key + "\" must be a number, got " + DescribeJsonValue(*it));
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
        diag.Error(where + ": field \"" + key + "\" must be an integer from " + std::to_string(minValue) + " to " +
                   std::to_string(maxValue) + ", got " + DescribeJsonValue(*it));
        return false;
    }
    value = static_cast<int>(number);
    present = true;
    return true;
}

// Reads a key colour field ("key" or "lock"). Returns false only on an error.
bool ReadKeyColorField(const json& object, const std::string& at, const char* key, bool required, KeyColor& value,
                       Diagnostics& diag) {
    const auto it = object.find(key);
    if (it == object.end()) {
        if (required) {
            diag.Error(at + ": field \"" + key + "\" is required (\"red\", \"blue\" or \"gold\")");
            return false;
        }
        return true;
    }
    if (!it->is_string() || !ParseKeyColor(it->get<std::string>(), value)) {
        diag.Error(at + ": field \"" + key + "\" must be \"red\", \"blue\" or \"gold\", got " + DescribeJsonValue(*it));
        return false;
    }
    return true;
}

// A field that applies to other placement types only is ignored, with a warning.
void WarnInapplicable(const json& object, const std::string& at, const char* key, const char* appliesTo,
                      Diagnostics& diag) {
    if (object.contains(key)) {
        diag.Warning(at + ": field \"" + key + "\" only applies to " + appliesTo + "; ignored");
    }
}

void WarnUnknownFields(const json& object, const std::string& where, std::initializer_list<const char*> knownFields,
                       Diagnostics& diag) {
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool known = false;
        for (const char* field : knownFields) {
            known = known || it.key() == field;
        }
        if (!known) {
            diag.Warning(where + ": unknown field \"" + it.key() + "\" ignored");
        }
    }
}

bool ReadPlacementType(const json& object, const std::string& where, PlacementType& outType, Diagnostics& diag) {
    const auto typeIt = object.find("type");
    if (typeIt == object.end()) {
        diag.Error(where +
                   ": field \"type\" is required "
                   "(player, enemy, pickup, door, trigger or decoration)");
        return false;
    }
    if (!typeIt->is_string() || !ParseType(typeIt->get<std::string>(), outType)) {
        diag.Error(where +
                   ": field \"type\" must be one of player, enemy, pickup, door, trigger, "
                   "decoration; got " +
                   DescribeJsonValue(*typeIt));
        return false;
    }
    return true;
}

// Door and trigger: "mapX" and "mapY", the map cell the entity controls. Their absence is
// reported only when `reportMissing` (no earlier field of the entity was invalid).
bool ReadCellFields(const json& object, const std::string& at, bool reportMissing, EntityPlacement& out,
                    Diagnostics& diag) {
    if (out.type != PlacementType::Door && out.type != PlacementType::Trigger) {
        WarnInapplicable(object, at, "mapX", "door and trigger", diag);
        WarnInapplicable(object, at, "mapY", "door and trigger", diag);
        return true;
    }
    bool hasX = false;
    bool hasY = false;
    bool ok = ReadInteger(object, "mapX", at, 0, kMaxMapCoordinate, out.mapX, hasX, diag);
    ok = ReadInteger(object, "mapY", at, 0, kMaxMapCoordinate, out.mapY, hasY, diag) && ok;
    if (reportMissing && ok && (!hasX || !hasY)) {
        diag.Error(at + ": fields \"mapX\" and \"mapY\" are required (the map cell this " +
                   PlacementTypeName(out.type) + " controls)");
        ok = false;
    }
    return ok;
}

bool ReadPlayerFields(const json& object, const std::string& at, EntityPlacement& out, Diagnostics& diag) {
    if (out.type != PlacementType::Player) {
        WarnInapplicable(object, at, "angle", "player", diag);
        return true;
    }
    bool present = false;
    return ReadNumber(object, "angle", at, false, out.angleDegrees, present, diag);
}

// "kind" (required), then "key" for a key pickup or the optional "value" for the others.
bool ReadPickupFields(const json& object, const std::string& at, EntityPlacement& out, Diagnostics& diag) {
    if (out.type != PlacementType::Pickup) {
        WarnInapplicable(object, at, "kind", "pickup", diag);
        WarnInapplicable(object, at, "value", "pickup", diag);
        WarnInapplicable(object, at, "key", "pickups of kind \"key\"", diag);
        return true;
    }
    bool ok = true;
    const auto kindIt = object.find("kind");
    const std::string kind = kindIt != object.end() && kindIt->is_string() ? kindIt->get<std::string>() : "";
    if (kindIt == object.end()) {
        diag.Error(at + ": field \"kind\" is required (\"health\", \"ammo\" or \"key\")");
        ok = false;
    } else if (!ParsePickupKind(kind, out.pickupKind)) {
        diag.Error(at + ": field \"kind\" must be \"health\", \"ammo\" or \"key\", got " + DescribeJsonValue(*kindIt));
        ok = false;
    }
    if (kind == "key") {
        ok = ReadKeyColorField(object, at, "key", true, out.key, diag) && ok;
        WarnInapplicable(object, at, "value", "health and ammo pickups", diag);
        return ok;
    }
    WarnInapplicable(object, at, "key", "pickups of kind \"key\"", diag);
    int value = 0;
    bool present = false;
    ok = ReadInteger(object, "value", at, 1, kMaxGameplayStat, value, present, diag) && ok;
    if (present) {
        out.pickupValue = value;
    }
    return ok;
}

bool ReadDoorFields(const json& object, const std::string& at, EntityPlacement& out, Diagnostics& diag) {
    if (out.type != PlacementType::Door) {
        WarnInapplicable(object, at, "lock", "door", diag);
        return true;
    }
    return ReadKeyColorField(object, at, "lock", false, out.lock, diag);
}

// The UTF-8 sequence a lead byte starts: its length (0 if the byte cannot start one: a
// continuation byte, C0/C1 (overlong) or F5-FF) and the allowed range of its second byte, which
// excludes overlong forms (E0 80-9F, F0 80-8F), surrogates (ED A0-BF) and code points above
// U+10FFFF (F4 90 and up).
struct Utf8Lead {
    std::size_t length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
};

Utf8Lead DescribeUtf8Lead(unsigned char lead) {
    if (lead < 0x80) {
        return {1, 0x80, 0xBF};
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        return {2, 0x80, 0xBF};
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        return {3, static_cast<unsigned char>(lead == 0xE0 ? 0xA0 : 0x80),
                static_cast<unsigned char>(lead == 0xED ? 0x9F : 0xBF)};
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        return {4, static_cast<unsigned char>(lead == 0xF0 ? 0x90 : 0x80),
                static_cast<unsigned char>(lead == 0xF4 ? 0x8F : 0xBF)};
    }
    return {};
}

// Whether text is well-formed UTF-8, by the rules of Python's "utf-8" codec, which the DevKit
// uses.
bool IsValidUtf8(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size()) {
        const Utf8Lead lead = DescribeUtf8Lead(static_cast<unsigned char>(text[i]));
        if (lead.length == 0 || text.size() - i < lead.length) {
            return false;
        }
        for (std::size_t k = 1; k < lead.length; ++k) {
            const auto byte = static_cast<unsigned char>(text[i + k]);
            const unsigned char low = k == 1 ? lead.low : 0x80;
            const unsigned char high = k == 1 ? lead.high : 0xBF;
            if (byte < low || byte > high) {
                return false;
            }
        }
        i += lead.length;
    }
    return true;
}

// A trigger's message as the CSV export can hold it: well-formed UTF-8 (so that the JSON twin
// holds the same text), no comma (the separator), no control character (a line break would end
// the row) and no space at either end (cells are trimmed).
bool IsTriggerMessage(const std::string& message) {
    if (message.empty() || message.size() > kMaxTriggerMessageLength || message.front() == ' ' ||
        message.back() == ' ' || !IsValidUtf8(message)) {
        return false;
    }
    return std::none_of(message.begin(), message.end(), [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return c == ',' || byte < 0x20 || byte == 0x7F;
    });
}

// Trigger: "message" (optional), a non-empty string of at most kMaxTriggerMessageLength
// characters (IsTriggerMessage).
bool ReadTriggerFields(const json& object, const std::string& at, EntityPlacement& out, Diagnostics& diag) {
    if (out.type != PlacementType::Trigger) {
        WarnInapplicable(object, at, "message", "trigger", diag);
        return true;
    }
    const auto messageIt = object.find("message");
    if (messageIt == object.end()) {
        return true;
    }
    if (!messageIt->is_string() || !IsTriggerMessage(messageIt->get<std::string>())) {
        diag.Error(at + ": field \"message\" must be a non-empty string of at most " +
                   std::to_string(kMaxTriggerMessageLength) +
                   " characters without a comma, a control character or a space at either end, got " +
                   DescribeJsonValue(*messageIt));
        return false;
    }
    out.message = messageIt->get<std::string>();
    return true;
}

bool ReadEnemyFields(const json& object, const std::string& at, EntityPlacement& out, Diagnostics& diag) {
    if (out.type != PlacementType::Enemy) {
        WarnInapplicable(object, at, "variant", "enemy", diag);
        return true;
    }
    const auto variantIt = object.find("variant");
    if (variantIt == object.end()) {
        return true;
    }
    if (!variantIt->is_string() || !ParseEnemyVariant(variantIt->get<std::string>(), out.variant)) {
        diag.Error(at + ": field \"variant\" must be \"guard\", \"brute\" or \"runner\", got " +
                   DescribeJsonValue(*variantIt));
        return false;
    }
    return true;
}

// Fills `out` from one entity object. `where` names the file and the entity. Fields that
// apply to some types only are ignored (warning) on the other types.
bool ReadPlacement(const json& object, const std::string& where, EntityPlacement& out, Diagnostics& diag) {
    if (!ReadPlacementType(object, where, out.type, diag)) {
        return false;
    }
    const std::string at = where + " (" + PlacementTypeName(out.type) + ")";
    out.source = where;

    bool ok = true;
    bool present = false;
    ok = ReadNumber(object, "x", at, true, out.position.x, present, diag) && ok;
    ok = ReadNumber(object, "y", at, true, out.position.y, present, diag) && ok;
    ok = ReadCellFields(object, at, ok, out, diag) && ok;
    ok = ReadPlayerFields(object, at, out, diag) && ok;
    ok = ReadPickupFields(object, at, out, diag) && ok;
    ok = ReadDoorFields(object, at, out, diag) && ok;
    ok = ReadEnemyFields(object, at, out, diag) && ok;
    ok = ReadTriggerFields(object, at, out, diag) && ok;
    WarnUnknownFields(object, at,
                      {"type", "x", "y", "mapX", "mapY", "angle", "kind", "value", "variant", "key", "lock", "message"},
                      diag);
    return ok;
}

// Fills `out` from one entry of the "lights" array. `where` names the file and the zone. Map
// bounds are checked later, when the level is built (BuildLevelFromPlacements has the map).
bool ReadLightZone(const json& object, const std::string& where, LightZone& out, Diagnostics& diag) {
    out.source = where;
    bool ok = true;
    const auto readCell = [&](const char* key, std::int64_t minValue, int& value) {
        bool present = false;
        if (!ReadInteger(object, key, where, minValue, kMaxMapCoordinate, value, present, diag)) {
            return false;
        }
        if (!present) {
            diag.Error(where + ": field \"" + key + "\" is required (an integer from " + std::to_string(minValue) +
                       " to " + std::to_string(kMaxMapCoordinate) + ")");
            return false;
        }
        return true;
    };
    ok = readCell("x", 0, out.x) && ok;
    ok = readCell("y", 0, out.y) && ok;
    ok = readCell("w", 1, out.width) && ok;
    ok = readCell("h", 1, out.height) && ok;

    const auto levelIt = object.find("level");
    if (levelIt == object.end()) {
        diag.Error(where + ": field \"level\" is required (a number from 0.2 to 1.0)");
        ok = false;
    } else if (!levelIt->is_number() || !std::isfinite(levelIt->get<double>()) ||
               levelIt->get<double>() < kMinLightZoneLevel || levelIt->get<double>() > kMaxLightZoneLevel) {
        diag.Error(where + ": field \"level\" must be a number from 0.2 to 1.0, got " + DescribeJsonValue(*levelIt));
        ok = false;
    } else {
        out.level = static_cast<float>(levelIt->get<double>());
    }

    WarnUnknownFields(object, where, {"x", "y", "w", "h", "level"}, diag);
    return ok;
}

// JSON top level: "level" should be a string; keys other than level, entities and lights are
// ignored (warning).
void WarnTopLevelFields(const json& document, const std::string& sourceName, Diagnostics& diag) {
    const auto levelIt = document.find("level");
    if (levelIt != document.end() && !levelIt->is_string()) {
        diag.Warning(sourceName + ": field \"level\" should be a string (the level id), got " +
                     DescribeJsonValue(*levelIt));
    }
    for (auto it = document.begin(); it != document.end(); ++it) {
        if (it.key() != "level" && it.key() != "entities" && it.key() != "lights") {
            diag.Warning(sourceName + ": unknown top-level field \"" + it.key() + "\" ignored");
        }
    }
}

// JSON "lights" (optional): an array of light zones, applied in order when the level is built.
void ReadLightZones(const json& document, const std::string& sourceName, std::vector<LightZone>& outLights,
                    Diagnostics& diag) {
    const auto lightsIt = document.find("lights");
    if (lightsIt == document.end()) {
        return;
    }
    if (!lightsIt->is_array()) {
        diag.Error(sourceName + ": field \"lights\" must be an array of light zones, got " +
                   DescribeJsonValue(*lightsIt));
        return;
    }
    std::size_t lightIndex = 0;
    for (const json& entry : *lightsIt) {
        const std::string where = sourceName + ": lights[" + std::to_string(lightIndex) + "]";
        ++lightIndex;
        if (!entry.is_object()) {
            diag.Error(where + ": each light zone must be an object, got " + DescribeJsonValue(entry));
            continue;
        }
        LightZone zone{};
        if (ReadLightZone(entry, where, zone, diag)) {
            outLights.push_back(zone);
        }
    }
}

// JSON "entities" (an array): every valid entry is added to outPlacements.
void ReadEntities(const json& entities, const std::string& sourceName, std::vector<EntityPlacement>& outPlacements,
                  Diagnostics& diag) {
    std::size_t index = 0;
    for (const json& entry : entities) {
        const std::string where = sourceName + ": entities[" + std::to_string(index) + "]";
        ++index;
        if (!entry.is_object()) {
            diag.Error(where + ": each entity must be an object, got " + DescribeJsonValue(entry));
            continue;
        }
        EntityPlacement placement{};
        if (ReadPlacement(entry, where, placement, diag)) {
            outPlacements.push_back(placement);
        }
    }
}

// CSV: one cell converted to the JSON value the shared reader expects.
enum class CsvColumnType { Text, Number, Integer };

struct CsvColumn {
    const char* name;
    CsvColumnType type;
};

constexpr std::size_t kCsvColumnCount = 12;
constexpr std::array<CsvColumn, kCsvColumnCount> kCsvColumns = {{
    {"type", CsvColumnType::Text},
    {"x", CsvColumnType::Number},
    {"y", CsvColumnType::Number},
    {"mapX", CsvColumnType::Integer},
    {"mapY", CsvColumnType::Integer},
    {"kind", CsvColumnType::Text},
    {"value", CsvColumnType::Integer},
    {"angle", CsvColumnType::Number},
    {"variant", CsvColumnType::Text},
    {"key", CsvColumnType::Text},
    {"lock", CsvColumnType::Text},
    {"message", CsvColumnType::Text},
}};

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
        diag.Error(where + ": column \"" + column.name + "\" must be a number, got '" + cell + "'");
        return false;
    }
    const long long number = std::strtoll(begin, &end, 10);
    if (end != begin && *end == '\0' && errno == 0) {
        out = static_cast<std::int64_t>(number);
        return true;
    }
    diag.Error(where + ": column \"" + column.name + "\" must be an integer, got '" + cell + "'");
    return false;
}

// The comma-separated cells of one CSV line, each trimmed.
std::vector<std::string> SplitCsvCells(const std::string& line) {
    std::vector<std::string> cells;
    std::size_t cellStart = 0;
    for (;;) {
        const std::size_t comma = line.find(',', cellStart);
        cells.push_back(
            Trim(line.substr(cellStart, comma == std::string::npos ? std::string::npos : comma - cellStart)));
        if (comma == std::string::npos) {
            return cells;
        }
        cellStart = comma + 1;
    }
}

// One CSV entity row: the cells become the JSON object ReadPlacement reads (an empty cell
// means "not given"). A valid entity is added to outPlacements.
void ReadCsvRow(const std::vector<std::string>& cells, const std::string& where,
                std::vector<EntityPlacement>& outPlacements, Diagnostics& diag) {
    if (cells.size() < kCsvRequiredColumns) {
        diag.Error(where + ": expected at least 3 columns (type,x,y), got " + std::to_string(cells.size()));
        return;
    }
    if (cells.size() > kCsvColumnCount) {
        diag.Warning(where + ": " + std::to_string(cells.size() - kCsvColumnCount) +
                     " extra column(s) after \"message\" ignored");
    }

    json object = json::object();
    bool ok = true;
    for (std::size_t i = 0; i < cells.size() && i < kCsvColumnCount; ++i) {
        if (cells[i].empty()) {
            continue;
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

bool HasExtension(const std::string& path, const std::string& extension) {
    return path.size() >= extension.size() && ToLower(path.substr(path.size() - extension.size())) == extension;
}
}  // namespace

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

bool ParseEntityPlacementsJson(const std::string& text, const std::string& sourceName,
                               std::vector<EntityPlacement>& outPlacements, std::vector<LightZone>& outLights,
                               std::vector<std::string>& errors, std::vector<std::string>& warnings) {
    outPlacements.clear();
    outLights.clear();
    const std::size_t errorsBefore = errors.size();
    Diagnostics diag{errors, warnings};

    json document;
    if (!ParseJsonText(text, sourceName, document, errors)) {
        return false;
    }
    if (!document.is_object()) {
        diag.Error(sourceName + ": the top level must be an object with an \"entities\" array, got " +
                   DescribeJsonValue(document));
        return false;
    }

    WarnTopLevelFields(document, sourceName, diag);
    ReadLightZones(document, sourceName, outLights, diag);

    const auto entitiesIt = document.find("entities");
    if (entitiesIt == document.end() || !entitiesIt->is_array()) {
        diag.Error(sourceName + ": field \"entities\" is required and must be an array" +
                   (entitiesIt == document.end() ? std::string() : ", got " + DescribeJsonValue(*entitiesIt)));
        return false;
    }
    ReadEntities(*entitiesIt, sourceName, outPlacements, diag);
    return errors.size() == errorsBefore;
}

bool ParseEntityPlacementsJson(const std::string& text, const std::string& sourceName,
                               std::vector<EntityPlacement>& outPlacements, std::vector<std::string>& errors,
                               std::vector<std::string>& warnings) {
    std::vector<LightZone> ignoredLights;
    return ParseEntityPlacementsJson(text, sourceName, outPlacements, ignoredLights, errors, warnings);
}

bool ParseEntityPlacementsCsv(const std::string& text, const std::string& sourceName,
                              std::vector<EntityPlacement>& outPlacements, std::vector<std::string>& errors,
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

        if (lineNumber == 1 && line.compare(0, kUtf8ByteOrderMarkLength, kUtf8ByteOrderMark) == 0) {
            line.erase(0, kUtf8ByteOrderMarkLength);
        }
        const std::string trimmed = Trim(line);  // also drops the \r of a CRLF file
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const std::string where =
            sourceName + ":" + std::to_string(lineNumber) + " (entity " + std::to_string(entityIndex) + ")";
        ++entityIndex;
        ReadCsvRow(SplitCsvCells(trimmed), where, outPlacements, diag);
    }
    return errors.size() == errorsBefore;
}

bool LoadEntityPlacementsFromFile(const std::string& filePath, std::vector<EntityPlacement>& outPlacements,
                                  std::vector<LightZone>& outLights, std::vector<std::string>& errors,
                                  std::vector<std::string>& warnings) {
    outPlacements.clear();
    outLights.clear();
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
    return isJson ? ParseEntityPlacementsJson(text, filePath, outPlacements, outLights, errors, warnings)
                  : ParseEntityPlacementsCsv(text, filePath, outPlacements, errors, warnings);
}

bool LoadEntityPlacementsFromFile(const std::string& filePath, std::vector<EntityPlacement>& outPlacements,
                                  std::vector<std::string>& errors, std::vector<std::string>& warnings) {
    std::vector<LightZone> ignoredLights;
    return LoadEntityPlacementsFromFile(filePath, outPlacements, ignoredLights, errors, warnings);
}
