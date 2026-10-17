#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AnjanathDropEventComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

struct FAnjanathDustParticle
{
	TObjectPtr<UStaticMeshComponent> Component;
	TObjectPtr<UMaterialInstanceDynamic> Material;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
	float Lifetime = 0.f;
	float StartScale = 0.f;
	bool bActive = false;
};

/** Drops the temporary Anjanath placeholder through the ceiling with a dust burst. */
UCLASS(ClassGroup=(Totoris), meta=(BlueprintSpawnableComponent))
class TOTORIS_API UAnjanathDropEventComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAnjanathDropEventComponent();

	void InitializeDrop();

	UFUNCTION(BlueprintCallable, Category="Totoris|Anjanath")
	void PlayDrop();

	UFUNCTION(BlueprintPure, Category="Totoris|Anjanath")
	bool IsDropPlaying() const { return bPlaying; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CreateDustPool();
	void SpawnCeilingDust();
	void UpdateDust(float DeltaTime);

	FTransform LandingTransform;
	FVector CeilingBreakLocation = FVector::ZeroVector;
	// Number of dust planes spawned when the ceiling breaks (was fixed at 64).
	// Set this before InitializeDrop; changing it later does not resize the pool.
	UPROPERTY(EditAnywhere, Category="Totoris|Anjanath|Dust", meta=(ClampMin="1", ClampMax="512", UIMin="16", UIMax="512"))
	int32 DustParticleCount = 192;

	TArray<FAnjanathDustParticle> DustParticles;
	float ElapsedSeconds = 0.f;
	bool bInitialized = false;
	bool bPlaying = false;

	static constexpr float FallDurationSeconds = 1.15f;
	static constexpr float CeilingClearance = 140.f;
};
