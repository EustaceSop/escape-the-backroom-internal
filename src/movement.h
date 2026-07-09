#pragma once
#include <Windows.h>
#include "SDK.hpp"
#include "sdk_wrapper.h"
#include "etb_math.h"

namespace etb
{
    // ---------- Movement toggles ----------
    inline bool bNoFallDamage = false;
    inline bool bSuperJump = false;
    inline float superJumpVelocity = 2500.f;

    inline Vector3 g_teleportTarget;   // set to non-zero to trigger teleport next frame
    inline bool g_requestTeleport = false;

    inline void NoFallDamage()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        local->HasFallDamage = false;
    }

    inline void TeleportTo(const Vector3& pos)
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local || !local->RootComponent) return;
        local->RootComponent->RelativeLocation = SDK::FVector(pos.x, pos.y, pos.z);
    }

    inline void SuperJump()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;

        // Method 1: boost base JumpZVelocity on the movement component.
        if (local->CharacterMovement)
            local->CharacterMovement->JumpZVelocity = superJumpVelocity;

        // Method 2: launch straight up (bXYOverride=false keeps horizontal velocity).
        local->LaunchCharacter(SDK::FVector(0.f, 0.f, superJumpVelocity), false, true);
    }

    inline void ProcessMovementRequests()
    {
        if (bNoFallDamage) NoFallDamage();

        if (g_requestTeleport)
        {
            TeleportTo(g_teleportTarget);
            g_requestTeleport = false;
        }
    }

    inline void RequestTeleport(const Vector3& pos)
    {
        g_teleportTarget = pos;
        g_requestTeleport = true;
    }
}
