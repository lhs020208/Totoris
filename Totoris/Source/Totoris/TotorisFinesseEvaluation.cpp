#include "TotorisFinesse.h"

// Stages 3-4: standard-placement grading, with a no-descent real-board
// fallback for alternative routes and non-spin wall kicks.
// Never convert a reference mismatch into a player's finesse fault.
namespace
{
    struct FReplayState
    {
        FIntPoint Position = FIntPoint::ZeroValue;
        uint8 Rotation = 0;
    };

    bool IsLegalOnBoard(ETotorisMino Mino, const FReplayState& State,
        const TSet<FIntPoint>& Locked, int32 LogicalRows)
    {
        for (FIntPoint Cell : TotorisGeneration::RotationCells(Mino, State.Rotation))
        {
            Cell += State.Position;
            if (Cell.X < 0 || Cell.X >= TotorisGeneration::BoardWidth ||
                Cell.Y < 1 || Cell.Y > LogicalRows || Locked.Contains(Cell))
            {
                return false;
            }
        }
        return true;
    }

    // Only reference paths valid at their computed airborne height are
    // accepted. This intentionally excludes paths that need natural gravity,
    // obstacles or a special kick to reach the same landing geometry.
    bool ReplayReferencePath(ETotorisMino Mino, const FIntPoint& Spawn,
        const TotorisFinesse::FMinimumResult& Reference,
        const TSet<FIntPoint>& Locked, int32 LogicalRows, FReplayState& Out)
    {
        FReplayState State{Spawn, 0};
        if (!IsLegalOnBoard(Mino, State, Locked, LogicalRows)) return false;

        for (const TotorisFinesse::EAction Action : Reference.Actions)
        {
            int32 Direction = 0;
            bool bDAS = false;
            switch (Action)
            {
            case TotorisFinesse::EAction::TapLeft: Direction = -1; break;
            case TotorisFinesse::EAction::TapRight: Direction = 1; break;
            case TotorisFinesse::EAction::DASLeft: Direction = -1; bDAS = true; break;
            case TotorisFinesse::EAction::DASRight: Direction = 1; bDAS = true; break;
            default: break;
            }

            if (Direction != 0)
            {
                FReplayState Next = State;
                if (bDAS)
                {
                    bool bMoved = false;
                    for (int32 Step = 0; Step < TotorisGeneration::BoardWidth; ++Step)
                    {
                        Next.Position.X += Direction;
                        if (!IsLegalOnBoard(Mino, Next, Locked, LogicalRows)) break;
                        State = Next;
                        bMoved = true;
                    }
                    if (!bMoved) return false;
                }
                else
                {
                    Next.Position.X += Direction;
                    if (!IsLegalOnBoard(Mino, Next, Locked, LogicalRows)) return false;
                    State = Next;
                }
                continue;
            }

            const bool b180 = Action == TotorisFinesse::EAction::Rotate180;
            const uint8 TargetRotation = static_cast<uint8>((State.Rotation +
                (b180 ? 2 : Action == TotorisFinesse::EAction::RotateCW ? 1 : 3)) & 3);
            const TArray<FIntPoint> Kicks = b180
                ? TotorisGeneration::RotationKicks180(Mino, State.Rotation, TargetRotation)
                : TotorisGeneration::RotationKicks(Mino, State.Rotation, TargetRotation);
            bool bRotated = false;
            for (const FIntPoint& Kick : Kicks)
            {
                FReplayState Next{State.Position + Kick, TargetRotation};
                if (IsLegalOnBoard(Mino, Next, Locked, LogicalRows))
                {
                    State = Next;
                    bRotated = true;
                    break;
                }
            }
            if (!bRotated) return false;
        }

        if (State.Position != Reference.ReachedAirPosition ||
            State.Rotation != Reference.ReachedAirRotation)
        {
            return false;
        }
        Out = State;
        return true;
    }

    bool HasSameLockedCells(ETotorisMino Mino, const FReplayState& Landed,
        const FTotorisPieceInputTrace& Trace)
    {
        const TArray<FIntPoint> Actual = TotorisGeneration::RotationCells(Mino, Trace.FinalRotation);
        const TArray<FIntPoint> Replayed = TotorisGeneration::RotationCells(Mino, Landed.Rotation);
        if (Actual.Num() != 4 || Replayed.Num() != 4) return false;
        TSet<FIntPoint> ActualCells;
        for (FIntPoint Cell : Actual) ActualCells.Add(Cell + Trace.FinalPosition);
        for (FIntPoint Cell : Replayed)
        {
            if (!ActualCells.Contains(Cell + Landed.Position)) return false;
        }
        return ActualCells.Num() == 4;
    }

