module;

#include <common.hxx>

export module hudfixes;

import common;
import chrome;

// The HUD and every menu are authored on a 1280x720 canvas, and two separate things go wrong on the
// way from that canvas to the display.
//
// The first is size. When the resolution changes the engine walks the widget tree multiplying each
// element by the new ratio, and it asks two questions of every element before doing so, both
// authored per widget in the .xui: KeepWidthOnResolutionChange and KeepHeightOnResolutionChange. A
// widget with those set keeps the pixel size it was drawn at and the walk moves on. This game sets
// them across its HUD, so at 3840x2160 the health bar is 185 pixels wide where the ratio of three
// should have made it 555. The interface is not failing to scale; it was authored asking not to be
// scaled, for a television it was going to be the right size on.
//
// Position is exempted by the same mechanism, through KeepPosXOnResolutionChange and its Y
// counterpart, and the two have to be answered together. Freeing size alone is worse than the
// original: the bar grows to its proper length while the number beside it stays at the coordinate
// 1280x720 gave it, so they overlap, and the ammunition count sits in the middle of the screen
// instead of in the corner.
//
// All four are honoured in one place, the property parser that reads the attributes out of the .xui
// and sets the bits. Blanking the four instructions that set them means no element in any .xui ever
// carries them, so the walk moves everything and no part of a row can disagree with another because
// none of them is exempt. It runs before a single screen is parsed, so nothing has to be undone
// afterwards. The OnParentSizeChange attributes are a different question and are left as authored.
//
// The second is shape. The per frame screen scale is computed one axis at a time:
//
//     scaleX = screenWidth  / 1280
//     scaleY = screenHeight /  720
//
// with no comparison between the two and no offset, so at 16:9 both agree and the result is right,
// and at any other shape the whole interface is stretched along one axis: about a ninth at 16:10, a
// third at 21:9, a third the other way at 4:3. The engine already contains a correct uniform fit in
// IGame::GetScreenResolutionScale, which the layout path simply never calls. Here it takes the
// smaller of the two for both axes and puts back the centring a uniform scale needs, the same read
// modify write on the screen's position that the engine's own safe area inset performs a few
// instructions later, through the same virtual.

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
