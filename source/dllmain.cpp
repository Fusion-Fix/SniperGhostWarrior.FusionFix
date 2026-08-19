#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mutex>
#include <functional>
#include <string_view>

import common;
import chrome;
import settings;
import skipintro;
import fov;
import displaymode;
import vsync;
import aniso;
import hudfixes;
import updatecheck;

void Init()
{
    FusionFixSettings.ReadIniSettings();
    FusionFix::onStartupPromptEvent().executeAll();
    FusionFix::onInitEvent().executeAll();
}

extern "C"
{
    void __declspec(dllexport) InitializeASI()
    {
        std::call_once(CallbackHandler::flag, []()
        {
            CallbackHandler::RegisterCallback(Init);
            CallbackHandler::RegisterCallback(L"engine_x86.dll", InitEngine);
            CallbackHandler::RegisterCallback(L"GameDLL_x86.dll", InitGameDLL);
        });
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        // UAL calls InitializeASI itself; under any other loader we have to kick it off here.
        if (!IsUALPresent()) { InitializeASI(); }
    }
    if (reason == DLL_PROCESS_DETACH)
    {
        FusionFix::onShutdownEvent().executeAll();
    }
    return TRUE;
}
