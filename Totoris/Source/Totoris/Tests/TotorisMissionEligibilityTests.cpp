#include "../TotorisMission.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisMissionGarbageEligibilityTest,
    "Totoris.Missions.GarbageEligibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisMissionGarbageEligibilityTest::RunTest(const FString& Parameters)
{
    const TSet<FName> GarbageMissions = {
        TEXT("E_Send_6_Attack"), TEXT("C_Send_14_Attack"),
        TEXT("C_Clear_Garbage_LJ_Spin"), TEXT("C_Clear_Garbage_SZ_Spin"),
        TEXT("B_No_Garbage_4_Seconds"), TEXT("B_Dont_Cancel_Garbage_8_Seconds")
    };
    int32 Found = 0;
    for (const auto& Mission : TotorisMissions::All())
    {
        const bool bRequiresGarbage = GarbageMissions.Contains(Mission.Id);
        Found += bRequiresGarbage ? 1 : 0;
        TestEqual(*FString::Printf(TEXT("OFF: %s"), *Mission.Id.ToString()),
            TotorisMissions::IsAllowedByGarbageSetting(Mission, false), !bRequiresGarbage);
        TestTrue(TEXT("ON allows every catalogue entry"),
            TotorisMissions::IsAllowedByGarbageSetting(Mission, true));
        TestEqual(TEXT("Switching back OFF rechecks current setting"),
            TotorisMissions::IsAllowedByGarbageSetting(Mission, false), !bRequiresGarbage);
    }
    TestEqual(TEXT("All six garbage/attack missions covered"), Found, GarbageMissions.Num());
    for (int32 Tier = 0; Tier < static_cast<int32>(ETotorisMissionTier::S); ++Tier)
    {
        int32 Eligible = 0;
        for (const auto& Mission : TotorisMissions::ForTier(static_cast<ETotorisMissionTier>(Tier)))
            Eligible += TotorisMissions::IsAllowedByGarbageSetting(Mission, false) ? 1 : 0;
        TestTrue(TEXT("Each normal tier can still supply a three-mission set"), Eligible >= 3);
    }
    return true;
}
#endif
