#include "TotorisFinesse.h"

namespace
{
    struct FSearchState
    {
        FIntPoint Position = FIntPoint::ZeroValue;
        uint8 Rotation = 0;
    };

    struct FSearchNode
    {
        FSearchState State;
        int32 Cost = MAX_int32;
        int32 ParentIndex = INDEX_NONE;
        TotorisFinesse::EAction ParentAction = TotorisFinesse::EAction::TapLeft;
        bool bClosed = false;
    };

    struct FFootprint
    {
        int32 LeftmostColumn = INDEX_NONE;
        uint16 ShapeMask = 0;
    };

    // A valid origin may be negative (e.g. vertical I has min local X=1).
    // Legal piece origins always fit this bounded key range.
    int32 StateKey(const FSearchState& State)
    {
        return ((State.Position.Y * 32 + State.Position.X + 8) * 4)
            + (State.Rotation & 3);
    }

    bool IsLegal(ETotorisMino Mino, const FSearchState& State, int32 LogicalRows)
    {
        for (FIntPoint Cell : TotorisGeneration::RotationCells(Mino, State.Rotation))
        {
            Cell += State.Position;
            if (Cell.X < 0 || Cell.X >= TotorisGeneration::BoardWidth ||
                Cell.Y < 1 || Cell.Y > LogicalRows)
            {
                return false;
            }
        }
        return true;
    }

    // Translation-invariant in Y, but NOT X: the actual landing column matters.
    // The 4x4 occupancy mask also recognizes visually identical rotations.
    FFootprint MakeFootprint(ETotorisMino Mino, const FSearchState& State)
    {
        const TArray<FIntPoint> Cells = TotorisGeneration::RotationCells(Mino, State.Rotation);
        int32 MinLocalX = MAX_int32;
        int32 MinLocalY = MAX_int32;
        for (const FIntPoint& Cell : Cells)
        {
            MinLocalX = FMath::Min(MinLocalX, Cell.X);
            MinLocalY = FMath::Min(MinLocalY, Cell.Y);
        }

        FFootprint Result;
        Result.LeftmostColumn = State.Position.X + MinLocalX;
        for (const FIntPoint& Cell : Cells)
        {
            const int32 Bit = (Cell.Y - MinLocalY) * 4 + (Cell.X - MinLocalX);
            Result.ShapeMask |= static_cast<uint16>(1u << Bit);
        }
        return Result;
    }

    // Matches UTotorisBlockGeneratorComponent::Rotate and Rotate180: first
    // valid kick wins. There are no locked cells at this baseline stage.
    bool TryRotate(ETotorisMino Mino, const FSearchState& Before,
        uint8 NewRotation, bool bIs180, int32 LogicalRows, FSearchState& Out)
    {
        const TArray<FIntPoint> Kicks = bIs180
            ? TotorisGeneration::RotationKicks180(Mino, Before.Rotation, NewRotation)
            : TotorisGeneration::RotationKicks(Mino, Before.Rotation, NewRotation);

        for (const FIntPoint& Kick : Kicks)
        {
            FSearchState Candidate;
            Candidate.Position = Before.Position + Kick;
            Candidate.Rotation = NewRotation;
            if (IsLegal(Mino, Candidate, LogicalRows))
            {
                Out = Candidate;
                return true;
            }
        }
        return false;
    }
}

