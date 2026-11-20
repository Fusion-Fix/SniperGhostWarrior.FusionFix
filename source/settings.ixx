module;

#include <common.hxx>
#include <asi_settings/asi_settings.hpp>
#include <atomic>

export module settings;

import common;

export enum Pref
{
    PREF_SKIPINTRO, PREF_SKIPPRESSANYKEY, PREF_DISPLAYMODE, PREF_INTERNALRESOLUTIONX,
    PREF_INTERNALRESOLUTIONY, PREF_SCALINGFILTER, PREF_VSYNC, PREF_MAXFRAMERATE,
    PREF_ANISOTROPICFILTERING, PREF_LODDISTANCE, PREF_SHADOWMAPSIZE, PREF_FIELDOFVIEW,
    PREF_NODETECTIONINDICATOR, PREF_NOWAYPOINTMARKER, COUNT
};

namespace FixSettings
{
    asi_settings::Plugin& Plugin()
    {
        static asi_settings::Plugin plugin;
        return plugin;
    }
}

export class CSettings
{
    static inline std::array<std::atomic<int32_t>, COUNT> values{};
    static inline std::atomic<float> fieldOfView{ 91.35f };
    static constexpr std::array<const char*, COUNT> ids{
        "General.SkipIntro", "General.SkipPressAnyKey", "Display.DisplayMode", "Display.InternalResolutionX",
        "Display.InternalResolutionY", "Display.ScalingFilter", "Display.VSync", "Display.MaxFrameRate",
        "Graphics.AnisotropicFiltering", "Graphics.LODDistance", "Graphics.ShadowMapSize", "FieldOfView.FieldOfView",
        "Gameplay.NoDetectionIndicator", "Gameplay.NoWaypointMarker"
    };

    static void Refresh()
    {
        auto& plugin = FixSettings::Plugin();
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (i != PREF_FIELDOFVIEW)
                values[i] = plugin.Int(ids[i]);
        }
        auto x = values[PREF_INTERNALRESOLUTIONX].load();
        auto y = values[PREF_INTERNALRESOLUTIONY].load();
        if (x < 1 || y < 1)
        {
            x = 0;
            y = 0;
        }
        else
        {
            x = std::clamp(x, 320, 16384);
            y = std::clamp(y, 240, 16384);
        }
        values[PREF_INTERNALRESOLUTIONX] = x;
        values[PREF_INTERNALRESOLUTIONY] = y;
        auto aniso = std::clamp(values[PREF_ANISOTROPICFILTERING].load(), 0, 16);
        values[PREF_ANISOTROPICFILTERING] = aniso == 1 ? 0 : aniso;
        auto shadow = values[PREF_SHADOWMAPSIZE].load();
        values[PREF_SHADOWMAPSIZE] = shadow > 0 ? static_cast<int32_t>(std::bit_ceil(static_cast<uint32_t>(std::clamp(shadow, 256, 4096)))) : 0;
        values[PREF_MAXFRAMERATE] = std::clamp(values[PREF_MAXFRAMERATE].load(), 0, 1000);
        fieldOfView = std::clamp(plugin.Float(ids[PREF_FIELDOFVIEW]), 45.0f, 140.0f);
    }

public:
    static void Initialize()
    {
        auto& plugin = FixSettings::Plugin();
        if (!plugin.Load())
            OutputDebugStringA(("Fusion Fix: settings unavailable, " + plugin.Error() + ".\n").c_str());
        Refresh();
        for (const auto* id : ids)
        {
            plugin.OnChange(id, []()
            {
                Refresh();
                FusionFix::onSettingsChange().executeAll();
            });
        }
        asi_settings::LiveLink::Start();
    }

    int32_t GetInt(Pref name) { return values[name].load(); }
    float GetFloat(Pref name) { return fieldOfView.load(); }
} FusionFixSettings;

ASI_SETTINGS_EXPORT(FixSettings::Plugin())
