#pragma once
#include <Windows.h>
#include <string>
#include "SDK.hpp"
#include "sdk_wrapper.h"
#include "etb_math.h"

namespace etb
{
    // ---------- Advanced toggles ----------
    inline bool bAutoLoot = false;
    inline float autoLootRange = 500.f;

    inline bool bNightVision = false;
    inline bool bNoPostProcess = false;
    inline bool bNoSanityEffects = false;
    inline bool bNoFear = false;
    inline bool bFlashlightAlwaysOn = false;

    inline void AutoLoot()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local || !local->RootComponent) return;

        SDK::FVector playerLoc = local->RootComponent->RelativeLocation;

        // Snapshot under lock to keep iterators valid across cache updates.
        std::vector<ItemEntry> snapshot;
        {
            std::lock_guard<std::mutex> lk(g_entitiesMutex);
            snapshot = g_items;
        }

        for (const auto& item : snapshot)
        {
            if (!item.Actor || item.Distance > autoLootRange) continue;

            // Teleport item to player's feet and focus it.
            if (item.Actor->RootComponent)
                item.Actor->RootComponent->RelativeLocation = playerLoc;

            local->CurrentFocusedItem = static_cast<SDK::ADroppedItem*>(item.Actor);
            local->TryPickup();
        }
    }

    inline void NightVision()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;

        // Turn flashlight on and extend its check length.
        local->IsFlashlightOn = true;
        local->FlashlightCheckLength = 10000.f;

        // Boost brightness by invoking the brightness timeline function.
        local->PlayBrightness();
    }

    inline void NoPostProcess()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;

        local->SetPostProcessing(
            0.f, // Chromatic_Distance
            0.f, // Tracking_Noise_Level
            0.f, // Signal_Distortion_Intensity
            0.f, // Color_Tornado_Intensity
            0.f, // Warp_Belt_Intensity
            0.f, // Screen_Hop_Frequency
            0.f, // Random_Horizontal_Offset_Frequency
            0.f, // Screen_Hop_Intensity
            0.f  // Random_Horizontal_Offset_Strength
        );
    }

    inline void NoSanityEffects()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;

        local->EndSanity();
        local->StopInsanity();
        local->SRV_ResetSanityWarning();
    }

    inline void NoFear()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;

        local->StopFear();
    }

    inline void FlashlightAlwaysOn()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;

        if (!local->IsFlashlightOn)
            local->ToggleFlashlight();
    }

    inline void RunAdvancedFeatures()
    {
        if (bAutoLoot) AutoLoot();
        if (bNightVision) NightVision();
        if (bNoPostProcess) NoPostProcess();
        if (bNoSanityEffects) NoSanityEffects();
        if (bNoFear) NoFear();
        if (bFlashlightAlwaysOn) FlashlightAlwaysOn();
    }
}
