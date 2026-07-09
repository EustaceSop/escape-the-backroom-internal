#pragma once
#include <Windows.h>

// Starts the rendezvous menu thread in-process.
// The menu renders in a separate topmost D3D11 window and binds directly
// to the etb:: toggle / hotkey globals.
DWORD WINAPI MenuThread(LPVOID);
void StartMenuThread(HMODULE hMod);
void RequestUnhook();

// Menu visibility toggled by the menu thread; read by other code if needed.
namespace etb { inline bool g_menuVisible = false; }
