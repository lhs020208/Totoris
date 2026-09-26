#pragma once

#include "CoreMinimal.h"
#include "TotorisGeneration.h"
#include "TotorisClassicTypes.h"

// Stage 2: empty-board baseline. Stage 3: conservative standard grading.
// Stage 4: occupied-board alternatives and special-placement diagnostics.
// Neither stage modifies gameplay or run-wide statistics. Soft-drop and
// unverified spin cases are excluded rather than treated as player faults.
// Full TETR.IO parity requires game-by-game comparison.
namespace TotorisFinesse
{
    enum class EAction : uint8
    {
        TapLeft,
        TapRight,
        DASLeft,
        DASRight,
        RotateCW,
        RotateCCW,
        Rotate180,
        // Only used by the advisory board-aware search (zero finesse cost).
        Descend
    };

    enum class ETargetMatch : uint8
    {
        // Compare the actual occupied columns + normalized shape. A hard drop
        // changes height, not the footprint; equivalent O/I/S/Z rotations
        // are accepted when they occupy exactly the same four cells.
        OccupiedFootprint,

        // Compare the exact origin X and rotation instead. Mainly useful
        // for debug and the later special-spin implementation.
        ExactOriginAndRotation
    };

    struct FSearchOptions
    {
        ETargetMatch Match = ETargetMatch::OccupiedFootprint;
        bool bAllow180 = true;

        // A physical 180 key press costs 1 by default. Its finesse *scoring*
        // cost is intentionally configurable until TETR.IO parity is verified.
        int32 Rotate180Cost = 1; // clamped to [1, 2]

        // Mirrors UTotorisBlockGeneratorComponent::MaxLogicalRows.
        int32 LogicalRows = 40;

        // False by default: public references do not fully specify TETR.IO's
        // spin finesse rules. Enable only for explicit Totoris-model tests.
        bool bGradeVerifiedSpins = false;
    };

    struct FMinimumResult
    {
        bool bFound = false;
        int32 MinimumInputs = INDEX_NONE;
        TArray<EAction> Actions; // one reproducible optimal sequence
        FIntPoint ReachedAirPosition = FIntPoint::ZeroValue;
        uint8 ReachedAirRotation = 0;
        bool bUsedFreeDescent = false;
        bool bLastActionWasRotation = false;
        bool bLastRotationWas180 = false;
        int32 LastRotationKickIndex = INDEX_NONE;
    };

    // Returns the least-cost sequence in an EMPTY board using the existing
    // SRS+/180 kick tables. Each tap, DAS-to-wall and 90-degree rotation
    // costs 1. No gravity/soft drop or player input trace is simulated.
    // FinalPosition.Y is validated, then intentionally ignored by the goal
    // matcher: the player can descend/hard-drop to the actual locking row.
    TOTORIS_API FMinimumResult FindMinimumStandardInputs(
        ETotorisMino Mino,
        const FIntPoint& SpawnPosition,
        const FIntPoint& FinalPosition,
        uint8 FinalRotation,
        const FSearchOptions& Options = FSearchOptions{});

    // Stage 3: evaluate an ordinary placement against the empty-board
    // 2-step reference, then verify the returned reference path and its
    // hard-drop landing against the REAL board before the piece was locked.
    // Exclude (rather than fault) any unsupported or ambiguous cases.
    // Spin flag comes from DetectSpin before LockedCells is modified.
    // No run-wide statistics are changed by this pure evaluation function.
    TOTORIS_API FTotorisPieceFinesseEvaluation EvaluateStandardPlacement(
        const FTotorisPieceInputTrace& Trace,
        const TSet<FIntPoint>& LockedCellsBeforeLock,
        bool bIsSpin,
        const FSearchOptions& Options = FSearchOptions{});

    // Stage 4: shortest path on the real pre-lock board. For ordinary
    // placements the terminal action is a hard drop to EXACTLY the actual
    // four cells. For spins the last movement must be the rotation itself;
    // DetectSpin (including All-Mini+) must agree with ExpectedSpin.
    // bAllowFreeDescent models gravity / soft drop at zero finesse cost;
    // its results are advisory only and must never directly assign Fault.
    TOTORIS_API FMinimumResult FindMinimumOccupiedInputs(
        const FTotorisPieceInputTrace& Trace,
        const TSet<FIntPoint>& LockedCellsBeforeLock,
        ETotorisSpinKind ExpectedSpin,
        bool bAllowFreeDescent,
        const FSearchOptions& Options = FSearchOptions{});

    TOTORIS_API FTotorisPieceSpecialAnalysis AnalyzeSpecialPlacement(
        const FTotorisPieceInputTrace& Trace,
        const TSet<FIntPoint>& LockedCellsBeforeLock,
        ETotorisSpinKind ActualSpin,
        const FSearchOptions& Options = FSearchOptions{},
        bool bCheckDescentDependence = false);

    // A verified spin can optionally be graded against the Totoris movement
    // model; by default it is Excluded pending TETR.IO parity validation.
    TOTORIS_API FTotorisPieceFinesseEvaluation EvaluateSpecialPlacement(
        const FTotorisPieceInputTrace& Trace,
        const TSet<FIntPoint>& LockedCellsBeforeLock,
        ETotorisSpinKind ActualSpin,
        const FTotorisPieceSpecialAnalysis& Analysis,
        const FSearchOptions& Options = FSearchOptions{});

    // Stage-1 bridge. Uses MinoIndex, SpawnPosition, FinalPosition and
    // FinalRotation; it does not grade Events or alter statistics.
    // A trace that used soft drop can still return a geometric baseline,
    // but the later evaluator must not treat it as a complete finesse score.
    TOTORIS_API FMinimumResult FindMinimumStandardInputs(
        const FTotorisPieceInputTrace& Trace,
        const FSearchOptions& Options = FSearchOptions{});
}
