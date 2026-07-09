#pragma once
#include <Windows.h>
#include <ddraw.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <string>

#include "globals.h"
#include "logger.h"
#include "read_write.h"

namespace etb
{
    inline Gdiplus::GdiplusStartupInput g_gdiInput;
    inline ULONG_PTR g_gdiToken = 0;
    inline HWND g_overlayHwnd = nullptr;
    inline bool g_overlayRunning = false;

    inline bool InitGDIPlus()
    {
        if (g_gdiToken) return true;
        g_gdiInput.GdiplusVersion = 1;
        return Gdiplus::GdiplusStartup(&g_gdiToken, &g_gdiInput, nullptr) == Gdiplus::Ok;
    }

    inline HWND FindGameWindow()
    {
        // Cached across frames because FindWindowA / GetForegroundWindow are
        // syscalls we don't need to pay every ~16ms overlay tick. Invalidate
        // when the OS says the handle is dead.
        static HWND s_cached = nullptr;
        if (s_cached && IsWindow(s_cached)) return s_cached;

        HWND hwnd = FindWindowA(nullptr, "Escape the Backrooms");
        if (!hwnd) hwnd = FindWindowA(nullptr, "Escape The Backrooms");
        if (!hwnd) hwnd = GetForegroundWindow(); // fallback for local testing
        s_cached = hwnd;
        return hwnd;
    }

    inline LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    inline bool CreateOverlayWindow()
    {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "ETBOverlayClass";
        RegisterClassExA(&wc);

        g_overlayHwnd = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            "ETBOverlayClass",
            "ETB Overlay",
            WS_POPUP,
            0, 0, 1920, 1080,
            nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);

        if (!g_overlayHwnd) return false;

        SetLayeredWindowAttributes(g_overlayHwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        ShowWindow(g_overlayHwnd, SW_SHOW);
        return true;
    }

    inline void DrawLine(Gdiplus::Graphics* gfx, int x1, int y1, int x2, int y2, Gdiplus::Color color, float width = 1.0f)
    {
        Gdiplus::Pen pen(color, width);
        gfx->DrawLine(&pen, x1, y1, x2, y2);
    }

    inline void DrawRect(Gdiplus::Graphics* gfx, int x, int y, int w, int h, Gdiplus::Color color, float width = 1.0f)
    {
        Gdiplus::Pen pen(color, width);
        gfx->DrawRectangle(&pen, x, y, w, h);
    }

    inline void DrawFilledRect(Gdiplus::Graphics* gfx, int x, int y, int w, int h, Gdiplus::Color color)
    {
        Gdiplus::SolidBrush brush(color);
        gfx->FillRectangle(&brush, x, y, w, h);
    }

    inline void DrawString(Gdiplus::Graphics* gfx, const std::wstring& text, int x, int y, Gdiplus::Color color, int size = 11)
    {
        Gdiplus::FontFamily family(L"Arial");
        Gdiplus::Font font(&family, (Gdiplus::REAL)size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(color);
        Gdiplus::PointF point((Gdiplus::REAL)x, (Gdiplus::REAL)y);
        gfx->DrawString(text.c_str(), -1, &font, point, &brush);
    }

    inline std::wstring ToWString(const std::string& s)
    {
        if (s.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, result.data(), size);
        return result;
    }

    inline void UpdateOverlayPosition()
    {
        if (!g_overlayHwnd) return;
        HWND game = FindGameWindow();
        if (!game) return;
        RECT rc;
        GetClientRect(game, &rc);
        ClientToScreen(game, reinterpret_cast<POINT*>(&rc.left));
        ClientToScreen(game, reinterpret_cast<POINT*>(&rc.right));
        SetWindowPos(g_overlayHwnd, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE);
    }

    inline void RenderFrame(void (*drawFunc)(Gdiplus::Graphics*, int, int));

    struct OverlayFrameCtx
    {
        Gdiplus::Graphics* gfx;
        int w;
        int h;
        void (*drawFunc)(Gdiplus::Graphics*, int, int);
    };

    inline void OverlayLoop(void (*drawFunc)(Gdiplus::Graphics*, int, int))
    {
        ETB_LOG("OverlayLoop: begin");
        if (!InitGDIPlus()) { ETB_LOG("OverlayLoop: InitGDIPlus failed"); return; }
        if (!CreateOverlayWindow()) { ETB_LOG("OverlayLoop: CreateOverlayWindow failed"); return; }

        g_overlayRunning = true;
        HDC screenDC = GetDC(nullptr);

        while (g_overlayRunning && !g_unhookRequested)
        {
            MSG msg;
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }

            UpdateOverlayPosition();

            RECT rc;
            GetClientRect(g_overlayHwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            if (w <= 0 || h <= 0) { Sleep(16); continue; }

            HDC memDC = CreateCompatibleDC(screenDC);
            HBITMAP bmp = CreateCompatibleBitmap(screenDC, w, h);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

            // Clear to transparent color-key (black)
            RECT fillRc = { 0, 0, w, h };
            FillRect(memDC, &fillRc, (HBRUSH)GetStockObject(BLACK_BRUSH));

            {
                Gdiplus::Graphics gfx(memDC);
                gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                if (drawFunc)
                {
                    OverlayFrameCtx ctx{ &gfx, w, h, drawFunc };
                    SehGuard("OverlayDraw",
                        [](void* p) {
                            auto* c = static_cast<OverlayFrameCtx*>(p);
                            c->drawFunc(c->gfx, c->w, c->h);
                        },
                        &ctx);
                }
            }

            POINT srcPos = { 0, 0 };
            SIZE size = { w, h };
            POINT dstPos = { 0, 0 };
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            HDC winDC = GetDC(g_overlayHwnd);
            if (winDC)
            {
                UpdateLayeredWindow(g_overlayHwnd, winDC, nullptr, &size, memDC, &srcPos, RGB(0, 0, 0), &blend, ULW_COLORKEY | ULW_ALPHA);
                ReleaseDC(g_overlayHwnd, winDC);
            }

            // These must run even if UpdateLayeredWindow failed / hwnd was
            // destroyed mid-frame, otherwise we leak a bitmap + DC every
            // iteration and the process eventually runs out of GDI handles.
            SelectObject(memDC, oldBmp);
            DeleteObject(bmp);
            DeleteDC(memDC);

            Sleep(16); // ~60 fps
        }

        ReleaseDC(nullptr, screenDC);

        // Destroy the window on the same thread that created it. Cross-thread
        // DestroyWindow calls are undefined behavior and were a crash source
        // during unhook.
        if (g_overlayHwnd)
        {
            DestroyWindow(g_overlayHwnd);
            g_overlayHwnd = nullptr;
        }

        Gdiplus::GdiplusShutdown(g_gdiToken);
        g_gdiToken = 0;
        ETB_LOG("OverlayLoop: exit");
    }

    inline void StopOverlay()
    {
        // Only signal the overlay loop to exit. It will destroy its own window
        // from its own thread on the next iteration, which is the only legal
        // way to tear down a HWND.
        g_overlayRunning = false;
    }
}
