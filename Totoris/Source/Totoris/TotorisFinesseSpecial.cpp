#include "TotorisFinesse.h"

// Stage 4: inspect special placements without turning uncertain TETR.IO
// behavior into finesse faults. All searches use the REAL pre-lock board and
// the existing Totoris SRS+/180 kick tables; there is no gameplay mutation.
namespace
{
    struct FBoardState
    {
        FIntPoint Position = FIntPoint::ZeroValue;
        uint8 Rotation = 0;
    };

    struct FBoardNode
    {
        FBoardState State;
        int32 Cost = MAX_int32;
        int32 Parent = INDEX_NONE;
        TotorisFinesse::EAction Action = TotorisFinesse::EAction::TapLeft;
    };

    int32 BoardStateKey(const FBoardState& S)
    {
        return ((S.Position.Y * 32 + S.Position.X + 8) * 4) + (S.Rotation & 3);
    }

    bool Legal(ETotorisMino Type, const FBoardState& State,
        const TSet<FIntPoint>& Locked, int32 Rows)
    {
        for (FIntPoint Cell : TotorisGeneration::RotationCells(Type, State.Rotation))
        {
            Cell += State.Position;
            if (Cell.X < 0 || Cell.X >= TotorisGeneration::BoardWidth ||
                Cell.Y < 1 || Cell.Y > Rows || Locked.Contains(Cell))
                return false;
        }
        return true;
    }

    bool SameFourCells(ETotorisMino Type, const FBoardState& A, const FBoardState& B)
    {
        TSet<FIntPoint> Goal;
        for (const FIntPoint& Cell : TotorisGeneration::RotationCells(Type, B.Rotation))
            Goal.Add(Cell + B.Position);
        if (Goal.Num() != 4) return false;
        for (const FIntPoint& Cell : TotorisGeneration::RotationCells(Type, A.Rotation))
            if (!Goal.Contains(Cell + A.Position)) return false;
        return true;
    }

    bool TryRotate(ETotorisMino Type, const FBoardState& Before,
        TotorisFinesse::EAction Action, const TSet<FIntPoint>& Locked,
        int32 Rows, FBoardState& Out, int32& OutKick)
    {
        const bool b180 = Action == TotorisFinesse::EAction::Rotate180;
        const uint8 To = static_cast<uint8>((Before.Rotation +
            (b180 ? 2 : Action == TotorisFinesse::EAction::RotateCW ? 1 : 3)) & 3);
        const TArray<FIntPoint> Kicks = b180
            ? TotorisGeneration::RotationKicks180(Type, Before.Rotation, To)
            : TotorisGeneration::RotationKicks(Type, Before.Rotation, To);
        for (int32 K = 0; K < Kicks.Num(); ++K)
        {
            const FBoardState Candidate{Before.Position + Kicks[K], To};
            if (Legal(Type, Candidate, Locked, Rows))
            {
                Out = Candidate;
                OutKick = K;
                return true;
            }
        }
        return false;
    }

    bool IsRotationInput(ETotorisFinesseInput Input)
    {
        return Input == ETotorisFinesseInput::RotateCW ||
            Input == ETotorisFinesseInput::RotateCCW ||
            Input == ETotorisFinesseInput::Rotate180;
    }
}

