#include <Windows.h>
#include <vector>
#include <cstring>

#include "logger.h"
#include "offsets.h"
#include "read_write.h"
#include "etb_math.h"
#include "sdk_wrapper.h"
#include "features.h"
#include "esp.h"
#include "etb_menu.h"
#include "globals.h"

// Globals used by the renderer / aimbot
uintptr_t base;
uintptr_t Uworld;
uintptr_t Gameinstance;
uintptr_t LocalPawn;
uintptr_t Localplayer;
uintptr_t PlayerController;
uintptr_t Persistentlevel;
uintptr_t ActorCount;
uintptr_t AActors;
uintptr_t RootComp;
int localplayerID;
static int hitbox = 66; // 66 head | 65 neck | 7 chest | 2 pelvis
// Defaults are just the initial guess before RefreshScreenSize runs; the real
// values come from the game window's client rect.
int Width = 1920;
int Height = 1080;

// Poll the game window's client rect so aimbot/ProjectWorldToScreen work at
// any resolution instead of assuming 1920x1080. Cached HWND avoids paying the
// FindWindowA syscall every frame.
static void RefreshScreenSize()
{
    static HWND s_cached = nullptr;
    if (!s_cached || !IsWindow(s_cached))
    {
        HWND hwnd = FindWindowA(nullptr, "Escape the Backrooms");
        if (!hwnd) hwnd = FindWindowA(nullptr, "Escape The Backrooms");
        s_cached = hwnd;
    }
    RECT rc{};
    if (s_cached && GetClientRect(s_cached, &rc) && rc.right > 0 && rc.bottom > 0)
    {
        Width  = rc.right - rc.left;
        Height = rc.bottom - rc.top;
        return;
    }
    // Fallback to primary display metrics if the game window isn't there yet.
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (sw > 0 && sh > 0) { Width = sw; Height = sh; }
}

struct FNlEntity
{
    uintptr_t Actor;
    int ID;
    uintptr_t mesh;
};

std::vector<FNlEntity> entityList;

// Fallback legacy cache using the manual pointer walk.
// The SDK cache in features.h is preferred, but this keeps the original aimbot path intact.
void LegacyCache()
{
    while (!etb::g_unhookRequested)
    {
        std::vector<FNlEntity> tmpList;

        base = (uintptr_t)GetModuleHandle(NULL);
        Uworld = read<uintptr_t>(offsets::uworld);
        Gameinstance = read<uintptr_t>(Uworld + offsets::gameinstance);
        Localplayer = read<uintptr_t>(Gameinstance + offsets::localplayer);
        PlayerController = read<uintptr_t>(Localplayer + offsets::playercontroller);
        LocalPawn = read<uintptr_t>(PlayerController + offsets::localpawn);
        Persistentlevel = read<uintptr_t>(Uworld + offsets::persistentlevel);
        ActorCount = read<uintptr_t>(Persistentlevel + offsets::actorcount);
        AActors = read<uintptr_t>(Persistentlevel + offsets::aactors);
        RootComp = read<uintptr_t>(LocalPawn + offsets::rootcomp);

        if (LocalPawn != 0)
        {
            localplayerID = read<int>(LocalPawn + 0x18);
        }

        for (int i = 0; i < (int)ActorCount; i++)
        {
            uintptr_t CurrentActor = read<uintptr_t>(AActors + i * 0x8);
            int curactorid = read<int>(CurrentActor + 0x18);
            if (curactorid == localplayerID || curactorid == localplayerID + 765)
            {
                FNlEntity fnlEntity{};
                fnlEntity.Actor = CurrentActor;
                fnlEntity.mesh = read<uintptr_t>(CurrentActor + 0x280);
                fnlEntity.ID = curactorid;
                tmpList.push_back(fnlEntity);
            }
        }
        entityList = tmpList;
        Sleep(1);
    }
}

FTransform GetBoneIndex(uintptr_t mesh, int index)
{
    uintptr_t bonearray = read<uintptr_t>(mesh + offsets::bonearray);
    if (bonearray == NULL)
    {
        bonearray = read<uintptr_t>(mesh + offsets::bonearray + 0x10);
    }
    return read<FTransform>(bonearray + (index * 0x30));
}

Vector3 GetBoneWithRotation(uintptr_t mesh, int id)
{
    FTransform bone = GetBoneIndex(mesh, id);
    FTransform ComponentToWorld = read<FTransform>(mesh + offsets::componenttoworld);
    D3DMATRIX Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());
    return Vector3(Matrix._41, Matrix._42, Matrix._43);
}

Vector3 ProjectWorldToScreen(Vector3 WorldLocation)
{
    CameraInfo cam = etb::GetCameraInfo();
    return ProjectWorldToScreenEx(WorldLocation, cam, (float)Width, (float)Height);
}

void aimbot(float x, float y)
{
    float ScreenCenterX = Width / 2.0f;
    float ScreenCenterY = Height / 2.0f;
    float AimSpeed = 1.0f;
    float TargetX = 0;
    float TargetY = 0;

    if (x != 0)
    {
        if (x > ScreenCenterX)
        {
            TargetX = -(ScreenCenterX - x);
            TargetX /= AimSpeed;
            if (TargetX + ScreenCenterX > ScreenCenterX * 2) TargetX = 0;
        }

        if (x < ScreenCenterX)
        {
            TargetX = x - ScreenCenterX;
            TargetX /= AimSpeed;
            if (TargetX + ScreenCenterX < 0) TargetX = 0;
        }
    }

    if (y != 0)
    {
        if (y > ScreenCenterY)
        {
            TargetY = -(ScreenCenterY - y);
            TargetY /= AimSpeed;
            if (TargetY + ScreenCenterY > ScreenCenterY * 2) TargetY = 0;
        }

        if (y < ScreenCenterY)
        {
            TargetY = y - ScreenCenterY;
            TargetY /= AimSpeed;
            if (TargetY + ScreenCenterY < 0) TargetY = 0;
        }
    }
    mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(TargetX), static_cast<DWORD>(TargetY), NULL, NULL);
}

