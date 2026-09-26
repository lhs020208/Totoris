#pragma once

#include "TotorisClassicTypes.h"

// Pure aggregation helpers. They do not change gameplay, emit events or
// decide whether an excluded special placement should be graded.
namespace TotorisRunStatistics
{
    inline void AccumulateFinesse(FTotorisRunStatistics& Statistics,
        const FTotorisPieceFinesseEvaluation& Evaluation)
    {
        switch (Evaluation.Judgement)
        {
        case ETotorisFinesseJudgement::Optimal:
            ++Statistics.FinesseEvaluatedPieces;
            ++Statistics.FinesseOptimalPieces;
            break;
        case ETotorisFinesseJudgement::Fault:
            ++Statistics.FinesseEvaluatedPieces;
            ++Statistics.FinesseFaults; // Per faulty piece, NOT per excess input.
            Statistics.FinesseExcessInputs += FMath::Max(0, Evaluation.ExcessInputs);
            break;
        case ETotorisFinesseJudgement::Excluded:
            ++Statistics.FinesseExcludedPieces;
            break;
        case ETotorisFinesseJudgement::NotEvaluated:
        default:
            ++Statistics.FinesseNotEvaluatedPieces;
            break;
        }

        Statistics.bFinesseMeasured = Statistics.FinesseEvaluatedPieces > 0;
        Statistics.FinessePercent = Statistics.bFinesseMeasured
            ? 100.0 * static_cast<double>(Statistics.FinesseOptimalPieces) /
                static_cast<double>(Statistics.FinesseEvaluatedPieces)
            : 0.0;
    }

    inline void UpdateDerivedRates(FTotorisRunStatistics& Statistics,
        int32 PlacedPieces, int32 ClearedLines, double ElapsedSeconds)
    {
        const int32 Pieces = FMath::Max(0, PlacedPieces);
        const int32 Lines = FMath::Max(0, ClearedLines);
        Statistics.KeysPerPiece = Pieces > 0
            ? static_cast<double>(Statistics.KeysPressed) / static_cast<double>(Pieces)
            : 0.0;
        if (ElapsedSeconds > 0.0)
        {
            Statistics.PiecesPerSecond = static_cast<double>(Pieces) / ElapsedSeconds;
            Statistics.LinesPerMinute = 60.0 * static_cast<double>(Lines) / ElapsedSeconds;
            Statistics.KeysPerSecond = static_cast<double>(Statistics.KeysPressed) / ElapsedSeconds;
        }
        else
        {
            Statistics.PiecesPerSecond = 0.0;
            Statistics.LinesPerMinute = 0.0;
            Statistics.KeysPerSecond = 0.0;
        }
    }

    inline FTotorisRunSummary MakeFinishedSummary(
        const FTotorisClassicSettings& Settings,
        ETotorisRunResult Result,
        int32 PlacedPieces, int32 ClearedLines, double ElapsedSeconds,
        int64 Score, const FTotorisRunStatistics& LiveStatistics)
    {
        FTotorisRunSummary Summary;
        if (Result == ETotorisRunResult::None) return Summary;
        Summary.bValid = true;
        Summary.Settings = Settings;
        Summary.Result = Result;
        Summary.PiecesPlaced = PlacedPieces;
        Summary.LinesCleared = ClearedLines;
        Summary.ElapsedSeconds = ElapsedSeconds;
        Summary.Score = Score;
        Summary.bScoreCalculated = false; // Scoring remains unimplemented.
        Summary.Statistics = LiveStatistics;
        UpdateDerivedRates(Summary.Statistics, PlacedPieces, ClearedLines, ElapsedSeconds);
        return Summary;
    }
}