namespace TotorisFinesse
{
    FMinimumResult FindMinimumOccupiedInputs(
        const FTotorisPieceInputTrace& Trace,
        const TSet<FIntPoint>& Locked,
        ETotorisSpinKind ExpectedSpin,
        bool bAllowFreeDescent,
        const FSearchOptions& Options)
    {
        FMinimumResult NotFound;
        if (!Trace.bValid || Trace.MinoIndex > static_cast<uint8>(ETotorisMino::J) ||
            Trace.FinalRotation > 3 || Options.LogicalRows < 1 || Options.LogicalRows > 40)
            return NotFound;

        const ETotorisMino Type = static_cast<ETotorisMino>(Trace.MinoIndex);
        const FBoardState Start{Trace.SpawnPosition, 0};
        const FBoardState Goal{Trace.FinalPosition, Trace.FinalRotation};
        if (!Legal(Type, Start, Locked, Options.LogicalRows) ||
            !Legal(Type, Goal, Locked, Options.LogicalRows)) return NotFound;

        TArray<FBoardNode> Nodes;
        TMap<int32, int32> IndexByKey;
        TArray<TArray<int32>> Buckets;
        FBoardNode Initial;
        Initial.State = Start;
        Initial.Cost = 0;
        Nodes.Add(Initial);
        IndexByKey.Add(BoardStateKey(Start), 0);
        Buckets.Add(TArray<int32>{0});

        int32 BestFinishCost = MAX_int32;
        int32 FinishParent = INDEX_NONE;
        EAction FinishAction = EAction::TapLeft;
        FBoardState FinishState;
        int32 FinishKick = INDEX_NONE;
        bool bFinishWas180 = false;

        auto Relax = [&](int32 Parent, const FBoardState& Next,
            EAction Action, int32 ActionCost)
        {
            const int32 NewCost = Nodes[Parent].Cost + ActionCost;
            if (!Legal(Type, Next, Locked, Options.LogicalRows) ||
                (Next.Position == Nodes[Parent].State.Position &&
                 Next.Rotation == Nodes[Parent].State.Rotation) ||
                NewCost >= BestFinishCost)
                return;

            const int32 Key = BoardStateKey(Next);
            int32 TargetIndex = INDEX_NONE;
            if (int32* Existing = IndexByKey.Find(Key))
            {
                TargetIndex = *Existing;
                if (NewCost >= Nodes[TargetIndex].Cost) return;
                Nodes[TargetIndex].State = Next;
                Nodes[TargetIndex].Cost = NewCost;
                Nodes[TargetIndex].Parent = Parent;
                Nodes[TargetIndex].Action = Action;
            }
            else
            {
                FBoardNode Created;
                Created.State = Next;
                Created.Cost = NewCost;
                Created.Parent = Parent;
                Created.Action = Action;
                TargetIndex = Nodes.Add(Created);
                IndexByKey.Add(Key, TargetIndex);
            }
            while (Buckets.Num() <= NewCost) Buckets.Add(TArray<int32>{});
            Buckets[NewCost].Add(TargetIndex);
        };

        for (int32 Cost = 0; Cost < Buckets.Num() && Cost <= BestFinishCost; ++Cost)
        {
            // A zero-cost descent may append additional states to this bucket.
            for (int32 Slot = 0; Slot < Buckets[Cost].Num(); ++Slot)
            {
                const int32 CurrentIndex = Buckets[Cost][Slot];
                if (Nodes[CurrentIndex].Cost != Cost || Cost >= BestFinishCost) continue;
                const FBoardState Current = Nodes[CurrentIndex].State;

                if (ExpectedSpin == ETotorisSpinKind::None)
                {
                    FBoardState Landing = Current;
                    while (Legal(Type,
                        FBoardState{Landing.Position + FIntPoint(0, -1), Landing.Rotation},
                        Locked, Options.LogicalRows))
                        --Landing.Position.Y;
                    if (SameFourCells(Type, Landing, Goal))
                    {
                        BestFinishCost = Cost;
                        FinishParent = CurrentIndex;
                        FinishState = Current;
                        continue;
                    }
                }

                if (bAllowFreeDescent)
                {
                    Relax(CurrentIndex,
                        FBoardState{Current.Position + FIntPoint(0, -1), Current.Rotation},
                        EAction::Descend, 0);
                }

                for (int32 Direction : {-1, 1})
                {
                    Relax(CurrentIndex,
                        FBoardState{Current.Position + FIntPoint(Direction, 0), Current.Rotation},
                        Direction < 0 ? EAction::TapLeft : EAction::TapRight, 1);

                    FBoardState Wall = Current;
                    for (int32 Step = 0; Step < TotorisGeneration::BoardWidth; ++Step)
                    {
                        const FBoardState Next{Wall.Position + FIntPoint(Direction, 0), Wall.Rotation};
                        if (!Legal(Type, Next, Locked, Options.LogicalRows)) break;
                        Wall = Next;
                    }
                    Relax(CurrentIndex, Wall,
                        Direction < 0 ? EAction::DASLeft : EAction::DASRight, 1);
                }

                for (const EAction Action : {EAction::RotateCW, EAction::RotateCCW, EAction::Rotate180})
                {
                    if (Action == EAction::Rotate180 && !Options.bAllow180) continue;
                    FBoardState Rotated;
                    int32 KickIndex = INDEX_NONE;
                    if (!TryRotate(Type, Current, Action, Locked, Options.LogicalRows,
                        Rotated, KickIndex)) continue;
                    const int32 ActionCost = Action == EAction::Rotate180
                        ? FMath::Clamp(Options.Rotate180Cost, 1, 2) : 1;

                    if (ExpectedSpin != ETotorisSpinKind::None &&
                        Cost + ActionCost < BestFinishCost &&
                        SameFourCells(Type, Rotated, Goal) &&
                        TotorisGeneration::DetectSpin(Type, Rotated.Position, Rotated.Rotation,
                            true, Action == EAction::Rotate180, KickIndex,
                            Locked, Options.LogicalRows) == ExpectedSpin)
                    {
                        BestFinishCost = Cost + ActionCost;
                        FinishParent = CurrentIndex;
                        FinishAction = Action;
                        FinishState = Rotated;
                        FinishKick = KickIndex;
                        bFinishWas180 = Action == EAction::Rotate180;
                    }
                    Relax(CurrentIndex, Rotated, Action, ActionCost);
                }
            }
        }

        if (FinishParent == INDEX_NONE) return NotFound;
        FMinimumResult Result;
        Result.bFound = true;
        Result.MinimumInputs = BestFinishCost;
        Result.ReachedAirPosition = FinishState.Position;
        Result.ReachedAirRotation = FinishState.Rotation;
        Result.bLastActionWasRotation = ExpectedSpin != ETotorisSpinKind::None;
        Result.bLastRotationWas180 = bFinishWas180;
        Result.LastRotationKickIndex = FinishKick;
        for (int32 At = FinishParent; Nodes[At].Parent != INDEX_NONE;
             At = Nodes[At].Parent)
            Result.Actions.Add(Nodes[At].Action);
        for (int32 A = 0, B = Result.Actions.Num() - 1; A < B; ++A, --B)
            Swap(Result.Actions[A], Result.Actions[B]);
        if (Result.bLastActionWasRotation) Result.Actions.Add(FinishAction);
        for (const EAction Action : Result.Actions)
            if (Action == EAction::Descend) Result.bUsedFreeDescent = true;
        return Result;
    }

