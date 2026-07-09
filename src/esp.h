#pragma once
#include <Windows.h>
#include <string>
#include "overlay.h"
#include "features.h"
#include "sdk_wrapper.h"
#include "etb_math.h"
#include "logger.h"

namespace etb
{
    // ---------- ESP toggles ----------
    inline bool bEsp = true;
    inline bool bEspBoxes = true;
    inline bool bEspLines = true;
    inline bool bEspNames = true;
    inline bool bEspDistance = true;
    inline bool bEspShowEnemies = true;
    inline bool bEspShowPlayers = true;
    inline bool bEspShowItems = true;
    inline float maxEspDistance = 10000.f;

    inline void DrawActorESP(Gdiplus::Graphics* gfx, const CameraInfo& cam, int screenW, int screenH,
        const Vector3& loc, const std::string& label, Gdiplus::Color color)
    {
        Vector3 screen = ProjectWorldToScreenEx(loc, cam, (float)screenW, (float)screenH);
        if (screen.z <= 0.f) return;
        if (screen.x < 0.f || screen.x > screenW || screen.y < 0.f || screen.y > screenH) return;

        const int boxW = 40;
        const int boxH = 60;
        int x = (int)screen.x - boxW / 2;
        int y = (int)screen.y - boxH / 2;

        if (bEspBoxes)
            DrawRect(gfx, x, y, boxW, boxH, color, 1.5f);

        if (bEspLines)
            DrawLine(gfx, screenW / 2, screenH, (int)screen.x, y + boxH, color, 1.0f);

        std::string text;
        if (bEspNames)
            text += label;
        if (bEspDistance)
        {
            if (!text.empty()) text += " ";
            text += std::to_string((int)cam.Location.Distance(loc)) + "m";
        }
        if (!text.empty())
            DrawString(gfx, ToWString(text), x + boxW + 4, y, color, 11);
    }

    // Inner render logic; must not contain __try/__except because it uses C++ objects.
    inline void RenderESP_Inner(Gdiplus::Graphics* gfx, int screenW, int screenH)
    {
        if (!bEsp) return;
        if (!IsWorldReady()) return;

        CameraInfo cam = GetCameraInfo();
        if (cam.FOV < 1.f) return;

        // Snapshot buckets under the entities mutex. The cache thread rewrites
        // these via std::move on a 33ms cadence; iterating them without a
        // snapshot races with that move and crashes the overlay thread.
        std::vector<EnemyEntry> enemies;
        std::vector<PlayerEntry> players;
        std::vector<ItemEntry> items;
        {
            std::lock_guard<std::mutex> lk(g_entitiesMutex);
            if (bEspShowEnemies) enemies = g_enemies;
            if (bEspShowPlayers) players = g_players;
            if (bEspShowItems)   items   = g_items;
        }

        if (bEspShowEnemies)
        {
            for (const auto& e : enemies)
            {
                if (e.Distance > maxEspDistance) continue;
                DrawActorESP(gfx, cam, screenW, screenH, ToInternal(e.Location),
                    "[" + e.Type + "]", Gdiplus::Color(255, 255, 50, 50));
            }
        }

        if (bEspShowPlayers)
        {
            for (const auto& p : players)
            {
                if (p.IsLocal || p.Distance > maxEspDistance) continue;
                DrawActorESP(gfx, cam, screenW, screenH, ToInternal(p.Location),
                    "Player", Gdiplus::Color(255, 50, 150, 255));
            }
        }

        if (bEspShowItems)
        {
            for (const auto& i : items)
            {
                if (i.Distance > maxEspDistance) continue;
                DrawActorESP(gfx, cam, screenW, screenH, ToInternal(i.Location),
                    i.Name, Gdiplus::Color(255, 50, 255, 50));
            }
        }
    }

    struct EspCtx { Gdiplus::Graphics* gfx; int w; int h; };

    inline void RenderESP(Gdiplus::Graphics* gfx, int screenW, int screenH)
    {
        EspCtx ctx{ gfx, screenW, screenH };
        SehGuard("RenderESP",
            [](void* p) {
                auto* c = static_cast<EspCtx*>(p);
                RenderESP_Inner(c->gfx, c->w, c->h);
            },
            &ctx);
    }
}