void AimAt(uintptr_t entity)
{
    uintptr_t currentactormesh = read<uintptr_t>(entity + offsets::currentactormesh);
    auto rootHead = GetBoneWithRotation(currentactormesh, hitbox);
    Vector3 rootHeadOut = ProjectWorldToScreen(rootHead);

    if (rootHeadOut.x != 0 || rootHeadOut.y != 0)
    {
        aimbot(rootHeadOut.x, rootHeadOut.y);
    }
}

// SDK-based feature loop
void FeatureLoop()
{
    ETB_LOG("FeatureLoop: begin");
    while (!etb::g_unhookRequested)
    {
        RefreshScreenSize();
        etb::RunFeatures();
        Sleep(16);
    }
    ETB_LOG("FeatureLoop: exit");
}

// SDK-based entity cache
void SDKCacheLoop()
{
    ETB_LOG("SDKCacheLoop: begin");
    // Cache runs slower than every 1ms — the actors array only changes on
    // spawn/despawn events. 33ms (~30Hz) is still frequent enough for ESP.
    while (!etb::g_unhookRequested)
    {
        etb::CacheEntities();
        Sleep(33);
    }
    ETB_LOG("SDKCacheLoop: exit");
}

void ESPDrawCallback(Gdiplus::Graphics* gfx, int w, int h)
{
    etb::RenderESP(gfx, w, h);
}

DWORD WINAPI OverlayThread(LPVOID)
{
    etb::OverlayLoop(ESPDrawCallback);
    return 0;
}

// Placeholder offsets are left at 0x123 until the legacy aimbot path is configured.
// Running the legacy cache with bogus offsets reads random memory and crashes the game.
static bool LegacyOffsetsConfigured()
{
    return offsets::uworld != 0x123 &&
           offsets::gameinstance != 0x123 &&
           offsets::localplayer != 0x123 &&
           offsets::playercontroller != 0x123 &&
           offsets::localpawn != 0x123 &&
           offsets::persistentlevel != 0x123 &&
           offsets::actorcount != 0x123 &&
           offsets::aactors != 0x123 &&
           offsets::rootcomp != 0x123;
}

// SEH-safe thread wrapper. Any unhandled SEH escape from a worker loop is
// caught here and logged, preventing it from taking down the host process.
static DWORD WINAPI FeatureLoopSafe(LPVOID)
{
    __try { FeatureLoop(); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ETB_LOG("FeatureLoop top-level SEH 0x%08X", GetExceptionCode());
    }
    return 0;
}

static DWORD WINAPI SDKCacheLoopSafe(LPVOID)
{
    __try { SDKCacheLoop(); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ETB_LOG("SDKCacheLoop top-level SEH 0x%08X", GetExceptionCode());
    }
    return 0;
}

static DWORD WINAPI OverlayThreadSafe(LPVOID p)
{
    __try { OverlayThread(p); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ETB_LOG("OverlayThread top-level SEH 0x%08X", GetExceptionCode());
    }
    return 0;
}

static DWORD WINAPI MenuThreadSafe(LPVOID p)
{
    __try { MenuThread(p); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ETB_LOG("MenuThread top-level SEH 0x%08X", GetExceptionCode());
    }
    return 0;
}

static DWORD WINAPI LegacyCacheSafe(LPVOID)
{
    __try { LegacyCache(); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ETB_LOG("LegacyCache top-level SEH 0x%08X", GetExceptionCode());
    }
    return 0;
}

DWORD WINAPI Entry(HMODULE hMod)
{
    etb::log::banner("Entry");
    ETB_LOG("Entry: hMod=%p", (void*)hMod);

    etb::g_hModule = hMod;

    // Try to init the SDK. If it fails we still bring up the overlay/menu so
    // the user can at least see the DLL loaded — features that need SDK state
    // will simply no-op via IsWorldReady().
    if (!etb::InitSDK())
    {
        ETB_LOG("Entry: InitSDK failed but continuing");
    }

    if (LegacyOffsetsConfigured())
    {
        ETB_LOG("Entry: legacy offsets configured, starting LegacyCache");
        etb::g_hLegacyCache = CreateThread(nullptr, 0, LegacyCacheSafe, nullptr, 0, nullptr);
    }
    else
    {
        ETB_LOG("Entry: legacy offsets not configured (still placeholder), skipping LegacyCache");
    }

    etb::g_hSDKCache    = CreateThread(nullptr, 0, SDKCacheLoopSafe, nullptr, 0, nullptr);
    etb::g_hFeatureLoop = CreateThread(nullptr, 0, FeatureLoopSafe,  nullptr, 0, nullptr);
    etb::g_hOverlay     = CreateThread(nullptr, 0, OverlayThreadSafe, nullptr, 0, nullptr);
    etb::g_hMenu        = CreateThread(nullptr, 0, MenuThreadSafe,   nullptr, 0, nullptr);

    ETB_LOG("Entry: all threads spawned");
    return 0;
}

static DWORD WINAPI EntrySafe(LPVOID p)
{
    __try { Entry(static_cast<HMODULE>(p)); }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ETB_LOG("Entry top-level SEH 0x%08X", GetExceptionCode());
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        // Use a direct thread instead of the broken start-address spoofer.
        CloseHandle(CreateThread(nullptr, 0, EntrySafe, hModule, 0, nullptr));
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
