module;

#include <common.hxx>

export module fov;

import common;
import chrome;
import settings;

static float fFieldOfView = 91.35f;

// The camera keeps the angle it was given so it can rebuild its own tan cache immediately after the
// call. Writing only the call argument would leave that cache describing the old angle.
static constexpr ptrdiff_t nCameraFieldOfView = 0x90;

class FieldOfView
{
public:
    FieldOfView()
    {
        FusionFix::onGameDLLInitEvent() += []()
        {
            auto pattern = gamedll_pattern("55 8B EC 83 E4 F8 51 56 8B F1 FF 15 ? ? ? ? D9 40 4C 51 D9 96 90 00 00 00 8D 4E F8 D9 1C 24 FF 15 ? ? ? ?");
            if (pattern.empty())
                return;

            static auto FieldOfViewCB = []()
            {
                fFieldOfView = FusionFixSettings.GetFloat(PREF_FIELDOFVIEW);
            };

            FieldOfViewCB();

            // The call itself, so the argument is already on the stack and the camera object is
            // still in ESI.
            static auto SetFOVHook = safetyhook::create_mid(pattern.get_first(32), [](SafetyHookContext& regs)
            {
                *reinterpret_cast<float*>(regs.esp) = fFieldOfView;
                *reinterpret_cast<float*>(regs.esi + nCameraFieldOfView) = fFieldOfView;
            });

            // The camera reads the level value when it is built, so a change lands the next time one
            // is, which in practice is the next level load.
            FusionFix::onIniFileChange() += []()
            {
                FieldOfViewCB();
            };
        };
    }
} FieldOfView;
