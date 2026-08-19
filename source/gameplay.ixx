module;

#include <common.hxx>

export module gameplay;

import common;
import chrome;
import settings;

// Chrome Engine 4 carries a decade of Call of Juarez HUD classes that Sniper never instantiates,
// so each option below is aimed at the class that actually draws on screen rather than at the one
// the EHudElements enum makes look obvious.

class Gameplay
{
public:
    Gameplay()
    {
        FusionFix::onGameDLLInitEvent() += []()
        {
            // The silhouette is HUDFpsMain's stature image, one of stand, crouch or lie, picked
            // every frame by HUDFpsMain::SetStature. The visibility bar and its backdrop sit
            // behind it and carry the white to red fill.
            auto staturePattern = gamedll_pattern("53 8B 5C 24 08 8D 43 01 83 F8 03 77 26 56 57 33 F6 8D 79 60");
            auto detectionSimplePattern = gamedll_pattern("8B 4B 58 85 C9 74 08 6A 01 FF 15 ? ? ? ? 8B 4B 54 85 C9 74 08 6A 01 FF 15 ? ? ? ? 8B 8B 9C 00 00 00");
            auto detectionPattern = gamedll_pattern("85 C9 74 08 6A 01 FF 15 ? ? ? ? 8B 4B 54 85 C9 74 08 6A 01 FF 15 ? ? ? ? 8B 4B 5C");

            // The two SetVisible calls on the group HUD3dMapLocation::Update places each frame.
            // Hiding the group takes the dot, the distance text, its shadow and the off screen
            // arrow with it.
            auto waypointPattern = gamedll_pattern("DF E0 F6 C4 41 75 12 B0 01 8B CB 50 FF 15 ? ? ? ? D9 E8 D9 5B 74 EB 2C");
            auto waypointGroupPattern = gamedll_pattern("8A 43 1C A8 02 74 08 A8 04 74 04 B0 01 EB 02 32 C0 50 8B CB FF 15");

            if (!staturePattern.empty() && !detectionSimplePattern.empty() && !detectionPattern.empty())
            {
                // The setz that decides which of the three stature images is the visible one,
                // turned into a plain zero so none of them is.
                static raw_mem fnStature(staturePattern.get_first(28), { 0x31, 0xD2, 0x90 });

                // The pushed argument of each bar and backdrop SetVisible call, turned from true
                // into false.
                static raw_mem fnSimpleBar(detectionSimplePattern.get_first(8), { 0x00 });
                static raw_mem fnSimpleBackground(detectionSimplePattern.get_first(23), { 0x00 });
                static raw_mem fnBar(detectionPattern.get_first(5), { 0x00 });
                static raw_mem fnBackground(detectionPattern.get_first(20), { 0x00 });

                static auto DetectionIndicatorCB = []()
                {
                    if (FusionFixSettings.GetInt(PREF_NODETECTIONINDICATOR))
                    {
                        fnStature.Write();
                        fnSimpleBar.Write();
                        fnSimpleBackground.Write();
                        fnBar.Write();
                        fnBackground.Write();
                    }
                    else
                    {
                        fnStature.Restore();
                        fnSimpleBar.Restore();
                        fnSimpleBackground.Restore();
                        fnBar.Restore();
                        fnBackground.Restore();
                    }
                };

                DetectionIndicatorCB();
                FusionFix::onIniFileChange() += []() { DetectionIndicatorCB(); };
            }

            if (!waypointPattern.empty() && !waypointGroupPattern.empty())
            {
                // Both sites load the visibility argument into AL. Zeroing it hides the group.
                static raw_mem fnWaypoint(waypointPattern.get_first(7), { 0x32, 0xC0 });
                static raw_mem fnWaypointGroup(waypointGroupPattern.get_first(11), { 0x32, 0xC0 });

                static auto WaypointMarkerCB = []()
                {
                    if (FusionFixSettings.GetInt(PREF_NOWAYPOINTMARKER))
                    {
                        fnWaypoint.Write();
                        fnWaypointGroup.Write();
                    }
                    else
                    {
                        fnWaypoint.Restore();
                        fnWaypointGroup.Restore();
                    }
                };

                WaypointMarkerCB();
                FusionFix::onIniFileChange() += []() { WaypointMarkerCB(); };
            }
        };
    }
} Gameplay;
