#include <Windows.h>
#include "hotkeys.h"
#include "globals.h"
#include "etb_menu.h"
#include "features.h"
#include "esp.h"
#include "items.h"
#include "enemies.h"
#include "movement.h"
#include "gamestate.h"
#include "advanced.h"
#include "sdk_wrapper.h"

namespace etb
{
    static bool HotkeyJustPressedImpl(int vk)
    {
        static bool prev[256] = {};
        if (vk <= 0 || vk >= 256) return false;
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool result = down && !prev[vk];
        prev[vk] = down;
        return result;
    }

    static bool IsKeyHeld(int vk)
    {
        return vk > 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    static void UpdateFeature(bool& enabled, KeyMode mode, int vk)
    {
        switch (mode)
        {
        case KeyMode::On:
            // Checkbox is the only control; do not touch |enabled| here.
            break;
        case KeyMode::Hold:
            enabled = IsKeyHeld(vk);
            break;
        case KeyMode::Toggle:
            if (HotkeyJustPressedImpl(vk))
                enabled = !enabled;
            break;
        }
    }

    void CheckHotkeys()
    {
        // Don't fire gameplay hotkeys while the user is binding keys in the menu.
        // AlwaysOn/Hold states are sticky from previous frames, so returning here
        // only prevents new toggles/one-shots from firing while binding.
        if (g_menuVisible) return;

        // UNHOOK — always available, even without a world.
        if (HotkeyJustPressedImpl(hkUnhook))
        {
            RequestUnhook();
            return;
        }

        // Everything below manipulates game objects. If the world isn't ready
        // yet (main menu, level transition), only allow the pure-toggle state
        // flips so hotkeys still respond, but skip one-shot actions that would
        // crash without a valid pawn.
        const bool worldOk = IsWorldReady();

        // Player
        UpdateFeature(bInfiniteStamina, modeInfiniteStamina, hkInfiniteStamina);
        UpdateFeature(bSuperSpeed, modeSuperSpeed, hkSuperSpeed);
        UpdateFeature(bNoClip, modeNoClip, hkNoClip);
        UpdateFeature(bGodMode, modeGodMode, hkGodMode);
        UpdateFeature(bFullBright, modeFullBright, hkFullBright);

        // ESP
        UpdateFeature(bEsp, modeEsp, hkEsp);
        UpdateFeature(bEspBoxes, modeEspBoxes, hkEspBoxes);
        UpdateFeature(bEspLines, modeEspLines, hkEspLines);
        UpdateFeature(bEspNames, modeEspNames, hkEspNames);
        UpdateFeature(bEspDistance, modeEspDistance, hkEspDistance);
        UpdateFeature(bEspShowEnemies, modeEspShowEnemies, hkEspShowEnemies);
        UpdateFeature(bEspShowPlayers, modeEspShowPlayers, hkEspShowPlayers);
        UpdateFeature(bEspShowItems, modeEspShowItems, hkEspShowItems);

        // Enemies
        UpdateFeature(bFreezeEnemies, modeFreezeEnemies, hkFreezeEnemies);
        UpdateFeature(bDisableEnemyAttacks, modeDisableEnemyAttacks, hkDisableEnemyAttacks);
        UpdateFeature(bDestroyEnemies, modeDestroyEnemies, hkDestroyEnemies);

        // Movement
        UpdateFeature(bNoFallDamage, modeNoFallDamage, hkNoFallDamage);
        if (worldOk && HotkeyJustPressedImpl(hkSuperJump)) { SuperJump(); }

        // Game state (one-shot or toggle)
        UpdateFeature(bMaxPoints, modeMaxPoints, hkMaxPoints);
        UpdateFeature(bMaxLevel, modeMaxLevel, hkMaxLevel);
        if (worldOk && HotkeyJustPressedImpl(hkUnlockHUB)) { UnlockHUB(); }
        if (worldOk && HotkeyJustPressedImpl(hkFinishGameAchievement)) { FinishGameAchievement(); }

        // Advanced
        UpdateFeature(bAutoLoot, modeAutoLoot, hkAutoLoot);
        UpdateFeature(bNightVision, modeNightVision, hkNightVision);
        UpdateFeature(bNoPostProcess, modeNoPostProcess, hkNoPostProcess);
        UpdateFeature(bNoSanityEffects, modeNoSanityEffects, hkNoSanityEffects);
        UpdateFeature(bNoFear, modeNoFear, hkNoFear);
        UpdateFeature(bFlashlightAlwaysOn, modeFlashlightAlwaysOn, hkFlashlightAlwaysOn);

        // Items (one-shot). Spawn requests go through the pending queue, but we
        // still gate on the world so we don't stack up spawn requests during
        // the main menu.
        if (worldOk)
        {
            if (HotkeyJustPressedImpl(hkSpawnFlashlight)) RequestSpawn("flashlight");
            if (HotkeyJustPressedImpl(hkSpawnAlmondWater)) RequestSpawn("almondwater");
            if (HotkeyJustPressedImpl(hkSpawnCrowbar)) RequestSpawn("crowbar");
            if (HotkeyJustPressedImpl(hkSpawnKnife)) RequestSpawn("knife");
            if (HotkeyJustPressedImpl(hkSpawnGlowstick)) RequestSpawn("glowstick");
            if (HotkeyJustPressedImpl(hkSpawnTicket)) RequestSpawn("ticket");
        }
    }
}
