module;

#include <common.hxx>

export module shadows;

import common;
import chrome;
import settings;

// Nothing in the engine clamps the shadow map size: the 1024 ceiling is two hardcoded combo box
// entries in the menu, 512 and 1024. CreateShadowMaps takes the per cascade size in EAX and
// everything downstream is derived from it, the 4*size by size atlas the four hierarchical
// cascades share, the SHADOWMAP_PARAMS shader constants and CHierarchicalShadowMap's own copy,
// so overriding that one register is the whole of the option.

static uint32_t nShadowMapSize = 0;

class ShadowMapSize
{
public:
    ShadowMapSize()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            auto pattern = engine_pattern("83 EC 08 53 55 8B 2D ? ? ? ? 83 7D 0C 00 56 57 8B F8");
            if (pattern.empty())
                return;

            static auto ShadowMapSizeCB = []()
            {
                nShadowMapSize = static_cast<uint32_t>(FusionFixSettings.GetInt(PREF_SHADOWMAPSIZE));
            };

            ShadowMapSizeCB();

            static auto CreateShadowMapsHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
            {
                // The four cascades share one texture four times as wide as this, and a texture
                // wider than the device allows fails every format the engine tries and leaves
                // shadows switched off with nothing on screen to say why.
                if (nShadowMapSize)
                    regs.eax = nShadowMapSize;
            });

            // The maps are only rebuilt when the video settings are applied, so a change lands on
            // the next mode change rather than immediately.
            FusionFix::onIniFileChange() += []()
            {
                ShadowMapSizeCB();
            };
        };
    }
} ShadowMapSize;
