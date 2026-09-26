
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "TotorisGeneration.h"
#include "TotorisClassicTypes.h"
#include "TotorisBlockGeneratorComponent.generated.h"

class UInputComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class APlayerController;

// Fired once per finished run. The summary is also retained after StopGame.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTotorisRunFinishedSignature,
    const FTotorisRunSummary&, Summary);

UCLASS(ClassGroup = (Totoris), meta = (BlueprintSpawnableComponent))
class TOTORIS_API UTotorisBlockGeneratorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTotorisBlockGeneratorComponent();

    // Gameplay is intentionally inactive at level start so the main menu can be shown
    // over the existing room without the Tetris simulation running in the background.
    // Called before StartGame; other common setup options remain pending until
    // garbage attack/countdown rules are specified and implemented.
    void ConfigureClassicGame(const FTotorisClassicSettings& Settings,
        bool bInStartGravity, bool bInGravityIncrease, bool bInCheeseGarbage = false,
        bool bInQuickStart = false);

    UFUNCTION(BlueprintCallable, Category = "Totoris|Gameplay")
    void StartGame();

    UFUNCTION(BlueprintCallable, Category = "Totoris|Gameplay")
    void StopGame();

    UFUNCTION(BlueprintCallable, Category = "Totoris|Gameplay")
    void SetGameplayVisible(bool bVisible);

    UFUNCTION(BlueprintPure, Category = "Totoris|Gameplay")
    bool IsGameplayActive() const { return bGameplayActive; }

    // The board is visible during the start countdown, but simulation and
    // gameplay input begin only when GO appears.
    UFUNCTION(BlueprintPure, Category = "Totoris|Gameplay")
    bool IsSimulationActive() const { return bSimulationActive; }

    double GetStartCountdownElapsedSecondsForHUD() const { return StartCountdownElapsedSeconds; }
    bool IsQuickStartEnabledForHUD() const { return bConfiguredQuickStart; }

    // Native HUD read-only access. These intentionally do not expose a Blueprint
    // HUD API; the separate overlay reads the existing gameplay counters.
    ETotorisClassicMode GetActiveClassicModeForHUD() const { return ClassicSettings.Mode; }
    double GetElapsedSecondsForHUD() const { return ElapsedSeconds; }
    int32 GetPlacedPieceCountForHUD() const { return PlacedPieceCount; }
    int32 GetClearedLineCountForHUD() const { return TotalClearedLines; }
    int32 GetRemainingSprintLinesForHUD() const { return RemainingSprintLines; }
    int32 GetRemainingCheeseLinesForHUD() const { return RemainingCheeseLines; }
    double GetRemainingBlitzSecondsForHUD() const
    {
        return FMath::Max(0.0, static_cast<double>(ClassicSettings.LimitTimeSeconds) - ElapsedSeconds);
    }

    // Kept independently of scoring so future Blitz score multipliers can use
    // the exact level that governed the currently falling piece.
    UFUNCTION(BlueprintPure, Category="Totoris|Gravity")
    int32 GetBlitzLevel() const { return BlitzLevel; }

    UFUNCTION(BlueprintPure, Category="Totoris|Gravity")
    float GetCurrentGravityG() const;

    // Current statistics; the derived rates are calculated from live counters.
    UFUNCTION(BlueprintPure, Category="Totoris|Statistics")
    FTotorisRunStatistics GetRunStatistics() const;

    // A finished run has a stable snapshot even after StopGame hides the board.
    // A new StartGame / DebugRestart invalidates the previous summary.
    UFUNCTION(BlueprintPure, Category="Totoris|Results")
    bool HasFinishedRunSummary() const { return LastFinishedRunSummary.bValid; }

    UFUNCTION(BlueprintPure, Category="Totoris|Results")
    FTotorisRunSummary GetLastFinishedRunSummary() const { return LastFinishedRunSummary; }

    UFUNCTION(BlueprintPure, Category="Totoris|Results")
    ETotorisRunResult GetRunResult() const { return RunResult; }

    UPROPERTY(BlueprintAssignable, Category="Totoris|Results")
    FTotorisRunFinishedSignature OnRunFinished;

    // Debug / future finesse evaluator: current piece and most recently locked piece.
    UFUNCTION(BlueprintPure, Category="Totoris|Finesse")
    FTotorisPieceInputTrace GetCurrentPieceInputTrace() const { return CurrentPieceInputTrace; }

    UFUNCTION(BlueprintPure, Category="Totoris|Finesse")
    FTotorisPieceInputTrace GetLastLockedPieceInputTrace() const { return LastLockedPieceInputTrace; }

    // Latest ordinary or special-placement result. Excluded/NotEvaluated
    // never count as faults or enter the measured finesse denominator.
    UFUNCTION(BlueprintPure, Category="Totoris|Finesse")
    FTotorisPieceFinesseEvaluation GetLastLockedPieceFinesseEvaluation() const
    {
        return LastLockedPieceFinesseEvaluation;
    }

    UFUNCTION(BlueprintPure, Category="Totoris|Finesse")
    FTotorisPieceSpecialAnalysis GetLastLockedPieceSpecialAnalysis() const
    {
        return LastLockedPieceSpecialAnalysis;
    }


    // Score is already reserved in this component; do not duplicate it in
    // FTotorisRunStatistics until an actual scoring policy is implemented.
    UFUNCTION(BlueprintPure, Category="Totoris|Statistics")
    int64 GetScoreForResults() const { return Score; }

    // Runtime handling. Lock delay remains fixed at 500 ms.
    UFUNCTION(BlueprintCallable, Category = "Totoris|Handling")
    void ApplyHandlingSettings(
        int32 InARRMilliseconds,
        int32 InDASMilliseconds,
        int32 InDCDMilliseconds,
        int32 InSDFMultiplier,
        bool bInSDFInfinite);

    UFUNCTION(BlueprintPure, Category = "Totoris|Handling")
    int32 GetARRMilliseconds() const { return HorizontalARRMilliseconds; }

    UFUNCTION(BlueprintPure, Category = "Totoris|Handling")
    int32 GetDASMilliseconds() const { return HorizontalDASMilliseconds; }

    UFUNCTION(BlueprintPure, Category = "Totoris|Handling")
    int32 GetDCDMilliseconds() const { return HorizontalDCDMilliseconds; }

    UFUNCTION(BlueprintPure, Category = "Totoris|Handling")
    int32 GetSDFMultiplier() const { return SoftDropMultiplier; }

    UFUNCTION(BlueprintPure, Category = "Totoris|Handling")
    bool IsSDFInfinite() const { return bSoftDropInfinite; }

    // Runtime key bindings supplied by ATotorisPlayerController. Each action
    // accepts up to three keys. Empty/invalid FKeys are ignored.
    void ApplyKeyBindings(
        const TArray<FKey>& InMoveLeftKeys,
        const TArray<FKey>& InMoveRightKeys,
        const TArray<FKey>& InSoftDropKeys,
        const TArray<FKey>& InHardDropKeys,
        const TArray<FKey>& InRotateCWKeys,
        const TArray<FKey>& InRotateCCWKeys,
        const TArray<FKey>& InRotate180Keys,
        const TArray<FKey>& InHoldKeys,
        const TArray<FKey>& InRestartKeys);

    // In-place game reset for spawn inspection with a fresh opening bag.
    UFUNCTION(BlueprintCallable, Category = "Totoris|Debug")
    void DebugRestart();

    UPROPERTY(EditAnywhere, Category = "Totoris|Layout", meta = (ClampMin = "1"))
    float CellSize = 10.f;

    UPROPERTY(EditAnywhere, Category = "Totoris|Layout")
    FVector2D NextCenter = FVector2D(80.f, 11.5f);

    UPROPERTY(EditAnywhere, Category = "Totoris|Layout", meta = (ClampMin = "1"))
    float NextSlotSpacing = 30.f;

    UPROPERTY(EditAnywhere, Category = "Totoris|Debug")
    bool bUseFixedSeed = false;

    UPROPERTY(EditAnywhere, Category = "Totoris|Debug")
    int32 FixedSeed = 2026;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    FString FirstBagOrder;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    FString ActivePieceName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    TArray<FString> NextPieceNames;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    TArray<FIntPoint> ActiveSpawnCells;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    int32 DebugRestartCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bGameplayActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bSimulationActive = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bGameOver = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    ETotorisMino ActiveMino = ETotorisMino::I;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    int32 ActiveColumn = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    int32 ActiveRow = TotorisGeneration::SpawnTopRow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    uint8 ActiveRotation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bGrounded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    ETotorisMino HeldMino = ETotorisMino::I;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bHasHold = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bCanHold = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    ETotorisSpinKind LastSpinKind = ETotorisSpinKind::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    ETotorisMino LastSpinMino = ETotorisMino::I;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    int32 LastClearedLineCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    int32 TotalClearedLines = 0;

    // Human-readable result of the most recent lock, e.g.
    // "Single", "T-Spin Double", "Mini T-Spin Single", "I-Spin Double", "Tetris".
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    FString LastActionName = TEXT("None");

    // TETR.IO-style combo index:
    // -1 = no active combo, 0 = first consecutive line clear,
    // 1 = second consecutive line clear, etc.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    int32 ComboCount = -1;

    // B2B xN value. The first eligible clear establishes the chain;
    // the second consecutive eligible clear becomes B2B x1.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    int32 BackToBackCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bLastClearWasBackToBack = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bLastClearWasDifficult = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Totoris|State")
    bool bLastPerfectClear = false;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void BuildRenderComponents();
    void SpawnFirstAndPreview();
    void RebuildRender();
    void AddBlock(ETotorisMino Type, float Right, float Up);
    void AddLogicalBlock(ETotorisMino Type, const FIntPoint& Cell);
    void AddGarbageBlock(const FIntPoint& Cell);
    void DrawPreviewPiece(ETotorisMino Type, const FVector2D& Center);
    TArray<FIntPoint> ActiveCells() const;
    bool IsValidPosition(ETotorisMino Type, const FIntPoint& Position, uint8 Rotation) const;
    FIntPoint GetGhostPosition() const;
    void AddGhostBlock(ETotorisMino Type, const FIntPoint& Cell);
    void SpawnMino(ETotorisMino Type);
    void LockActiveMino();
    int32 ClearCompletedLines();
    void UpdateGroundedState();
    void MoveHorizontal(int32 Direction);
    void MoveHorizontalToWall(int32 Direction);
    void HorizontalLeftPressed();
    void HorizontalLeftReleased();
    void HorizontalRightPressed();
    void HorizontalRightReleased();
    void TickCountdownHorizontalCharge(float DeltaSeconds);
    void TickHorizontalHandling(float DeltaSeconds);
    void StartDCD();
    void Rotate(int32 Direction);
    void RotateCCW();
    void RotateCW();
    void Rotate180();
    void SoftDropPressed();
    void SoftDropReleased();
    void HardDrop();
    void Hold();
    void TickGravity(float DeltaSeconds);
    bool UsesTimeBasedGravity() const;
    float ResolveGravityForNewPiece() const;
    void UpdateBlitzLevelFromClearedLines();
    void SettleActiveMinoForMaxGravity();
    void SetGameOver();
    void ResetActiveActionTracking();
    void BeginPieceInputTrace();
    void RecordPieceInput(ETotorisFinesseInput Input, const FIntPoint& BeforePosition,
        uint8 BeforeRotation, bool bSucceeded, int32 KickIndex = INDEX_NONE);

    void MarkTranslation();
    void MarkRotation(bool bWas180, int32 KickIndex);
    void InitializeDefaultKeyBindings();
    void RebuildInputBindings();

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY(EditAnywhere, Category = "Totoris|Visuals")
    TObjectPtr<UMaterialInterface> BlockMaterial;

    UPROPERTY(EditAnywhere, Category = "Totoris|Layout")
    FVector2D HoldCenter = FVector2D(-80.f, 77.5f);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> Bodies;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> Faces;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> GhostBodies;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> GhostFaces;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> GarbageBody;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> GarbageFace;

    UPROPERTY(Transient)
    TObjectPtr<UInputComponent> RestartInput;

    TWeakObjectPtr<APlayerController> InputController;
    TArray<FKey> MoveLeftKeys;
    TArray<FKey> MoveRightKeys;
    TArray<FKey> SoftDropKeys;
    TArray<FKey> HardDropKeys;
    TArray<FKey> RotateCWKeys;
    TArray<FKey> RotateCCWKeys;
    TArray<FKey> Rotate180Keys;
    TArray<FKey> HoldKeys;
    TArray<FKey> RestartKeys;

    // All garbage cells (cheese race rows + incoming attack rows).
    TSet<FIntPoint> GarbageCells;

    // A cheese cell remains marked after player cells complete its row.
    TSet<FIntPoint> CheeseCells;
    FTotorisClassicSettings ClassicSettings;
    ETotorisRunResult RunResult = ETotorisRunResult::None;
    double ElapsedSeconds = 0.0;
    double StartCountdownElapsedSeconds = 0.0;
    // Blitz owns this level state.  It is intentionally not inferred from
    // score, because scoring is a later system.
    int32 BlitzLevel = 1;
    // Blitz level changes take effect only when the next mino spawns.
    float ActiveGravityG = 0.020f;
    int64 Score = 0; // Reserved: no scoring rules defined yet.
    // Key presses, HOLDs and finesse aggregate during play. Derived rates
    // are computed by GetRunStatistics / the finished-run snapshot.
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Statistics", meta=(AllowPrivateAccess="true"))
    FTotorisRunStatistics RunStatistics;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Totoris|Results", meta=(AllowPrivateAccess="true"))
    FTotorisRunSummary LastFinishedRunSummary;
    // Only the active and last locked piece are retained; no unbounded run history.
    FTotorisPieceInputTrace CurrentPieceInputTrace;
    FTotorisPieceInputTrace LastLockedPieceInputTrace;
    FTotorisPieceFinesseEvaluation LastLockedPieceFinesseEvaluation;
    FTotorisPieceSpecialAnalysis LastLockedPieceSpecialAnalysis;

    int32 PlacedPieceCount = 0;
    int32 RemainingSprintLines = 0;
    int32 RemainingCheeseLines = 0;
    int32 QueuedCheeseLines = 0;
    int32 CheeseHoleColumn = INDEX_NONE;
    bool bConfiguredStartGravity = true;
    bool bConfiguredGravityIncrease = false;
    bool bConfiguredQuickStart = false;

    // External attacks are queued by segment; no attack simulation is performed here.
