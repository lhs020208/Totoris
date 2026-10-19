#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MinecraftBlockEventComponent.generated.h"

class UMinecraftLavaComponent;
class USkeletalMeshComponent;
class UPoseableMeshComponent;
class UAnimSequence;

/**
 * Backwards bridge building with bone-masked animation clips, followed by
 * a free fall. The authored block/model are retained across event restarts.
 */
UCLASS(ClassGroup=(Totoris), meta=(BlueprintSpawnableComponent))
class TOTORIS_API UMinecraftBlockEventComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinecraftBlockEventComponent();

	/** Starts the backwards bridge-building presentation along world Y+. */
	UFUNCTION(BlueprintCallable, Category="Minecraft Event")
	void PlayEvent();

	/** Removes runtime presentation objects and restores the authored model. */
	UFUNCTION(BlueprintCallable, Category="Minecraft Event")
	void StopEvent();

	/** Prepare the authored held block and hand-tip socket in the editor. */
	UFUNCTION(BlueprintCallable, Category="Minecraft Event")
	bool PrepareHeldBlock();

	bool IsEventPlaying() const { return bPlaying; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void SpawnNextBlock();
	void StartLavaOnFloor();
	bool StartAsito();
	bool CacheFootVertices();
	void UpdateAsitoPose();
	void AdvanceAsito(float Seconds);
	FBox GetFootBounds() const;
	void BeginFall();
	TWeakObjectPtr<AActor> AsitoActor;
	TWeakObjectPtr<USkeletalMeshComponent> OriginalMesh;
	TWeakObjectPtr<AActor> HeldBlock;
	UPROPERTY(Transient)
	TObjectPtr<UPoseableMeshComponent> EventMesh;
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Clips;
	// Local foot-sole vertices paired with their rigid skin bone indices.
	TArray<TPair<int32, FVector>> FootVertices;
	TArray<FTransform> ComponentPose;
	FTransform AsitoOriginalTransform;
	FVector FallOrigin = FVector::ZeroVector;
	float BlockTop = 0.f;
	float FirstBlockEnd = 0.f;
	float WalkTime = 0.f;
	float WalkStopTime = 0.f;
	float PlaceTime = -1.f;
	float FallTime = 0.f;
	float LandingFloorZ = 0.f;
	bool bHasLandingFloor = false;
	bool bLanded = false;
	bool bFalling = false;
	bool bOriginalVisible = true;
	UPROPERTY(Transient)
	TObjectPtr<UMinecraftLavaComponent> LavaComponent;
	FTransform EventOrigin;

	TArray<TWeakObjectPtr<AActor>> SpawnedBlocks;
	int32 SpawnedBlockCount = 0;
	bool bPlaying = false;

	static constexpr int32 BlocksToSpawn = 3;
	static constexpr float MoveSpeedCentimetres = 100.f;
	static constexpr float BlockStepCentimetres = 100.f;
};