    bool IsAtPhysicalWall(ETotorisMino Mino, const FTotorisFinesseInputEvent& LastAuto)
    {
        int32 Leftmost = TotorisGeneration::BoardWidth;
        int32 Rightmost = INDEX_NONE;
        for (const FIntPoint& Cell : TotorisGeneration::RotationCells(Mino, LastAuto.AfterRotation))
        {
            Leftmost = FMath::Min(Leftmost, Cell.X + LastAuto.AfterPosition.X);
            Rightmost = FMath::Max(Rightmost, Cell.X + LastAuto.AfterPosition.X);
        }
        return LastAuto.Input == ETotorisFinesseInput::AutoMoveLeft
            ? Leftmost == 0 : Rightmost == TotorisGeneration::BoardWidth - 1;
    }
}

namespace TotorisFinesse
{
    FTotorisPieceFinesseEvaluation EvaluateStandardPlacement(
        const FTotorisPieceInputTrace& Trace, const TSet<FIntPoint>& LockedCellsBeforeLock,
        bool bIsSpin, const FSearchOptions& Options)
    {
        FTotorisPieceFinesseEvaluation Evaluation;
        auto Exclude = [&](ETotorisFinesseExclusionReason Reason)
        {
            Evaluation.Judgement = ETotorisFinesseJudgement::Excluded;
            Evaluation.ExclusionReason = Reason;
            return Evaluation;
        };

        if (!Trace.bValid || Trace.MinoIndex > static_cast<uint8>(ETotorisMino::J) ||
            Trace.FinalRotation > 3 || Options.LogicalRows < 1 || Options.LogicalRows > 40)
        {
            return Exclude(ETotorisFinesseExclusionReason::InvalidTrace);
        }
        const ETotorisMino Mino = static_cast<ETotorisMino>(Trace.MinoIndex);
        Evaluation.ActualInputs = 0;
        if (bIsSpin) return Exclude(ETotorisFinesseExclusionReason::Spin);
        if (Trace.bUsedSoftDrop) return Exclude(ETotorisFinesseExclusionReason::SoftDrop);

        // An auto-repeat is free, but only if the physical key-down for that
        // direction occurred on THIS piece and the repeat ultimately hit the
        // board wall. Intermediate, time-controlled DAS release is outside the
        // Stage-2 empty-board action model; do not incorrectly grade it.
        bool bPressedLeft = false;
        bool bPressedRight = false;
        const FTotorisFinesseInputEvent* LastAuto = nullptr;
        bool bHardDropped = false;
        auto EndAutoGroup = [&]()
        {
            if (LastAuto && !IsAtPhysicalWall(Mino, *LastAuto))
            {
                // Stage 4: a DAS run may also stop at a real stack obstacle.
                // A timed release while a further move is possible stays excluded.
                const int32 Direction = LastAuto->Input == ETotorisFinesseInput::AutoMoveLeft ? -1 : 1;
                const FReplayState After{LastAuto->AfterPosition, LastAuto->AfterRotation};
                if (IsLegalOnBoard(Mino,
                    FReplayState{After.Position + FIntPoint(Direction, 0), After.Rotation},
                    LockedCellsBeforeLock, Options.LogicalRows)) return false;
            }
            LastAuto = nullptr;
            return true;
        };
        for (const FTotorisFinesseInputEvent& Event : Trace.Events)
        {
            if (bHardDropped) return Exclude(ETotorisFinesseExclusionReason::InvalidTrace);
            if (Event.Input == ETotorisFinesseInput::AutoMoveLeft ||
                Event.Input == ETotorisFinesseInput::AutoMoveRight)
            {
                const bool bLeft = Event.Input == ETotorisFinesseInput::AutoMoveLeft;
                if (!(bLeft ? bPressedLeft : bPressedRight))
                    return Exclude(ETotorisFinesseExclusionReason::PrechargedOrCarriedDAS);
                if (!Event.bSucceeded || Event.BeforeRotation != Event.AfterRotation)
                    return Exclude(ETotorisFinesseExclusionReason::InvalidTrace);
                if (LastAuto && LastAuto->Input != Event.Input && !EndAutoGroup())
                    return Exclude(ETotorisFinesseExclusionReason::TimedOrBlockedDAS);
                LastAuto = &Event;
                continue;
            }
            if (!EndAutoGroup())
                return Exclude(ETotorisFinesseExclusionReason::TimedOrBlockedDAS);

            switch (Event.Input)
            {
            case ETotorisFinesseInput::MoveLeft:
                ++Evaluation.ActualInputs;
                bPressedLeft = true;
                break;
            case ETotorisFinesseInput::MoveRight:
                ++Evaluation.ActualInputs;
                bPressedRight = true;
                break;
            case ETotorisFinesseInput::RotateCW:
            case ETotorisFinesseInput::RotateCCW:
                ++Evaluation.ActualInputs;
                break;
            case ETotorisFinesseInput::Rotate180:
                if (!Options.bAllow180)
                    return Exclude(ETotorisFinesseExclusionReason::UnsupportedInput);
                Evaluation.ActualInputs += FMath::Clamp(Options.Rotate180Cost, 1, 2);
                break;
            case ETotorisFinesseInput::HardDrop:
                bHardDropped = true;
                break;
            case ETotorisFinesseInput::RejectedHold:
                return Exclude(ETotorisFinesseExclusionReason::RejectedHold);
            case ETotorisFinesseInput::SoftDropPress:
            case ETotorisFinesseInput::SoftDropRelease:
            default:
                return Exclude(ETotorisFinesseExclusionReason::UnsupportedInput);
            }
        }
        if (!EndAutoGroup())
            return Exclude(ETotorisFinesseExclusionReason::TimedOrBlockedDAS);

        const FReplayState Goal{Trace.FinalPosition, Trace.FinalRotation};
        if (!IsLegalOnBoard(Mino, Goal, LockedCellsBeforeLock, Options.LogicalRows))
            return Exclude(ETotorisFinesseExclusionReason::InvalidTrace);

        const FMinimumResult Reference = FindMinimumStandardInputs(Trace, Options);
        // If the first empty-board reference is invalid on a real stack,
        // search ALL real-board no-descent routes before declaring exclusion.
        // No inferred gravity or soft drop is permitted in a graded fallback.
        bool bReferenceValid = Reference.bFound;
        ETotorisFinesseExclusionReason PreviousReason =
            ETotorisFinesseExclusionReason::ReferenceNotFound;
        if (bReferenceValid)
        {
            FReplayState ReferenceAir;
            bReferenceValid = ReplayReferencePath(Mino, Trace.SpawnPosition, Reference,
                LockedCellsBeforeLock, Options.LogicalRows, ReferenceAir);
            if (!bReferenceValid)
                PreviousReason = ETotorisFinesseExclusionReason::ReferencePathBlocked;
            else
            {
                FReplayState ReferenceLanded = ReferenceAir;
                while (IsLegalOnBoard(Mino,
                    FReplayState{ReferenceLanded.Position + FIntPoint(0, -1), ReferenceLanded.Rotation},
                    LockedCellsBeforeLock, Options.LogicalRows))
                    --ReferenceLanded.Position.Y;
                bReferenceValid = HasSameLockedCells(Mino, ReferenceLanded, Trace);
                if (!bReferenceValid)
                    PreviousReason = ETotorisFinesseExclusionReason::ReferenceLandingMismatch;
            }
        }
        if (bReferenceValid)
            Evaluation.MinimumInputs = Reference.MinimumInputs;
        else
        {
            const FMinimumResult Occupied = FindMinimumOccupiedInputs(
                Trace, LockedCellsBeforeLock, ETotorisSpinKind::None, false, Options);
            if (!Occupied.bFound) return Exclude(PreviousReason);
            Evaluation.MinimumInputs = Occupied.MinimumInputs;
        }
        if (Evaluation.ActualInputs < Evaluation.MinimumInputs)
        {
            // E.g. a timed DAS stop not expressible by the Stage-2 reference.
            return Exclude(ETotorisFinesseExclusionReason::ActualBelowReference);
        }

        Evaluation.ExcessInputs = Evaluation.ActualInputs - Evaluation.MinimumInputs;
        Evaluation.Judgement = Evaluation.ExcessInputs == 0
            ? ETotorisFinesseJudgement::Optimal : ETotorisFinesseJudgement::Fault;
        return Evaluation;
    }
}
