#pragma once
#include <Windows.h>
#include <cstring>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include "SDK.hpp"
#include "etb_math.h"
#include "read_write.h"
#include "logger.h"

namespace etb
{
    // ---------- SDK init ----------
    // We defer SDK use until GObjects can actually be resolved. On some builds
    // the module scan inside Dumper-7's GObjects wrapper needs the process to
    // have finished loading its .rdata; if we touch it too early we AV.
    inline std::atomic_bool g_sdkReady{ false };

    inline bool TryInitSDKOnce()
    {
        if (g_sdkReady.load()) return true;

        __try
        {
            SDK::FName::InitInternal();
            SDK::UObject::GObjects.GetTypedPtr();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ETB_LOG("TryInitSDKOnce: SEH 0x%08X", GetExceptionCode());
            return false;
        }

        g_sdkReady.store(true);
        ETB_LOG("SDK ready");
        return true;
    }

    // Blocking init used from Entry(). Retries until it succeeds or unhook is
    // requested. Uses etb::g_unhookRequested via forward declaration; if not
    // yet declared at inclusion time, the linker will see it via globals.h.
    inline bool InitSDK()
    {
        ETB_LOG("InitSDK: begin");
        for (int i = 0; i < 500; ++i)
        {
            if (TryInitSDKOnce()) return true;
            Sleep(20);
        }
        ETB_LOG("InitSDK: gave up after retries");
        return false;
    }

    // Direct GWorld read. Avoids linking Engine_functions.cpp.
    inline SDK::UWorld* GetWorld()
    {
        if (!g_sdkReady.load()) return nullptr;

        uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandle(NULL));
        uintptr_t addr = base + SDK::Offsets::GWorld;
        if (!IsValidPointer(addr, sizeof(void*))) return nullptr;

