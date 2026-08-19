module;

#include <common.hxx>

export module hudfixes;

import common;
import chrome;

static constexpr ptrdiff_t nGameScreenWidth = 0x70;
static constexpr ptrdiff_t nGameScreenHeight = 0x74;

static constexpr ptrdiff_t nScreenPositionX = 0xA4;
static constexpr ptrdiff_t nScreenPositionY = 0xA8;
static constexpr ptrdiff_t nScreenPositionZ = 0xAC;
static constexpr ptrdiff_t nScreenFlags = 0x280;
static constexpr uint8_t nScreenAllowScaleToSafeArea = 0x2;

// CUIVisual::SetPosition, taking a pointer to three floats. Overridden per class, so it is read
// from the object rather than resolved once.
static constexpr size_t nSetPositionVTableIndex = 0x294 / sizeof(void*);
using SetPosition_t = void(__fastcall*)(void* pThis, void* pUnused, float* pPosition);

static void** ppGame = nullptr;

// What the scale site decided this frame, so the centring site can put the gap back without
// recomputing anything.
static float fCanvasWidth = 1280.0f;
static float fCanvasHeight = 720.0f;
static float fUniformScale = 1.0f;
static bool bUniformScaleApplied = false;

static void ReadScreenSize(float& fWidth, float& fHeight)
{
    fWidth = 0.0f;
    fHeight = 0.0f;

    if (ppGame == nullptr || *ppGame == nullptr)
        return;

    const auto pGame = reinterpret_cast<uintptr_t>(*ppGame);
    fWidth = static_cast<float>(*reinterpret_cast<int32_t*>(pGame + nGameScreenWidth));
    fHeight = static_cast<float>(*reinterpret_cast<int32_t*>(pGame + nGameScreenHeight));
}

class HudFixes
{
public:
    HudFixes()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            // The four OR instructions in CUIVisual::SetObjectProperty that set the resolution
            // change exemptions.
            auto keepWidthPattern = engine_pattern("83 8E 34 01 00 00 20");
            auto keepHeightPattern = engine_pattern("81 8E 34 01 00 00 80 00 00 00");
            auto keepPosXPattern = engine_pattern("81 8E 34 01 00 00 00 02 00 00");
            auto keepPosYPattern = engine_pattern("81 8E 34 01 00 00 00 08 00 00");

            if (!keepWidthPattern.empty())
            {
                static raw_mem fnKeepWidth(keepWidthPattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                fnKeepWidth.Write();
            }

            if (!keepHeightPattern.empty())
            {
                static raw_mem fnKeepHeight(keepHeightPattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                fnKeepHeight.Write();
            }

            if (!keepPosXPattern.empty())
            {
                static raw_mem fnKeepPosX(keepPosXPattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                fnKeepPosX.Write();
            }

            if (!keepPosYPattern.empty())
            {
                static raw_mem fnKeepPosY(keepPosYPattern.get_first(0), { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
                fnKeepPosY.Write();
            }

            // The per frame screen scale, from the load of the game object through to the point
            // where both axis scales are on the stack.
            auto scalePattern = engine_pattern("A1 ? ? ? ? DB 40 74 DB 40 70 8B 45 10 D8 30 8B 33 8B 53 04 D9 5C 24 14 D8 70 04 8D 04 96 3B F0 D9 5C 24 18 D9 E8 D9 5C 24 1C");
            if (scalePattern.empty())
                return;

            ppGame = *scalePattern.get_first<void**>(1);

            static auto ScaleHook = safetyhook::create_mid(scalePattern.get_first(37), [](SafetyHookContext& regs)
            {
                bUniformScaleApplied = false;

                auto* pScaleX = reinterpret_cast<float*>(regs.esp + 0x14);
                auto* pScaleY = reinterpret_cast<float*>(regs.esp + 0x18);

                // The canvas the screens being drawn this pass were authored on. Read from the
                // frame rather than assumed, because it is an argument.
                const auto* pCanvas = *reinterpret_cast<const float**>(regs.ebp + 0x10);
                if (pCanvas == nullptr || pCanvas[0] <= 0.0f || pCanvas[1] <= 0.0f)
                    return;

                // Parenthesised so Windows.h's min macro cannot claim the call.
                const auto fScale = (std::min)(*pScaleX, *pScaleY);
                if (!std::isfinite(fScale) || fScale <= 0.0f)
                    return;

                *pScaleX = fScale;
                *pScaleY = fScale;

                fCanvasWidth = pCanvas[0];
                fCanvasHeight = pCanvas[1];
                fUniformScale = fScale;
                bUniformScaleApplied = true;
            });

            // The guard on the engine's own safe area inset, reached once per screen with the
            // screen still in ESI and its position already in display pixels.
            auto centrePattern = engine_pattern("80 3D ? ? ? ? 00 74 76 8B 06 8B 90 50 02 00 00 8B CE FF D2 84 C0");
            if (centrePattern.empty())
                return;

            static auto CentreHook = safetyhook::create_mid(centrePattern.get_first(0), [](SafetyHookContext& regs)
            {
                const auto pScreen = regs.esi;
                if (pScreen == 0 || !bUniformScaleApplied)
                    return;

                float fScreenWidth = 0.0f;
                float fScreenHeight = 0.0f;
                ReadScreenSize(fScreenWidth, fScreenHeight);

                const auto fOffsetX = (fScreenWidth - fCanvasWidth * fUniformScale) * 0.5f;
                const auto fOffsetY = (fScreenHeight - fCanvasHeight * fUniformScale) * 0.5f;

                // At 16:9 the canvas already fills the display and there is nothing to centre.
                if (std::abs(fOffsetX) < 0.5f && std::abs(fOffsetY) < 0.5f)
                    return;

                // Screens that opt out of being moved are left where they are, matching what the
                // engine's own inset honours.
                if ((*reinterpret_cast<uint8_t*>(pScreen + nScreenFlags) & nScreenAllowScaleToSafeArea) == 0)
                    return;

                float position[3] =
                {
                    *reinterpret_cast<float*>(pScreen + nScreenPositionX) + fOffsetX,
                    *reinterpret_cast<float*>(pScreen + nScreenPositionY) + fOffsetY,
                    *reinterpret_cast<float*>(pScreen + nScreenPositionZ),
                };

                auto** pVTable = *reinterpret_cast<void***>(pScreen);
                auto SetPosition = reinterpret_cast<SetPosition_t>(pVTable[nSetPositionVTableIndex]);
                SetPosition(reinterpret_cast<void*>(pScreen), nullptr, position);
            });
        };
    }
} HudFixes;
