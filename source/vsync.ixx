module;

#include <common.hxx>

export module vsync;

import common;
import chrome;
import settings;

// The video settings object carries a vsync flag that Video.scr sets and that the renderer folds
// into a bit of the device flags on its way to CreateDevice and to every Reset. That fold is a
// single conditional, so forcing the branch one way or the other decides vsync for the presentation
// interval without touching the parsed setting, which means the game's own menu and Video.scr are
// left saying whatever the player wrote in them.
class VSync
{
public:
    VSync()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            auto pattern = engine_pattern("80 7D 18 00 89 44 24 24 74 07 83 C8 20 89 44 24 24");
            if (pattern.empty())
            {
                FusionFixLog::Write("vsync: no pattern matched, nothing patched");
                return;
            }

            // The branch that skips the flag bit when the setting is off.
            static raw_mem fnForceOn(pattern.get_first(8), { 0x90, 0x90 });
            static raw_mem fnForceOff(pattern.get_first(8), { 0xEB, 0x07 });

            static auto VSyncCB = []()
            {
                switch (FusionFixSettings.GetInt(PREF_VSYNC))
                {
                case 1:
                    fnForceOff.Write();
                    break;
                case 2:
                    fnForceOn.Write();
                    break;
                default:
                    fnForceOn.Restore();
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
