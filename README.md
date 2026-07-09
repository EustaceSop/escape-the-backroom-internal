# Escape the Backrooms - Internal SDK Base

Updated UE4 internal base using the **Dumper-7** generated SDK for *Escape the Backrooms* (4.27.2). Now includes an in-process **rendezvous** D3D11 menu with checkbox toggles and per-feature hotkeys.

## What changed

- Integrated the full Dumper-7 SDK (`C:\Dumper-7\4.27.2-0+++UE4+Release-4.27-EscapeTheBackrooms\CppSDK`).
- Added `src/sdk_wrapper.h` for typed access to `UWorld`, local player, controller, and pawn.
- Added `src/features.h` with SDK-backed toggles:
  - Infinite stamina
  - Super speed
  - God mode
  - No-clip / fly
  - Player / enemy / item cache buckets ready for ESP. Enemies detected: SkinStealer, Bacteria, BoneThief, Hound, Moth, RoamingSmiler, Ragdoll.
- Added `src/esp.h` + `src/overlay.h` for a GDI+ transparent ESP overlay.
- Added `src/items.h` for item spawning via `SpawnEquipItem_SERVER`, inventory add via `InvAddByName`, and drop via `DropItem_SERVER`.
- Added `src/enemies.h` for freezing enemies, disabling their attacks, or destroying them via `K2_DestroyActor`.
- Added `src/movement.h` for teleport, super jump (`LaunchCharacter` + `JumpZVelocity`), and no fall damage.
- Added `src/gamestate.h` for modifying `MP_PS_C` Points/Level, unlocking HUB, and triggering achievements.
- Added `src/advanced.h` for auto-loot, night vision, no post-process, sanity/fear suppression, and flashlight always-on.
- Added `src/etb_math.h` with the original vector/transform math + a shared `CameraInfo` / `ProjectWorldToScreenEx`, separated from `src/dllmain.cpp`.
- Added `src/etb_menu.h` / `src/etb_menu.cpp` for the in-DLL rendezvous menu (separate topmost D3D11 window).
- Added `src/hotkeys.h` / `src/hotkeys.cpp` for per-feature hotkey storage and polling.
- Kept the original legacy aimbot/bone path so old offsets still work once updated.

## Files

| File | Purpose |
|------|---------|
| `src/dllmain.cpp` | Entry point, threads, legacy aimbot |
| `src/sdk_wrapper.h` | SDK init + safe world/player accessors + `GetCameraInfo()` |
| `src/features.h` | Cheat toggles + SDK entity cache + feature loop |
| `src/advanced.h` | Auto-loot, night vision, no post-process, sanity/fear control, flashlight always on |
| `src/gamestate.h` | Points / level / HUB unlock / achievement helpers |
| `src/movement.h` | Teleport, super jump, no fall damage |
| `src/enemies.h` | Enemy freeze / disable attack / destroy helpers |
| `src/items.h` | Item spawning / inventory / drop helpers |
| `src/esp.h` | ESP render logic consuming the entity buckets |
| `src/overlay.h` | GDI+ transparent topmost overlay window |
| `src/etb_menu.h` / `src/etb_menu.cpp` | Rendezvous D3D11 menu window |
| `src/hotkeys.h` / `src/hotkeys.cpp` | Global hotkey variables + polling |
| `src/etb_math.h` | Vector3, matrix, world-to-screen, `CameraInfo` |
| `src/offsets.h` | Legacy hard-coded offsets (update these) |
| `src/read_write.h` | Memory read/write helpers |
| `src/spoof.h` | Thread start-address spoof helper |
| `compile.bat` | Build script |
| `bin/EscapeInternal.dll` | Compiled output |

## Build

1. Make sure the SDK path in `compile.bat` matches your Dumper-7 output folder.
2. The build script expects `D:\Download\rendezvous-main` for the GUI framework and `third_party\stb` for stb headers (both are already set up).
3. Open a VS2022 x64 developer prompt.
4. Run:

```bat
compile.bat
```

This compiles `src/dllmain.cpp`, `src/etb_menu.cpp`, `src/hotkeys.cpp`, the required rendezvous source files, `SDK/Basic.cpp`, `SDK/CoreUObject_functions.cpp`, `SDK/Engine_functions.cpp`, `SDK/Backrooms_functions.cpp`, `SDK/BPCharacter_Demo_functions.cpp`, `SDK/BP_MyGameInstance_functions.cpp`, `SDK/MP_GameMode_functions.cpp`, and `SDK/MP_PlayerController_functions.cpp` into `bin/EscapeInternal.dll`, linking `user32.lib`, `gdi32.lib`, `gdiplus.lib`, and `d3d11.lib`. Intermediate `.obj` files are written to `obj/`.

## Menu

After injection a separate topmost D3D11 window is created. It is hidden by default.

- Press **INSERT** to show / hide the menu.
- The menu toggle key itself can be rebound in the title bar.
- Every cheat has a **checkbox** to enable/disable it.
- Every cheat has a **key_bind** widget to set a global hotkey. Click the key box and press the desired key.
- One-shot actions (Super Jump, Unlock HUB, Finish Game Achievement, item spawns) have a button plus an optional hotkey.

Hotkeys are polled in `FeatureLoop`. While the menu is visible, gameplay hotkeys are suppressed so you can bind keys without triggering cheats.

## Before injecting

Update the legacy offsets in `src/offsets.h` from your current dump. The SDK path works without them, but the original aimbot still relies on:

