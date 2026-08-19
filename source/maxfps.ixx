module;

#include <common.hxx>

export module maxfps;

import common;
import chrome;
import settings;

static constexpr uint32_t nMinFrameRate = 10;
static constexpr uint32_t nMaxFrameRate = 1000;

// EnumDisplaySettings answers 0 or 1 for a display with no fixed rate, both meaning "the hardware
// default" rather than zero or one hertz.
static constexpr DWORD nUnknownRefreshRate = 1;
static constexpr int32_t nFallbackFrameRate = 60;

enum MaxFrameRateSetting
{
    MAXFPS_UNLOCKED = 0,
    MAXFPS_DISPLAY = 1,
};

// Sleep is only accurate to the timer period, which the engine has already dropped to 1ms with
// timeBeginPeriod. The last stretch is spun instead, so the cap lands on the requested rate rather
// than a millisecond either side of it.
static constexpr double fSpinThresholdMs = 2.0;

// A frame that ran long enough to be a level load or an alt-tab is not a frame the pacing should
// try to make up for, so the schedule restarts from now instead.
static constexpr double fResyncSeconds = 0.25;

static uint32_t nCurrentFrameRate = 0;

static LARGE_INTEGER nCounterFrequency{};
static LARGE_INTEGER nNextFrame{};

// ENUM_CURRENT_SETTINGS is the mode the display is running now, which is what a limiter has to
// match. Walking the adapter's supported modes instead would give the highest rate the display can
// do, and the two disagree on anything the player has not set to its maximum.
static uint32_t GetDisplayRefreshRate()
{
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);

    if (!EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm))
        return nFallbackFrameRate;

    if ((dm.dmFields & DM_DISPLAYFREQUENCY) == 0 || dm.dmDisplayFrequency <= nUnknownRefreshRate)
        return nFallbackFrameRate;

    // Whole hertz, so a 59.94Hz mode arrives as 59 and the cap lands just under the refresh rather
    // than just over it.
    return static_cast<uint32_t>(dm.dmDisplayFrequency);
}

static uint32_t ResolveMaxFrameRate()
{
    const auto nSetting = static_cast<uint32_t>(FusionFixSettings.GetInt(PREF_MAXFRAMERATE));

    if (nSetting == MAXFPS_UNLOCKED)
        return 0;

    if (nSetting == MAXFPS_DISPLAY)
        return GetDisplayRefreshRate();

    return std::clamp(nSetting, nMinFrameRate, nMaxFrameRate);
}

static void PaceFrame()
{
    const auto nFrameRate = nCurrentFrameRate;
    if (nFrameRate == 0 || nCounterFrequency.QuadPart == 0)
        return;

    LARGE_INTEGER nNow{};
    QueryPerformanceCounter(&nNow);

    const auto nResync = static_cast<LONGLONG>(nCounterFrequency.QuadPart * fResyncSeconds);
    if (nNextFrame.QuadPart == 0 || nNow.QuadPart > nNextFrame.QuadPart + nResync)
        nNextFrame.QuadPart = nNow.QuadPart;

    nNextFrame.QuadPart += nCounterFrequency.QuadPart / static_cast<LONGLONG>(nFrameRate);

    for (;;)
    {
        QueryPerformanceCounter(&nNow);

        const auto nRemaining = nNextFrame.QuadPart - nNow.QuadPart;
        if (nRemaining <= 0)
            break;

        const auto fRemainingMs = static_cast<double>(nRemaining) * 1000.0 / static_cast<double>(nCounterFrequency.QuadPart);
        if (fRemainingMs > fSpinThresholdMs)
            Sleep(static_cast<DWORD>(fRemainingMs - 1.0));
        else
            YieldProcessor();
    }
}

class MaxFrameRate
{
public:
    MaxFrameRate()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            // CEngine::Frame, at the test of the dedicated server flag that its own limiter hangs
            // off. Entry, so the wait happens before the update and before the render, which keeps
            // the frame delta the engine measures and the rate the player sees describing the same
            // interval.
            auto pattern = engine_pattern("56 57 8B F9 80 BF F2 02 00 00 00 74 38");
            if (pattern.empty())
                return;

            QueryPerformanceFrequency(&nCounterFrequency);

            static auto MaxFrameRateCB = []()
            {
                nCurrentFrameRate = ResolveMaxFrameRate();

                // The schedule describes the old rate, so it is dropped rather than carried across.
                nNextFrame.QuadPart = 0;
            };

            MaxFrameRateCB();

            static auto FrameHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
            {
                PaceFrame();
            });

            // Read at the top of every frame, so a change lands on the next one.
            FusionFix::onIniFileChange() += []()
            {
                MaxFrameRateCB();
            };
        };
    }
} MaxFrameRate;
