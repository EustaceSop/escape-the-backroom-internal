#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <atomic>
#include <thread>

#include "gui/element.hpp"
#include "gui/gui.hpp"
#include "gui/builders.hpp"
#include "gui/comp/button.hpp"
#include "gui/comp/text.hpp"
#include "gui/comp/checkbox.hpp"
#include "gui/comp/key_bind.hpp"
#include "gui/comp/panel.hpp"
#include "render/impl/dx11.hpp"
#include "input/win32.hpp"
#include "input/key_names.hpp"

#include "etb_menu.h"
#include "globals.h"
#include "features.h"
#include "esp.h"
#include "overlay.h"
#include "items.h"
#include "enemies.h"
#include "movement.h"
#include "gamestate.h"
#include "advanced.h"
#include "hotkeys.h"
#include "sdk_wrapper.h"
#include "read_write.h"
#include "logger.h"

namespace
{
    std::atomic_bool g_menuRunning{ true };
    rv::vector_2d<float> g_screenSize = { 520.f, 800.f };
    shared_ptr_t<rv::win32_input> g_input;

    rv::element* g_titleBar = nullptr;
    rv::key_bind* g_menuToggleKb = nullptr;
    bool g_titleDragging = false;
    POINT g_titleDragStartMouse = {};
    RECT g_titleDragStartWindow = {};

