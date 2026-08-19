module;

#include <common.hxx>

export module chrome;

import common;

// Sniper_x86.exe is a thin launcher. Chrome Engine 4 splits the game across two modules and
// every patch target lives in one of them, so patterns scan those images rather than the main
// module: engine_x86.dll holds the D3D9 renderer, the window and the video settings, and
// GameDLL_x86.dll holds game logic, cameras, the intro sequence and the menus.
export HMODULE hEngine = nullptr;
export HMODULE hGameDLL = nullptr;

export hook::pattern engine_pattern(std::string_view bytes)
{
    return hook::module_pattern(hEngine, bytes);
}

export hook::pattern gamedll_pattern(std::string_view bytes)
{
    return hook::module_pattern(hGameDLL, bytes);
}

export void InitEngine()
{
    hEngine = GetModuleHandleW(L"engine_x86.dll");
    if (!hEngine)
        return;

    FusionFix::onEngineInitEvent().executeAll();
}

export void InitGameDLL()
{
    hGameDLL = GetModuleHandleW(L"GameDLL_x86.dll");
    if (!hGameDLL)
        return;

    FusionFix::onGameDLLInitEvent().executeAll();
}