    FTotorisPieceSpecialAnalysis AnalyzeSpecialPlacement(
        const FTotorisPieceInputTrace& Trace,
        const TSet<FIntPoint>& Locked,
        ETotorisSpinKind ActualSpin,
        const FSearchOptions& Options,
        bool bCheckDescentDependence)
    {
        FTotorisPieceSpecialAnalysis Result;
        Result.DetectedSpin = ActualSpin;
        Result.bUsedSoftDrop = Trace.bUsedSoftDrop;
        if (!Trace.bValid || Trace.MinoIndex > static_cast<uint8>(ETotorisMino::J))
            return Result;

        const ETotorisMino Type = static_cast<ETotorisMino>(Trace.MinoIndex);
        const FTotorisFinesseInputEvent* LastEffectiveRotation = nullptr;
        for (const FTotorisFinesseInputEvent& Event : Trace.Events)
        {
            if (Event.Input == ETotorisFinesseInput::Rotate180) Result.bUsed180 = true;
            if (Event.Input == ETotorisFinesseInput::SoftDropPress)
                Result.bUsedSoftDrop = true;
            if (IsRotationInput(Event.Input) && Event.bSucceeded)
            {
                Result.bHadVerticalKick |= Event.BeforePosition.Y != Event.AfterPosition.Y;
                LastEffectiveRotation = &Event;
            }
            else if (Event.bSucceeded && (
                Event.Input == ETotorisFinesseInput::MoveLeft ||
                Event.Input == ETotorisFinesseInput::MoveRight ||
                Event.Input == ETotorisFinesseInput::AutoMoveLeft ||
                Event.Input == ETotorisFinesseInput::AutoMoveRight ||
                (Event.Input == ETotorisFinesseInput::HardDrop &&
                    Event.BeforePosition != Event.AfterPosition)))
            {
                LastEffectiveRotation = nullptr;
            }
        }
        Result.bFinalSpinRotationVerified = ActualSpin != ETotorisSpinKind::None &&
            LastEffectiveRotation && LastEffectiveRotation->AfterPosition == Trace.FinalPosition &&
            LastEffectiveRotation->AfterRotation == Trace.FinalRotation &&
            TotorisGeneration::DetectSpin(Type, Trace.FinalPosition, Trace.FinalRotation,
                true, LastEffectiveRotation->Input == ETotorisFinesseInput::Rotate180,
                LastEffectiveRotation->KickIndex, Locked, Options.LogicalRows) == ActualSpin;

        if (ActualSpin != ETotorisSpinKind::None)
            Result.Kind = Type != ETotorisMino::T
                ? ETotorisFinesseSpecialKind::AllMiniPlusSpin
                : (ActualSpin == ETotorisSpinKind::Full
                    ? ETotorisFinesseSpecialKind::TSpinFull
                    : ETotorisFinesseSpecialKind::TSpinMini);
        else if (Result.bUsedSoftDrop)
            Result.Kind = ETotorisFinesseSpecialKind::SoftDrop;
        else if (Result.bHadVerticalKick)
            Result.Kind = ETotorisFinesseSpecialKind::VerticalKick;

        // Run advisory search only for a case the ordinary reference cannot
        // describe. Free descent ignores timing; no Fault may rely on it.
        if (ActualSpin != ETotorisSpinKind::None || Result.bUsedSoftDrop ||
            Result.bHadVerticalKick || bCheckDescentDependence)
        {
            // Avoid interpreting a timed/blocked reference as a finesse fault:
            // if a zero-descent route is absent but a free-descent route exists,
            // classify this as a gravity/tuck-dependent placement.
            const FMinimumResult WithoutDescent = bCheckDescentDependence
                ? FindMinimumOccupiedInputs(Trace, Locked,
                    ActualSpin, false, Options)
                : FMinimumResult{};
            const FMinimumResult Advisory = FindMinimumOccupiedInputs(
                Trace, Locked, ActualSpin, true, Options);
            Result.bAdvisoryPathFound = Advisory.bFound;
            Result.bAdvisoryPathUsesDescent = Advisory.bUsedFreeDescent;
            Result.AdvisoryMinimumInputs = Advisory.MinimumInputs;
            if (ActualSpin == ETotorisSpinKind::None &&
                !Result.bUsedSoftDrop && Advisory.bFound &&
                !WithoutDescent.bFound && Advisory.bUsedFreeDescent)
                Result.Kind = ETotorisFinesseSpecialKind::DescentDependent;
        }
        return Result;
    }

