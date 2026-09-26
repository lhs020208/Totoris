#pragma once

#include "CoreMinimal.h"
#include "TotorisGeneration.h"
#include "TotorisClassicTypes.h"

// Stage 2: an independent, empty-board optimizer for ordinary placements.
// It does not alter movement, judge a player's trace or update run statistics.
// Special spins, soft-drop/tuck paths, gravity timing and occupied-board
// paths belong to a later phase. This is a Totoris movement-model baseline,
// not a claim to reproduce every undocumented TETR.IO finesse exception.
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
        Rotate180
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
    };

    struct FMinimumResult
    {
        bool bFound = false;
        int32 MinimumInputs = INDEX_NONE;
        TArray<EAction> Actions; // one reproducible optimal sequence
        FIntPoint ReachedAirPosition = FIntPoint::ZeroValue;
        uint8 ReachedAirRotation = 0;
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

    // Stage-1 bridge. Uses MinoIndex, SpawnPosition, FinalPosition and
    // FinalRotation; it does not grade Events or alter statistics.
    // A trace that used soft drop can still return a geometric baseline,
    // but the later evaluator must not treat it as a complete finesse score.
    TOTORIS_API FMinimumResult FindMinimumStandardInputs(
        const FTotorisPieceInputTrace& Trace,
        const FSearchOptions& Options = FSearchOptions{});
}
