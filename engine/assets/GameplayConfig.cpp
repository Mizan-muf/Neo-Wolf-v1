#include "engine/assets/GameplayConfig.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include "engine/core/JsonDocument.h"

using nlohmann::json;

namespace {
// Value ranges. They are documented in docs/CONTENT_DEFINITIONS.md; keep the two in sync.
struct IntRange {
    std::int64_t min;
    std::int64_t max;
};

struct FloatRange {
    double min;
    double max;
    bool minExclusive;
    bool maxExclusive;
};

constexpr std::int64_t kMaxStat = kMaxGameplayStat;
constexpr IntRange kPositiveStat{1, kMaxStat};
constexpr IntRange kNonNegativeStat{0, kMaxStat};
constexpr FloatRange kSpeed{0.0, 20.0, true, false};             // > 0, <= 20
constexpr FloatRange kNonNegativeSpeed{0.0, 20.0, false, false};  // 0 = stands still
constexpr FloatRange kDistance{0.0, 1000.0, false, false};
constexpr FloatRange kPositiveDistance{0.0, 1000.0, true, false};
constexpr FloatRange kPlayerRadius{0.0, 0.5, true, true};  // must fit through a 1-cell corridor
constexpr FloatRange kCooldown{0.0, 60.0, false, false};
constexpr FloatRange kPositiveCooldown{0.0, 60.0, true, false};

std::string FormatNumber(double value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

std::string DescribeRange(const FloatRange& range) {
    return std::string(range.minExclusive ? "> " : ">= ") + FormatNumber(range.min) + " and " +
           (range.maxExclusive ? "< " : "<= ") + FormatNumber(range.max);
}

std::string Join(const std::string& path, const std::string& key) {
    return path.empty() ? key : path + "." + key;
}

// Reads typed, range-checked values from one JSON object and records every problem.
class GameplayReader {
public:
    GameplayReader(const std::string& source, std::vector<std::string>& errors, std::vector<std::string>& warnings)
        : source_(source), errors_(errors), warnings_(warnings) {}

    void Error(const std::string& message) { errors_.push_back(source_ + ": " + message); }
    void Warning(const std::string& message) { warnings_.push_back(source_ + ": " + message); }

    // The object stored under `key`, or nullptr if the key is absent (or, after an error, not
    // an object).
    const json* Section(const json& parent, const std::string& parentPath, const char* key) {
        const auto it = parent.find(key);
        if (it == parent.end()) {
            return nullptr;
        }
        if (!it->is_object()) {
            Error(Join(parentPath, key) + " must be an object, got " + DescribeJsonValue(*it));
            return nullptr;
        }
        return &*it;
    }

    void Int(const json& section, const std::string& path, const char* key, const IntRange& range, int& value) {
        const auto it = section.find(key);
        if (it == section.end()) {
            return;  // optional: keep the default
        }
        const std::string name = Join(path, key);
        const std::string expected = "an integer from " + std::to_string(range.min) + " to " + std::to_string(range.max);
        if (!it->is_number_integer()) {
            Error(name + " must be " + expected + ", got " + DescribeJsonValue(*it));
            return;
        }
        bool inRange = false;
        std::int64_t number = 0;
        if (it->is_number_unsigned()) {
            const std::uint64_t unsignedNumber = it->get<std::uint64_t>();
            inRange = unsignedNumber <= static_cast<std::uint64_t>(range.max);
            number = inRange ? static_cast<std::int64_t>(unsignedNumber) : 0;
        } else {
            number = it->get<std::int64_t>();
            inRange = number <= range.max;
        }
        if (!inRange || number < range.min) {
            Error(name + " must be " + expected + ", got " + DescribeJsonValue(*it));
            return;
        }
        value = static_cast<int>(number);
    }

    void Float(const json& section, const std::string& path, const char* key, const FloatRange& range, float& value) {
        const auto it = section.find(key);
        if (it == section.end()) {
            return;
        }
        const std::string name = Join(path, key);
        const std::string expected = "a number " + DescribeRange(range);
        if (!it->is_number()) {
            Error(name + " must be " + expected + ", got " + DescribeJsonValue(*it));
            return;
        }
        const double number = it->get<double>();
        const bool aboveMin = range.minExclusive ? number > range.min : number >= range.min;
        const bool belowMax = range.maxExclusive ? number < range.max : number <= range.max;
        if (!std::isfinite(number) || !aboveMin || !belowMax) {
            Error(name + " must be " + expected + ", got " + DescribeJsonValue(*it));
            return;
        }
        value = static_cast<float>(number);
    }

    // Warns about keys this version does not read (typos, or keys from a newer schema).
    void WarnUnknownKeys(const json& section, const std::string& path, std::initializer_list<const char*> known) {
        for (auto it = section.begin(); it != section.end(); ++it) {
            bool isKnown = false;
            for (const char* name : known) {
                if (it.key() == name) {
                    isKnown = true;
                    break;
                }
            }
            if (!isKnown) {
                Warning("unknown key " + Join(path, it.key()) + " ignored" + Hint(Join(path, it.key())));
            }
        }
    }

private:
    // Keys of the pre-Phase 4 layout, which moved.
    static std::string Hint(const std::string& name) {
        if (name == "pickups.health.maxPlayerHealth") {
            return " (moved to player.maxHealth)";
        }
        if (name == "pickups.ammo.maxPlayerAmmo") {
            return " (moved to weapon.maxAmmo)";
        }
        return {};
    }

    const std::string& source_;
    std::vector<std::string>& errors_;
    std::vector<std::string>& warnings_;
};

void ReadPlayer(GameplayReader& reader, const json& section, PlayerDefinition& player) {
    const std::string path = "player";
    reader.Int(section, path, "initialHealth", kPositiveStat, player.initialHealth);
    reader.Int(section, path, "maxHealth", kPositiveStat, player.maxHealth);
    reader.Float(section, path, "moveSpeed", kSpeed, player.moveSpeed);
    reader.Float(section, path, "turnSpeed", kSpeed, player.turnSpeed);
    reader.Float(section, path, "collisionRadius", kPlayerRadius, player.collisionRadius);
    reader.WarnUnknownKeys(section, path, {"initialHealth", "maxHealth", "moveSpeed", "turnSpeed", "collisionRadius"});
}

void ReadWeapon(GameplayReader& reader, const json& section, WeaponDefinition& weapon) {
    const std::string path = "weapon";
    reader.Int(section, path, "damage", kPositiveStat, weapon.damage);
    reader.Float(section, path, "fireRate", kCooldown, weapon.fireRate);
    reader.Float(section, path, "range", kPositiveDistance, weapon.range);
    reader.Int(section, path, "initialAmmo", kNonNegativeStat, weapon.initialAmmo);
    reader.Int(section, path, "maxAmmo", kNonNegativeStat, weapon.maxAmmo);
    reader.Float(section, path, "noiseRadius", kDistance, weapon.noiseRadius);
    reader.WarnUnknownKeys(section, path, {"damage", "fireRate", "range", "initialAmmo", "maxAmmo", "noiseRadius"});
}

// One enemy archetype: "enemies.<variant>" (or the legacy "enemy" section).
void ReadEnemy(GameplayReader& reader, const json& section, const std::string& path, EnemyDefinition& enemy) {
    reader.Int(section, path, "health", kPositiveStat, enemy.health);
    reader.Int(section, path, "damage", kNonNegativeStat, enemy.damage);
    reader.Float(section, path, "moveSpeed", kNonNegativeSpeed, enemy.moveSpeed);
    reader.Float(section, path, "alertRadius", kDistance, enemy.alertRadius);
    reader.Float(section, path, "attackRadius", kDistance, enemy.attackRadius);
    reader.Float(section, path, "attackRate", kPositiveCooldown, enemy.attackRate);
    reader.Float(section, path, "reactionTime", kCooldown, enemy.reactionTime);
    reader.Int(section, path, "score", kNonNegativeStat, enemy.score);
    reader.WarnUnknownKeys(section, path,
                           {"health", "damage", "moveSpeed", "alertRadius", "attackRadius", "attackRate",
                            "reactionTime", "score"});
}

void ReadEnemies(GameplayReader& reader, const json& document, EnemyDefinitions& enemies) {
    const json* legacy = reader.Section(document, "", "enemy");
    const json* archetypes = reader.Section(document, "", "enemies");
    if (legacy != nullptr && archetypes != nullptr) {
        reader.Error("use either \"enemies\" or the old \"enemy\" section, not both (\"enemy\" is now "
                     "\"enemies.guard\")");
        return;
    }
    if (legacy != nullptr) {
        // Before Phase 6 there was one enemy type; its section still defines the guard.
        reader.Warning("section \"enemy\" is deprecated: it is read as \"enemies.guard\" (move it there)");
        ReadEnemy(reader, *legacy, "enemy", enemies.guard);
        return;
    }
    if (archetypes == nullptr) {
        return;
    }
    EnemyDefinition* const targets[kEnemyVariantCount] = {&enemies.guard, &enemies.brute, &enemies.runner};
    for (int i = 0; i < kEnemyVariantCount; ++i) {
        const char* name = EnemyVariantName(kEnemyVariants[i]);
        if (const json* section = reader.Section(*archetypes, "enemies", name)) {
            ReadEnemy(reader, *section, std::string("enemies.") + name, *targets[i]);
        }
    }
    reader.WarnUnknownKeys(*archetypes, "enemies", {"guard", "brute", "runner"});
}

void ReadPickup(GameplayReader& reader, const json& pickups, const char* key, PickupDefinition& pickup) {
    const json* section = reader.Section(pickups, "pickups", key);
    if (section == nullptr) {
        return;
    }
    const std::string path = std::string("pickups.") + key;
    reader.Int(*section, path, "value", kPositiveStat, pickup.value);
    reader.WarnUnknownKeys(*section, path, {"value"});
}
} // namespace

bool ParseGameplayDefinitions(
    const std::string& jsonText,
    const std::string& sourceName,
    GameplayDefinitions& outDefinitions,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) {
    const std::size_t errorsBefore = errors.size();

    json document;
    if (!ParseJsonText(jsonText, sourceName, document, errors)) {
        return false;
    }

    GameplayReader reader(sourceName, errors, warnings);
    if (!document.is_object()) {
        reader.Error("the top level must be an object, got " + DescribeJsonValue(document));
        return false;
    }

    GameplayDefinitions definitions{};  // documented defaults for every missing key
    if (const json* player = reader.Section(document, "", "player")) {
        ReadPlayer(reader, *player, definitions.player);
    }
    if (const json* weapon = reader.Section(document, "", "weapon")) {
        ReadWeapon(reader, *weapon, definitions.weapon);
    }
    ReadEnemies(reader, document, definitions.enemies);
    if (const json* pickups = reader.Section(document, "", "pickups")) {
        ReadPickup(reader, *pickups, "health", definitions.pickups.health);
        ReadPickup(reader, *pickups, "ammo", definitions.pickups.ammo);
        reader.WarnUnknownKeys(*pickups, "pickups", {"health", "ammo"});
    }
    reader.WarnUnknownKeys(document, "", {"player", "weapon", "enemy", "enemies", "pickups"});

    if (errors.size() == errorsBefore) {
        // Cross-field rules, checked once every value is known.
        if (definitions.player.initialHealth > definitions.player.maxHealth) {
            reader.Error("player.initialHealth (" + std::to_string(definitions.player.initialHealth) +
                         ") must not exceed player.maxHealth (" + std::to_string(definitions.player.maxHealth) + ")");
        }
        if (definitions.weapon.initialAmmo > definitions.weapon.maxAmmo) {
            reader.Error("weapon.initialAmmo (" + std::to_string(definitions.weapon.initialAmmo) +
                         ") must not exceed weapon.maxAmmo (" + std::to_string(definitions.weapon.maxAmmo) + ")");
        }
    }

    if (errors.size() != errorsBefore) {
        return false;
    }
    outDefinitions = definitions;
    return true;
}

bool LoadGameplayDefinitions(
    const std::string& filePath,
    GameplayDefinitions& outDefinitions,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings) {
    std::string text;
    if (!ReadTextFile(filePath, "gameplay definitions", text, errors)) {
        return false;
    }
    return ParseGameplayDefinitions(text, filePath, outDefinitions, errors, warnings);
}

std::string DescribeGameplayDefinitions(const GameplayDefinitions& definitions) {
    const PlayerDefinition& player = definitions.player;
    const WeaponDefinition& weapon = definitions.weapon;
    char buffer[512] = {};
    std::snprintf(buffer, sizeof(buffer),
                  "player health %d/%d, move %.2f, turn %.2f, radius %.2f; "
                  "weapon damage %d, fire interval %.2f s, range %.1f, ammo %d/%d, noise %.1f; ",
                  player.initialHealth, player.maxHealth, static_cast<double>(player.moveSpeed),
                  static_cast<double>(player.turnSpeed), static_cast<double>(player.collisionRadius),
                  weapon.damage, static_cast<double>(weapon.fireRate), static_cast<double>(weapon.range),
                  weapon.initialAmmo, weapon.maxAmmo, static_cast<double>(weapon.noiseRadius));
    std::string text = buffer;
    for (const EnemyVariant variant : kEnemyVariants) {
        const EnemyDefinition& enemy = definitions.enemies.For(variant);
        std::snprintf(buffer, sizeof(buffer),
                      "%s health %d, damage %d, move %.2f, alert %.1f, attack %.1f every %.2f s, reaction %.2f s, "
                      "score %d; ",
                      EnemyVariantName(variant), enemy.health, enemy.damage, static_cast<double>(enemy.moveSpeed),
                      static_cast<double>(enemy.alertRadius), static_cast<double>(enemy.attackRadius),
                      static_cast<double>(enemy.attackRate), static_cast<double>(enemy.reactionTime), enemy.score);
        text += buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "pickups health +%d, ammo +%d", definitions.pickups.health.value,
                  definitions.pickups.ammo.value);
    return text + buffer;
}
