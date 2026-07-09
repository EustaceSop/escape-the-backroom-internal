#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <string>

// Lightweight logger. Writes to:
//   - OutputDebugString (visible in DebugView / VS Output)
//   - %TEMP%\etb_internal.log (persistent, survives crashes)
//
// Never throws. Never touches the SDK. Safe to call from any thread,
// including inside SEH __except handlers.
namespace etb::log
{
    inline std::mutex& mtx()
    {
        static std::mutex m;
        return m;
    }

    inline const std::string& log_path()
    {
        static std::string p = []() {
            char buf[MAX_PATH] = {};
            DWORD n = GetTempPathA(MAX_PATH, buf);
            std::string s = (n > 0) ? std::string(buf, n) : std::string("C:\\");
            s += "etb_internal.log";
            return s;
        }();
        return p;
    }

    inline void write_line(const char* line)
    {
        OutputDebugStringA(line);
        OutputDebugStringA("\n");

        std::lock_guard<std::mutex> lk(mtx());
        FILE* f = nullptr;
        if (fopen_s(&f, log_path().c_str(), "a") == 0 && f)
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            fprintf(f, "[%02d:%02d:%02d.%03d tid=%lu] %s\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                GetCurrentThreadId(), line);
            fclose(f);
        }
    }

    inline void logf(const char* fmt, ...)
    {
        char buf[1024];
        va_list ap;
        va_start(ap, fmt);
        _vsnprintf_s(buf, _TRUNCATE, fmt, ap);
        va_end(ap);
        write_line(buf);
    }

    inline void banner(const char* tag)
    {
        char buf[128];
        _snprintf_s(buf, _TRUNCATE, "==== %s ====", tag);
        write_line(buf);
    }
}

#define ETB_LOG(...) ::etb::log::logf(__VA_ARGS__)
