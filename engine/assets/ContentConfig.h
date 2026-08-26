#pragma once

#include <string>
#include <vector>

struct WallTextureAssignments {
    std::string wall;
    std::string wallAltA;
    std::string wallAltB;
    std::string door;
    std::string floor;
    std::string ceiling;
};

struct SpriteTextureDefinitions {
    std::string enemy;
    std::string pickup;
    std::string decoration;
};

struct GameplayDefinitions {
    // Enemy
    int enemyHealth = 50;
    int enemyDamage = 12;
    float enemyMoveSpeed = 1.6f;
    float enemyAlertRadius = 6.5f;
    float enemyAttackRadius = 1.1f;
    float enemyAttackRate = 1.0f;

    // Pickups
    int healthPickupValue = 25;
    int maxPlayerHealth = 100;
    int ammoPickupValue = 12;
    int maxPlayerAmmo = 99;

    // Weapon
    int weaponDamage = 35;
    float weaponFireRate = 0.25f;
    float weaponRange = 12.0f;
    int initialAmmo = 24;

    // Player
    int initialHealth = 100;
    float playerMoveSpeed = 3.0f;
    float playerTurnSpeed = 1.8f;
    float playerCollisionRadius = 0.22f;
};

bool LoadWallTextureAssignments(
    const std::string& filePath,
    WallTextureAssignments& outAssignments,
    std::vector<std::string>& errors);

bool LoadSpriteTextureDefinitions(
    const std::string& filePath,
    SpriteTextureDefinitions& outDefinitions,
    std::vector<std::string>& errors);

bool LoadGameplayDefinitions(
    const std::string& filePath,
    GameplayDefinitions& outDefinitions,
    std::vector<std::string>& errors);
