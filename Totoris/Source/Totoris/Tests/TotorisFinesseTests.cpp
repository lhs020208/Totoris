#include "../TotorisFinesse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
    bool IsLegalEmptyPlacement(ETotorisMino Mino, FIntPoint Position, uint8 Rotation)
    {
        for (FIntPoint Cell : TotorisGeneration::RotationCells(Mino, Rotation))
        {
            Cell += Position;
            if (Cell.X < 0 || Cell.X >= TotorisGeneration::BoardWidth ||
                Cell.Y < 1 || Cell.Y > 40) return false;
        }
        return true;
    }

    // Independently replay the returned action list with the same gameplay
    // rotation/kick tables. This catches invalid reconstruction and wall paths.
    bool ReplayPath(ETotorisMino Mino, FIntPoint Start, uint8 StartRotation,
        const TArray<TotorisFinesse::EAction>& Actions,
        FIntPoint& OutPosition, uint8& OutRotation)
    {
        FIntPoint Position = Start;
        uint8 Rotation = StartRotation;
        for (TotorisFinesse::EAction Action : Actions)
        {
            int32 Direction = 0;
            if (Action == TotorisFinesse::EAction::TapLeft || Action == TotorisFinesse::EAction::DASLeft)
                Direction = -1;
            if (Action == TotorisFinesse::EAction::TapRight || Action == TotorisFinesse::EAction::DASRight)
                Direction = 1;

            if (Direction != 0)
            {
                if (Action == TotorisFinesse::EAction::TapLeft || Action == TotorisFinesse::EAction::TapRight)
                {
                    Position.X += Direction;
                    if (!IsLegalEmptyPlacement(Mino, Position, Rotation)) return false;
                }
                else
                {
                    const FIntPoint Before = Position;
                    while (IsLegalEmptyPlacement(Mino, Position + FIntPoint(Direction, 0), Rotation))
                        Position.X += Direction;
                    if (Position == Before) return false;
                }
                continue;
            }

            uint8 To = Rotation;
            if (Action == TotorisFinesse::EAction::RotateCW) To = (Rotation + 1) & 3;
            else if (Action == TotorisFinesse::EAction::RotateCCW) To = (Rotation + 3) & 3;
            else To = (Rotation + 2) & 3;
            const TArray<FIntPoint> Kicks = Action == TotorisFinesse::EAction::Rotate180
                ? TotorisGeneration::RotationKicks180(Mino, Rotation, To)
                : TotorisGeneration::RotationKicks(Mino, Rotation, To);
            bool bRotated = false;
            for (const FIntPoint& Kick : Kicks)
            {
                if (IsLegalEmptyPlacement(Mino, Position + Kick, To))
                {
                    Position += Kick;
                    Rotation = To;
                    bRotated = true;
                    break;
                }
            }
            if (!bRotated) return false;
        }
        OutPosition = Position;
        OutRotation = Rotation;
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisFinesseMinimumTest,
    "Totoris.Finesse.StandardAirMinimum",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisFinesseMinimumTest::RunTest(const FString& Parameters)
{
    using namespace TotorisFinesse;
    const FIntPoint OSpawn = TotorisGeneration::SpawnPosition(ETotorisMino::O);
    const auto GetO = [&](int32 X, uint8 R = 0)
    {
        return FindMinimumStandardInputs(ETotorisMino::O, OSpawn, FIntPoint(X, 1), R);
    };

    TestEqual(TEXT("O already at the target needs zero inputs"), GetO(OSpawn.X).MinimumInputs, 0);
    TestEqual(TEXT("O one cell right takes one tap"), GetO(OSpawn.X + 1).MinimumInputs, 1);
    TestEqual(TEXT("O right wall takes one DAS"), GetO(8).MinimumInputs, 1);
    TestEqual(TEXT("O one cell left of right wall takes DAS + tap"), GetO(7).MinimumInputs, 2);
    TestEqual(TEXT("O left wall takes one DAS"), GetO(0).MinimumInputs, 1);
    TestEqual(TEXT("O footprint ignores an unneeded rotation"), GetO(OSpawn.X, 3).MinimumInputs, 0);
    TestFalse(TEXT("O cannot be placed outside the board"), GetO(9).bFound);

    FSearchOptions Exact;
    Exact.Match = ETargetMatch::ExactOriginAndRotation;
    TestEqual(TEXT("Exact O rotation 2 takes one 180 input"),
        FindMinimumStandardInputs(ETotorisMino::O, OSpawn, FIntPoint(OSpawn.X, 1), 2, Exact).MinimumInputs, 1);
    Exact.Rotate180Cost = 2;
    TestEqual(TEXT("Configurable 180 input cost is respected"),
        FindMinimumStandardInputs(ETotorisMino::O, OSpawn, FIntPoint(OSpawn.X, 1), 2, Exact).MinimumInputs, 2);
    Exact.bAllow180 = false;
    TestEqual(TEXT("Without 180, two quarter turns are required"),
        FindMinimumStandardInputs(ETotorisMino::O, OSpawn, FIntPoint(OSpawn.X, 1), 2, Exact).MinimumInputs, 2);

    // Regression: vertical I uses an origin outside the board so its four
    // occupied cells can still reach the extreme leftmost column.
    const FIntPoint ISpawn = TotorisGeneration::SpawnPosition(ETotorisMino::I);
    const FMinimumResult ILeft = FindMinimumStandardInputs(
        ETotorisMino::I, ISpawn, FIntPoint(-2, 1), 1);
    TestTrue(TEXT("Vertical I can occupy the left wall"), ILeft.bFound);
    TestEqual(TEXT("Vertical I at the left wall takes rotation + DAS"), ILeft.MinimumInputs, 2);

    FTotorisPieceInputTrace Trace;
    Trace.bValid = true;
    Trace.MinoIndex = static_cast<uint8>(ETotorisMino::O);
    Trace.SpawnPosition = OSpawn;
    Trace.FinalPosition = FIntPoint(8, 1);
    Trace.FinalRotation = 0;
    Trace.bUsedSoftDrop = true;
    TestEqual(TEXT("Stage 1 trace can request a geometric baseline"),
        FindMinimumStandardInputs(Trace).MinimumInputs, 1);
    Trace.bValid = false;
    TestFalse(TEXT("Invalid trace is rejected"), FindMinimumStandardInputs(Trace).bFound);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisFinesseCoverageTest,
    "Totoris.Finesse.StandardAirReachability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisFinesseCoverageTest::RunTest(const FString& Parameters)
{
    int32 Count = 0;
    for (uint8 Index = 0; Index < 7; ++Index)
    {
        const ETotorisMino Mino = static_cast<ETotorisMino>(Index);
        const FIntPoint Start = TotorisGeneration::SpawnPosition(Mino);
        for (uint8 Rotation = 0; Rotation < 4; ++Rotation)
        {
            for (int32 X = -3; X <= 9; ++X)
            {
                const FIntPoint Goal(X, 1);
                if (!IsLegalEmptyPlacement(Mino, Goal, Rotation)) continue;
                const TotorisFinesse::FMinimumResult Result =
                    TotorisFinesse::FindMinimumStandardInputs(Mino, Start, Goal, Rotation);
                if (!Result.bFound)
                {
                    AddError(FString::Printf(TEXT("Unreachable empty-board goal: type=%d rotation=%d x=%d"),
                        Index, Rotation, X));
                    continue;
                }
                FIntPoint Replayed = FIntPoint::ZeroValue;
                uint8 ReplayedRotation = 0;
                TestTrue(TEXT("Returned minimum path replays"),
                    ReplayPath(Mino, Start, 0, Result.Actions, Replayed, ReplayedRotation));
                TestEqual(TEXT("Replay reaches reported X"), Replayed.X, Result.ReachedAirPosition.X);
                TestEqual(TEXT("Replay reaches reported Y"), Replayed.Y, Result.ReachedAirPosition.Y);
                TestEqual(TEXT("Replay reaches reported rotation"), ReplayedRotation, Result.ReachedAirRotation);
                TestTrue(TEXT("Minimum cost is nonnegative"), Result.MinimumInputs >= 0);
                ++Count;
            }
        }
    }
    TestTrue(TEXT("All seven tetrominoes and all rotations were covered"), Count > 100);
    return true;
}
#endif
