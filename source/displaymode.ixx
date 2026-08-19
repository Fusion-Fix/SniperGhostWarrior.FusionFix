module;

#include <common.hxx>

export module displaymode;

import common;
import chrome;
import settings;

// Video.scr's Fullscreen() line resolves to a single boolean on the video settings object, written
// in one place for both the live value and the pending one. Everything downstream follows from it:
// the window style picked for CreateWindowEx, and the Windowed field of the present parameters
// handed to CreateDevice and to every Reset. So the mode is decided by replacing the three bytes
// that compute that boolean, and nothing else has to be kept in step.
class DisplayMode
{
public:
    DisplayMode()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            auto pattern = engine_pattern("80 3D ? ? ? ? 00 C6 87 BB 06 00 00 01 0F 94 C0 88 47 2C 88 47 10");
            if (pattern.empty())
            {
                FusionFixLog::Write("displaymode: no pattern matched, nothing patched");
                return;
            }

            // setz al, replaced with mov al, 1 or mov al, 0 and a pad byte.
            static raw_mem fnFullscreen(pattern.get_first(14), { 0xB0, 0x01, 0x90 });
            static raw_mem fnWindowed(pattern.get_first(14), { 0xB0, 0x00, 0x90 });

            static auto DisplayModeCB = []()
            {
                switch (FusionFixSettings.GetInt(PREF_DISPLAYMODE))
                {
                case 1:
                    fnFullscreen.Write();
                    break;
                case 2:
                    fnWindowed.Write();
                    break;
                default:
                    fnFullscreen.Restore();
                    break;
                }
            };

            DisplayModeCB();

            // Video.scr is parsed once during startup, so a change lands on the next launch.
            FusionFix::onIniFileChange() += []()
            {
                DisplayModeCB();
            };
        };
    }
} DisplayMode;
