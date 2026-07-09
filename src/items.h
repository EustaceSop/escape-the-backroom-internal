#pragma once
#include <Windows.h>
#include <string>
#include <map>
#include "SDK.hpp"
#include "sdk_wrapper.h"

namespace etb
{
    // ---------- Spawn / inventory request queue ----------
    inline std::string g_pendingSpawnClass;   // full BP class, e.g. "BP_Item_Flashlight_C"
    inline std::string g_pendingAddName;      // DataTable row name, e.g. "Flashlight"
    inline std::string g_pendingDropName;     // DataTable row name, e.g. "Flashlight"

    // Friendly name -> full BlueprintGeneratedClass name for SpawnEquipItem_SERVER
    inline const std::map<std::string, std::string> g_itemClassMap =
    {
        { "flashlight",    "BP_Item_Flashlight_C" },
        { "almondwater",   "BP_Item_AlmondWater_C" },
        { "almondbottle",  "BP_Item_AlmondBottle_C" },
        { "crowbar",       "BP_Item_Crowbar_C" },
        { "knife",         "BP_Item_Knife_C" },
        { "glowstick",     "BP_Item_Glowstick_C" },
        { "glowstick_yellow", "BP_Item_Glowstick_Yellow_C" },
        { "glowstick_red",    "BP_Item_Glowstick_Red_C" },
        { "glowstick_blue",   "BP_Item_Glowstick_Blue_C" },
        { "bugspray",      "BP_Item_BugSpray_C" },
        { "camera",        "BP_Item_Camera_C" },
        { "firework",      "BP_Item_Firework_C" },
        { "flaregun",      "BP_Item_FlareGun_C" },
        { "toy",           "BP_Item_Toy_C" },
        { "ticket",        "BP_Item_Ticket_C" },
        { "chainsaw",      "BP_Item_Chainsaw_C" },
        { "chainsaw_fast", "BP_Item_Chainsaw_Fast_C" },
    };

    inline SDK::FName StringToFName(const std::string& s)
    {
        std::wstring ws(s.begin(), s.end());
        SDK::FString fstr(ws.c_str());
        return SDK::UKismetStringLibrary::Conv_StringToName(fstr);
    }

    // Spawn the item class and equip it (server RPC).
    inline bool SpawnEquipItem(const std::string& itemClassName)
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return false;

        SDK::UClass* cls = SDK::UObject::FindClassFast(itemClassName.c_str());
        if (!cls) return false;

        local->SpawnEquipItem_SERVER(cls);
        return true;
    }

    // Try friendly name first, then treat as full class name.
    inline bool SpawnEquipItemByName(const std::string& name)
    {
        std::string lower;
        for (char c : name) lower += (char)tolower(c);

        auto it = g_itemClassMap.find(lower);
        if (it != g_itemClassMap.end())
            return SpawnEquipItem(it->second);

        return SpawnEquipItem(name);
    }

    // Add item to inventory by DataTable row name (uses InvAddByName).
    inline bool AddItemByName(const std::string& itemName)
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return false;

        SDK::FName name = StringToFName(itemName);
        local->InvAddByName(name);
        return true;
    }

    // Drop currently held item by DataTable row name (server RPC).
    inline bool DropItem(const std::string& itemName)
    {
        SDK::ABPCharacter_Demo_C* local = GetLocalCharacter();
        if (!local) return false;

        SDK::FName name = StringToFName(itemName);
        local->DropItem_SERVER(name);
        return true;
    }

    // Process one pending request per frame. Called from FeatureLoop.
    inline void ProcessItemRequests()
    {
        if (!g_pendingSpawnClass.empty())
        {
            SpawnEquipItemByName(g_pendingSpawnClass);
            g_pendingSpawnClass.clear();
        }

        if (!g_pendingAddName.empty())
        {
            AddItemByName(g_pendingAddName);
            g_pendingAddName.clear();
        }

        if (!g_pendingDropName.empty())
        {
            DropItem(g_pendingDropName);
            g_pendingDropName.clear();
        }
    }

    // Convenience helpers for direct use / hotkeys.
    inline void RequestSpawn(const std::string& name) { g_pendingSpawnClass = name; }
    inline void RequestAdd(const std::string& name)   { g_pendingAddName = name; }
    inline void RequestDrop(const std::string& name)  { g_pendingDropName = name; }
}