    FTotorisPieceFinesseEvaluation EvaluateSpecialPlacement(
        const FTotorisPieceInputTrace& Trace,
        const TSet<FIntPoint>& Locked,
        ETotorisSpinKind ActualSpin,
        const FTotorisPieceSpecialAnalysis& Analysis,
        const FSearchOptions& Options)
    {
        FTotorisPieceFinesseEvaluation Result;
        Result.Judgement = ETotorisFinesseJudgement::Excluded;
        Result.ExclusionReason = ETotorisFinesseExclusionReason::SpecialSpinUnverified;
        if (ActualSpin == ETotorisSpinKind::None || !Trace.bValid)
        {
            Result.ExclusionReason = ETotorisFinesseExclusionReason::InvalidTrace;
            return Result;
        }
        if (Analysis.bUsedSoftDrop)
        {
            Result.ExclusionReason = ETotorisFinesseExclusionReason::SoftDrop;
            return Result;
        }
        if (!Analysis.bFinalSpinRotationVerified)
        {
            Result.ExclusionReason = ETotorisFinesseExclusionReason::SpinTraceMismatch;
            return Result;
        }
        if (!Options.bGradeVerifiedSpins) return Result;

        // Explicit opt-in, Totoris movement-model grade: only routes that
        // need NO inferred gravity / soft drop can be assessed safely.
        const FMinimumResult Minimum = FindMinimumOccupiedInputs(
            Trace, Locked, ActualSpin, false, Options);
        if (!Minimum.bFound)
        {
            Result.ExclusionReason = ETotorisFinesseExclusionReason::DescentDependentPath;
            return Result;
        }
        int32 PhysicalCount = 0;
        bool bPressedLeft = false, bPressedRight = false;
        const FTotorisFinesseInputEvent* LastAuto = nullptr;
        bool bHardDropped = false;
        const ETotorisMino Type = static_cast<ETotorisMino>(Trace.MinoIndex);
        auto EndAuto = [&]()
        {
            if (LastAuto)
            {
                const int32 Direction = LastAuto->Input == ETotorisFinesseInput::AutoMoveLeft ? -1 : 1;
                const FBoardState After{LastAuto->AfterPosition, LastAuto->AfterRotation};
                if (Legal(Type, FBoardState{After.Position + FIntPoint(Direction, 0), After.Rotation},
                    Locked, Options.LogicalRows)) return false;
            }
            LastAuto = nullptr;
            return true;
        };
        for (const FTotorisFinesseInputEvent& Event : Trace.Events)
        {
            if (bHardDropped)
            {
                Result.ExclusionReason = ETotorisFinesseExclusionReason::InvalidTrace;
                return Result;
            }
            if (Event.Input == ETotorisFinesseInput::AutoMoveLeft ||
                Event.Input == ETotorisFinesseInput::AutoMoveRight)
            {
                if (!Event.bSucceeded ||
                    !(Event.Input == ETotorisFinesseInput::AutoMoveLeft ? bPressedLeft : bPressedRight))
                {
                    Result.ExclusionReason = ETotorisFinesseExclusionReason::PrechargedOrCarriedDAS;
                    return Result;
                }
                if (LastAuto && LastAuto->Input != Event.Input && !EndAuto())
                {
                    Result.ExclusionReason = ETotorisFinesseExclusionReason::TimedOrBlockedDAS;
                    return Result;
                }
                LastAuto = &Event;
                continue;
            }
            if (!EndAuto())
            {
                Result.ExclusionReason = ETotorisFinesseExclusionReason::TimedOrBlockedDAS;
                return Result;
            }
            switch (Event.Input)
            {
            case ETotorisFinesseInput::MoveLeft: ++PhysicalCount; bPressedLeft = true; break;
            case ETotorisFinesseInput::MoveRight: ++PhysicalCount; bPressedRight = true; break;
            case ETotorisFinesseInput::RotateCW:
            case ETotorisFinesseInput::RotateCCW: ++PhysicalCount; break;
            case ETotorisFinesseInput::Rotate180:
                if (!Options.bAllow180) return Result;
                PhysicalCount += FMath::Clamp(Options.Rotate180Cost, 1, 2); break;
            case ETotorisFinesseInput::HardDrop: bHardDropped = true; break;
            default:
                Result.ExclusionReason = ETotorisFinesseExclusionReason::UnsupportedInput;
                return Result;
            }
        }
        if (!EndAuto())
        {
            Result.ExclusionReason = ETotorisFinesseExclusionReason::TimedOrBlockedDAS;
            return Result;
        }
        Result.ActualInputs = PhysicalCount;
        Result.MinimumInputs = Minimum.MinimumInputs;
        if (PhysicalCount < Minimum.MinimumInputs)
        {
            Result.ExclusionReason = ETotorisFinesseExclusionReason::ActualBelowReference;
            return Result;
        }
        Result.ExclusionReason = ETotorisFinesseExclusionReason::None;
        Result.ExcessInputs = PhysicalCount - Minimum.MinimumInputs;
        Result.Judgement = Result.ExcessInputs == 0
            ? ETotorisFinesseJudgement::Optimal : ETotorisFinesseJudgement::Fault;
        return Result;
    }
}
