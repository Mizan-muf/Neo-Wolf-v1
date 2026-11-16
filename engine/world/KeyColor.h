#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Key cards and locked doors (Roadmap Phase 6). A door placement may name a lock colour; a
// pickup of kind "key" gives the player that colour's key. Keys belong to the level: they are
// cleared whenever a level is loaded or restarted (docs/GAMEPLAY.md "Keys and locked doors").
enum class KeyColor : std::uint8_t {
    None = 0,  // an unlocked door / not a key
    Red,
    Blue,
    Gold
};

inline constexpr int kKeyColorCount = 3;  // Red, Blue, Gold
inline constexpr KeyColor kKeyColors[kKeyColorCount] = {KeyColor::Red, KeyColor::Blue, KeyColor::Gold};

// "red", "blue", "gold" (the names used in content files); "none" for KeyColor::None.
inline const char* KeyColorName(KeyColor color) {
    switch (color) {
    case KeyColor::Red:
        return "red";
    case KeyColor::Blue:
        return "blue";
    case KeyColor::Gold:
        return "gold";
    case KeyColor::None:
        break;
    }
    return "none";
}

// "RED", "BLUE", "GOLD" (HUD messages such as "NEED RED KEY"); "" for KeyColor::None.
inline const char* KeyColorLabel(KeyColor color) {
    switch (color) {
    case KeyColor::Red:
        return "RED";
    case KeyColor::Blue:
        return "BLUE";
    case KeyColor::Gold:
        return "GOLD";
    case KeyColor::None:
        break;
    }
    return "";
}

// Parses "red", "blue" or "gold"; false for anything else (including "none").
inline bool ParseKeyColor(const std::string& name, KeyColor& out) {
    for (const KeyColor color : kKeyColors) {
        if (name == KeyColorName(color)) {
            out = color;
            return true;
        }
    }
    return false;
}

// 0 for Red, 1 for Blue, 2 for Gold; -1 for None.
inline int KeyColorIndex(KeyColor color) {
    return color == KeyColor::None ? -1 : static_cast<int>(color) - 1;
}

// The keys the player holds in the current level.
struct KeyRing {
    std::array<bool, kKeyColorCount> held{};

    bool Has(KeyColor color) const {
        const int index = KeyColorIndex(color);
        return index >= 0 && held[static_cast<std::size_t>(index)];
    }
    void Add(KeyColor color) {
        const int index = KeyColorIndex(color);
        if (index >= 0) {
            held[static_cast<std::size_t>(index)] = true;
        }
    }
    void Clear() { held = {}; }
};