public:
    UFUNCTION(BlueprintCallable, Category = "Totoris|Garbage")
    void QueueIncomingGarbage(int32 Lines);

    UFUNCTION(BlueprintPure, Category = "Totoris|Garbage")
    int32 GetPendingGarbageLines() const;

private:
    TArray<int32> PendingGarbageSegments;
    bool bIncomingCheeseGarbage = false;
    FRandomStream GarbageRandom;
    int32 CheeseRowsOnBoard = 0;

    void InitializeCheeseBoard();
    bool InjectGarbageRow(int32 Hole, bool bCheeseRaceRow);
    void RefillCheeseBoard();
    void ApplyPendingGarbage();
    int32 RandomGarbageHole();
    void CompleteRun(ETotorisRunResult Result);
    FVector InitialOwnerScale = FVector::OneVector;
    double EndPresentationElapsedSeconds = 0.0;

    TSet<FIntPoint> LockedCells;
    TMap<FIntPoint, ETotorisMino> LockedTypes;
    FIntPoint ActivePosition = FIntPoint::ZeroValue;
    float GravityAccumulator = 0.f;
    float LockTimer = 0.f;
    int32 LockResets = 0;
    bool bSoftDropHeld = false;
    bool bLeftHeld = false;
    bool bRightHeld = false;
    int32 ActiveHorizontalDirection = 0;
    float HorizontalHeldSeconds = 0.f;
    float HorizontalARRAccumulator = 0.f;
    float DCDRemainingSeconds = 0.f;
    bool bLastActionWasRotation = false;
    bool bLastRotationWas180 = false;
    int32 LastRotationKickIndex = INDEX_NONE;

    // Number of consecutive B2B-eligible clears, including the starter.
    // BackToBackCount is max(0, DifficultClearStreak - 1).
    int32 DifficultClearStreak = 0;

    // Runtime-configurable handling values.
    // ARR: 0..83 ms, DAS: 17..333 ms, DCD: 0..333 ms.
    // SDF: 5X..40X, or infinite. Lock delay intentionally stays fixed.
    int32 HorizontalARRMilliseconds = 33;
    int32 HorizontalDASMilliseconds = 167;
    int32 HorizontalDCDMilliseconds = 17;
    int32 SoftDropMultiplier = 6;
    bool bSoftDropInfinite = false;

    static constexpr int32 MaxLogicalRows = 40;
    static constexpr float BaseGravityG = 0.020f;
    static constexpr float BlitzInitialGravityG = 0.0167f;
    static constexpr float TimeGravityIncreasePerSecondG = 0.0005f;
    static constexpr float MaximumGravityG = 20.0f;
    static constexpr float LockDelaySeconds = 0.5f;

    FTotorisSevenBag Sequence;
};
