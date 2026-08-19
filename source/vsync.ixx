module;

#include <common.hxx>

export module vsync;

import common;
import chrome;
import settings;

class VSync
{
public:
    VSync()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            auto pattern = engine_pattern("80 7D 18 00 89 44 24 24 74 07 83 C8 20 89 44 24 24");
            if (pattern.empty())
                return;

            // The branch that skips the flag bit when the setting is off.
            static raw_mem fnForceOn(pattern.get_first(8), { 0x90, 0x90 });
            static raw_mem fnForceOff(pattern.get_first(8), { 0xEB, 0x07 });

            static auto VSyncCB = []()
            {
                switch (FusionFixSettings.GetInt(PREF_VSYNC))
                {
                case 1:
                    fnForceOn.Write();
                    break;
                default:
                    fnForceOff.Write();
                    break;
                }
            };

            VSyncCB();

            // Read on every device reset, so a change lands the next time the game changes mode.
            FusionFix::onIniFileChange() += []()
            {
                VSyncCB();
            };
        };
    }
} VSync;