- `uworld`
- `gameinstance`
- `localplayer`
- `playercontroller`
- `localpawn`
- `persistentlevel`
- `actorcount`
- `aactors`
- `componenttoworld`
- `bonearray`
- `currentactormesh`
- `rootcomp`

## ESP

Toggle the overlay rendering in the menu or in src/esp.h:

- `etb::bEsp` — master switch
- `etb::bEspBoxes`, `bEspLines`, `bEspNames`, `bEspDistance` — element toggles
- `etb::bEspShowEnemies`, `bEspShowPlayers`, `bEspShowItems` — category filters
- `etb::maxEspDistance` — max render distance

The overlay thread starts automatically from `src/dllmain.cpp`.

## Items / Inventory

Use the helpers in src/items.h from any thread, or set the pending-request globals and let `FeatureLoop` execute them safely:

```cpp
etb::RequestSpawn("flashlight");   // spawns + equips BP_Item_Flashlight_C
etb::RequestAdd("Flashlight");     // adds "Flashlight" row to inventory via InvAddByName
etb::RequestDrop("Flashlight");    // drops the held item row via DropItem_SERVER
```

Direct calls are also available:

```cpp
etb::SpawnEquipItemByName("crowbar");
etb::AddItemByName("AlmondWater");
etb::DropItem("Knife");
```

Supported friendly spawn names: `flashlight`, `almondwater`, `almondbottle`, `crowbar`, `knife`, `glowstick`, `glowstick_yellow`, `glowstick_red`, `glowstick_blue`, `bugspray`, `camera`, `firework`, `flaregun`, `toy`, `ticket`, `chainsaw`, `chainsaw_fast`. You can also pass the full class name (e.g. `BP_Item_Flashlight_C`).

## Enemy Control

Toggle enemy behavior in the menu or in src/enemies.h:

- `etb::bFreezeEnemies` — sets `ShouldMove = false` / `CanAttack = false` on SkinStealer, Bacteria, Hound, Moth, RoamingSmiler; stops BoneThief ticking.
- `etb::bDisableEnemyAttacks` — only disables `CanAttack`.
- `etb::bDestroyEnemies` — calls `K2_DestroyActor()` on every cached enemy.

These are applied every frame inside `RunFeatures()`.

## Movement / Teleport

Controls in the menu or in src/movement.h:

- `etb::bNoFallDamage` — sets `HasFallDamage = false` every frame.
- `etb::bSuperJump` + `etb::superJumpVelocity` — boosts `JumpZVelocity` and calls `LaunchCharacter` upward.
- `etb::RequestTeleport(Vector3{x, y, z})` — writes the target directly to `RootComponent->RelativeLocation` next frame.

Example:

```cpp
etb::RequestTeleport(Vector3(1000.f, 2000.f, 150.f));
etb::bSuperJump = true;
etb::bNoFallDamage = true;
```

## Game State / Unlocks

Helpers in the menu or in src/gamestate.h:

- `etb::bMaxPoints` — sets `MP_PS_C->Points` to 999999 every frame.
- `etb::bMaxLevel` — sets `MP_PS_C->Level` to 999 every frame.
- `etb::bUnlockHUB` — calls `MP_PlayerController::Unlock_HUB()` and `MP_GameMode::UnlockHUBForAllPlayers()` once.
- `etb::bFinishGameAchievement` — calls `MP_GameMode::FinishGameAchievement()` once.
- `etb::RequestUnlockAchievement("AchievementName")` — queues `BP_MyGameInstance::UnlockAchievement()` for next frame.

```cpp
etb::bMaxPoints = true;
etb::bMaxLevel = true;
etb::bUnlockHUB = true;
etb::RequestUnlockAchievement("EscapeTheBackrooms");
```

## Advanced Features

Toggles in the menu or in src/advanced.h:

- `etb::bAutoLoot` + `etb::autoLootRange` — teleports nearby dropped items to the player's feet, sets `CurrentFocusedItem`, and calls `TryPickup()`.
- `etb::bNightVision` — forces flashlight on with extended `FlashlightCheckLength` and calls `PlayBrightness()`.
- `etb::bNoPostProcess` — calls `SetPostProcessing()` with all distortion parameters set to 0.
- `etb::bNoSanityEffects` — calls `EndSanity()`, `StopInsanity()`, and `SRV_ResetSanityWarning()`.
- `etb::bNoFear` — calls `StopFear()` every frame.
- `etb::bFlashlightAlwaysOn` — toggles flashlight back on if it turns off.

## Next steps / ideas using the SDK

- [x] **ESP**: iterate `g_enemies`, `g_players`, `g_items` and project their `Location`.
- [x] **Item spawning**: `SpawnEquipItem_SERVER`, `InvAddByName`, `DropItem_SERVER`.
- [x] **Enemies**: freeze / disable attacks / destroy.
- [x] **Teleport**: write `RootComponent->RelativeLocation`.
- [x] **Game state / unlocks**: modify `MP_PS_C::Points/Level`, unlock HUB, finish-game achievement.
- [x] **Advanced**: auto-loot, night vision, no post-process, sanity/fear suppression, flashlight always-on.
- [x] **Menu**: in-DLL rendezvous menu with checkbox toggles and per-feature hotkeys.

All features are implemented and the DLL builds successfully.

Pull requests welcome.



#GUI Using https://github.com/noahware/rendezvous
