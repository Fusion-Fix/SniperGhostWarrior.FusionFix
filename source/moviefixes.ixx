module;

#include <common.hxx>

export module moviefixes;

import common;
import chrome;

// Menu .wmv plays greyscale and upside down.
//
// CWMFDecoder scores the decoder's output subtypes against a table of four (RGB565, RGB24, RGB32,
// I420) and keeps the highest index, so I420 wins on current Windows. Its "output is planar" flag
// is set from `index == 0` (RGB565) instead, so the frame takes the packed path: the Y plane alone
// is uploaded at 8bpp (greyscale), and top-down I420 is drawn with the bottom-up DIB winding the
// RGB subtypes need (flipped).
//
// Cutting the table to three entries drops I420 and leaves RGB32, the path everything downstream
// was written for. Full write-up: docs/research/2026-08-23-menu-fmv-greyscale-flipped.md
class MovieFixes
{
public:
    MovieFixes()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            // ADD ESI,1 / ADD EDI,10 / CMP ESI,4 / JC - the subtype table scan loop.
            auto pattern = engine_pattern("83 C6 01 83 C7 10 83 FE 04 72 E8");
            if (pattern.empty())
                return;

            injector::WriteMemory<uint8_t>(pattern.get_first(8), 3, true);
        };
    }
} MovieFixes;
