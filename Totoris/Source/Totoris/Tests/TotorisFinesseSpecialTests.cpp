#include "../TotorisFinesse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
    FTotorisPieceInputTrace MakeTrace(ETotorisMino Mino, FIntPoint End,
        uint8 EndRotation = 0)
    {
        FTotorisPieceInputTrace Trace;
        Trace.bValid = true;
        Trace.MinoIndex = static_cast<uint8>(Mino);
        Trace.SpawnPosition = TotorisGeneration::SpawnPosition(Mino);
        Trace.FinalPosition = End;
        Trace.FinalRotation = EndRotation;
        return Trace;
    }

    void AddTurn(FTotorisPieceInputTrace& Trace,
        ETotorisFinesseInput Action, FIntPoint Position,
        uint8 FromRotation, uint8 ToRotation, int32 Kick = 0)
    {
        FTotorisFinesseInputEvent Event;
        Event.Input = Action;
        Event.bSucceeded = true;
        Event.BeforePosition = Position;
        Event.AfterPosition = Position;
        Event.BeforeRotation = FromRotation;
        Event.AfterRotation = ToRotation;
        Event.KickIndex = Kick;
        Trace.Events.Add(Event);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisFinesseSpecialPlacementTest,
    "Totoris.Finesse.SpecialPlacementAnalysis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisFinesseSpecialPlacementTest::RunTest(const FString& Parameters)
{
    using namespace TotorisFinesse;
    TSet<FIntPoint> Empty;
    const auto OCenter = MakeTrace(ETotorisMino::O, FIntPoint(4, 1));
    const auto Ordinary = FindMinimumOccupiedInputs(
        OCenter, Empty, ETotorisSpinKind::None, false);
    TestTrue(TEXT("Empty board occupied search finds central O"), Ordinary.bFound);
    TestEqual(TEXT("Center O costs zero inputs"), Ordinary.MinimumInputs, 0);
    TestFalse(TEXT("No arbitrary descent in graded search"), Ordinary.bUsedFreeDescent);

    const auto OWall = MakeTrace(ETotorisMino::O, FIntPoint(8, 1));
    const auto WallResult = FindMinimumOccupiedInputs(
        OWall, Empty, ETotorisSpinKind::None, false);
    TestTrue(TEXT("Wall O is reachable"), WallResult.bFound);
    TestEqual(TEXT("DAS O wall is one input"), WallResult.MinimumInputs, 1);

    auto Soft = OCenter;
    Soft.bUsedSoftDrop = true;
    FTotorisFinesseInputEvent SoftEvent;
    SoftEvent.Input = ETotorisFinesseInput::SoftDropPress;
    SoftEvent.bSucceeded = true;
    Soft.Events.Add(SoftEvent);
    const auto SoftAnalysis = AnalyzeSpecialPlacement(
        Soft, Empty, ETotorisSpinKind::None);
    TestEqual(TEXT("Soft-drop classification"),
        static_cast<int32>(SoftAnalysis.Kind),
        static_cast<int32>(ETotorisFinesseSpecialKind::SoftDrop));
    TestTrue(TEXT("Soft-drop advisory reference exists"), SoftAnalysis.bAdvisoryPathFound);
    TestEqual(TEXT("Soft-drop remains excluded, never faulty"),
        static_cast<int32>(EvaluateStandardPlacement(Soft, Empty, false).Judgement),
        static_cast<int32>(ETotorisFinesseJudgement::Excluded));

    // Non-T immobility: left wall, floor, blocker on right and above.
    // The input trace represents a final O rotation at the target location.
    TSet<FIntPoint> MiniBoard;
    MiniBoard.Add(FIntPoint(2, 1));
    MiniBoard.Add(FIntPoint(0, 3));
    auto MiniTrace = MakeTrace(ETotorisMino::O, FIntPoint(0, 1), 1);
    AddTurn(MiniTrace, ETotorisFinesseInput::RotateCW, FIntPoint(0, 1), 0, 1);
    const auto MiniSpin = TotorisGeneration::DetectSpin(ETotorisMino::O,
        MiniTrace.FinalPosition, 1, true, false, 0, MiniBoard, 40);
    TestEqual(TEXT("O is All-Mini+ mini on constrained board"),
        static_cast<int32>(MiniSpin), static_cast<int32>(ETotorisSpinKind::Mini));
    const auto MiniAnalysis = AnalyzeSpecialPlacement(MiniTrace, MiniBoard, MiniSpin);
    TestEqual(TEXT("Non-T spin classified as All-Mini+"),
        static_cast<int32>(MiniAnalysis.Kind),
        static_cast<int32>(ETotorisFinesseSpecialKind::AllMiniPlusSpin));
    TestTrue(TEXT("Final O rotation trace checked against DetectSpin"),
        MiniAnalysis.bFinalSpinRotationVerified);
    TestEqual(TEXT("Unverified TETR.IO spin scoring stays excluded"),
        static_cast<int32>(EvaluateSpecialPlacement(
            MiniTrace, MiniBoard, MiniSpin, MiniAnalysis).Judgement),
        static_cast<int32>(ETotorisFinesseJudgement::Excluded));

    // Four corners around the T pivot give a Full T-Spin (first kick).
    TSet<FIntPoint> TBoard;
    for (const FIntPoint& P : {FIntPoint(3, 3), FIntPoint(5, 3),
                              FIntPoint(3, 1), FIntPoint(5, 1)}) TBoard.Add(P);
    auto TTrace = MakeTrace(ETotorisMino::T, FIntPoint(3, 1), 0);
    AddTurn(TTrace, ETotorisFinesseInput::RotateCCW, FIntPoint(3, 1), 1, 0);
    const auto TSpin = TotorisGeneration::DetectSpin(
        ETotorisMino::T, TTrace.FinalPosition, 0,
        true, false, 0, TBoard, 40);
    TestEqual(TEXT("Four-corner T-spin is Full"),
        static_cast<int32>(TSpin), static_cast<int32>(ETotorisSpinKind::Full));
    const auto TAnalysis = AnalyzeSpecialPlacement(TTrace, TBoard, TSpin);
    TestEqual(TEXT("Full T-spin classification"),
        static_cast<int32>(TAnalysis.Kind),
        static_cast<int32>(ETotorisFinesseSpecialKind::TSpinFull));
    TestTrue(TEXT("Final T rotation preserved"), TAnalysis.bFinalSpinRotationVerified);

    // Fake/unfinished trace cannot prove that the last action was a spin.
    auto Invalid = MiniTrace;
    Invalid.Events.Reset();
    const auto InvalidAnalysis = AnalyzeSpecialPlacement(Invalid, MiniBoard, MiniSpin);
    TestFalse(TEXT("Missing rotation is not verified"),
        InvalidAnalysis.bFinalSpinRotationVerified);
    TestEqual(TEXT("A mismatched spin trace must not generate a fault"),
        static_cast<int32>(EvaluateSpecialPlacement(
            Invalid, MiniBoard, MiniSpin, InvalidAnalysis).ExclusionReason),
        static_cast<int32>(ETotorisFinesseExclusionReason::SpinTraceMismatch));

    // A blocker in the airborne DAS route forces natural descent before a
    // lateral entry. This is a supported advisory tuck, NOT a finesse fault.
    TSet<FIntPoint> AirBlocker;
    AirBlocker.Add(FIntPoint(8, TotorisGeneration::SpawnPosition(ETotorisMino::O).Y));
    const auto AirRoute = FindMinimumOccupiedInputs(
        OWall, AirBlocker, ETotorisSpinKind::None, false);
    TestFalse(TEXT("Air-blocked O has no spawn-level reference"), AirRoute.bFound);
    const auto AirAnalysis = AnalyzeSpecialPlacement(
        OWall, AirBlocker, ETotorisSpinKind::None, FSearchOptions{}, true);
    TestEqual(TEXT("Gravity-dependent tuck gets its own classification"),
        static_cast<int32>(AirAnalysis.Kind),
        static_cast<int32>(ETotorisFinesseSpecialKind::DescentDependent));
    TestTrue(TEXT("Advisory descent path found"), AirAnalysis.bAdvisoryPathFound);
    TestTrue(TEXT("Descent flag is preserved"), AirAnalysis.bAdvisoryPathUsesDescent);
    TestEqual(TEXT("No false fault for gravity/tuck board"),
        static_cast<int32>(EvaluateStandardPlacement(
            OWall, AirBlocker, false).Judgement),
        static_cast<int32>(ETotorisFinesseJudgement::Excluded));

    // An artificial high-stack immobile O spin is reachable without descent;
    // its optional Totoris-model grade is opt-in and never used by gameplay.
    const FIntPoint HighSpawn = TotorisGeneration::SpawnPosition(ETotorisMino::O);
    TSet<FIntPoint> HighBoard;
    HighBoard.Add(HighSpawn + FIntPoint(-1, 0));
    HighBoard.Add(HighSpawn + FIntPoint(2, 0));
    HighBoard.Add(HighSpawn + FIntPoint(0, 2));
    HighBoard.Add(HighSpawn + FIntPoint(0, -1));
    auto HighTrace = MakeTrace(ETotorisMino::O, HighSpawn, 1);
    AddTurn(HighTrace, ETotorisFinesseInput::RotateCW, HighSpawn, 0, 1);
    const auto HighKind = TotorisGeneration::DetectSpin(
        ETotorisMino::O, HighSpawn, 1, true, false, 0, HighBoard, 40);
    TestEqual(TEXT("High-stack O spin is immobile"),
        static_cast<int32>(HighKind), static_cast<int32>(ETotorisSpinKind::Mini));
    const auto HighAnalysis = AnalyzeSpecialPlacement(HighTrace, HighBoard, HighKind);
    TestTrue(TEXT("High-stack final rotation verified"), HighAnalysis.bFinalSpinRotationVerified);
    FSearchOptions OptIn;
    OptIn.bGradeVerifiedSpins = true;
    const auto Graded = EvaluateSpecialPlacement(
        HighTrace, HighBoard, HighKind, HighAnalysis, OptIn);
    TestEqual(TEXT("Opt-in grades direct high-stack spin as optimal"),
        static_cast<int32>(Graded.Judgement),
        static_cast<int32>(ETotorisFinesseJudgement::Optimal));
    TestEqual(TEXT("Opt-in O spin needs one input"), Graded.MinimumInputs, 1);

    // A vertical kick is distinguished from plain horizontal movement.
    auto KickTrace = MakeTrace(ETotorisMino::T, FIntPoint(3, 2), 1);
    AddTurn(KickTrace, ETotorisFinesseInput::RotateCW,
        FIntPoint(3, 2), 0, 1, 1);
    KickTrace.Events[0].AfterPosition.Y++;
    KickTrace.FinalPosition.Y++;
    const auto KickAnalysis = AnalyzeSpecialPlacement(
        KickTrace, Empty, ETotorisSpinKind::None);
    TestTrue(TEXT("Vertical kick recorded"), KickAnalysis.bHadVerticalKick);
    TestEqual(TEXT("Vertical kick classified"),
        static_cast<int32>(KickAnalysis.Kind),
        static_cast<int32>(ETotorisFinesseSpecialKind::VerticalKick));
    return true;
}
#endif
