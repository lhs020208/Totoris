#include "../TotorisFinesse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
    FTotorisPieceInputTrace OTrace(int32 Column, int32 Row = 1, uint8 Rotation = 0)
    {
        FTotorisPieceInputTrace Trace;
        Trace.bValid = true;
        Trace.MinoIndex = static_cast<uint8>(ETotorisMino::O);
        Trace.SpawnPosition = TotorisGeneration::SpawnPosition(ETotorisMino::O);
        Trace.FinalPosition = FIntPoint(Column, Row);
        Trace.FinalRotation = Rotation;
        return Trace;
    }

    void AddInput(FTotorisPieceInputTrace& Trace, ETotorisFinesseInput Type,
        FIntPoint Before, FIntPoint After, uint8 BeforeRotation = 0,
        uint8 AfterRotation = 0)
    {
        FTotorisFinesseInputEvent Event;
        Event.Input = Type;
        Event.BeforePosition = Before;
        Event.AfterPosition = After;
        Event.BeforeRotation = BeforeRotation;
        Event.AfterRotation = AfterRotation;
        Event.bSucceeded = true;
        Trace.Events.Add(Event);
    }

    int32 Status(const FTotorisPieceFinesseEvaluation& Result)
    {
        return static_cast<int32>(Result.Judgement);
    }
    int32 Reason(const FTotorisPieceFinesseEvaluation& Result)
    {
        return static_cast<int32>(Result.ExclusionReason);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisFinesseStandardEvaluationTest,
    "Totoris.Finesse.StandardPlacementEvaluation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisFinesseStandardEvaluationTest::RunTest(const FString& Parameters)
{
    using namespace TotorisFinesse;
    TSet<FIntPoint> Empty;
    const FIntPoint Spawn = TotorisGeneration::SpawnPosition(ETotorisMino::O);
    const int32 Optimal = static_cast<int32>(ETotorisFinesseJudgement::Optimal);
    const int32 Fault = static_cast<int32>(ETotorisFinesseJudgement::Fault);
    const int32 Excluded = static_cast<int32>(ETotorisFinesseJudgement::Excluded);

    // A gravity-only lock is a zero-input placement when the occupied cells
    // match an ordinary hard drop from spawn.
    const auto NoInput = EvaluateStandardPlacement(OTrace(Spawn.X), Empty, false);
    TestEqual(TEXT("Gravity-only O has optimal zero-input finesse"), Status(NoInput), Optimal);
    TestEqual(TEXT("Gravity-only O uses zero physical inputs"), NoInput.ActualInputs, 0);
    TestEqual(TEXT("Spawn-position O minimum is zero"), NoInput.MinimumInputs, 0);

    // Initial right press + three automatic steps reaches the physical wall.
    // All automatic steps belong to that single physical key-down.
    auto DAS = OTrace(8);
    AddInput(DAS, ETotorisFinesseInput::MoveRight, Spawn, Spawn + FIntPoint(1, 0));
    AddInput(DAS, ETotorisFinesseInput::AutoMoveRight,
        Spawn + FIntPoint(1, 0), FIntPoint(8, Spawn.Y));
    const auto Wall = EvaluateStandardPlacement(DAS, Empty, false);
    TestEqual(TEXT("DAS-to-wall is optimal"), Status(Wall), Optimal);
    TestEqual(TEXT("DAS-to-wall consumes one key press"), Wall.ActualInputs, 1);
    TestEqual(TEXT("DAS-to-wall reference costs one"), Wall.MinimumInputs, 1);

    // Three physical inputs for a one-input placement = one fault, two excess.
    auto Waste = OTrace(Spawn.X + 1);
    AddInput(Waste, ETotorisFinesseInput::MoveLeft, Spawn, Spawn + FIntPoint(-1, 0));
    AddInput(Waste, ETotorisFinesseInput::MoveRight,
        Spawn + FIntPoint(-1, 0), Spawn);
    AddInput(Waste, ETotorisFinesseInput::MoveRight,
        Spawn, Spawn + FIntPoint(1, 0));
    const auto WasteResult = EvaluateStandardPlacement(Waste, Empty, false);
    TestEqual(TEXT("Unnecessary left-right detour is a finesse fault"), Status(WasteResult), Fault);
    TestEqual(TEXT("Wasted placement actual input count"), WasteResult.ActualInputs, 3);
    TestEqual(TEXT("Wasted placement minimum input count"), WasteResult.MinimumInputs, 1);
    TestEqual(TEXT("Wasted placement excess input count"), WasteResult.ExcessInputs, 2);

    auto Precharged = OTrace(8);
    AddInput(Precharged, ETotorisFinesseInput::AutoMoveRight,
        Spawn, FIntPoint(8, Spawn.Y));
    auto PrechargedResult = EvaluateStandardPlacement(Precharged, Empty, false);
    TestEqual(TEXT("Auto-repeat without a current-piece press is excluded"), Status(PrechargedResult), Excluded);
    TestEqual(TEXT("Precharged/carry DAS reason"), Reason(PrechargedResult),
        static_cast<int32>(ETotorisFinesseExclusionReason::PrechargedOrCarriedDAS));

    auto Timed = OTrace(7);
    AddInput(Timed, ETotorisFinesseInput::MoveRight, Spawn, Spawn + FIntPoint(1, 0));
    AddInput(Timed, ETotorisFinesseInput::AutoMoveRight,
        Spawn + FIntPoint(1, 0), FIntPoint(7, Spawn.Y));
    const auto TimedResult = EvaluateStandardPlacement(Timed, Empty, false);
    TestEqual(TEXT("Intermediate timed DAS is excluded, not faulted"), Status(TimedResult), Excluded);
    TestEqual(TEXT("Timed DAS exclusion reason"), Reason(TimedResult),
        static_cast<int32>(ETotorisFinesseExclusionReason::TimedOrBlockedDAS));

    auto SoftDrop = OTrace(Spawn.X);
    SoftDrop.bUsedSoftDrop = true;
    TestEqual(TEXT("Soft drop is not graded by standard reference"),
        Status(EvaluateStandardPlacement(SoftDrop, Empty, false)), Excluded);
    TestEqual(TEXT("All-Mini+ spin is excluded before the board changes"),
        Status(EvaluateStandardPlacement(OTrace(Spawn.X), Empty, true)), Excluded);

    auto Held = OTrace(Spawn.X);
    AddInput(Held, ETotorisFinesseInput::RejectedHold, Spawn, Spawn);
    TestEqual(TEXT("Rejected HOLD is deferred to the special-input policy"),
        Status(EvaluateStandardPlacement(Held, Empty, false)), Excluded);

    // Before-lock cells can block the reference route even if the landing is
    // otherwise legal; an empty-board reference must not generate a fault.
    TSet<FIntPoint> AirBlocker;
    AirBlocker.Add(FIntPoint(8, Spawn.Y));
    const auto AirBlocked = EvaluateStandardPlacement(DAS, AirBlocker, false);
    TestEqual(TEXT("Reference path obstructed by real board is excluded"), Status(AirBlocked), Excluded);
    TestEqual(TEXT("Obstructed reference exclusion reason"), Reason(AirBlocked),
        static_cast<int32>(ETotorisFinesseExclusionReason::ReferencePathBlocked));

    TSet<FIntPoint> BottomBlocker;
    BottomBlocker.Add(FIntPoint(Spawn.X + 1, 1));
    TestEqual(TEXT("Normal hard-drop landing atop the stack can be graded"),
        Status(EvaluateStandardPlacement(OTrace(Spawn.X, 2), BottomBlocker, false)), Optimal);
    TestEqual(TEXT("Unsupported gravity/tuck landing is excluded"),
        Status(EvaluateStandardPlacement(OTrace(Spawn.X, 4), BottomBlocker, false)), Excluded);

    auto NoEventsAtWall = OTrace(8);
    TestEqual(TEXT("A below-reference trace is excluded instead of accepted"),
        Status(EvaluateStandardPlacement(NoEventsAtWall, Empty, false)), Excluded);

    auto RotatedO = OTrace(Spawn.X, 1, 2);
    AddInput(RotatedO, ETotorisFinesseInput::Rotate180, Spawn, Spawn, 0, 2);
    FSearchOptions Exact;
    Exact.Match = ETargetMatch::ExactOriginAndRotation;
    const auto ExactOne = EvaluateStandardPlacement(RotatedO, Empty, false, Exact);
    TestEqual(TEXT("One 180 matches exact-rotation reference"), Status(ExactOne), Optimal);
    Exact.Rotate180Cost = 2;
    const auto ExactTwo = EvaluateStandardPlacement(RotatedO, Empty, false, Exact);
    TestEqual(TEXT("180 cost 2 affects both reference and actual"), Status(ExactTwo), Optimal);
    TestEqual(TEXT("180 exact reference cost 2"), ExactTwo.MinimumInputs, 2);

    return true;
}
#endif
