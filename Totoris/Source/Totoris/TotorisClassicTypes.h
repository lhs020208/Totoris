#pragma once

#include "CoreMinimal.h"
#include "TotorisGeneration.h"
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

// Raw per-piece input trace for a future finesse evaluator. No scores are calculated here.
UENUM(BlueprintType)
enum class ETotorisFinesseInput : uint8
{
    MoveLeft, MoveRight, AutoMoveLeft, AutoMoveRight,
    RotateCW, RotateCCW, Rotate180,
    SoftDropPress, SoftDropRelease, HardDrop, RejectedHold
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisFinesseInputEvent
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    ETotorisFinesseInput Input = ETotorisFinesseInput::MoveLeft;

    // An attempted movement/rotation is recorded even if collision rejects it.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bSucceeded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    FIntPoint BeforePosition = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    FIntPoint AfterPosition = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    uint8 BeforeRotation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    uint8 AfterRotation = 0;

    // -1 when no kick was involved (or the attempted rotation failed).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    int32 KickIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisPieceInputTrace
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bValid = false;

    // Mino enum index; kept independent of the generation header.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    uint8 MinoIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    FIntPoint SpawnPosition = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    FIntPoint FinalPosition = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    uint8 FinalRotation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bUsedSoftDrop = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bUsedHardDrop = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    TArray<FTotorisFinesseInputEvent> Events;
};

// Stage 3: ordinary-placement judgement for the most recently locked piece.
// Excluded is deliberately different from Fault: spin/tuck/soft-drop and any
// reference path not verified on the real pre-lock board are not graded yet.
UENUM(BlueprintType)
enum class ETotorisFinesseJudgement : uint8
{
    NotEvaluated, Optimal, Fault, Excluded
};

UENUM(BlueprintType)
enum class ETotorisFinesseExclusionReason : uint8
{
    None,
    InvalidTrace,
    Spin,
    SoftDrop,
    RejectedHold,
    PrechargedOrCarriedDAS,
    TimedOrBlockedDAS,
    UnsupportedInput,
    ReferenceNotFound,
    ReferencePathBlocked,
    ReferenceLandingMismatch,
    ActualBelowReference,
    // Stage 4 diagnostics, never counted as a player fault.
    SpecialSpinUnverified,
    DescentDependentPath,
    SpinTraceMismatch
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisPieceFinesseEvaluation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    ETotorisFinesseJudgement Judgement = ETotorisFinesseJudgement::NotEvaluated;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    ETotorisFinesseExclusionReason ExclusionReason = ETotorisFinesseExclusionReason::None;

    // Physical horizontal/rotation key-downs, with the selected 180 cost.
    // -1 means the input trace was invalid and could not be counted.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    int32 ActualInputs = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    int32 MinimumInputs = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    int32 ExcessInputs = 0;
};

// Stage 4 special-placement diagnostics. This is separate from finesse
// scoring: a geometric path is NOT evidence that TETR.IO would grade a spin.
UENUM(BlueprintType)
enum class ETotorisFinesseSpecialKind : uint8
{
    None, TSpinMini, TSpinFull, AllMiniPlusSpin,
    SoftDrop, VerticalKick, ObstacleDependent, DescentDependent
};

USTRUCT(BlueprintType)
struct TOTORIS_API FTotorisPieceSpecialAnalysis
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    ETotorisFinesseSpecialKind Kind = ETotorisFinesseSpecialKind::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    ETotorisSpinKind DetectedSpin = ETotorisSpinKind::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bUsedSoftDrop = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bUsed180 = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bHadVerticalKick = false;

    // Whether the final successful rotation in the trace really ends at the
    // detected spin position. Gravity without an input event is not inferred.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bFinalSpinRotationVerified = false;

    // Advisory path search on the real pre-lock board. Downward transitions
    // have zero finesse cost; timing / TETR.IO equivalence is NOT proven.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bAdvisoryPathFound = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    bool bAdvisoryPathUsesDescent = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Finesse")
    int32 AdvisoryMinimumInputs = INDEX_NONE;
};

// Storage for the future Game Clear OVERVIEW and FULL pages.
// KeysPressed and Holds are recorded; other future fields remain unpopulated.
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

    // OVERVIEW: KeysPressed and Holds count input; derived rates are not calculated yet.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    int32 KeysPressed = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    double KeysPerPiece = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    double KeysPerSecond = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Input")
    int32 Holds = 0;

    // Finesse run aggregates are reserved for Stage 5; Stage 3 judges one piece only.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Finesse")
    double FinessePercent = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Finesse")
    int32 FinesseFaults = 0;

    // False distinguishes "not yet measured" from a genuine 0% result.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics|Finesse")
    bool bFinesseMeasured = false;
};
