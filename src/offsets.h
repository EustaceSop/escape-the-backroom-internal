#pragma once
#include <Windows.h>

namespace offsets
{
    // These offsets are ONLY used by the legacy aimbot / bone-reader path.
    // The SDK path (sdk_wrapper.h + features.h) uses Dumper-7 generated offsets automatically.
    // Update these from your current dump before using the aimbot.

    inline uintptr_t uworld = 0x123;
    inline uintptr_t gameinstance = 0x123;
    inline uintptr_t localpawn = 0x123;
    inline uintptr_t localplayer = 0x123;
    inline uintptr_t playercontroller = 0x123;
    inline uintptr_t persistentlevel = 0x123;
    inline uintptr_t actorcount = 0x123;
    inline uintptr_t aactors = 0x123;
    inline uintptr_t componenttoworld = 0x123;
    inline uintptr_t bonearray = 0x123;
    inline uintptr_t currentactormesh = 0x123;
    inline uintptr_t rootcomp = 0x123;
}
