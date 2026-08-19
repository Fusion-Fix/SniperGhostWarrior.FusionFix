module;

#include <common.hxx>

export module aniso;

import common;
import chrome;
import settings;

static constexpr uint32_t nFilterNone = 0;
static constexpr uint32_t nFilterPoint = 1;
static constexpr uint32_t nFilterLinear = 2;
static constexpr uint32_t nFilterAnisotropic = 3;

// Offsets inside the descriptor block the renderer holds in EDI while it programs a sampler.
static constexpr ptrdiff_t nDescMagFilter = 0x00;
static constexpr ptrdiff_t nDescMinFilter = 0x04;
static constexpr ptrdiff_t nDescMipFilter = 0x08;
static constexpr ptrdiff_t nDescMaxAnisotropy = 0x18;

static uint32_t nAnisotropicFiltering = 0;

// Whether this sampler is one anisotropy belongs on, decided from the descriptor rather than from
// the value currently on its way to the device, so every state agrees about the same sampler.
static bool WantsAnisotropy(uintptr_t pDesc)
{
    if (nAnisotropicFiltering < 2)
        return false;

    const auto nMagFilter = *reinterpret_cast<uint32_t*>(pDesc + nDescMagFilter);
    const auto nMinFilter = *reinterpret_cast<uint32_t*>(pDesc + nDescMinFilter);
    const auto nMipFilter = *reinterpret_cast<uint32_t*>(pDesc + nDescMipFilter);

    if (nMipFilter == nFilterNone)
        return false;

    if (nMagFilter == nFilterPoint || nMinFilter == nFilterPoint || nMinFilter == nFilterNone)
        return false;

    return true;
}

class AnisotropicFiltering
{
public:
    AnisotropicFiltering()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            auto minPattern = engine_pattern("8B 57 04 A1 ? ? ? ? 8B 08 52 6A 06 56 50 8B 81 14 01 00 00 FF D0");
            auto maxPattern = engine_pattern("8B 57 18 A1 ? ? ? ? 8B 08 52 6A 0A 56 50 8B 81 14 01 00 00 FF D0");
            auto mipPattern = engine_pattern("8B 57 08 A1 ? ? ? ? 8B 08 52 6A 07 56 50 8B 81 14 01 00 00 FF D0");

            if (minPattern.empty() || maxPattern.empty())
                return;

            static auto AnisotropicFilteringCB = []()
            {
                nAnisotropicFiltering = static_cast<uint32_t>(FusionFixSettings.GetInt(PREF_ANISOTROPICFILTERING));
            };

            AnisotropicFilteringCB();

            // Each hook sits immediately after the descriptor field has been read into EDX and
            // before it is pushed, so rewriting EDX is the whole of the change.
            static auto MinFilterHook = safetyhook::create_mid(minPattern.get_first(3), [](SafetyHookContext& regs)
            {
                if (WantsAnisotropy(regs.edi))
                    regs.edx = nFilterAnisotropic;
            });

            static auto MaxAnisotropyHook = safetyhook::create_mid(maxPattern.get_first(3), [](SafetyHookContext& regs)
            {
                if (WantsAnisotropy(regs.edi))
                    regs.edx = nAnisotropicFiltering;
            });

            // Anisotropic minification with point mip selection still shows the mip seam it is
            // being asked to hide, so the pair moves together where the sampler already filters.
            if (!mipPattern.empty())
            {
                static auto MipFilterHook = safetyhook::create_mid(mipPattern.get_first(3), [](SafetyHookContext& regs)
                {
                    if (WantsAnisotropy(regs.edi) && regs.edx == nFilterPoint)
                        regs.edx = nFilterLinear;
                });
            }
            else

            // The samplers are programmed from the descriptor on every bind, so a change lands
            // within a frame or two rather than on the next launch.
            FusionFix::onIniFileChange() += []()
            {
                AnisotropicFilteringCB();
            };
        };
    }
} AnisotropicFiltering;
