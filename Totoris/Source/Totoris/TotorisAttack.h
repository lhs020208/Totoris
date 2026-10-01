#pragma once

#include "CoreMinimal.h"
#include "TotorisClassicTypes.h"

// TETR.IO-style attack calculation, deliberately independent from sending or
// cancelling garbage.  This allows the single-player board to retain the
// exact attack value now and connect it to cancellation later.
struct FTotorisAttackCalculation
{
    int32 BaseAttack = 0;
    int32 BackToBackBonus = 0;
    int32 ComboAttack = 0;
    int32 AllClearBonus = 0;
    int32 GarbageSpecialBonus = 0;
    int32 ReleasedSurgeAttack = 0;
    int32 PendingSurgeAttack = 0;
    int32 AttackBeforeOpenerMultiplier = 0;
    int32 TotalAttack = 0;
    bool bOpenerDoubleAttack = false;
};

namespace TotorisAttack
{
    inline int32 BaseAttackFor(ETotorisSpinKind SpinKind, int32 ClearedLines)
    {
        static constexpr int32 Normal[] = { 0, 0, 1, 2, 4 };
        static constexpr int32 FullSpin[] = { 0, 2, 4, 6, 10 };
        const int32 Index = FMath::Clamp(ClearedLines, 0, 4);
        return SpinKind == ETotorisSpinKind::Full ? FullSpin[Index] : Normal[Index];
    }

    inline FTotorisAttackCalculation CalculatePlacement(
        ETotorisSpinKind SpinKind, int32 ClearedLines, int32 ComboCount,
        bool bApplyBackToBackBonus, bool bAllClear, bool bClearedGarbage,
        int32 ReleasedSurgeAttack, int32 PendingSurgeAttack,
        int32 PlacedPieceCount)
    {
        FTotorisAttackCalculation Result;
        Result.BaseAttack = BaseAttackFor(SpinKind, ClearedLines);
        Result.BackToBackBonus = bApplyBackToBackBonus && ClearedLines > 0 ? 1 : 0;

        const int32 MultipliedBase = Result.BaseAttack + Result.BackToBackBonus;
        if (MultipliedBase > 0)
        {
            Result.ComboAttack = FMath::FloorToInt(
                MultipliedBase * (1.f + .25f * FMath::Max(0, ComboCount)));
        }
        else if (ComboCount >= 2)
        {
            // TETR.IO's special anti-four-wide curve for zero-base clears.
            Result.ComboAttack = FMath::FloorToInt(FMath::Loge(1.f + 1.25f * ComboCount));
        }

        // Totoris intentionally uses the requested Season 1 value rather
        // than the current TETR.IO Season 2 / QUICK PLAY all-clear variants.
        Result.AllClearBonus = bAllClear ? 10 : 0;
        Result.GarbageSpecialBonus = bClearedGarbage && ClearedLines > 0 &&
            (ClearedLines >= 4 || SpinKind != ETotorisSpinKind::None) ? 1 : 0;
        Result.ReleasedSurgeAttack = FMath::Max(0, ReleasedSurgeAttack);
        Result.PendingSurgeAttack = FMath::Max(0, PendingSurgeAttack);
        Result.AttackBeforeOpenerMultiplier = Result.ComboAttack +
            Result.AllClearBonus + Result.GarbageSpecialBonus + Result.ReleasedSurgeAttack;
        Result.bOpenerDoubleAttack = PlacedPieceCount >= 1 && PlacedPieceCount <= 14;
        Result.TotalAttack = Result.AttackBeforeOpenerMultiplier *
            (Result.bOpenerDoubleAttack ? 2 : 1);
        return Result;
    }
}
