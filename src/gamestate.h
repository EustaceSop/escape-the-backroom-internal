#pragma once
#include <Windows.h>
#include <string>
#include "SDK.hpp"
#include "sdk_wrapper.h"

namespace etb
{
    // ---------- Game-state / unlock toggles ----------
    inline bool bMaxPoints = false;
    inline bool bMaxLevel = false;
    inline bool bUnlockHUB = false;
    inline bool bFinishGameAchievement = false;

    inline std::string g_pendingAchievement;

    inline SDK::AMP_PS_C* GetPlayerState()
    {
        SDK::APlayerController* pc = GetPlayerController();
        if (!pc) return nullptr;
        return static_cast<SDK::AMP_PS_C*>(pc->PlayerState);
    }

    inline SDK::UBP_MyGameInstance_C* GetMyGameInstance()
    {
        SDK::UWorld* world = GetWorld();
        if (!world) return nullptr;
        return static_cast<SDK::UBP_MyGameInstance_C*>(world->OwningGameInstance);
    }

    inline SDK::AMP_GameMode_C* GetGameMode()
    {
        SDK::UWorld* world = GetWorld();
        if (!world) return nullptr;
        return static_cast<SDK::AMP_GameMode_C*>(world->AuthorityGameMode);
    }

    inline SDK::AMP_PlayerController_C* GetMyPlayerController()
    {
        SDK::APlayerController* pc = GetPlayerController();
        if (!pc) return nullptr;
        return static_cast<SDK::AMP_PlayerController_C*>(pc);
    }

    inline void MaxPoints()
    {
        SDK::AMP_PS_C* ps = GetPlayerState();
        if (ps) ps->Points = 999999;
    }

    inline void MaxLevel()
    {
        SDK::AMP_PS_C* ps = GetPlayerState();
        if (ps) ps->Level = 999;
    }

    inline void UnlockHUB()
    {
        // Try the player controller client RPC first.
        SDK::AMP_PlayerController_C* pc = GetMyPlayerController();
        if (pc) pc->Unlock_HUB();

        // Also try the game mode if we are host / listen server.
        SDK::AMP_GameMode_C* gm = GetGameMode();
        if (gm) gm->UnlockHUBForAllPlayers();
    }

    inline void UnlockAchievement(const std::string& achievementName)
    {
        SDK::UBP_MyGameInstance_C* gi = GetMyGameInstance();
        SDK::APlayerController* pc = GetPlayerController();
        if (gi && pc)
        {
            std::wstring ws(achievementName.begin(), achievementName.end());
            SDK::FName name = SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(ws.c_str()));
            gi->UnlockAchievement(name, pc);
        }
    }

    inline void FinishGameAchievement()
    {
        SDK::AMP_GameMode_C* gm = GetGameMode();
        if (gm) gm->FinishGameAchievement();
    }

    inline void UnlockAchievementGameMode(const std::string& achievementName)
    {
        SDK::AMP_GameMode_C* gm = GetGameMode();
        if (!gm) return;
        std::wstring ws(achievementName.begin(), achievementName.end());
        SDK::FName name = SDK::UKismetStringLibrary::Conv_StringToName(SDK::FString(ws.c_str()));
        gm->UnlockAchivement(name);
    }

    inline void ProcessGameStateRequests()
    {
        if (bMaxPoints) MaxPoints();
        if (bMaxLevel) MaxLevel();
        if (bUnlockHUB) { UnlockHUB(); bUnlockHUB = false; }
        if (bFinishGameAchievement) { FinishGameAchievement(); bFinishGameAchievement = false; }

        if (!g_pendingAchievement.empty())
        {
            UnlockAchievement(g_pendingAchievement);
            g_pendingAchievement.clear();
        }
    }

    inline void RequestUnlockAchievement(const std::string& name) { g_pendingAchievement = name; }
}
