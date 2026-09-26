#pragma once

#include "CoreMinimal.h"
#include "TotorisClassicTypes.h"

// Pure scoring rules.  This header deliberately owns no gameplay state: the
// block generator supplies the already-determined spin, B2B and combo facts.
namespace TotorisScoring
{
    struct FPlacementScore
    {
        int64 LineClearScore = 0;
        int64 SpinScore = 0;
        int64 BackToBackBonusScore = 0;
        int64 ComboBonusScore = 0;
        int64 AllClearBonusScore = 0;

        int64 Total() const
        {
            return LineClearScore + SpinScore + BackToBackBonusScore +
                ComboBonusScore + AllClearBonusScore;
        }
    };

    inline int64 NormalClearBaseScore(int32 Lines)
    {
        switch (Lines)
        {
        case 1: return 100;
        case 2: return 300;
        case 3: return 500;
        case 4: return 800;
        default: return 0;
        }
    }

    inline int64 SpinBaseScore(ETotorisSpinKind SpinKind, int32 Lines)
    {
        const int32 ClampedLines = FMath::Clamp(Lines, 0, 4);
        if (SpinKind == ETotorisSpinKind::Full)
        {
            static constexpr int64 FullScores[] = { 400, 800, 1200, 1600, 2600 };
            return FullScores[ClampedLines];
        }
        if (SpinKind == ETotorisSpinKind::Mini)
        {
            static constexpr int64 MiniScores[] = { 100, 200, 400, 800, 1600 };
            return MiniScores[ClampedLines];
        }
        return 0;
    }

    inline bool IsBlitzSpinRecognized(ETotorisMino Mino, ETotorisSpinKind SpinKind,
        bool bThreeCornerTSpin)
    {
        return Mino == ETotorisMino::T && SpinKind != ETotorisSpinKind::None &&
            bThreeCornerTSpin;
    }

    inline bool IsBackToBackEligible(bool bRecognizedSpin, int32 Lines)
    {
        return Lines > 0 && (Lines >= 4 || bRecognizedSpin);
    }

    inline FPlacementScore CalculatePlacement(ETotorisClassicMode Mode,
        ETotorisSpinKind SpinKind, int32 ClearedLines, bool bSpinRecognized,
        bool bApplyBackToBack, int32 ComboCount, bool bAllClear, int32 BlitzLevel)
    {
        FPlacementScore Result;
        const int64 ScoreLevel = Mode == ETotorisClassicMode::Blitz
            ? FMath::Max(1, BlitzLevel) : 1;
        const int64 BaseScore = bSpinRecognized
            ? SpinBaseScore(SpinKind, ClearedLines)
            : NormalClearBaseScore(ClearedLines);
        const int64 ScaledBaseScore = BaseScore * ScoreLevel;

        if (bSpinRecognized)
        {
            Result.SpinScore = ScaledBaseScore;
        }
        else
        {
            Result.LineClearScore = ScaledBaseScore;
        }
        // 1.5x is represented as the separately reportable extra half.
        if (bApplyBackToBack && ClearedLines > 0)
        {
            Result.BackToBackBonusScore = ScaledBaseScore / 2;
        }
        if (ClearedLines > 0)
        {
            Result.ComboBonusScore = static_cast<int64>(FMath::Max(0, ComboCount)) * 50 * ScoreLevel;
        }
        if (bAllClear)
        {
            Result.AllClearBonusScore = 3500 * ScoreLevel;
        }
        return Result;
    }
}
