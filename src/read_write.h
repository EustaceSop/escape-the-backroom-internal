#pragma once
#include <Windows.h>
#include "logger.h"

// Use VirtualQuery for a cheap, non-faulting validity check.
// IsBadWritePtr is deprecated and can deadlock or give false positives in games.
inline bool IsValidPointer(uintptr_t address, SIZE_T size = 8)
{
    if (!address) return false;
    // Reject obviously bogus low pointers (e.g. offset placeholders like 0x123).
    if (address < 0x10000) return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))) return false;

    // Make sure the whole range is committed/readable.
    uintptr_t end = address + size - 1;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    if (end >= regionEnd)
    {
        MEMORY_BASIC_INFORMATION mbi2{};
        if (!VirtualQuery((LPCVOID)end, &mbi2, sizeof(mbi2))) return false;
        if (mbi2.State != MEM_COMMIT) return false;
        if (mbi2.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
        if (!(mbi2.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY))) return false;
    }
    return true;
}

template<typename ReadT>
ReadT read(uintptr_t address, const ReadT& def = ReadT())
{
    if (IsValidPointer(address, sizeof(ReadT)))
    {
        __try
        {
            return *(ReadT*)address;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return def;
        }
    }
    return def;
}

template<typename WriteT>
bool write(uintptr_t address, WriteT value)
{
    if (IsValidPointer(address, sizeof(WriteT)))
    {
        __try
        {
            *(WriteT*)((PBYTE)address) = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }
    return false;
}

// SEH-guarded caller for a plain function pointer. Use for wrapping any block of
// risky code where you can accept the "either finished or logged and skipped"
// contract. Because SEH __try/__except cannot live in functions with C++ object
// unwinding, we take a raw function pointer + user context and forward the call.
//
// Typical usage from a caller that already has locals:
//
//   auto body = []() { /* no captures required */ risky(); };
//   SehGuard("tag", +[](void*){ risky(); }, nullptr);
//
// For scoped blocks with local access it's cleaner to just inline __try/__except
// in a dedicated helper function.
typedef void(*SehFn)(void* ctx);

inline bool SehGuard(const char* tag, SehFn fn, void* ctx)
{
    __try
    {
        fn(ctx);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ETB_LOG("SehGuard(%s): SEH caught 0x%08X", tag, GetExceptionCode());
        return false;
    }
}