        SDK::UWorld* w = nullptr;
        __try
        {
            w = *reinterpret_cast<SDK::UWorld**>(addr);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(w), sizeof(void*))) return nullptr;
        return w;
    }

    inline SDK::ULocalPlayer* GetLocalPlayer()
    {
        SDK::UWorld* world = GetWorld();
        if (!world) return nullptr;

        SDK::UGameInstance* gi = nullptr;
        __try
        {
            gi = world->OwningGameInstance;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(gi), sizeof(void*))) return nullptr;

        SDK::ULocalPlayer* lp = nullptr;
        __try
        {
            if (gi->LocalPlayers.Num() == 0) return nullptr;
            lp = gi->LocalPlayers[0];
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(lp), sizeof(void*))) return nullptr;
        return lp;
    }

    inline SDK::APlayerController* GetPlayerController()
    {
        SDK::ULocalPlayer* lp = GetLocalPlayer();
        if (!lp) return nullptr;
        SDK::APlayerController* pc = nullptr;
        __try { pc = lp->PlayerController; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(pc), sizeof(void*))) return nullptr;
        return pc;
    }

    inline SDK::APawn* GetLocalPawn()
    {
        SDK::APlayerController* pc = GetPlayerController();
        if (!pc) return nullptr;
        SDK::APawn* pawn = nullptr;
        __try { pawn = pc->AcknowledgedPawn; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(pawn), sizeof(void*))) return nullptr;
        return pawn;
    }

    inline SDK::AActor* GetLocalActor()
    {
        return GetLocalPawn();
    }

    // Walks the actor's Class chain and returns true if it is (or inherits from)
    // `wanted`. Uses safe raw reads via read<>() so no C++ unwinding is needed;
    // this is our replacement for a->IsA(wanted) inside performance-critical or
    // SEH-sensitive code paths.
    //   UObject::Class     is at 0x10
    //   UStruct::SuperStruct is at 0x40
    inline bool ActorIsOrDerivesFrom(SDK::AActor* actor, SDK::UClass* wanted)
    {
        if (!actor || !wanted) return false;
        uintptr_t cls = read<uintptr_t>(reinterpret_cast<uintptr_t>(actor) + 0x10);
        for (int i = 0; i < 32 && cls; ++i)
        {
            if (cls == reinterpret_cast<uintptr_t>(wanted)) return true;
            cls = read<uintptr_t>(cls + 0x40);
        }
        return false;
    }

    // Returns true only when the game is in a stable, playable level state.
    // During level transitions / seamless travel the world and actors arrays
    // can be in flux; touching them is what causes the level-enter crash.
    inline bool IsWorldReady()
    {
        if (!g_sdkReady.load()) return false;

        SDK::UWorld* world = GetWorld();
        if (!world) return false;

        SDK::ULevel* level = nullptr;
        __try { level = world->PersistentLevel; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(level), sizeof(void*))) return false;

        SDK::ULocalPlayer* lp = GetLocalPlayer();
        if (!lp) return false;

        SDK::APlayerController* pc = GetPlayerController();
        if (!pc) return false;

        SDK::APawn* pawn = GetLocalPawn();
        if (!pawn) return false;

        SDK::USceneComponent* rc = nullptr;
        __try { rc = pawn->RootComponent; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        if (!IsValidPointer(reinterpret_cast<uintptr_t>(rc), sizeof(void*))) return false;

        return true;
    }

    // ---------- Entity buckets used by the cache / render loop / enemy control ----------
    struct PlayerEntry
    {
        SDK::ABPCharacter_Demo_C* Character = nullptr;
        SDK::USkeletalMeshComponent* Mesh = nullptr;
        SDK::FVector Location;
        float Distance = 0.f;
        bool IsLocal = false;
    };

    struct EnemyEntry
    {
        SDK::AActor* Actor = nullptr;
        SDK::USkeletalMeshComponent* Mesh = nullptr;
        SDK::FVector Location;
        float Distance = 0.f;
        std::string Type;
    };

    struct ItemEntry
    {
        SDK::AActor* Actor = nullptr;
        SDK::FVector Location;
        float Distance = 0.f;
        std::string Name;
    };

    inline std::vector<PlayerEntry> g_players;
    inline std::vector<EnemyEntry> g_enemies;
    inline std::vector<ItemEntry> g_items;

    // Protects g_players / g_enemies / g_items. The cache thread writes them
    // every ~33ms via std::move; feature loop + ESP overlay both iterate them
    // on other threads. Without this lock, concurrent read-during-move causes
    // AVs (bad iterator, dangling pointer). Callers should hold the lock only
    // long enough to snapshot into a local copy for iteration.
    inline std::mutex g_entitiesMutex;

    // Cache of class pointers so we can identify actors without calling IsA/ProcessEvent.
    // We only re-scan GObjects when at least one entry is still null; once every
    // class is resolved we stop touching GObjects entirely, which drastically
    // reduces the race window against the game thread during level transitions.
    struct ClassCache
    {
        SDK::UClass* BPCharacter_Demo = nullptr;
        SDK::UClass* BP_SkinStealer = nullptr;
        SDK::UClass* Bacteria_BP = nullptr;
        SDK::UClass* BP_BoneThief = nullptr;
        SDK::UClass* BP_Hound = nullptr;
        SDK::UClass* BP_Moth = nullptr;
        SDK::UClass* BP_Roaming_Smiler = nullptr;
        SDK::UClass* BP_Ragdoll = nullptr;
        SDK::UClass* BP_DroppedItem = nullptr;

        bool AllResolved() const
        {
            return BPCharacter_Demo && BP_SkinStealer && Bacteria_BP && BP_BoneThief &&
                   BP_Hound && BP_Moth && BP_Roaming_Smiler && BP_Ragdoll && BP_DroppedItem;
        }

        void Init()
        {
            if (AllResolved()) return;
            if (!g_sdkReady.load()) return;
            // FindClassFast takes const std::string&, so constructing the argument
            // creates a temporary with a destructor. That mixes C++ unwinding with
            // the SEH block, which /EHsc rejects as C2712. Split into a helper
            // that has NO SEH itself; then guard the whole batch via SehGuard
            // (which is a plain function containing only __try/__except).
            SehGuard("ClassCache::Init", +[](void* self){
                auto* c = static_cast<ClassCache*>(self);
                if (!c->BPCharacter_Demo)   c->BPCharacter_Demo   = SDK::UObject::FindClassFast("BPCharacter_Demo_C");
                if (!c->BP_SkinStealer)     c->BP_SkinStealer     = SDK::UObject::FindClassFast("BP_SkinStealer_C");
                if (!c->Bacteria_BP)        c->Bacteria_BP        = SDK::UObject::FindClassFast("Bacteria_BP_C");
                if (!c->BP_BoneThief)       c->BP_BoneThief       = SDK::UObject::FindClassFast("BP_BoneThief_C");
                if (!c->BP_Hound)           c->BP_Hound           = SDK::UObject::FindClassFast("BP_Hound_C");
                if (!c->BP_Moth)            c->BP_Moth            = SDK::UObject::FindClassFast("BP_Moth_C");
                if (!c->BP_Roaming_Smiler)  c->BP_Roaming_Smiler  = SDK::UObject::FindClassFast("BP_Roaming_Smiler_C");
                if (!c->BP_Ragdoll)         c->BP_Ragdoll         = SDK::UObject::FindClassFast("BP_Ragdoll_C");
                if (!c->BP_DroppedItem)     c->BP_DroppedItem     = SDK::UObject::FindClassFast("BP_DroppedItem_C");
            }, this);
        }

        // Use raw class-chain walk instead of a->IsA(); the latter dereferences
        // BaseChain.StructBaseChainArray which is a pointer table that can be
        // torn during level transitions. ActorIsOrDerivesFrom() uses SEH-guarded
        // read<>() and simply walks SuperStruct, matching UE4's fallback path.
        bool IsPlayer(SDK::AActor* a) const { return a && read<uintptr_t>(reinterpret_cast<uintptr_t>(a) + 0x10) == reinterpret_cast<uintptr_t>(BPCharacter_Demo); }
        bool IsSkinStealer(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, BP_SkinStealer); }
        bool IsBacteria(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, Bacteria_BP); }
        bool IsBoneThief(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, BP_BoneThief); }
        bool IsHound(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, BP_Hound); }
        bool IsMoth(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, BP_Moth); }
        bool IsRoamingSmiler(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, BP_Roaming_Smiler); }
        bool IsRagdoll(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, BP_Ragdoll); }
        bool IsDroppedItem(SDK::AActor* a) const { return ActorIsOrDerivesFrom(a, BP_DroppedItem); }
    };

    inline ClassCache g_classCache;

    // Defined after g_classCache so we can reference it directly. Verifies pawn
    // Class chain contains BPCharacter_Demo before doing an unsafe static_cast.
    inline SDK::ABPCharacter_Demo_C* GetLocalCharacter()
    {
        SDK::APawn* pawn = GetLocalPawn();
        if (!pawn) return nullptr;
        if (!ActorIsOrDerivesFrom(pawn, g_classCache.BPCharacter_Demo)) return nullptr;
        return static_cast<SDK::ABPCharacter_Demo_C*>(pawn);
    }

    // Reconstruct the original internal's camera pointer chain using SDK-typed roots.
    inline CameraInfo GetCameraInfo()
    {
        CameraInfo cam;
        SDK::ULocalPlayer* lp = GetLocalPlayer();
        SDK::APawn* pawn = GetLocalPawn();
        if (!lp || !pawn) return cam;

        SDK::USceneComponent* root = nullptr;
        __try { root = pawn->RootComponent; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return cam; }
        if (!root) return cam;

        uintptr_t localPlayerPtr = reinterpret_cast<uintptr_t>(lp);
        uintptr_t rootCompPtr = reinterpret_cast<uintptr_t>(root);

        auto chain69 = read<uintptr_t>(localPlayerPtr + 0xa8);
        if (!chain69) return cam;
        uint64_t chain699 = read<uintptr_t>(chain69 + 8);
        if (!chain699) return cam;

        float pitchRaw = read<float>(chain699 + 0x7F8);
        float yawRaw = read<float>(rootCompPtr + 0x12C);

        cam.Rotation.x = asinf(pitchRaw) * (180.0 / M_PI);
        cam.Rotation.y = (yawRaw < 0.f) ? yawRaw + 360.f : yawRaw;
        cam.Rotation.z = 0.f;

        uint64_t chain = read<uint64_t>(localPlayerPtr + 0x70);
        if (!chain) return cam;
        uint64_t chain1 = read<uint64_t>(chain + 0x98);
        if (!chain1) return cam;
        uint64_t chain2 = read<uint64_t>(chain1 + 0x130);
        if (!chain2) return cam;
        cam.Location = read<Vector3>(chain2 + 0x10);

        float zoom = read<float>(chain699 + 0x590);
        if (zoom != 0.f)
            cam.FOV = 80.0f / (zoom / 1.19f);

        return cam;
    }
}
