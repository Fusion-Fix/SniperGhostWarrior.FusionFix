module;

#include <common.hxx>

export module skipintro;

import common;
import chrome;
import settings;

// The game already ships the switch this needs. `-nologos` on the command line sets a single byte
// that the intro state machine reads twice: once in the publisher logo gate, where a non-zero value
// returns before logo.wmv is played, and once in StartIntro, where a non-zero value calls
// EndIntro(false) and drops straight to the main menu. Setting that byte is therefore the same path
// the developers used, rather than a branch of our own invention.
//
// It covers the logo, the intro movie and the "press start" attract screen together. The animated
// main menu background, in-game cutscenes and the credits are reached separately and are untouched.
class SkipIntro
{
public:
    SkipIntro()
    {
        FusionFix::onGameDLLInitEvent() += []()
        {
            // The StartIntro gate. The absolute address the CMP reads is the flag itself.
            auto pattern = gamedll_pattern("80 3D ? ? ? ? 00 57 8B F9 74 18 8B 07 8B 90 B4 01 00 00 6A 00 C7 87 44 05 00 00");
            uint8_t* pNoLogos = pattern.empty() ? nullptr : *pattern.get_first<uint8_t*>(2);

            // A separate prompt, shown when a level has finished loading rather than at startup.
            auto keyPattern = gamedll_pattern("80 A6 8C 01 00 00 BF 80 3D ? ? ? ? 00 75 21 8B 0D ? ? ? ? 85 C9 74 0E");
            uint8_t* pSkipPressAnyKey = keyPattern.empty() ? nullptr : *keyPattern.get_first<uint8_t*>(9);

            if (pNoLogos == nullptr && pSkipPressAnyKey == nullptr)
                return;

            // Whatever the command line already decided, so turning the setting off gives the game
            // back the state it would have had.
            static const auto nNoLogosDefault = pNoLogos != nullptr ? *pNoLogos : uint8_t(0);
            static const auto nSkipPressAnyKeyDefault = pSkipPressAnyKey != nullptr ? *pSkipPressAnyKey : uint8_t(0);

            static auto SkipIntroCB = [pNoLogos, pSkipPressAnyKey]()
            {
                if (pNoLogos != nullptr)
                {
                    const auto nValue = FusionFixSettings.GetInt(PREF_SKIPINTRO) ? uint8_t(1) : nNoLogosDefault;
                    injector::WriteMemory<uint8_t>(pNoLogos, nValue, true);
                }

                if (pSkipPressAnyKey != nullptr)
                {
                    const auto nValue = FusionFixSettings.GetInt(PREF_SKIPPRESSANYKEY) ? uint8_t(1) : nSkipPressAnyKeyDefault;
                    injector::WriteMemory<uint8_t>(pSkipPressAnyKey, nValue, true);
                }
            };

            SkipIntroCB();

            // The flags are read at startup, so a change made mid-session lands on the next launch.
            // Written anyway, because the ini is the state the next launch reads.
            FusionFix::onIniFileChange() += []()
            {
                SkipIntroCB();
            };
        };
    }
} SkipIntro;
