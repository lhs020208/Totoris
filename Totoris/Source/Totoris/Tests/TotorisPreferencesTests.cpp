#include "../TotorisPreferences.h"
#include "../TotorisMission.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisPreferencesPersistenceTest,
    "Totoris.Settings.PreferencesPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisPreferencesPersistenceTest::RunTest(const FString& Parameters)
{
    if (!GConfig) return false;
    // Isolated INI: never change the player's actual saved preferences.
    const FString TestFile = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("Automation"), FGuid::NewGuid().ToString() + TEXT(".ini"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(TestFile), true);
    TGuardValue<FString> FileGuard(GGameUserSettingsIni, TestFile);
    // Real GameUserSettings is preloaded by the engine. Register our isolated
    // file too; SetBool deliberately ignores files absent from the cache.
    GConfig->Add(TestFile, FConfigFile());
    using namespace TotorisPreferences;
    TestFalse(TEXT("Quick Start defaults OFF"), ReadBool(TEXT("QuickStart"), false));
    TestTrue(TEXT("New missions default ON"), ReadBool(TEXT("NewMission"), true, MissionsSection));
    TestEqual(TEXT("Default volume"), ReadVolume(), 1.f);
    WriteBool(TEXT("QuickStart"), true);
    WriteBool(TEXT("SitoMode"), true);
    WriteBool(TEXT("MissionEventsEnabled"), false);
    WriteVolume(.42f);
    for (int32 Tier = 0; Tier <= static_cast<int32>(ETotorisMissionTier::S); ++Tier)
        for (const auto& Mission : TotorisMissions::ForTier(static_cast<ETotorisMissionTier>(Tier)))
            WriteBool(*Mission.Id.ToString(), false, MissionsSection);
    GConfig->UnloadFile(TestFile);
    GConfig->LoadFile(TestFile);
    TestTrue(TEXT("Quick Start reloads from disk"), ReadBool(TEXT("QuickStart"), false));
    TestTrue(TEXT("Sito mode reloads from disk"), ReadBool(TEXT("SitoMode"), false));
    TestFalse(TEXT("Event preference reloads from disk"), ReadBool(TEXT("MissionEventsEnabled"), true));
    TestEqual(TEXT("Volume reloads from disk"), ReadVolume(), .42f);
    for (int32 Tier = 0; Tier <= static_cast<int32>(ETotorisMissionTier::S); ++Tier)
        for (const auto& Mission : TotorisMissions::ForTier(static_cast<ETotorisMissionTier>(Tier)))
            TestFalse(*Mission.Id.ToString(), ReadBool(*Mission.Id.ToString(), true, MissionsSection));
    WriteBool(TEXT("QuickStart"), false);
    WriteBool(TEXT("SitoMode"), false);
    WriteBool(TEXT("MissionEventsEnabled"), true);
    WriteVolume(1.f);
    WriteBool(TEXT("S_Combo_10_Perfect_Clear"), true, MissionsSection);
    GConfig->UnloadFile(TestFile);
    GConfig->LoadFile(TestFile);
    TestFalse(TEXT("Quick Start can be saved OFF again"), ReadBool(TEXT("QuickStart"), true));
    TestFalse(TEXT("Reset Sito mode persists"), ReadBool(TEXT("SitoMode"), true));
    TestTrue(TEXT("Reset event preference persists"), ReadBool(TEXT("MissionEventsEnabled"), false));
    TestEqual(TEXT("Reset volume persists"), ReadVolume(), 1.f);
    TestTrue(TEXT("Stable mission ID can be re-enabled"), ReadBool(TEXT("S_Combo_10_Perfect_Clear"), false, MissionsSection));
    TestEqual(TEXT("Volume upper bound"), SanitizeVolume(9.f), 2.f);
    TestEqual(TEXT("Volume lower bound"), SanitizeVolume(-1.f), 0.f);
    GConfig->UnloadFile(TestFile);
    IFileManager::Get().Delete(*TestFile);
    return true;
}
#endif
