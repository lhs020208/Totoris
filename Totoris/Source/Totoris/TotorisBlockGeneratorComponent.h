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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BuildRenderComponents();
	void SpawnFirstAndPreview();
	void AddBlock(ETotorisMino Type, float Right, float Up);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(EditAnywhere, Category="Totoris|Visuals")
	TObjectPtr<UMaterialInterface> BlockMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Bodies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Faces;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> RestartInput;

	TWeakObjectPtr<APlayerController> InputController;
	FTotorisSevenBag Sequence;
};
