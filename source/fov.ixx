module;

#include <common.hxx>

export module fov;

import common;
import chrome;
import settings;

// What the ini asks for, as a horizontal angle.
static float fFieldOfView = 91.35f;

// The camera keeps the angle it was given so it can rebuild its own tan cache immediately after the
// call. Writing only the call argument would leave that cache describing the old angle.
static constexpr ptrdiff_t nCameraFieldOfView = 0x90;

static HWND hGameWindow = nullptr;

static BOOL CALLBACK FindGameWindowProc(HWND hWnd, LPARAM lParam)
{
    DWORD nProcessId = 0;
    GetWindowThreadProcessId(hWnd, &nProcessId);

    if (nProcessId != GetCurrentProcessId() || !IsWindowVisible(hWnd) || GetWindow(hWnd, GW_OWNER) != nullptr)
        return TRUE;

    *reinterpret_cast<HWND*>(lParam) = hWnd;
    return FALSE;
}

// The client area of the game's own window, which is the back buffer in both display modes. 16:9
// while there is no window to ask, which is what the game is authored for.
static float GetAspectRatio()
{
    static constexpr auto fDefaultAspectRatio = 16.0f / 9.0f;

    if (hGameWindow == nullptr || !IsWindow(hGameWindow))
    {
        hGameWindow = nullptr;
        EnumWindows(FindGameWindowProc, reinterpret_cast<LPARAM>(&hGameWindow));
    }

    RECT rect{};
    if (hGameWindow == nullptr || !GetClientRect(hGameWindow, &rect))
        return fDefaultAspectRatio;

    const auto fAspectRatio = static_cast<float>(rect.right - rect.left) / static_cast<float>(rect.bottom - rect.top);
    if (!std::isfinite(fAspectRatio) || fAspectRatio <= 0.0f)
        return fDefaultAspectRatio;

    return fAspectRatio;
}

// The camera is given a vertical angle and the horizontal one falls out of the display's aspect,
// so a horizontal setting is converted back the same way round: half angle, over the aspect, in
// tangent space. At 16:9 the two are the pair the game already shipped with.
static float VerticalFromHorizontal(float fHorizontal, float fAspectRatio)
{
    static constexpr auto fPi = 3.14159265358979323846f;
    static constexpr auto fDegreesToRadians = fPi / 180.0f;
    static constexpr auto fRadiansToDegrees = 180.0f / fPi;

    const auto fVertical = 2.0f * std::atan(std::tan(fHorizontal * 0.5f * fDegreesToRadians) / fAspectRatio) * fRadiansToDegrees;
    if (!std::isfinite(fVertical))
        return fHorizontal;

    return std::clamp(fVertical, 1.0f, 179.0f);
}

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
            // still in ESI. The aspect is read here rather than once at startup, so a resolution
            // change is picked up by the next camera without anything having to notice it.
            static auto SetFOVHook = safetyhook::create_mid(pattern.get_first(32), [](SafetyHookContext& regs)
            {
                const auto fVerticalFieldOfView = VerticalFromHorizontal(fFieldOfView, GetAspectRatio());

                *reinterpret_cast<float*>(regs.esp) = fVerticalFieldOfView;
                *reinterpret_cast<float*>(regs.esi + nCameraFieldOfView) = fVerticalFieldOfView;
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
