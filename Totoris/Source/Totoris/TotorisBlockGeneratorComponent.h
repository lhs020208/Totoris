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
	void SpawnMino(ETotorisMino Type);
	void LockActiveMino();
	void UpdateGroundedState();
	void MoveHorizontal(int32 Direction);
	void MoveLeft();
	void MoveRight();
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
	TObjectPtr<UInputComponent> RestartInput;

	TWeakObjectPtr<APlayerController> InputController;
	TSet<FIntPoint> LockedCells;
	TMap<FIntPoint, ETotorisMino> LockedTypes;
	FIntPoint ActivePosition = FIntPoint::ZeroValue;
	float GravityAccumulator = 0.f;
	float LockTimer = 0.f;
	int32 LockResets = 0;
	bool bSoftDropHeld = false;
	static constexpr int32 MaxLogicalRows = 40;
	static constexpr float GravityCellsPerSecond = 1.2f;
	static constexpr float SoftDropCellsPerSecond = 7.2f;
	static constexpr float LockDelaySeconds = 0.5f;
	FTotorisSevenBag Sequence;
};
