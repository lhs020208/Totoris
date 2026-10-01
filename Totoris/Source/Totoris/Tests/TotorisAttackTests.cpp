#include "../TotorisAttack.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisAttackTest,
    "Totoris.Attack.PlacementRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisAttackTest::RunTest(const FString& Parameters)
{
    using namespace TotorisAttack;

    const FTotorisAttackCalculation Double = CalculatePlacement(
        ETotorisSpinKind::None, 2, 0, false, false, false, 0, 0, 15);
    TestEqual(TEXT("Double base attack"), Double.TotalAttack, 1);

    const FTotorisAttackCalculation ComboQuad = CalculatePlacement(
        ETotorisSpinKind::None, 4, 2, true, false, false, 0, 1, 15);
    TestEqual(TEXT("B2B quad at combo 2"), ComboQuad.TotalAttack, 7);

    const FTotorisAttackCalculation PerfectClear = CalculatePlacement(
        ETotorisSpinKind::None, 4, 0, false, true, false, 0, 2, 15);
    TestEqual(TEXT("Season 1 all clear attack"), PerfectClear.TotalAttack, 14);
    TestEqual(TEXT("All clear retains B2B-based surge"), PerfectClear.PendingSurgeAttack, 2);

    const FTotorisAttackCalculation Opener = CalculatePlacement(
        ETotorisSpinKind::Full, 2, 0, false, false, false, 0, 0, 14);
    TestEqual(TEXT("First 14 pieces double attack"), Opener.TotalAttack, 8);

    const FTotorisAttackCalculation SurgeBreak = CalculatePlacement(
        ETotorisSpinKind::None, 1, 0, false, false, false, 3, 0, 15);
    TestEqual(TEXT("Broken B2B releases its matching surge"), SurgeBreak.TotalAttack, 3);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
