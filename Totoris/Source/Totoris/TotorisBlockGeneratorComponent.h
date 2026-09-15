#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TotorisGeneration.h"
#include "TotorisBlockGeneratorComponent.generated.h"

class UInputComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class APlayerController;

UCLASS(ClassGroup=(Totoris), meta=(BlueprintSpawnableComponent))
class TOTORIS_API UTotorisBlockGeneratorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTotorisBlockGeneratorComponent();

	// Gameplay is intentionally inactive at level start so the main menu can be shown
	// over the existing room without the Tetris simulation running in the background.
	UFUNCTION(BlueprintCallable, Category="Totoris|Gameplay")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category="Totoris|Gameplay")
	void StopGame();

	UFUNCTION(BlueprintCallable, Category="Totoris|Gameplay")
	void SetGameplayVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category="Totoris|Gameplay")
	bool IsGameplayActive() const { return bGameplayActive; }

	// Runtime handling. Lock delay remains fixed at 500 ms.
	UFUNCTION(BlueprintCallable, Category="Totoris|Handling")
	void ApplyHandlingSettings(
		int32 InARRMilliseconds,
		int32 InDASMilliseconds,
		int32 InDCDMilliseconds,
		int32 InSDFMultiplier,
		bool bInSDFInfinite);

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	int32 GetARRMilliseconds() const { return HorizontalARRMilliseconds; }

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	int32 GetDASMilliseconds() const { return HorizontalDASMilliseconds; }

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	int32 GetDCDMilliseconds() const { return HorizontalDCDMilliseconds; }

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	int32 GetSDFMultiplier() const { return SoftDropMultiplier; }

	UFUNCTION(BlueprintPure, Category="Totoris|Handling")
	bool IsSDFInfinite() const { return bSoftDropInfinite; }

	// In-place game reset for spawn inspection; only the first bag is rotated.
	UFUNCTION(BlueprintCallable, Category="Totoris|Debug")
	void DebugRestart();

	UPROPERTY(EditAnywhere, Category="Totoris|Layout", meta=(ClampMin="1"))
	float CellSize = 10.f;

	UPROPERTY(EditAnywhere, Category="Totoris|Layout")
	FVector2D NextCenter = FVector2D(80.f, 11.5f);

	UPROPERTY(EditAnywhere, Category="Totoris|Layout", meta=(ClampMin="1"))
	float NextSlotSpacing = 30.f;

	UPROPERTY(EditAnywhere, Category="Totoris|Debug")
	bool bUseFixedSeed = false;

	UPROPERTY(EditAnywhere, Category="Totoris|Debug")
	int32 FixedSeed = 2026;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	FString FirstBagOrder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	FString ActivePieceName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	TArray<FString> NextPieceNames;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	TArray<FIntPoint> ActiveSpawnCells;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	int32 DebugRestartCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	bool bGameplayActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	bool bGameOver = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	ETotorisMino ActiveMino = ETotorisMino::I;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	int32 ActiveColumn = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	int32 ActiveRow = TotorisGeneration::SpawnTopRow;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	uint8 ActiveRotation = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	bool bGrounded = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	ETotorisMino HeldMino = ETotorisMino::I;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	bool bHasHold = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	bool bCanHold = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	ETotorisSpinKind LastSpinKind = ETotorisSpinKind::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	ETotorisMino LastSpinMino = ETotorisMino::I;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	int32 LastClearedLineCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	int32 TotalClearedLines = 0;

	// Human-readable result of the most recent lock, e.g.
	// "Single", "T-Spin Double", "Mini T-Spin Single", "I-Spin Double", "Tetris".
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	FString LastActionName = TEXT("None");

	// TETR.IO-style combo index:
	// -1 = no active combo, 0 = first consecutive line clear,
	// 1 = second consecutive line clear, etc.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	int32 ComboCount = -1;

	// B2B xN value. The first eligible clear establishes the chain;
	// the second consecutive eligible clear becomes B2B x1.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	int32 BackToBackCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	bool bLastClearWasBackToBack = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
	bool bLastClearWasDifficult = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Totoris|State")
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
	void SetGameOver();
	void ResetActiveActionTracking();
	void MarkTranslation();
	void MarkRotation(bool bWas180, int32 KickIndex);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(EditAnywhere, Category="Totoris|Visuals")
	TObjectPtr<UMaterialInterface> BlockMaterial;

	UPROPERTY(EditAnywhere, Category="Totoris|Layout")
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
	TObjectPtr<UInputComponent> RestartInput;

	TWeakObjectPtr<APlayerController> InputController;
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
	static constexpr float GravityCellsPerSecond = 1.2f;
	static constexpr float LockDelaySeconds = 0.5f;
	FTotorisSevenBag Sequence;
};