    inline bool MenuToggleJustPressed(int vk)
    {
        static bool prev = false;
        if (vk <= 0 || vk >= 256) return false;
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool result = down && !prev;
        prev = down;
        return result;
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if (msg == WM_SIZE && wparam != SIZE_MINIMIZED)
        {
            g_screenSize.x = static_cast<float>(LOWORD(lparam));
            g_screenSize.y = static_cast<float>(HIWORD(lparam));
        }

        if (g_input)
            g_input->handle_message(hwnd, msg, wparam, lparam);

        if (msg == WM_CLOSE)
        {
            etb::g_menuVisible = false;
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    // Helper to bind a key_bind widget to one of our int VK storage variables.
    rv::key_bind& BindHotkey(rv::key_bind& kb, int& vk)
    {
        return kb.bind(reinterpret_cast<rv::key*>(&vk));
    }

    // Helper: checkbox + mode combo + hotkey in one row.
    void AddToggle(rv::element& section, const char* label, bool& value, int& vk, etb::KeyMode& mode)
    {
        auto& row = section.add_row();
        row.gap(8.f).align(rv::alignment::center);
        auto& cb = row.add_checkbox(label);
        cb.bind(&value);
        auto& filler = row.add_container();
        filler.flex_grow(1.f);

        auto& combo = row.add_combo_box({ "On", "Toggle", "Hold" });
        combo.bind(reinterpret_cast<int*>(&mode))
             .set_declared_size({ rv::styled_size::px(70.f), rv::styled_size::px(26.f) });

        auto& kb = row.add_key_bind();
        BindHotkey(kb, vk).set_declared_size({ rv::styled_size::px(70.f), rv::styled_size::px(26.f) });
    }

    // Helper: button + hotkey for one-shot item spawn.
    void AddSpawnButton(rv::element& section, const char* label, const char* itemName, int& vk)
    {
        auto& row = section.add_row();
        row.gap(8.f).align(rv::alignment::center);
        auto& btn = row.add_button(label);
        btn.on_click([itemName]() { etb::RequestSpawn(itemName); })
           .background_color({ 0.18f, 0.22f, 0.30f, 1.f })
           .rounding(6.f);
        auto& filler = row.add_container();
        filler.flex_grow(1.f);
        auto& kb = row.add_key_bind();
        BindHotkey(kb, vk).set_declared_size({ rv::styled_size::px(90.f), rv::styled_size::px(26.f) });
    }
}

DWORD WINAPI MenuThread(LPVOID)
{
    ETB_LOG("MenuThread: begin");

    // Wait until the game has finished bringing up its own D3D device / world.
    // Creating our own D3D11 device concurrently with the game's initial DXGI
    // handshake was one of the main crash sources; this defers our device
    // creation until the process is quiet.
    for (int i = 0; i < 600; ++i) // up to ~30 seconds
    {
        if (etb::g_unhookRequested) { ETB_LOG("MenuThread: unhook before init"); return 0; }
        if (etb::IsWorldReady()) break;
        Sleep(50);
    }
    ETB_LOG("MenuThread: world ready, creating window");

    // Small additional grace period so DXGI has a clean moment.
    Sleep(500);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"etb_rendezvous_menu";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    RegisterClassExW(&wc);

    const DWORD style = WS_POPUP;
    const DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, L"ETB Internal",
                                style, CW_USEDEFAULT, CW_USEDEFAULT,
                                static_cast<int>(g_screenSize.x),
                                static_cast<int>(g_screenSize.y),
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { ETB_LOG("MenuThread: CreateWindow failed"); return 1; }

    // D3D11 setup
    rv::dx11_object<IDXGISwapChain> swapChain;
    rv::dx11_object<ID3D11Device> device;
    rv::dx11_object<ID3D11DeviceContext> context;
    rv::dx11_object<ID3D11RenderTargetView> rtv;

    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount = 2;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = hwnd;
    scDesc.SampleDesc.Count = 1;
    scDesc.Windowed = TRUE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               nullptr, 0, D3D11_SDK_VERSION,
                                               &scDesc, swapChain.release_and_get(),
                                               device.release_and_get(), nullptr,
                                               context.release_and_get());
    if (hr != S_OK)
    {
        ETB_LOG("MenuThread: D3D11CreateDeviceAndSwapChain failed 0x%08X", hr);
        DestroyWindow(hwnd);
        return 1;
    }

    auto createRtv = [&]()
    {
        rv::dx11_object<ID3D11Texture2D> backBuffer;
        swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.release_and_get()));
        device->CreateRenderTargetView(backBuffer.value(), nullptr, rtv.release_and_get());
    };
    createRtv();

    auto renderer = cstd::make_shared<rv::dx11_renderer>(device.value(), context.value());
    if (!renderer->init())
    {
        ETB_LOG("MenuThread: renderer init failed");
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    g_input = cstd::make_shared<rv::win32_input>();

    auto guiRenderer = cstd::make_unique<rv::gui_renderer_impl>(renderer);
    auto gui = rv::make_gui(cstd::move(guiRenderer), g_input);

    gui->default_style().gap = 8.f;
    gui->default_style().direction = rv::layout_direction::vertical;

    // Load font
    optional_t<rv::font> font = renderer->add_font("C:\\Windows\\Fonts\\arial.ttf", 18.f);
    if (!font)
    {
        ETB_LOG("MenuThread: font load failed");
        renderer.reset();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    auto guiFont = cstd::make_shared<rv::gui_font_impl>(*font);
    gui->set_font(guiFont);

    auto root = gui->root();
    root->direction(rv::layout_direction::vertical).gap(0.f);

    // Title bar
    {
        auto& header = root->add_container();
        header.set_declared_size({ rv::styled_size::fill(), rv::styled_size::px(44.f) });
        header.direction(rv::layout_direction::horizontal).padding({ 0.f, 16.f, 0.f, 16.f })
            .align(rv::alignment::center)
            .background_color({ 0.10f, 0.10f, 0.12f, 1.f })
            .border_width({ 0.f, 0.f, 1.f, 0.f }).border_color({ 0.25f, 0.25f, 0.30f, 1.f });

        auto& title = header.add_label("Escape The Backrooms - Internal");
        title.text_size(18.f).text_color({ 0.40f, 0.70f, 1.00f, 1.f });

        auto& filler = header.add_container();
        filler.flex_grow(1.f);

        auto& kb = header.add_key_bind();
        BindHotkey(kb, etb::g_menuToggleKey).set_declared_size({ rv::styled_size::px(90.f), rv::styled_size::px(26.f) });

        g_titleBar = &header;
        g_menuToggleKb = &kb;
    }

    // Scrollable body
    auto& body = root->add_container();
    body.set_declared_size({ rv::styled_size::fill(), rv::styled_size::fill() });
    body.direction(rv::layout_direction::vertical).gap(10.f).padding(12.f)
        .overflow(rv::overflow_mode::scroll).show_scrollbar(true)
        .background_color({ 0.08f, 0.08f, 0.10f, 1.f });

    // Player
    {
        auto& section = body.add_container("Player");
        AddToggle(section, "Infinite Stamina", etb::bInfiniteStamina, etb::hkInfiniteStamina, etb::modeInfiniteStamina);

        AddToggle(section, "Super Speed", etb::bSuperSpeed, etb::hkSuperSpeed, etb::modeSuperSpeed);
        {
            auto& row = section.add_row();
            row.gap(8.f).align(rv::alignment::center);
            auto& lbl = row.add_label("  Speed Multiplier");
            lbl.text_size(13.f);
            auto& filler = row.add_container();
            filler.flex_grow(1.f);
            auto& slider = row.add_slider(1.f, 10.f, etb::superSpeedMultiplier);
            slider.bind(&etb::superSpeedMultiplier)
                  .range(1.f, 10.f)
                  .set_declared_size({ rv::styled_size::px(160.f), rv::styled_size::px(26.f) });
        }

        AddToggle(section, "No Clip", etb::bNoClip, etb::hkNoClip, etb::modeNoClip);
        {
            auto& row = section.add_row();
            row.gap(8.f).align(rv::alignment::center);
            auto& lbl = row.add_label("  Fly Speed");
            lbl.text_size(13.f);
            auto& filler = row.add_container();
            filler.flex_grow(1.f);
            auto& slider = row.add_slider(100.f, 5000.f, etb::noClipSpeed);
            slider.bind(&etb::noClipSpeed)
                  .range(100.f, 5000.f)
                  .set_declared_size({ rv::styled_size::px(160.f), rv::styled_size::px(26.f) });
        }

        AddToggle(section, "God Mode", etb::bGodMode, etb::hkGodMode, etb::modeGodMode);
        AddToggle(section, "Full Bright", etb::bFullBright, etb::hkFullBright, etb::modeFullBright);
    }

    // ESP
    {
        auto& section = body.add_container("ESP");
        AddToggle(section, "ESP Enabled", etb::bEsp, etb::hkEsp, etb::modeEsp);
        AddToggle(section, "Boxes", etb::bEspBoxes, etb::hkEspBoxes, etb::modeEspBoxes);
        AddToggle(section, "Lines", etb::bEspLines, etb::hkEspLines, etb::modeEspLines);
        AddToggle(section, "Names", etb::bEspNames, etb::hkEspNames, etb::modeEspNames);
        AddToggle(section, "Distance", etb::bEspDistance, etb::hkEspDistance, etb::modeEspDistance);
        AddToggle(section, "Show Enemies", etb::bEspShowEnemies, etb::hkEspShowEnemies, etb::modeEspShowEnemies);
        AddToggle(section, "Show Players", etb::bEspShowPlayers, etb::hkEspShowPlayers, etb::modeEspShowPlayers);
        AddToggle(section, "Show Items", etb::bEspShowItems, etb::hkEspShowItems, etb::modeEspShowItems);
    }

    // Enemies
    {
        auto& section = body.add_container("Enemies");
        AddToggle(section, "Freeze Enemies", etb::bFreezeEnemies, etb::hkFreezeEnemies, etb::modeFreezeEnemies);
        AddToggle(section, "Disable Attacks", etb::bDisableEnemyAttacks, etb::hkDisableEnemyAttacks, etb::modeDisableEnemyAttacks);
        AddToggle(section, "Destroy Enemies", etb::bDestroyEnemies, etb::hkDestroyEnemies, etb::modeDestroyEnemies);
    }

    // Movement
    {
        auto& section = body.add_container("Movement");
        AddToggle(section, "No Fall Damage", etb::bNoFallDamage, etb::hkNoFallDamage, etb::modeNoFallDamage);
        auto& jumpRow = section.add_row();
        jumpRow.gap(8.f).align(rv::alignment::center);
        auto& jumpBtn = jumpRow.add_button("Super Jump");
        jumpBtn.on_click([]() { etb::SuperJump(); })
              .background_color({ 0.18f, 0.22f, 0.30f, 1.f })
              .rounding(6.f);
        auto& filler = jumpRow.add_container();
        filler.flex_grow(1.f);
        auto& jumpKb = jumpRow.add_key_bind();
        BindHotkey(jumpKb, etb::hkSuperJump).set_declared_size({ rv::styled_size::px(90.f), rv::styled_size::px(26.f) });
    }

    // Game State
    {
        auto& section = body.add_container("Game State");
        AddToggle(section, "Max Points", etb::bMaxPoints, etb::hkMaxPoints, etb::modeMaxPoints);
        AddToggle(section, "Max Level", etb::bMaxLevel, etb::hkMaxLevel, etb::modeMaxLevel);

        auto& hubRow = section.add_row();
        hubRow.gap(8.f).align(rv::alignment::center);
        auto& hubBtn = hubRow.add_button("Unlock HUB");
        hubBtn.on_click([]() { etb::UnlockHUB(); })
              .background_color({ 0.18f, 0.22f, 0.30f, 1.f })
              .rounding(6.f);
        auto& hubFiller = hubRow.add_container();
        hubFiller.flex_grow(1.f);
        auto& hubKb = hubRow.add_key_bind();
        BindHotkey(hubKb, etb::hkUnlockHUB).set_declared_size({ rv::styled_size::px(90.f), rv::styled_size::px(26.f) });

        auto& achRow = section.add_row();
        achRow.gap(8.f).align(rv::alignment::center);
        auto& achBtn = achRow.add_button("Finish Game Achievement");
        achBtn.on_click([]() { etb::FinishGameAchievement(); })
              .background_color({ 0.18f, 0.22f, 0.30f, 1.f })
              .rounding(6.f);
        auto& achFiller = achRow.add_container();
        achFiller.flex_grow(1.f);
        auto& achKb = achRow.add_key_bind();
        BindHotkey(achKb, etb::hkFinishGameAchievement).set_declared_size({ rv::styled_size::px(90.f), rv::styled_size::px(26.f) });
    }

    // Advanced
    {
        auto& section = body.add_container("Advanced");
        AddToggle(section, "Auto Loot", etb::bAutoLoot, etb::hkAutoLoot, etb::modeAutoLoot);
        AddToggle(section, "Night Vision", etb::bNightVision, etb::hkNightVision, etb::modeNightVision);
        AddToggle(section, "No Post Process", etb::bNoPostProcess, etb::hkNoPostProcess, etb::modeNoPostProcess);
        AddToggle(section, "No Sanity Effects", etb::bNoSanityEffects, etb::hkNoSanityEffects, etb::modeNoSanityEffects);
        AddToggle(section, "No Fear", etb::bNoFear, etb::hkNoFear, etb::modeNoFear);
        AddToggle(section, "Flashlight Always On", etb::bFlashlightAlwaysOn, etb::hkFlashlightAlwaysOn, etb::modeFlashlightAlwaysOn);
    }

    // Items
    {
        auto& section = body.add_container("Spawn Items");
        AddSpawnButton(section, "Flashlight", "flashlight", etb::hkSpawnFlashlight);
        AddSpawnButton(section, "Almond Water", "almondwater", etb::hkSpawnAlmondWater);
        AddSpawnButton(section, "Crowbar", "crowbar", etb::hkSpawnCrowbar);
        AddSpawnButton(section, "Knife", "knife", etb::hkSpawnKnife);
        AddSpawnButton(section, "Glowstick", "glowstick", etb::hkSpawnGlowstick);
        AddSpawnButton(section, "Ticket", "ticket", etb::hkSpawnTicket);
    }

    rv::vector_2d<float> lastSize = g_screenSize;
    MSG msg = {};

    while (g_menuRunning && !etb::g_unhookRequested)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                g_menuRunning = false;
        }
        if (!g_menuRunning || etb::g_unhookRequested)
            break;

        if (MenuToggleJustPressed(etb::g_menuToggleKey))
        {
            etb::g_menuVisible = !etb::g_menuVisible;
            ShowWindow(hwnd, etb::g_menuVisible ? SW_SHOW : SW_HIDE);
            if (etb::g_menuVisible)
                SetForegroundWindow(hwnd);
        }

        if (!etb::g_menuVisible)
        {
            Sleep(16);
            continue;
        }

        // Title bar drag: click the header (not the menu-toggle key_bind) to move the window.
        if (g_titleBar && g_menuToggleKb)
        {
            const auto t_min = g_titleBar->visual_pos();
            const auto t_size = g_titleBar->computed_size();
            const auto kb_min = g_menuToggleKb->visual_pos();
            const auto kb_size = g_menuToggleKb->computed_size();
            const auto mouse = g_input->mouse_pos();

            const bool inTitle = mouse.x >= t_min.x && mouse.x <= t_min.x + t_size.x &&
                                 mouse.y >= t_min.y && mouse.y <= t_min.y + t_size.y;
            const bool inKb = mouse.x >= kb_min.x && mouse.x <= kb_min.x + kb_size.x &&
                              mouse.y >= kb_min.y && mouse.y <= kb_min.y + kb_size.y;
            const bool clicked = g_input->is_mouse_clicked(0);
            const bool down = g_input->is_mouse_down(0);

            if (!g_titleDragging && inTitle && !inKb && clicked)
            {
                g_titleDragging = true;
                GetCursorPos(&g_titleDragStartMouse);
                GetWindowRect(hwnd, &g_titleDragStartWindow);
            }

            if (g_titleDragging)
            {
                if (down)
                {
                    POINT current;
                    GetCursorPos(&current);
                    const int dx = current.x - g_titleDragStartMouse.x;
                    const int dy = current.y - g_titleDragStartMouse.y;
                    SetWindowPos(hwnd, nullptr,
                                 g_titleDragStartWindow.left + dx,
                                 g_titleDragStartWindow.top + dy,
                                 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }
                else
                {
                    g_titleDragging = false;
                }
            }
        }

        if (lastSize != g_screenSize)
        {
            context->OMSetRenderTargets(0, nullptr, nullptr);
            rtv.release();
            swapChain->ResizeBuffers(0,
                static_cast<cstd::uint32_t>(g_screenSize.x),
                static_cast<cstd::uint32_t>(g_screenSize.y),
                DXGI_FORMAT_UNKNOWN, 0);
            createRtv();
            lastSize = g_screenSize;
        }

        struct FrameCtx {
            ID3D11DeviceContext* ctx;
            ID3D11RenderTargetView* tmpRtv;
            IDXGISwapChain* swap;
            rv::dx11_renderer* renderer_raw;
            rv::gui* gui_raw;
            rv::win32_input* input_raw;
            rv::vector_2d<float>* screen;
        };
        constexpr array_t<float, 4> clearColor = { 0.08f, 0.08f, 0.10f, 1.f };
        ID3D11RenderTargetView* tmpRtv = rtv.value();
        FrameCtx fctx{
            context.value(),
            tmpRtv,
            swapChain.value(),
            renderer.get(),
            gui.get(),
            g_input.get(),
            &g_screenSize
        };

        // SehGuard callback: any hardware fault inside the render frame is
        // swallowed so a transient D3D hiccup can't take the DLL down.
        SehGuard("MenuFrame",
            [](void* p) {
                auto* c = static_cast<FrameCtx*>(p);
                float cc[4] = { 0.08f, 0.08f, 0.10f, 1.f };
                c->ctx->OMSetRenderTargets(1, &c->tmpRtv, nullptr);
                c->ctx->ClearRenderTargetView(c->tmpRtv, cc);

                c->renderer_raw->begin_frame(*c->screen);
                c->input_raw->set_cursor(rv::cursor_type::arrow);
                c->gui_raw->render(*c->screen);
                c->renderer_raw->end_frame();
                c->swap->Present(0, 0);
                c->input_raw->reset();
            },
            &fctx);

        // 60 fps is plenty for an IMGUI-style menu overlay; Sleep(1) burned
        // ~15% of a core for no visual benefit.
        Sleep(16);
    }

    ETB_LOG("MenuThread: exit");
    renderer.reset();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

void StartMenuThread(HMODULE)
{
    CreateThread(nullptr, 0, MenuThread, nullptr, 0, nullptr);
}

void RequestUnhook()
{
    // Idempotent: multiple hotkey presses / menu clicks must not spawn more
    // than one unhook thread. Otherwise two threads race to CloseHandle the
    // same worker handles and FreeLibraryAndExitThread twice.
    static std::atomic_bool s_unhookInFlight{ false };
    bool expected = false;
    if (!s_unhookInFlight.compare_exchange_strong(expected, true))
        return;

    etb::g_unhookRequested = true;

    // Spin off a thread that waits for all worker threads to exit cleanly,
    // then frees the library and terminates. This thread must be the last
    // one executing DLL code to avoid crashes after unload.
    CreateThread(nullptr, 0,
        [](LPVOID param) -> DWORD
        {
            // Give worker loops a chance to see the flag and start cleanup.
            Sleep(100);

            // Signal the overlay to tear itself down from its own thread.
            // Never call DestroyWindow across threads — it corrupts the
            // owning thread's message queue and crashes randomly.
            etb::StopOverlay();

            HANDLE handles[] = {
                etb::g_hLegacyCache,
                etb::g_hSDKCache,
                etb::g_hFeatureLoop,
                etb::g_hOverlay,
                etb::g_hMenu
            };
            HANDLE valid[5] = {};
            DWORD count = 0;
            for (int i = 0; i < 5; ++i)
            {
                if (handles[i] && handles[i] != INVALID_HANDLE_VALUE)
                    valid[count++] = handles[i];
            }

            if (count > 0)
            {
                // Wait up to 5 seconds for every worker to finish. Overlay
                // needs an extra beat to drain its own message queue after
                // StopOverlay() flips the flag.
                WaitForMultipleObjects(count, valid, TRUE, 5000);

                // Best-effort cleanup of handles we kept open. Null the
                // globals afterwards so nothing else re-uses a stale handle.
                for (DWORD i = 0; i < count; ++i)
                    CloseHandle(valid[i]);
                etb::g_hLegacyCache = nullptr;
                etb::g_hSDKCache = nullptr;
                etb::g_hFeatureLoop = nullptr;
                etb::g_hOverlay = nullptr;
                etb::g_hMenu = nullptr;
            }

            FreeLibraryAndExitThread(static_cast<HMODULE>(param), 0);
        },
        etb::g_hModule, 0, nullptr);
}
