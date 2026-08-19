module;

#include <common.hxx>
#include <FileWatch.hpp>
#include <variant>

export module settings;

import common;

export enum Pref
{
    PREF_SKIPINTRO,
    PREF_SKIPPRESSANYKEY,
    PREF_DISPLAYMODE,
    PREF_VSYNC,
    PREF_MAXFRAMERATE,
    PREF_ANISOTROPICFILTERING,
    PREF_FIELDOFVIEW,

    COUNT,
};

export class CSettings
{
private:
    static inline std::array<std::variant<int32_t, float, std::string>, static_cast<size_t>(Pref::COUNT)> mPrefs;

public:
    static inline void ReadIniSettings()
    {
        CIniReader iniReader("");

        mPrefs[PREF_SKIPINTRO] = std::clamp(iniReader.ReadInteger("General", "SkipIntro", 1), 0, 1);
        mPrefs[PREF_SKIPPRESSANYKEY] = std::clamp(iniReader.ReadInteger("General", "SkipPressAnyKey", 1), 0, 1);

        mPrefs[PREF_DISPLAYMODE] = std::clamp(iniReader.ReadInteger("Display", "DisplayMode", 0), 0, 1);

        mPrefs[PREF_VSYNC] = std::clamp(iniReader.ReadInteger("Display", "VSync", 0), 0, 1);

        mPrefs[PREF_MAXFRAMERATE] = std::clamp(iniReader.ReadInteger("Display", "MaxFrameRate", 0), 0, 1000);


        auto nAnisotropicFiltering = std::clamp(iniReader.ReadInteger("Graphics", "AnisotropicFiltering", 16), 0, 16);
        if (nAnisotropicFiltering == 1)
            nAnisotropicFiltering = 0;
        mPrefs[PREF_ANISOTROPICFILTERING] = nAnisotropicFiltering;

        mPrefs[PREF_FIELDOFVIEW] = std::clamp(iniReader.ReadFloat("FieldOfView", "FieldOfView", 91.35f), 45.0f, 140.0f);

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            if (std::filesystem::exists(iniReader.GetIniPath()))
            {
                static filewatch::FileWatch<std::string> watch(iniReader.GetIniPath().string(), [](const std::string&, const filewatch::Event change_type)
                {
                    if (change_type == filewatch::Event::modified)
                    {
                        ReadIniSettings();
                        FusionFix::onIniFileChange().executeAll();
                    }
                });
            }
        });
    }

public:
    int32_t GetInt(Pref name) { return std::get<int32_t>(mPrefs[name]); }
    float GetFloat(Pref name) { return std::get<float>(mPrefs[name]); }
    std::string GetString(Pref name) { return std::get<std::string>(mPrefs[name]); }
    void SetInt(Pref name, int32_t value) { mPrefs[name] = value; }
    void SetFloat(Pref name, float value) { mPrefs[name] = value; }
    void SetString(Pref name, std::string value) { mPrefs[name] = value; }
} FusionFixSettings;
