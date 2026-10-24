#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"

namespace TotorisPreferences
{
    inline constexpr const TCHAR* Section = TEXT("Totoris.Preferences");
    inline constexpr const TCHAR* MissionsSection = TEXT("Totoris.MissionSelection");

    inline bool ReadBool(const TCHAR* Key, bool DefaultValue, const TCHAR* InSection = Section)
    {
        bool Value = DefaultValue;
        if (GConfig) GConfig->GetBool(InSection, Key, Value, GGameUserSettingsIni);
        return Value;
    }

    inline void WriteBool(const TCHAR* Key, bool Value, const TCHAR* InSection = Section)
    {
        if (!GConfig) return;
        GConfig->SetBool(InSection, Key, Value, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    inline float SanitizeVolume(float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 2.f) : 1.f;
    }

    inline float ReadVolume()
    {
        float Value = 1.f;
        if (GConfig) GConfig->GetFloat(Section, TEXT("MasterVolume"), Value, GGameUserSettingsIni);
        return SanitizeVolume(Value);
    }

    inline void WriteVolume(float Value)
    {
        if (!GConfig) return;
        GConfig->SetFloat(Section, TEXT("MasterVolume"), SanitizeVolume(Value), GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}
