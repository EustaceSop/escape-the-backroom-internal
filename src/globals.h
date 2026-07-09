#pragma once
#include <Windows.h>
#include <atomic>

namespace etb
{
    // DLL module handle, set during DLL_PROCESS_ATTACH.
    inline HMODULE g_hModule = nullptr;

    // Set to true to signal all worker threads to exit (used by UNHOOK).
    inline std::atomic_bool g_unhookRequested{ false };

    // Worker thread handles. Kept open so UNHOOK can wait for clean exit.
    inline HANDLE g_hLegacyCache = nullptr;
    inline HANDLE g_hSDKCache = nullptr;
    inline HANDLE g_hFeatureLoop = nullptr;
    inline HANDLE g_hOverlay = nullptr;
    inline HANDLE g_hMenu = nullptr;
}
