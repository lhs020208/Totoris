#include "../TotorisScoring.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisScoringTest,
    "Totoris.Scoring.PlacementRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisScoringTest::RunTest(const FString& Parameters)
{
    using namespace TotorisScoring;

    const auto Single = CalculatePlacement(ETotorisClassicMode::Sprint,
        ETotorisSpinKind::None, 1, false, false, 0, false, 1);
    TestEqual(TEXT("Level 1 single"), Single.Total(), int64(100));

    const auto Double = CalculatePlacement(ETotorisClassicMode::Endless,
        ETotorisSpinKind::None, 2, false, false, 0, false, 1);
    TestEqual(TEXT("Level 1 double"), Double.Total(), int64(300));

    const auto SpinDouble = CalculatePlacement(ETotorisClassicMode::Sprint,
        ETotorisSpinKind::Full, 2, true, false, 0, false, 1);
    TestEqual(TEXT("Level 1 full spin double"), SpinDouble.Total(), int64(1200));

    const auto MiniSingle = CalculatePlacement(ETotorisClassicMode::Sprint,
        ETotorisSpinKind::Mini, 1, true, false, 0, false, 1);
    TestEqual(TEXT("Level 1 mini spin single"), MiniSingle.Total(), int64(200));

    const auto BackToBackQuad = CalculatePlacement(ETotorisClassicMode::Sprint,
        ETotorisSpinKind::None, 4, false, true, 0, false, 1);
    TestEqual(TEXT("B2B quad base"), BackToBackQuad.LineClearScore, int64(800));
    TestEqual(TEXT("B2B quad bonus"), BackToBackQuad.BackToBackBonusScore, int64(400));
    TestEqual(TEXT("B2B quad total"), BackToBackQuad.Total(), int64(1200));

    const auto ComboTwo = CalculatePlacement(ETotorisClassicMode::Sprint,
        ETotorisSpinKind::None, 1, false, false, 2, false, 1);
    TestEqual(TEXT("Combo count 2 bonus"), ComboTwo.ComboBonusScore, int64(100));

    const auto AllClearQuad = CalculatePlacement(ETotorisClassicMode::Sprint,
        ETotorisSpinKind::None, 4, false, false, 0, true, 1);
    TestEqual(TEXT("Quad all clear"), AllClearQuad.Total(), int64(4300));

    const auto BlitzQuad = CalculatePlacement(ETotorisClassicMode::Blitz,
        ETotorisSpinKind::None, 4, false, false, 0, false, 5);
    TestEqual(TEXT("Blitz level 5 quad"), BlitzQuad.Total(), int64(4000));

    const auto BlitzAllClear = CalculatePlacement(ETotorisClassicMode::Blitz,
        ETotorisSpinKind::None, 4, false, false, 0, true, 15);
    TestEqual(TEXT("Blitz level 15 all clear bonus"), BlitzAllClear.AllClearBonusScore, int64(52500));

    TestTrue(TEXT("Blitz accepts only a three-corner T spin"),
        IsBlitzSpinRecognized(ETotorisMino::T, ETotorisSpinKind::Mini, true));
    TestFalse(TEXT("Blitz rejects All-Mini+ non-T spin"),
        IsBlitzSpinRecognized(ETotorisMino::O, ETotorisSpinKind::Mini, true));
    TestTrue(TEXT("Recognized spin clear is B2B eligible"), IsBackToBackEligible(true, 1));
    TestFalse(TEXT("Spin zero does not advance B2B"), IsBackToBackEligible(true, 0));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
