#pragma once
#include <Windows.h>
#include <string>
#include "SDK.hpp"
#include "sdk_wrapper.h"

namespace etb
{
    // ---------- Enemy control toggles ----------
    inline bool bFreezeEnemies = false;
    inline bool bDisableEnemyAttacks = false;
    inline bool bDestroyEnemies = false;

    inline bool bFreezeEnemiesWasActive = false;
    inline bool bDisableEnemyAttacksWasActive = false;

    inline void FreezeEnemy(SDK::AActor* actor, const std::string& type)
    {
        if (!actor) return;

        if (type == "SkinStealer")
        {
            auto* e = static_cast<SDK::ABP_SkinStealer_C*>(actor);
            e->ShouldMove = false;
            e->CanAttack = false;
        }
        else if (type == "Bacteria")
        {
            auto* e = static_cast<SDK::ABacteria_BP_C*>(actor);
            e->ShouldMove = false;
            e->CanAttack = false;
        }
        else if (type == "Hound")
        {
            auto* e = static_cast<SDK::ABP_Hound_C*>(actor);
            e->ShouldMove = false;
            e->CanAttack = false;
        }
        else if (type == "Moth")
        {
            auto* e = static_cast<SDK::ABP_Moth_C*>(actor);
            e->ShouldMove = false;
            e->CanAttack = false;
        }
        else if (type == "RoamingSmiler")
        {
            auto* e = static_cast<SDK::ABP_Roaming_Smiler_C*>(actor);
            e->ShouldMove = false;
            e->CanAttack = false;
        }
        else if (type == "BoneThief")
        {
            auto* e = static_cast<SDK::ABP_BoneThief_C*>(actor);
            e->ShouldTick = false;
        }
    }

    inline void UnfreezeEnemy(SDK::AActor* actor, const std::string& type)
    {
        if (!actor) return;

        if (type == "SkinStealer")
        {
            auto* e = static_cast<SDK::ABP_SkinStealer_C*>(actor);
            e->ShouldMove = true;
            e->CanAttack = true;
        }
        else if (type == "Bacteria")
        {
            auto* e = static_cast<SDK::ABacteria_BP_C*>(actor);
            e->ShouldMove = true;
            e->CanAttack = true;
        }
        else if (type == "Hound")
        {
            auto* e = static_cast<SDK::ABP_Hound_C*>(actor);
            e->ShouldMove = true;
            e->CanAttack = true;
        }
        else if (type == "Moth")
        {
            auto* e = static_cast<SDK::ABP_Moth_C*>(actor);
            e->ShouldMove = true;
            e->CanAttack = true;
        }
        else if (type == "RoamingSmiler")
        {
            auto* e = static_cast<SDK::ABP_Roaming_Smiler_C*>(actor);
            e->ShouldMove = true;
            e->CanAttack = true;
        }
        else if (type == "BoneThief")
        {
            auto* e = static_cast<SDK::ABP_BoneThief_C*>(actor);
            e->ShouldTick = true;
        }
    }

    inline void DisableEnemyAttack(SDK::AActor* actor, const std::string& type)
    {
        if (!actor) return;

        if (type == "SkinStealer")
            static_cast<SDK::ABP_SkinStealer_C*>(actor)->CanAttack = false;
        else if (type == "Bacteria")
            static_cast<SDK::ABacteria_BP_C*>(actor)->CanAttack = false;
        else if (type == "Hound")
            static_cast<SDK::ABP_Hound_C*>(actor)->CanAttack = false;
        else if (type == "Moth")
            static_cast<SDK::ABP_Moth_C*>(actor)->CanAttack = false;
        else if (type == "RoamingSmiler")
            static_cast<SDK::ABP_Roaming_Smiler_C*>(actor)->CanAttack = false;
    }

    inline void EnableEnemyAttack(SDK::AActor* actor, const std::string& type)
    {
        if (!actor) return;

        if (type == "SkinStealer")
            static_cast<SDK::ABP_SkinStealer_C*>(actor)->CanAttack = true;
        else if (type == "Bacteria")
            static_cast<SDK::ABacteria_BP_C*>(actor)->CanAttack = true;
        else if (type == "Hound")
            static_cast<SDK::ABP_Hound_C*>(actor)->CanAttack = true;
        else if (type == "Moth")
            static_cast<SDK::ABP_Moth_C*>(actor)->CanAttack = true;
        else if (type == "RoamingSmiler")
            static_cast<SDK::ABP_Roaming_Smiler_C*>(actor)->CanAttack = true;
    }

    // Guarded per-actor invocation. All enemy operations may deref the actor's
    // vtable via SDK method calls, so any actor that was destroyed between
    // CacheEntities and now must not take the thread down.
    inline void SafeFreeze(SDK::AActor* a, const std::string& t)   { __try { FreezeEnemy(a, t); }        __except (EXCEPTION_EXECUTE_HANDLER) {} }
    inline void SafeUnfreeze(SDK::AActor* a, const std::string& t) { __try { UnfreezeEnemy(a, t); }      __except (EXCEPTION_EXECUTE_HANDLER) {} }
    inline void SafeDisableAtk(SDK::AActor* a, const std::string& t){ __try { DisableEnemyAttack(a, t);} __except (EXCEPTION_EXECUTE_HANDLER) {} }
    inline void SafeEnableAtk(SDK::AActor* a, const std::string& t){ __try { EnableEnemyAttack(a, t); }  __except (EXCEPTION_EXECUTE_HANDLER) {} }
    inline void SafeDestroy(SDK::AActor* a)                        { __try { if (a) a->K2_DestroyActor(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }

    inline void RunEnemyControls()
    {
        // Snapshot under lock so the cache thread's std::move can't invalidate
        // our iterators mid-loop.
        std::vector<EnemyEntry> snapshot;
        {
            std::lock_guard<std::mutex> lk(g_entitiesMutex);
            snapshot = g_enemies;
        }

        if (bFreezeEnemies)
        {
            for (const auto& e : snapshot)
            {
                if (!e.Actor) continue;
                SafeFreeze(e.Actor, e.Type);
            }
            bFreezeEnemiesWasActive = true;
        }
        else if (bFreezeEnemiesWasActive)
        {
            for (const auto& e : snapshot)
            {
                if (!e.Actor) continue;
                SafeUnfreeze(e.Actor, e.Type);
            }
            bFreezeEnemiesWasActive = false;
        }

        if (bDisableEnemyAttacks)
        {
            for (const auto& e : snapshot)
            {
                if (!e.Actor) continue;
                SafeDisableAtk(e.Actor, e.Type);
            }
            bDisableEnemyAttacksWasActive = true;
        }
        else if (bDisableEnemyAttacksWasActive)
        {
            for (const auto& e : snapshot)
            {
                if (!e.Actor) continue;
                SafeEnableAtk(e.Actor, e.Type);
            }
            bDisableEnemyAttacksWasActive = false;
        }

        if (bDestroyEnemies)
        {
            for (const auto& e : snapshot)
            {
                SafeDestroy(e.Actor);
            }
            bDestroyEnemies = false;
        }
    }
}
