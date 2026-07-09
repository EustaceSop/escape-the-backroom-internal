#pragma once

// Hotkey storage (Windows VK codes; 0 = unbound).
// These are edited by the rendezvous menu and polled by FeatureLoop.
namespace etb
{
    enum class KeyMode : int
    {
        On = 0,     // Checkbox only, no hotkey
        Toggle = 1, // Key press toggles
        Hold = 2    // Key held activates
    };

    // Menu
    inline int g_menuToggleKey = 0x2D; // INSERT
    inline int hkUnhook = 0x23;        // END

    // Player
    inline int hkInfiniteStamina = 0;
    inline int hkSuperSpeed = 0;
    inline int hkNoClip = 0;
    inline int hkGodMode = 0;
    inline int hkFullBright = 0;
    inline KeyMode modeInfiniteStamina = KeyMode::Toggle;
    inline KeyMode modeSuperSpeed = KeyMode::Toggle;
    inline KeyMode modeNoClip = KeyMode::Toggle;
    inline KeyMode modeGodMode = KeyMode::Toggle;
    inline KeyMode modeFullBright = KeyMode::Toggle;

    // ESP
    inline int hkEsp = 0;
    inline int hkEspBoxes = 0;
    inline int hkEspLines = 0;
    inline int hkEspNames = 0;
    inline int hkEspDistance = 0;
    inline int hkEspShowEnemies = 0;
    inline int hkEspShowPlayers = 0;
    inline int hkEspShowItems = 0;
    inline KeyMode modeEsp = KeyMode::Toggle;
    inline KeyMode modeEspBoxes = KeyMode::Toggle;
    inline KeyMode modeEspLines = KeyMode::Toggle;
    inline KeyMode modeEspNames = KeyMode::Toggle;
    inline KeyMode modeEspDistance = KeyMode::Toggle;
    inline KeyMode modeEspShowEnemies = KeyMode::Toggle;
    inline KeyMode modeEspShowPlayers = KeyMode::Toggle;
    inline KeyMode modeEspShowItems = KeyMode::Toggle;

    // Enemies
    inline int hkFreezeEnemies = 0;
    inline int hkDisableEnemyAttacks = 0;
    inline int hkDestroyEnemies = 0;
    inline KeyMode modeFreezeEnemies = KeyMode::Toggle;
    inline KeyMode modeDisableEnemyAttacks = KeyMode::Toggle;
    inline KeyMode modeDestroyEnemies = KeyMode::Toggle;

    // Movement
    inline int hkNoFallDamage = 0;
    inline int hkSuperJump = 0;
    inline KeyMode modeNoFallDamage = KeyMode::Toggle;

    // Game state
    inline int hkMaxPoints = 0;
    inline int hkMaxLevel = 0;
    inline int hkUnlockHUB = 0;
    inline int hkFinishGameAchievement = 0;
    inline KeyMode modeMaxPoints = KeyMode::Toggle;
    inline KeyMode modeMaxLevel = KeyMode::Toggle;

    // Advanced
    inline int hkAutoLoot = 0;
    inline int hkNightVision = 0;
    inline int hkNoPostProcess = 0;
    inline int hkNoSanityEffects = 0;
    inline int hkNoFear = 0;
    inline int hkFlashlightAlwaysOn = 0;
    inline KeyMode modeAutoLoot = KeyMode::Toggle;
    inline KeyMode modeNightVision = KeyMode::Toggle;
    inline KeyMode modeNoPostProcess = KeyMode::Toggle;
    inline KeyMode modeNoSanityEffects = KeyMode::Toggle;
    inline KeyMode modeNoFear = KeyMode::Toggle;
    inline KeyMode modeFlashlightAlwaysOn = KeyMode::Toggle;

    // Items (one-shot actions)
    inline int hkSpawnFlashlight = 0;
    inline int hkSpawnAlmondWater = 0;
    inline int hkSpawnCrowbar = 0;
    inline int hkSpawnKnife = 0;
    inline int hkSpawnGlowstick = 0;
    inline int hkSpawnTicket = 0;

    void CheckHotkeys();
}
