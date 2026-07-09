#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include "SDK.hpp"
#include "sdk_wrapper.h"
#include "etb_math.h"
#include "logger.h"
#include "items.h"
#include "enemies.h"
#include "movement.h"
#include "gamestate.h"
#include "advanced.h"
#include "hotkeys.h"

namespace etb
{
    // ---------- Toggles ----------
    inline bool bInfiniteStamina = false;
    inline bool bSuperSpeed = false;
    inline bool bNoClip = false;
    inline bool bNoClipWasActive = false;
    inline bool bGodMode = false;
    inline bool bFullBright = false;

    inline float superSpeedMultiplier = 3.f;
    inline float defaultWalkSpeed = 600.f;
    inline float defaultSprintSpeed = 1200.f;
    inline float noClipSpeed = 2000.f;

    // ---------- Helpers ----------
    inline Vector3 ToInternal(const SDK::FVector& v)
    {
        return Vector3(v.X, v.Y, v.Z);
    }

    inline SDK::FVector ToSDK(const Vector3& v)
    {
        return SDK::FVector(v.x, v.y, v.z);
    }

    // For the root component, RelativeLocation is the world location.
    inline SDK::FVector GetActorWorldLocation(SDK::AActor* actor)
    {
        if (!actor) return SDK::FVector(0, 0, 0);
        SDK::USceneComponent* root = nullptr;
        __try { root = actor->RootComponent; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return SDK::FVector(0, 0, 0); }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(root), sizeof(void*)))
            return SDK::FVector(0, 0, 0);
        __try { return root->RelativeLocation; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return SDK::FVector(0, 0, 0); }
    }

    // ---------- Player mods ----------
    inline void InfiniteStamina(bool active)
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        __try
        {
            if (active)
            {
                if (local->Stamina < local->MaxStamina)
                    local->Stamina = local->MaxStamina;
                local->ShouldUseStamina = false;
            }
            else
            {
                local->ShouldUseStamina = true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void SuperSpeed(bool active)
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        __try
        {
            if (active)
            {
                local->WalkSpeed = defaultWalkSpeed * superSpeedMultiplier;
                local->SprintSpeed = defaultSprintSpeed * superSpeedMultiplier;
            }
            else
            {
                local->WalkSpeed = defaultWalkSpeed;
                local->SprintSpeed = defaultSprintSpeed;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void ResetSpeed()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        __try
        {
            local->WalkSpeed = defaultWalkSpeed;
            local->SprintSpeed = defaultSprintSpeed;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void GodMode()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        __try
        {
            local->IsDead = false;
            local->CanKill = false;
            local->ShouldRagdoll = false;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void FullBright()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        __try { local->IsFlashlightOn = true; }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void ApplyNoClip()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        __try
        {
            if (local->CharacterMovement)
                local->CharacterMovement->MovementMode = SDK::EMovementMode::MOVE_Flying;
            local->SetActorEnableCollision(false);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void UpdateNoClip()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local || !local->CharacterMovement || !local->RootComponent) return;

        // Keep the speed cap as a fallback in case CharacterMovement is still active.
        __try { local->CharacterMovement->MaxFlySpeed = noClipSpeed; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return; }

        // Read camera orientation so noclip flies where the player looks.
        CameraInfo cam = GetCameraInfo();
        if (cam.Location.x == 0.f && cam.Location.y == 0.f && cam.Location.z == 0.f)
            return;

        D3DMATRIX rotMatrix = Matrix(cam.Rotation);
        Vector3 vForward(rotMatrix.m[0][0], rotMatrix.m[0][1], rotMatrix.m[0][2]);
        Vector3 vRight(rotMatrix.m[1][0], rotMatrix.m[1][1], rotMatrix.m[1][2]);
        Vector3 vWorldUp(0.f, 0.f, 1.f);

        Vector3 input(0.f, 0.f, 0.f);
        if (GetAsyncKeyState('W') & 0x8000) input = input + vForward;
        if (GetAsyncKeyState('S') & 0x8000) input = input - vForward;
        if (GetAsyncKeyState('D') & 0x8000) input = input + vRight;
        if (GetAsyncKeyState('A') & 0x8000) input = input - vRight;
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) input = input + vWorldUp;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) input = input - vWorldUp;

        float len = sqrtf(input.x * input.x + input.y * input.y + input.z * input.z);
        if (len > 0.001f)
        {
            input = input * (1.0f / len);
        }
        else
        {
            // No movement keys held: kill engine velocity so we don't drift.
            __try { local->CharacterMovement->Velocity = SDK::FVector(0.f, 0.f, 0.f); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
            return;
        }

        static ULONGLONG lastTick = 0;
        ULONGLONG now = GetTickCount64();
        if (lastTick == 0) lastTick = now;
        float dt = (now - lastTick) / 1000.0f;
        lastTick = now;
        if (dt <= 0.f || dt > 0.1f) dt = 0.016f;

        float speed = noClipSpeed;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) speed *= 2.f;

        SDK::FVector current = GetActorWorldLocation(local);
        Vector3 delta = input * (speed * dt);
        SDK::FVector next(current.X + delta.x, current.Y + delta.y, current.Z + delta.z);

        __try
        {
            local->RootComponent->RelativeLocation = next;
            local->CharacterMovement->Velocity = SDK::FVector(0.f, 0.f, 0.f);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void RemoveNoClip()
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return;
        __try
        {
            if (local->CharacterMovement)
            {
                local->CharacterMovement->MovementMode = SDK::EMovementMode::MOVE_Walking;
                local->CharacterMovement->Velocity = SDK::FVector(0.f, 0.f, 0.f);
            }
            local->SetActorEnableCollision(true);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // Individual feature runners are wrapped in SEH-safe callers below.
    inline void RunFeatures_Inner()
    {
        if (bInfiniteStamina) InfiniteStamina(true); else InfiniteStamina(false);
        if (bSuperSpeed) SuperSpeed(true); else SuperSpeed(false);
        if (bGodMode) GodMode();
        if (bFullBright) FullBright();
        if (bNoClip != bNoClipWasActive)
        {
            if (bNoClip) ApplyNoClip();
            else RemoveNoClip();
            bNoClipWasActive = bNoClip;
        }
        if (bNoClip) UpdateNoClip();

        ProcessItemRequests();
        RunEnemyControls();
        ProcessMovementRequests();
        ProcessGameStateRequests();
        RunAdvancedFeatures();
    }

    // ---------- Feature loop ----------
    inline void RunFeatures()
    {
        // Hotkeys always run so END unhook and menu toggle work even when the
        // world isn't ready. Individual feature hotkeys themselves check the
        // world; see hotkeys.cpp.
        __try { CheckHotkeys(); }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ETB_LOG("CheckHotkeys SEH 0x%08X", GetExceptionCode());
        }

        if (!IsWorldReady()) return;

        __try
        {
            RunFeatures_Inner();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ETB_LOG("RunFeatures_Inner SEH 0x%08X", GetExceptionCode());
        }
    }

    // ---------- Cache loop ----------
    // Two-stage design: first snapshot the raw pointer array under SEH, then
    // do all the C++ heavy lifting (vectors / strings) with the snapshot only.
    // Rationale: __try blocks cannot coexist with C++ objects that need
    // unwinding, so we keep the risky pointer walk in a plain function.
    struct RawActorSnapshot
    {
        SDK::AActor* actors[4096];
        int count;
    };

    inline bool CaptureActorSnapshot(RawActorSnapshot& out)
    {
        out.count = 0;

        SDK::UWorld* world = GetWorld();
        if (!world) return false;

        SDK::ULevel* level = nullptr;
        __try { level = world->PersistentLevel; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(level), sizeof(void*)))
            return false;

        // PersistentLevel->Actors is at 0x98 inside ULevel.
        uintptr_t actorsAddr = reinterpret_cast<uintptr_t>(level) + 0x98;
        if (!IsValidPointer(actorsAddr, sizeof(SDK::TArray<SDK::AActor*>)))
            return false;

        __try
        {
            auto* arr = reinterpret_cast<SDK::TArray<SDK::AActor*>*>(actorsAddr);
            int n = arr->Num();
            if (n <= 0 || n > 65536) return false;
            if (n > 4096) n = 4096;

            for (int i = 0; i < n; ++i)
            {
                SDK::AActor* a = (*arr)[i];
                if (!IsValidPointer(reinterpret_cast<uintptr_t>(a), 8)) continue;
                out.actors[out.count++] = a;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ETB_LOG("CaptureActorSnapshot SEH 0x%08X", GetExceptionCode());
            return false;
        }
        return true;
    }

    // POD result populated by the SEH-safe classify step below. Passing a POD
    // out of a __try-guarded helper lets us keep all C++ container ops (which
    // require unwinding) in the caller, avoiding C2712.
    enum class ActorKind : uint8_t
    {
        None,
        Player,
        SkinStealer,
        Bacteria,
        BoneThief,
        Hound,
        Moth,
        RoamingSmiler,
        Ragdoll,
        DroppedItem,
    };

    struct ClassifyResult
    {
        ActorKind kind = ActorKind::None;
        SDK::FVector loc{};
        float dist = 0.f;
        SDK::USkeletalMeshComponent* mesh = nullptr;
    };

    // Reads the actor's location + mesh under SEH and picks a kind. NO C++
    // container access or std::string here, so the SEH block is legal.
    inline void ClassifyActorRaw(SDK::AActor* actor, SDK::FVector localLoc,
                                 ClassifyResult& out)
    {
        __try
        {
            SDK::USceneComponent* root = actor->RootComponent;
            if (!IsValidPointer(reinterpret_cast<uintptr_t>(root), sizeof(void*)))
                return;
            out.loc = root->RelativeLocation;
            out.dist = localLoc.GetDistanceTo(out.loc);

            if (g_classCache.IsPlayer(actor))
            {
                out.kind = ActorKind::Player;
                out.mesh = static_cast<SDK::ABPCharacter_Demo_C*>(actor)->Mesh;
            }
            else if (g_classCache.IsSkinStealer(actor))
            {
                out.kind = ActorKind::SkinStealer;
                out.mesh = static_cast<SDK::ACharacter*>(actor)->Mesh;
            }
            else if (g_classCache.IsBacteria(actor))
            {
                out.kind = ActorKind::Bacteria;
                out.mesh = static_cast<SDK::ACharacter*>(actor)->Mesh;
            }
            else if (g_classCache.IsBoneThief(actor))
            {
                out.kind = ActorKind::BoneThief;
                out.mesh = static_cast<SDK::ACharacter*>(actor)->Mesh;
            }
            else if (g_classCache.IsHound(actor))
            {
                out.kind = ActorKind::Hound;
                out.mesh = static_cast<SDK::ACharacter*>(actor)->Mesh;
            }
            else if (g_classCache.IsMoth(actor))
            {
                out.kind = ActorKind::Moth;
                out.mesh = static_cast<SDK::ACharacter*>(actor)->Mesh;
            }
            else if (g_classCache.IsRoamingSmiler(actor))
            {
                out.kind = ActorKind::RoamingSmiler;
                out.mesh = static_cast<SDK::ACharacter*>(actor)->Mesh;
            }
            else if (g_classCache.IsRagdoll(actor))
            {
                out.kind = ActorKind::Ragdoll;
            }
            else if (g_classCache.IsDroppedItem(actor))
            {
                out.kind = ActorKind::DroppedItem;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { /* actor destroyed mid-classify */ }
    }

    // Push a classified actor into the correct bucket. Only C++ container ops
    // live here; no SEH, so std::string construction / push_back are fine.
    inline void ClassifyActor(SDK::AActor* actor, SDK::AActor* localActor,
                              SDK::FVector localLoc,
                              std::vector<PlayerEntry>& players,
                              std::vector<EnemyEntry>& enemies,
                              std::vector<ItemEntry>& items)
    {
        if (!actor || actor == localActor) return;

        ClassifyResult r;
        ClassifyActorRaw(actor, localLoc, r);

        switch (r.kind)
        {
        case ActorKind::None:
            return;
        case ActorKind::Player:
            players.push_back({ static_cast<SDK::ABPCharacter_Demo_C*>(actor), r.mesh, r.loc, r.dist, false });
            return;
        case ActorKind::SkinStealer:
            enemies.push_back({ actor, r.mesh, r.loc, r.dist, "SkinStealer" }); return;
        case ActorKind::Bacteria:
            enemies.push_back({ actor, r.mesh, r.loc, r.dist, "Bacteria" }); return;
        case ActorKind::BoneThief:
            enemies.push_back({ actor, r.mesh, r.loc, r.dist, "BoneThief" }); return;
        case ActorKind::Hound:
            enemies.push_back({ actor, r.mesh, r.loc, r.dist, "Hound" }); return;
        case ActorKind::Moth:
            enemies.push_back({ actor, r.mesh, r.loc, r.dist, "Moth" }); return;
        case ActorKind::RoamingSmiler:
            enemies.push_back({ actor, r.mesh, r.loc, r.dist, "RoamingSmiler" }); return;
        case ActorKind::Ragdoll:
            enemies.push_back({ actor, nullptr, r.loc, r.dist, "Ragdoll" }); return;
        case ActorKind::DroppedItem:
            items.push_back({ actor, r.loc, r.dist, "DroppedItem" }); return;
        }
    }

    inline void CacheEntities()
    {
        if (!IsWorldReady()) return;

        g_classCache.Init();

        SDK::AActor* localActor = GetLocalActor();
        SDK::FVector localLoc = GetActorWorldLocation(localActor);

        RawActorSnapshot snap;
        if (!CaptureActorSnapshot(snap)) return;

        std::vector<PlayerEntry> players;
        std::vector<EnemyEntry> enemies;
        std::vector<ItemEntry> items;
        players.reserve(8);
        enemies.reserve(32);
        items.reserve(64);

        for (int i = 0; i < snap.count; ++i)
        {
            ClassifyActor(snap.actors[i], localActor, localLoc, players, enemies, items);
        }

        {
            std::lock_guard<std::mutex> lk(g_entitiesMutex);
            g_players = std::move(players);
            g_enemies = std::move(enemies);
            g_items = std::move(items);
        }
    }
}
