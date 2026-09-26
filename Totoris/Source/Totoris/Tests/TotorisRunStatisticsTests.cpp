#include "../TotorisRunStatistics.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisRunStatisticsFinesseTest,
    "Totoris.Statistics.FinesseAggregation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisRunStatisticsFinesseTest::RunTest(const FString& Parameters)
{
    FTotorisRunStatistics Stats;
    TestFalse(TEXT("No graded pieces is unmeasured"), Stats.bFinesseMeasured);
    TestEqual(TEXT("Initial finesse percent is zero"), Stats.FinessePercent, 0.0);

    FTotorisPieceFinesseEvaluation Excluded;
    Excluded.Judgement = ETotorisFinesseJudgement::Excluded;
    Excluded.ExclusionReason = ETotorisFinesseExclusionReason::SpecialSpinUnverified;
    TotorisRunStatistics::AccumulateFinesse(Stats, Excluded);
    TestEqual(TEXT("Excluded spin is counted independently"), Stats.FinesseExcludedPieces, 1);
    TestEqual(TEXT("Excluded spin does not enter denominator"), Stats.FinesseEvaluatedPieces, 0);
    TestFalse(TEXT("All-excluded run is unmeasured"), Stats.bFinesseMeasured);

    FTotorisPieceFinesseEvaluation Optimal;
    Optimal.Judgement = ETotorisFinesseJudgement::Optimal;
    TotorisRunStatistics::AccumulateFinesse(Stats, Optimal);
    TestEqual(TEXT("Optimal piece enters denominator"), Stats.FinesseEvaluatedPieces, 1);
    TestEqual(TEXT("Optimal piece enters numerator"), Stats.FinesseOptimalPieces, 1);
    TestTrue(TEXT("One graded piece enables measurement"), Stats.bFinesseMeasured);
    TestEqual(TEXT("One optimal piece is 100 percent"), Stats.FinessePercent, 100.0);

    FTotorisPieceFinesseEvaluation Fault;
    Fault.Judgement = ETotorisFinesseJudgement::Fault;
    Fault.ExcessInputs = 3;
    TotorisRunStatistics::AccumulateFinesse(Stats, Fault);
    TestEqual(TEXT("Fault enters denominator"), Stats.FinesseEvaluatedPieces, 2);
    TestEqual(TEXT("One faulty piece means exactly one fault"), Stats.FinesseFaults, 1);
    TestEqual(TEXT("Extra inputs are tracked separately"), Stats.FinesseExcessInputs, 3);
    TestEqual(TEXT("One optimal of two graded is 50 percent"), Stats.FinessePercent, 50.0);

    FTotorisPieceFinesseEvaluation NotEvaluated;
    TotorisRunStatistics::AccumulateFinesse(Stats, NotEvaluated);
    TestEqual(TEXT("NotEvaluated tracked separately"), Stats.FinesseNotEvaluatedPieces, 1);
    TestEqual(TEXT("Excluded and ungraded leave denominator alone"), Stats.FinesseEvaluatedPieces, 2);
    TestEqual(TEXT("Excluded and ungraded leave percent alone"), Stats.FinessePercent, 50.0);
    TestEqual(TEXT("One event counted per placement"),
        Stats.FinesseEvaluatedPieces + Stats.FinesseExcludedPieces + Stats.FinesseNotEvaluatedPieces, 4);

    FTotorisRunStatistics Reset;
    Stats = Reset;
    TestFalse(TEXT("Run reset removes finesse measurement"), Stats.bFinesseMeasured);
    TestEqual(TEXT("Run reset removes previous faults"), Stats.FinesseFaults, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTotorisRunStatisticsResultTest,
    "Totoris.Statistics.FinishedSummary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTotorisRunStatisticsResultTest::RunTest(const FString& Parameters)
{
    FTotorisRunStatistics Stats;
    Stats.KeysPressed = 90;
    Stats.Holds = 7;
    FTotorisPieceFinesseEvaluation Optimal;
    Optimal.Judgement = ETotorisFinesseJudgement::Optimal;
    FTotorisPieceFinesseEvaluation Fault;
    Fault.Judgement = ETotorisFinesseJudgement::Fault;
    Fault.ExcessInputs = 2;
    TotorisRunStatistics::AccumulateFinesse(Stats, Optimal);
    TotorisRunStatistics::AccumulateFinesse(Stats, Fault);

    FTotorisClassicSettings Settings;
    Settings.Mode = ETotorisClassicMode::Sprint;
    Settings.TargetLines = 40;
    const auto Snapshot = TotorisRunStatistics::MakeFinishedSummary(
        Settings, ETotorisRunResult::Completed, 30, 40, 60.0, 0, Stats);
    TestTrue(TEXT("Completed run yields valid snapshot"), Snapshot.bValid);
    TestEqual(TEXT("Mode copied into snapshot"),
        static_cast<int32>(Snapshot.Settings.Mode), static_cast<int32>(ETotorisClassicMode::Sprint));
    TestEqual(TEXT("Target settings copied"), Snapshot.Settings.TargetLines, 40);
    TestEqual(TEXT("Snapshot placed pieces"), Snapshot.PiecesPlaced, 30);
    TestEqual(TEXT("Snapshot lines"), Snapshot.LinesCleared, 40);
    TestEqual(TEXT("Snapshot elapsed time"), Snapshot.ElapsedSeconds, 60.0);
    TestEqual(TEXT("Snapshot result"), static_cast<int32>(Snapshot.Result),
        static_cast<int32>(ETotorisRunResult::Completed));
    TestTrue(TEXT("Score is calculated"), Snapshot.bScoreCalculated);
    TestEqual(TEXT("Pieces per second"), Snapshot.Statistics.PiecesPerSecond, 0.5);
    TestEqual(TEXT("Lines per minute"), Snapshot.Statistics.LinesPerMinute, 40.0);
    TestEqual(TEXT("Keys per piece"), Snapshot.Statistics.KeysPerPiece, 3.0);
    TestEqual(TEXT("Keys per second"), Snapshot.Statistics.KeysPerSecond, 1.5);
    TestEqual(TEXT("Finesse percent survives snapshot"), Snapshot.Statistics.FinessePercent, 50.0);
    TestEqual(TEXT("Holds survive snapshot"), Snapshot.Statistics.Holds, 7);

    // A frozen result must not reference the mutable working counters.
    Stats.KeysPressed = 200;
    TestEqual(TEXT("Snapshot does not change with active counters"), Snapshot.Statistics.KeysPressed, 90);

    const auto NoResult = TotorisRunStatistics::MakeFinishedSummary(
        Settings, ETotorisRunResult::None, 30, 40, 60.0, 0, Stats);
    TestFalse(TEXT("In-progress run cannot be a finished summary"), NoResult.bValid);

    auto Zero = FTotorisRunStatistics{};
    Zero.KeysPressed = 5;
    TotorisRunStatistics::UpdateDerivedRates(Zero, 0, 0, 0.0);
    TestEqual(TEXT("Zero time gives no pieces/sec"), Zero.PiecesPerSecond, 0.0);
    TestEqual(TEXT("Zero time gives no keys/sec"), Zero.KeysPerSecond, 0.0);
    TestEqual(TEXT("Zero pieces gives no keys/piece"), Zero.KeysPerPiece, 0.0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
