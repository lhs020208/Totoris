#pragma once

#include "CoreMinimal.h"
#include "TotorisClassicTypes.generated.h"

UENUM(BlueprintType)
enum class ETotorisClassicMode : uint8
{
    Endless UMETA(DisplayName="Endless"),
    Sprint UMETA(DisplayName="Sprint"),
    Blitz UMETA(DisplayName="Blitz"),
    CheeseRace UMETA(DisplayName="Cheese Race")
};

UENUM(BlueprintType)
enum class ETotorisRunResult : uint8
{
    None UMETA(DisplayName="In Progress"),
    Completed UMETA(DisplayName="Completed"),
    ToppedOut UMETA(DisplayName="Topped Out"),
    TimeExpired UMETA(DisplayName="Time Expired")
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisClassicSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic")
    ETotorisClassicMode Mode = ETotorisClassicMode::Endless;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic", meta=(ClampMin="1", ClampMax="1000"))
    int32 TargetLines = 40;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic", meta=(ClampMin="1", ClampMax="359"))
    int32 LimitTimeSeconds = 120;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Totoris|Classic", meta=(ClampMin="1", ClampMax="100"))
    int32 CheeseCount = 18;
};

// Storage for the future Game Clear OVERVIEW and FULL pages.
// These fields are declarations only: gameplay events do not update them yet.
// PIECES PLACED, LINES, TIME and the reserved Score already live in the
// block-generator component and are intentionally not duplicated here.
USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisRunStatistics
{
    GENERATED_BODY()

    // OVERVIEW: derived rates and end-of-run summaries.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Overview")
    double PiecesPerSecond = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Overview")
    double LinesPerMinute = 0.0;

    // Total full + mini spin events; FULL below keeps both types separately.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Overview")
    int32 TotalSpins = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Overview")
    int32 MaximumCombo = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Overview")
    int32 MaximumBackToBackChain = 0;

    // Shared by OVERVIEW and FULL; do not count it twice.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Overview")
    int32 AllClears = 0;

    // FULL: ordinary line clears (spin clears have separate fields).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 Singles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 Doubles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 Triples = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 Quads = 0;

    // Full/mini spin events, including events that clear no lines.
    // Whether zero-line spins should count can be finalized when recording is added.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 FullSpins = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 SpinMinis = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 SpinMiniSingles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 SpinSingles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 SpinMiniDoubles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 SpinDoubles = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 SpinMiniTriples = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Full")
    int32 SpinTriples = 0;

    // OVERVIEW: input-related values. Their input hooks are not implemented.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    int32 KeysPressed = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    double KeysPerPiece = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    double KeysPerSecond = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    int32 Holds = 0;

    // Finesse needs a separate ruleset/measurement implementation.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Finesse")
    double FinessePercent = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Finesse")
    int32 FinesseFaults = 0;

    // False distinguishes "not yet measured" from a genuine 0% result.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Finesse")
    bool bFinesseMeasured = false;
};
