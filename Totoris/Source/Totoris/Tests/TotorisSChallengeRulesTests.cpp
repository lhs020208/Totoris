#include "../TotorisSChallengeRules.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisSChallengeRulesTest, "Totoris.Missions.SChallengeCounters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTotorisSChallengeRulesTest::RunTest(const FString& Parameters)
{
    using namespace TotorisSChallengeRules;
    FTotorisMissionPlacementEvent E;
    E.Mino = ETotorisMino::T; E.SpinKind = ETotorisSpinKind::Full; E.bDifficultClear = true;
    int32 Stage = 0;
    for (int32 Lines = 1; Lines <= 3; ++Lines)
    {
        E.ClearedLines = Lines;
        Stage = AdvanceTSpinSequence(Stage, E);
        TestEqual(TEXT("TSS TSD TST sequence"), Stage, Lines);
    }
    E.ClearedLines = 1;
    TestEqual(TEXT("Wrong full spin resets, no restart on same placement"), AdvanceTSpinSequence(1, E), 0);
    E.SpinKind = ETotorisSpinKind::Mini; E.ClearedLines = 2;
    TestEqual(TEXT("Mini resets"), AdvanceTSpinSequence(1, E), 0);
    E.ClearedLines = 0;
    TestEqual(TEXT("Nonclearing mini resets"), AdvanceTSpinSequence(1, E), 0);
    E.SpinKind = ETotorisSpinKind::None;
    TestEqual(TEXT("Nonclearing placement preserves chain"), AdvanceTSpinSequence(1, E), 1);
    E.ClearedLines = 4;
    TestEqual(TEXT("Quad preserves sequence"), AdvanceTSpinSequence(1, E), 1);
    E.ClearedLines = 1; E.bDifficultClear = false;
    TestEqual(TEXT("Broken chain resets"), AdvanceTSpinSequence(1, E), 0);
    E.ComboCount = 10;
    TestFalse(TEXT("Ten combo without PC or color clear is insufficient"), CompletesComboPC(E));
    E.bPerfectClear = true; E.ComboCount = 9;
    TestFalse(TEXT("Nine combo PC insufficient"), CompletesComboPC(E));
    E.ComboCount = 10; TestTrue(TEXT("Ten combo PC"), CompletesComboPC(E));
    E.ComboCount = 12; TestTrue(TEXT("Over ten combo PC"), CompletesComboPC(E));
    E.ComboCount = -1; TestFalse(TEXT("Broken combo"), CompletesComboPC(E));
    E.bPerfectClear = false; E.bColorClear = true; E.ComboCount = 9;
    TestFalse(TEXT("Nine combo color clear insufficient"), CompletesComboPC(E));
    E.ComboCount = 10; TestTrue(TEXT("Ten combo color clear"), CompletesComboPC(E));
    E.ComboCount = 12; TestTrue(TEXT("Over ten combo color clear"), CompletesComboPC(E));
    E.ClearedLines = 0; TestFalse(TEXT("Color clear requires line clear"), CompletesComboPC(E));
    E.ClearedLines = 1; E.ComboCount = -1;
    TestFalse(TEXT("Broken combo color clear"), CompletesComboPC(E));
    E.UniqueSpinCenterCount = 4; TestFalse(TEXT("Four centers insufficient"), CompletesUniqueSpins(E));
    E.UniqueSpinCenterCount = 5; TestTrue(TEXT("Five centers and clear"), CompletesUniqueSpins(E));
    E.ClearedLines = 0; TestFalse(TEXT("Five centers without clear"), CompletesUniqueSpins(E));
    TSet<FIntPoint> Centers;
    Centers.Add(FIntPoint(2, 3)); Centers.Add(FIntPoint(2, 3));
    TestEqual(TEXT("Repeated center neither increments nor resets"), Centers.Num(), 1);
    uint8 Mask = 0;
    E = FTotorisMissionPlacementEvent{};
    E.ClearedLines = 1; E.bDifficultClear = true; E.SpinKind = ETotorisSpinKind::Mini;
    for (int32 Index = 0; Index < 7; ++Index)
    {
        E.Mino = static_cast<ETotorisMino>(Index);
        Mask = AdvanceAllSpins(Mask, E);
    }
    TestEqual(TEXT("Six spin types excluding O, minis accepted"), int32(Mask), 0x6F);
    E.ClearedLines = 0;
    TestEqual(TEXT("Nonclear preserves spin checklist"), int32(AdvanceAllSpins(Mask, E)), 0x6F);
    E.ClearedLines = 4; E.SpinKind = ETotorisSpinKind::None;
    TestEqual(TEXT("Quad preserves spin checklist"), int32(AdvanceAllSpins(Mask, E)), 0x6F);
    E.ClearedLines = 1; E.bDifficultClear = false;
    TestEqual(TEXT("B2B break resets checklist"), int32(AdvanceAllSpins(Mask, E)), 0);
    Mask = 0;
    for (int32 Index = 0; Index < 7; ++Index)
    {
        E.Mino = static_cast<ETotorisMino>(Index); E.ComboCount = Index;
        Mask = AdvanceAllTypes(Mask, E);
        if (Index < 6) TestTrue(TEXT("Not complete before seventh clear"), Mask != 0x7F);
    }
    TestEqual(TEXT("Seven consecutive different types complete"), int32(Mask), 0x7F);
    TestEqual(TEXT("Duplicate resets without reseeding"), int32(AdvanceAllTypes(Mask, E)), 0);
    E.ClearedLines = 0; E.ComboCount = -1;
    TestEqual(TEXT("Combo break resets all types"), int32(AdvanceAllTypes(3, E)), 0);
    E.ClearedLines = 1; E.ComboCount = 0; E.Mino = ETotorisMino::I;
    TestEqual(TEXT("New combo starts clean"), int32(AdvanceAllTypes(0x7F, E)), 1 << static_cast<uint8>(ETotorisMino::I));
    return true;
}
#endif
