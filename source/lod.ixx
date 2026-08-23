module;

#include <common.hxx>

export module lod;

import common;
import chrome;
import settings;

// The LOD switch distances themselves are per mesh, stored as squares inside
// SMeshVisibilityParams, so there is no one table to raise. Every visibility test first scales
// the squared camera distance by a single field on the game object, at +0xB8, which the camera
// projection update recomputes from the current zoom as k / s^2. The geometry LOD walk, the
// shader LOD walk and the vis_range cull all read that same scaled distance, so dividing the
// field by N^2 pushes every switch and the cull out by N without touching a mesh.

static float fLodDistanceScale = 1.0f;

class LodDistance
{
public:
    LodDistance()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            // The tail of the projection update: the divide that produces k / s^2 followed by the
            // store into +0xB8. The hook sits on the instruction after the store, where ECX still
            // holds the game object, so the multiplier rides on top of whatever the zoom decided
            // rather than replacing it.
            auto pattern = engine_pattern("D9 C2 DE F2 D9 C9 D9 99 B4 00 00 00 DE F9 D9 99 B8 00 00 00");
            if (pattern.empty())
                return;

            static auto LodDistanceCB = []()
            {
                const auto nMultiplier = 1 << FusionFixSettings.GetInt(PREF_LODDISTANCE);
                fLodDistanceScale = 1.0f / static_cast<float>(nMultiplier * nMultiplier);
            };

            LodDistanceCB();

            static auto LodDistanceHook = safetyhook::create_mid(pattern.get_first(20), [](SafetyHookContext& regs)
            {
                *reinterpret_cast<float*>(regs.ecx + 0xB8) *= fLodDistanceScale;
            });

            // The field is rewritten on every camera update, so a change lands the same frame.
            FusionFix::onIniFileChange() += []()
            {
                LodDistanceCB();
            };
        };
    }
} LodDistance;