namespace TotorisFinesse
{
    FMinimumResult FindMinimumStandardInputs(
        ETotorisMino Mino, const FIntPoint& SpawnPosition,
        const FIntPoint& FinalPosition, uint8 FinalRotation,
        const FSearchOptions& Options)
    {
        FMinimumResult NotFound;
        if (static_cast<uint8>(Mino) > static_cast<uint8>(ETotorisMino::J) ||
            FinalRotation > 3 || Options.LogicalRows < 1 || Options.LogicalRows > 40)
        {
            return NotFound;
        }

        const FSearchState Start{SpawnPosition, 0};
        const FSearchState Goal{FinalPosition, FinalRotation};
        if (!IsLegal(Mino, Start, Options.LogicalRows) ||
            !IsLegal(Mino, Goal, Options.LogicalRows))
        {
            return NotFound;
        }

        const FFootprint GoalFootprint = MakeFootprint(Mino, Goal);
        const int32 Cost180 = FMath::Clamp(Options.Rotate180Cost, 1, 2);
        TArray<FSearchNode> Nodes;
        TMap<int32, int32> IndexByKey;
        FSearchNode StartNode;
        StartNode.State = Start;
        StartNode.Cost = 0;
        Nodes.Add(StartNode);
        IndexByKey.Add(StateKey(Start), 0);

        // Small bounded graph (10 columns x <=40 rows x 4 rotations).
        // Dijkstra instead of BFS keeps 180 cost=2 correct when selected.
        for (;;)
        {
            int32 CurrentIndex = INDEX_NONE;
            int32 LowestCost = MAX_int32;
            for (int32 Index = 0; Index < Nodes.Num(); ++Index)
            {
                if (!Nodes[Index].bClosed && Nodes[Index].Cost < LowestCost)
                {
                    CurrentIndex = Index;
                    LowestCost = Nodes[Index].Cost;
                }
            }
            if (CurrentIndex == INDEX_NONE) return NotFound;

            const FSearchState Current = Nodes[CurrentIndex].State;
            const bool bReached = Options.Match == ETargetMatch::ExactOriginAndRotation
                ? Current.Position.X == Goal.Position.X && Current.Rotation == Goal.Rotation
                : MakeFootprint(Mino, Current).LeftmostColumn == GoalFootprint.LeftmostColumn &&
                  MakeFootprint(Mino, Current).ShapeMask == GoalFootprint.ShapeMask;

            if (bReached)
            {
                FMinimumResult Result;
                Result.bFound = true;
                Result.MinimumInputs = Nodes[CurrentIndex].Cost;
                Result.ReachedAirPosition = Current.Position;
                Result.ReachedAirRotation = Current.Rotation;

                for (int32 At = CurrentIndex; Nodes[At].ParentIndex != INDEX_NONE;
                     At = Nodes[At].ParentIndex)
                {
                    Result.Actions.Add(Nodes[At].ParentAction);
                }
                // The parent walk produces the sequence backwards.
                for (int32 A = 0, B = Result.Actions.Num() - 1; A < B; ++A, --B)
                {
                    Swap(Result.Actions[A], Result.Actions[B]);
                }
                return Result;
            }
            Nodes[CurrentIndex].bClosed = true;

            auto Relax = [&](const FSearchState& Candidate, EAction Action, int32 ActionCost)
            {
                if (Candidate.Position == Current.Position &&
                    Candidate.Rotation == Current.Rotation) return;
                if (!IsLegal(Mino, Candidate, Options.LogicalRows)) return;

                const int32 Key = StateKey(Candidate);
                const int32 CandidateCost = LowestCost + ActionCost;
                if (int32* ExistingIndex = IndexByKey.Find(Key))
                {
                    FSearchNode& Existing = Nodes[*ExistingIndex];
                    if (CandidateCost < Existing.Cost)
                    {
                        Existing.Cost = CandidateCost;
                        Existing.ParentIndex = CurrentIndex;
                        Existing.ParentAction = Action;
                        Existing.bClosed = false;
                    }
                }
                else
                {
                    FSearchNode Added;
                    Added.State = Candidate;
                    Added.Cost = CandidateCost;
                    Added.ParentIndex = CurrentIndex;
                    Added.ParentAction = Action;
                    const int32 AddedIndex = Nodes.Add(Added);
                    IndexByKey.Add(Key, AddedIndex);
                }
            };

            for (int32 Direction : {-1, 1})
            {
                FSearchState Tap = Current;
                Tap.Position.X += Direction;
                Relax(Tap, Direction < 0 ? EAction::TapLeft : EAction::TapRight, 1);

                // Model a standard instant-DAS-to-wall action, counted as
                // one key press. Timed releases at intermediate ARR positions
                // are outside this standard-reference search.
                FSearchState Wall = Current;
                for (int32 Step = 0; Step < TotorisGeneration::BoardWidth; ++Step)
                {
                    FSearchState Next = Wall;
                    Next.Position.X += Direction;
                    if (!IsLegal(Mino, Next, Options.LogicalRows)) break;
                    Wall = Next;
                }
                Relax(Wall, Direction < 0 ? EAction::DASLeft : EAction::DASRight, 1);
            }

            FSearchState Rotated;
            if (TryRotate(Mino, Current, (Current.Rotation + 1) & 3, false,
                Options.LogicalRows, Rotated))
            {
                Relax(Rotated, EAction::RotateCW, 1);
            }
            if (TryRotate(Mino, Current, (Current.Rotation + 3) & 3, false,
                Options.LogicalRows, Rotated))
            {
                Relax(Rotated, EAction::RotateCCW, 1);
            }
            if (Options.bAllow180 && TryRotate(Mino, Current,
                (Current.Rotation + 2) & 3, true, Options.LogicalRows, Rotated))
            {
                Relax(Rotated, EAction::Rotate180, Cost180);
            }
        }
    }

    FMinimumResult FindMinimumStandardInputs(
        const FTotorisPieceInputTrace& Trace, const FSearchOptions& Options)
    {
        if (!Trace.bValid || Trace.MinoIndex > static_cast<uint8>(ETotorisMino::J))
        {
            return FMinimumResult{};
        }
        return FindMinimumStandardInputs(static_cast<ETotorisMino>(Trace.MinoIndex),
            Trace.SpawnPosition, Trace.FinalPosition, Trace.FinalRotation, Options);
    }
}
